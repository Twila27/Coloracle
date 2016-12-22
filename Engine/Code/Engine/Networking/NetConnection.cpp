#include "Engine/Networking/NetConnection.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Engine/Networking/NetPacket.hpp"
#include "Engine/Memory/BitUtils.hpp"
#include "Engine/Time/Time.hpp"


//--------------------------------------------------------------------------------------------------------------
#define MAX_UNRELIABLES		(10000)
#define MAX_RELIABLES		(10000)
#define MAX_VALID_OFFSET	( sizeof(uint16_t) * NUM_BITS_IN_BYTE ) //2 * 8.



//--------------------------------------------------------------------------------------------------------------
NetConnection::NetConnection( NetSession* ns, NetConnectionIndex index, const char* guid, sockaddr_in addr )
	: m_session( ns )
	, m_isABadConnection( false )
	, m_highestReceivedAck( INVALID_PACKET_ACK ) //UnsignedGreaterThan treats as "-1", thus immediately supplanted by ack 0.
	, m_previousReceivedAcksAsBitfield( 0 )
	, m_nextSentAck( 0 )
	, m_lastReceivedAck( 0 )
	, m_lastConfirmedAck( 0 )
	, m_nextSentReliableID( 0 )
	, m_nextExpectedReliableID( 0 )
	, m_oldestUnconfirmedReliableID( 0 )
	, m_secondsSinceLastRecv( -1.0 )
	, m_secondsSinceLastSend( -1.0 )
	, m_lastRecvTimeSeconds( GetCurrentTimeSeconds() )
	, m_lastSendTimeSeconds( GetCurrentTimeSeconds() )
{
	m_connectionInfo.address = addr;
	m_connectionInfo.connectionIndex = index;
	for ( int i = 0; i < MAX_GUID_LENGTH; i++ ) m_connectionInfo.guid[ i ] = guid[ i ];
	m_unreliablesPool.Init( MAX_UNRELIABLES );
	m_reliablesPool.Init( MAX_RELIABLES );

	m_connectionState = ( IsMe() ? CONNECTION_STATE_LOCAL : CONNECTION_STATE_UNCONFIRMED );
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::IsMe() const
{
	sockaddr_in out_sessionSocketAddr;
	m_session->GetAddressObject( &out_sessionSocketAddr );
	return NetSystem::DoSockAddrMatch( out_sessionSocketAddr, this->GetAddressObject() );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::QueueUnreliable( NetMessage& msg )
{
	NetMessage* out_cloneMsg = m_unreliablesPool.Allocate(); //May potentially run out of allocator blocks.
	NetMessage::Duplicate( msg, *out_cloneMsg );

	m_unsentUnreliables.push_back( out_cloneMsg );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::QueueReliable( NetMessage& msg )
{
	NetMessage* out_cloneMsg = m_reliablesPool.Allocate(); //May potentially run out of allocator blocks.
	NetMessage::Duplicate( msg, *out_cloneMsg );

	if ( msg.IsInOrder() )
		out_cloneMsg->SetSequenceID( m_channelData.GetNextSequenceIDToSend() );

	m_unsentReliables.push( out_cloneMsg );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::SendMessageToThem( NetMessage& msg )
{
	bool foundDefn = msg.FinalizeMessageDefinition( m_session );
	if ( !foundDefn )
		return; //Throw out the handler-less message. Above reroute to SendMessageDirect resolves edge case of having 1 thrown-out message hit SendTo below.

	if ( msg.IsReliable() )
		QueueReliable( msg );
	else
		QueueUnreliable( msg );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::SendMessagesToThem( NetMessage msgs[], int numMessages )
{
	for ( int msgIndex = 0; msgIndex < numMessages; msgIndex++ )
		this->SendMessageToThem( msgs[ msgIndex ] );
}



//--------------------------------------------------------------------------------------------------------------
static bool IsMessageStarved( NetMessage* msg )
{
	uint32_t msSinceLastSendAttempt = (uint32_t)GetCurrentTimeSeconds() - msg->GetTimestamp();
	
	const uint32_t millisecondsUntilConsideredStarved = 200U; //.2 seconds for now.

	return ( msSinceLastSendAttempt >= millisecondsUntilConsideredStarved );
}


//--------------------------------------------------------------------------------------------------------------
static bool CanSendAnotherReliable( NetPacket& packet )
{
	//Note this logic only holds when unreliables are known to come after reliables in a packet.
	return ( ( packet.GetTotalAddedMessages() + 1 ) < MAX_RELIABLES_PER_PACKET );
}


//--------------------------------------------------------------------------------------------------------------
uint8_t NetConnection::ResendSentReliables( NetPacket& packet, AckBundle* bundle )
{
	//From the queue of reliables we've already sent--"old" reliables.
	uint8_t numMessagesWritten = 0;

	while ( !m_sentReliables.empty() && CanSendAnotherReliable( packet ) )
	{
		NetMessage* msg = m_sentReliables.front();

		if ( IsReliableConfirmed( msg->GetReliableID() ) )
		{
			m_sentReliables.pop();

			m_reliablesPool.Delete( msg ); //Cleanup even though ID's still in bundle.
				//But we cycle over our array/queue of messages as mentioned above at NetCon::bundles when enough time passes.

			continue;
		}

		if ( IsMessageStarved( msg ) ) //We determine this based on msg.m_secondsSinceSend! Anti-message starvation safeguard.
		{
			m_sentReliables.pop();

			msg->SetTimestamp( (uint32_t)GetCurrentTimeSeconds() * 1000U ); 
			
			bundle->AddReliable( msg->GetReliableID() ); //Anytime we add a message, add its ID to ackBundle.

			packet.WriteMessageToBuffer( *msg, m_session );

			++numMessagesWritten;
		}
		else
		{
			break; //And we call QueueReliable such that if one is not old, all the others after it are also not old.
		}
	}

	return numMessagesWritten;
}


//--------------------------------------------------------------------------------------------------------------
uint8_t NetConnection::SendUnsentReliables( NetPacket& packet, AckBundle* bundle )
{
	uint8_t numMessagesWritten = 0;

	while ( !m_unsentReliables.empty() && CanSendAnotherReliable( packet ) )
	{
		NetMessage* msg = m_unsentReliables.front();
		msg->SetReliableID( GetNextSentReliableID() ); //Increments each Get() call.

		bundle->AddReliable( msg->GetReliableID() );

		bool hadRoomToWrite = packet.WriteMessageToBuffer( *msg, m_session );
		if ( !hadRoomToWrite )
			break; //If you want to be more efficient, go over all unsent to see if any will fit.			
			//i.e. Rather than just break immediately at the first won't-fit instance.

		++numMessagesWritten;

		m_unsentReliables.pop();
		msg->SetTimestamp( (uint32_t)GetCurrentTimeSeconds() * 1000 );
		m_sentReliables.push( msg );
	}

	return numMessagesWritten;
}


//--------------------------------------------------------------------------------------------------------------
uint8_t NetConnection::SendUnreliables( NetPacket& packet )
{
	uint8_t numMessagesWritten = 0;

	TODO( "Compare below to NetSession::SendMessagesDirect from A2 and create a WriteAllMessage to remove repetitions." );
	for ( unsigned int msgIndex = 0; msgIndex < m_unsentUnreliables.size(); msgIndex++ )
	{
		NetMessage& msg = *m_unsentUnreliables[ msgIndex ];

		bool hadEnoughRoom = packet.WriteMessageToBuffer( msg, m_session );
		if ( !hadEnoughRoom ) //sendto and start anew.
			break; //Unlike SendMessagesDirect, we just drop everything after hitting a packet's MTU. The fate of unreliable traffic.

		++numMessagesWritten;
	}

	//Dump the rest out, they will have to be SendMessage'd again or won't get sent.
	for each ( NetMessage* msg in m_unsentUnreliables )
		m_unreliablesPool.Delete( msg );
	m_unsentUnreliables.clear();

	return numMessagesWritten;
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ConstructAndSendPacket()
{
	bool writeSuccess; //Note that unlike recv-side, sending-side fails silently and just doesn't send in NetSession::Update()'s calls to here.
	NetPacket packet;
	PacketHeader ph;
	ph.connectionIndex = m_session->GetMyConnectionIndex();
	ph.ack = GetNextSentAck(); //This will be INVALID_PACKET_ACK for connectionless things like ping commands that use SendMessageDirect.
	ph.highestReceivedAck = m_highestReceivedAck;
	ph.previousReceivedAcksAsBitfield = m_previousReceivedAcksAsBitfield;
	writeSuccess = packet.WritePacketHeader( ph );
	if ( !writeSuccess )
		return;

	AckBundle* bundle = CreateAckBundle( ph.ack ); //These are only for locally tracking packet IDs, i.e. unlike msg reliableIDs they aren't sent.
	ByteBufferBookmark numMsgsBufferOffset = packet.ReserveForWriting<uint8_t>( 0U );

	//Note that return counts from these are just to help debug, packet +1's its numMessages each WriteMessage call.
	if ( !IsConnectionBad() )
	{
		ResendSentReliables( packet, bundle ); //Note we resend first, to cover for any potentially lost packets.
		SendUnsentReliables( packet, bundle );
		SendUnreliables( packet );
		//[Can also send not-so-old reliables here, if room exists and their msg.reliableIDs aren't already in the packet.]
	}

	uint8_t numMessagesInPacket = packet.GetTotalAddedMessages();
	if ( numMessagesInPacket == 0 )
	{
		const double SECONDS_PER_HEARTBEAT = PI;
		if ( m_secondsSinceLastSend < SECONDS_PER_HEARTBEAT )
		{
			m_secondsSinceLastSend += GetCurrentTimeSeconds() - m_lastSendTimeSeconds;
			return; //Don't send 0-msg packets, unless it's a heartbeat.
		}
	}

	packet.WriteAtBookmark( numMsgsBufferOffset, numMessagesInPacket );
	m_session->SendPacket( m_connectionInfo.address, packet ); //Dispatch filled packet.
	m_lastSendTimeSeconds = GetCurrentTimeSeconds(); //For below.
}


//--------------------------------------------------------------------------------------------------------------
uint16_t NetConnection::GetNextSentAck()
{
	//Will need to hop over the INVALID_PACKET_ACK of 0xFFFF. Be mindful of overflow by using CyclicGreaterThan and similar functions.
	uint16_t ack = m_nextSentAck;

	++m_nextSentAck;

	if ( m_nextSentAck == INVALID_PACKET_ACK )
		++m_nextSentAck;

	return ack;
}


//--------------------------------------------------------------------------------------------------------------
uint16_t NetConnection::GetNextSentReliableID()
{
	//Be mindful of overflow by using CyclicGreaterThan and similar functions.
	uint16_t rid = m_nextSentReliableID;

	++m_nextSentReliableID; //No need to hop over a sentinel value like in GetNextAck.

	return rid;
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::CanProcessMessage( NetMessage& msg ) const
{
	if ( msg.IsReliable() )
		return !HasReceivedReliable( msg.GetReliableID() ); //No processing the already-marked!

	return true;
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ProcessInOrder( const NetSender& from, NetMessage& msg )
{
	if ( m_channelData.nextExpectedSequenceID == msg.GetSequenceID() )
	{
//		LogAndShowPrintfWithTag( "InOrderTesting", "Processing seqID %u relID %u.", msg.GetSequenceID(), msg.GetReliableID() );
		msg.Process( from );
		++m_channelData.nextExpectedSequenceID;
		//Need to add code here to go through the OOO vector and remove anything that matches the above, ++ it if so.
		NetMessage* foundMsg = m_channelData.FindAndRemoveMessageForSequenceID( m_channelData.nextExpectedSequenceID );
		while ( foundMsg != nullptr )
		{
//			LogAndShowPrintfWithTag( "InOrderTesting", "Removing from outOfOrderMsgs and Processing seqID %u relID %u.", msg.GetSequenceID(), msg.GetReliableID() );
			foundMsg->Process( from );
			++m_channelData.nextExpectedSequenceID;
			foundMsg = m_channelData.FindAndRemoveMessageForSequenceID( m_channelData.nextExpectedSequenceID );
		}
	}
	else
	{
		NetMessage* out_cloneMsg = m_reliablesPool.Allocate(); //May potentially run out of allocator blocks.
		NetMessage::Duplicate( msg, *out_cloneMsg );
		m_channelData.outOfOrderReceivedSequencedMessages.insert( out_cloneMsg );
//		LogAndShowPrintfWithTag( "InOrderTesting", "Stored seqID %u relID %u in outOfOrderMsgs.", out_cloneMsg->GetSequenceID(), out_cloneMsg->GetReliableID() );
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ProcessMessage( const NetSender& from, NetMessage& msg )
{
	if ( msg.IsInOrder() )
	{
		ProcessInOrder( from, msg );
	}
	else 
	{
		msg.Process( from );
	}
	//Note MarkMessageReceived now moved outside this function into its caller, NetSession::TryProcessPacket.
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::MarkMessageReceived( const NetMessage& msg )
{
	if ( msg.IsReliable() )
	{
		MarkReliableReceived( msg.GetReliableID() );
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::MarkPacketReceived( const PacketHeader& ph )
{
	//Update what we put into the next PacketHeader of acks on response to the recv.
	UpdateHighestReceivedAck( ph.ack/*newValue*/ );

	//Now do some cleanup book-keeping--this is where we mark reliables as not just "I got it" recv, but "I checked it off" CONFIRMED.
	ProcessConfirmedAcks( ph.highestReceivedAck, ph.previousReceivedAcksAsBitfield ); //Basically make sure we're in sync with the other side's acks.
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ProcessConfirmedAcks( uint16_t packetHighestAckReceived, uint16_t packetPreviousAcksBitfield )
{
	ConfirmAck( packetHighestAckReceived );
	uint16_t numBitsInBitfield = sizeof( packetPreviousAcksBitfield ) * NUM_BITS_IN_BYTE;
	for ( uint16_t bitIndex = 0; bitIndex < numBitsInBitfield; ++bitIndex )
	{
		//This checks whether the packet's record of the other side has something in its prevAcksBitfield set that we don't.
		if ( IS_BITFIELD_SET_AT_INDEX( packetPreviousAcksBitfield, bitIndex ) ) //If so, update us at the equivalent slot.
			ConfirmAck( packetHighestAckReceived - ( bitIndex + 1 ) );
	}
}

//--------------------------------------------------------------------------------------------------------------
AckBundle* NetConnection::FindBundle( uint16_t ackID )
{
	for each ( AckBundle& ab in m_ackBundles )
	{
		if ( ab.ackID == ackID )
			return &ab;
	}
	return nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ConfirmAck( uint16_t ack )
{
	if ( ack == INVALID_PACKET_ACK )
		return; //If an ack is invalid, we assume no action needed, primarily for unreliable traffic.

	AckBundle* correspondingBundle = FindBundle( ack );
	if ( correspondingBundle != nullptr )
	{
		for ( unsigned int reliableIdIndex = 0; reliableIdIndex < correspondingBundle->numReliableIDsSent; reliableIdIndex++ )
			MarkReliableConfirmed( correspondingBundle->sentReliableIDs[ reliableIdIndex ] );
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::MarkReliableConfirmed( uint16_t rid )
{
	if ( UnsignedLessThan( rid, m_oldestUnconfirmedReliableID ) )
	{
		return; //We already know it was confirmed--because it's below the oldest unconfirmed we have.
	}
	else if ( rid == m_oldestUnconfirmedReliableID )
	{
		//The main reason we insert rid just to remove it, is to increment m_oldest correctly.
		std::set<uint16_t, UnsignedComparator>::iterator ridIter = m_confirmedReliableIDs.insert( rid ).first; //Iter to inserted key.

		//This may not be as simple as just incrementing once.
		//e.g. m_oldestUnconfirmedReliableID was 3, and now we've received 2 for the first time.
		//So, we need to advance as many times as it takes to find m_oldest.

		//Each time we remove at the oldest, we'll increment oldest for the next go-around, so we'll remove 0, 1, 2, but not 4 if find(3) fails.
		while ( ridIter != m_confirmedReliableIDs.end() )
		{
			m_confirmedReliableIDs.erase( ridIter );
			++m_oldestUnconfirmedReliableID; //Be sure it comes after the erase!
			ridIter = m_confirmedReliableIDs.find( m_oldestUnconfirmedReliableID );
		}
	}
	else //The oldest is staying the oldest, but the new received id exceeds it. We get 3, but still lack 2.
	{
		m_confirmedReliableIDs.insert( rid ); //Could enforce ordered-insert manually on an array, too.
	}

	m_lastConfirmedAck = rid; //Note this is then not updated by the first case.
}


//--------------------------------------------------------------------------------------------------------------
const uint16_t HALF_MAX_UINT16_RANGE = ( 0xFFFF >> 1 ); //65535 div 2.
//Returns whether a > b even when one has overflowed, so long as it's still within half the max-uint16-range of b.
bool UnsignedGreaterThan( uint16_t a, uint16_t b )
{
	//Less general than a MathUtils function, but for uints generally useful if we can assume the half-range trick works (e.g. as for acks).

	uint16_t diff = a - b; //0 when equal, so comment out diff < 0 predicate below to get GreaterThanEquals.

	return ( diff > 0 && diff < HALF_MAX_UINT16_RANGE ); //Remember, diff cannot be < 0 because it's unsigned. 
		//i.e. diff < 0 is only ever filtering out the diff==0 => a==b case.

	//Summary: The problem is that a-b for UNSIGNED ints will never be negative. 
	//Therefore, we assume that if it's over half the range apart, a-b would've been negative for ints.
	/* Example Invocations (Endian mode may be flipped on the hex representations)
		(3,3) = 3-3 = 0 => 0x0000 => F when diff > 0 clause is included, T when left out.
		(3,2) = 3-2 = +1 => 0x0001 => T.
		(2,3) = 2-3 = -1 => 0xFFFF => F, failing 0xFFFF <= 0x7FFF.
		(0,65535) = 0-65535 = +1 again => T, which we want for the wrapping of ack IDs.
		(65535,0) = 65535-0 = 65535 => 0xFFFF again => F. 
			KEY: 65535 here COULD have simply gotten ahead of 0 by 0x7FFF, but we assume this isn't possible for this func's use case.
	*/
}
bool UnsignedGreaterThanOrEqual( uint16_t a, uint16_t b )
{
	uint16_t diff = a - b;
	return ( diff < HALF_MAX_UINT16_RANGE );
}
bool UnsignedLessThan( uint16_t a, uint16_t b )
{
	return !UnsignedGreaterThanOrEqual( a, b );
}
bool UnsignedLessThanOrEqual( uint16_t a, uint16_t b )
{
	return !UnsignedGreaterThan( a, b );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::UpdateHighestReceivedAck( uint16_t newAckValue )
{
	m_lastReceivedAck = newAckValue;
	//KEY: you don't write the highest value into the previous values' bitfield UNTIL you see you've gotten a NEW value.

	//We can guarantee, in this ack-based use case, that we will never get a duplicate such that newValue == highest.

	//We assume that "m_highest.." == the most && greatest-value (accounting for uint overflow via CyclicGT below) recent ack.
	if ( UnsignedGreaterThan( newAckValue, m_highestReceivedAck ) )
	{
		uint16_t offsetShiftAmt = newAckValue - m_highestReceivedAck; //On the first row below, this is us putting a "1" into 0's slot 2-0=2 places behind highest.
//		ASSERT_RECOVERABLE( ( offsetShiftAmt /*- 1, 0-1 caused it to underflow*/ ) <= MAX_VALID_OFFSET, "Invalid offset in UpdateHighestReceivedAck case 1!" );
			//WARNING: these are extremely likely to go off when you use a breakpoint, because it quickly gets MAX_VALID_OFFSET acks away!

		m_previousReceivedAcksAsBitfield = m_previousReceivedAcksAsBitfield << offsetShiftAmt; //No -1 here, this is not an index.
			//We want to index from 0, hence the <<, simplifying IsReceived(). Could do << or >> as long as all methods are consistent though.

		m_highestReceivedAck = newAckValue;

		SET_BIT( m_previousReceivedAcksAsBitfield, offsetShiftAmt - 1 ); //Stores the previous m_highest value into m_prevBF as a 1 for "I did get this."
			//The extra -1 is because we start indexing at prevBF[0].
	}
	else //newValue < m_highest, so reverse the subtraction order for the shift. Implies we just received a packet sent before our most recent/highest packet.
	{
		uint16_t offsetShiftAmt = m_highestReceivedAck - newAckValue;
//		ASSERT_RECOVERABLE( ( offsetShiftAmt /*- 1, 0-1 caused it to underflow*/ ) <= MAX_VALID_OFFSET, "Invalid offset in UpdateHighestReceivedAck case 2!" );
			//WARNING: these are extremely likely to go off when you use a breakpoint, because it quickly gets MAX_VALID_OFFSET acks away!

		SET_BIT( m_previousReceivedAcksAsBitfield, offsetShiftAmt - 1 ); //Keeps m_highest the same while storing new value into m_prevBF as a 1 for "I did get this."
			//The extra -1 is because we start indexing at prevBF[0].
	}

	/* Example Case
		m_highest	m_prevBitfield (tracks up to the last 16 unique acks received)
		0			0000 0000 0000 0000	=> Now UHRA(2) hits. We'll assume 0 started in the list, hence it receives a "1" in the line below 2-0=2 places behind highest.
											- (Note "2 places behind highest" means prevBF[1] due to indices starting  at zero in C++, hence the extra -1 above.)
		2			0100 0000 0000 0000	=> Thus prevBF denotes that 0 was already in that list, but 1 hasn't been seen yet. Now say we call UHRA(4).
		4			0101 0000 0000 0000	=> Now UHRA(3). Note we'll read/write such that the leftmost spot in prevBF is at index 0 for us (LSB).
		4			1101 0000 0000 0000	=> Now UHRA(7). Highest is still 4, because 3 < 4, but we see 3 marked as received in the leftmost bitfield slot prevBF[0].
		7			0011 1010 0000 0000	=> Says there is a 7, a 4, a 2, a 3, a zero. 3 down from our highest is marked as received, so we know 4 is received, etc.
											- We know we have not gotten a 5 or 6 at this point. If we get packet 7, we can assume there were 6 packets before it.
											- Because highest will always represent the next packet going up,
											- so it's going to ideally represent the most recent packet (unless something was lost).
	*/
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::WasAckReceived( uint16_t ackValue )
{
	//Check changes where it looks based on what the most recent ack received (i.e. m_highest's value).
	if ( ackValue > m_highestReceivedAck )
	{
		return false;
	}
	else if ( ackValue == m_highestReceivedAck )
	{
		return true; //m_highest isn't a bit value yet, no bit ops needed.
	}
	else //ackValue < m_highest, so we need to access bitfield here.
	{
		uint16_t offsetShiftAmt = m_highestReceivedAck - ackValue; //How many places back along the bitfield the sought value would be.
		ASSERT_RECOVERABLE( ( offsetShiftAmt - 1 ) <= MAX_VALID_OFFSET, "Invalid offset in WasAckReceived!" );
		return IS_BITFIELD_SET_AT_INDEX( m_previousReceivedAcksAsBitfield, offsetShiftAmt - 1 ); //-1 b/c indices start from 0.
	}
}


//--------------------------------------------------------------------------------------------------------------
AckBundle* NetConnection::CreateAckBundle( uint16_t packetAck )
{
	uint16_t index = packetAck % MAX_ACK_BUNDLES; //If this wraps around to as of yet unconfirmed packet acks, we treat those packets as having been lost.

	AckBundle* bundle = &( m_ackBundles[ index ] );

	bundle->ackID = packetAck;

	bundle->numReliableIDsSent = 0;

	return bundle;
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::RemoveAllReliableIDsLessThan( uint16_t newCutoffID )
{
	//We can receive reliable IDs out of order and wrapped around, e.g. one sends 65535 0 1 2 3 4 but we get 2 3 0 65535 4 1, so doing a naive loop.

	//Doing a naive loop because we can't assume m_receivedReliableIDs is sorted, e.g. receiving 4 1 65535 2 3 0 instead of 65535 0 1 2 3 4.
	for ( auto ridIter = m_receivedReliableIDs.begin(); ridIter != m_receivedReliableIDs.end(); )
	{
		const uint16_t& rid = *ridIter;
		if ( UnsignedLessThan( rid, newCutoffID ) )
		{
			m_receivedReliableIDs.erase( ridIter );
		}
		else ++ridIter;
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::MarkReliableReceived( uint16_t receivedReliableID )
{
	if ( UnsignedGreaterThanOrEqual( receivedReliableID, m_nextExpectedReliableID + RELIABLE_RANGE_RADIUS ) ) //This interval defines sliding window.
	{
		uint16_t distance = receivedReliableID - m_nextExpectedReliableID;
		if ( UnsignedGreaterThan( distance, RELIABLE_RANGE_RADIUS ) )
			ERROR_AND_DIE( "MarkReliableReceived found reliable ahead of expected range!" ); //SEND-SIDE SHOULD BLOCK THIS FROM HAPPENING.
		
		TODO( "In a release build, should drop this violating connection." );

		m_nextExpectedReliableID = receivedReliableID + 1; //Advances sliding window.
		RemoveAllReliableIDsLessThan( receivedReliableID - RELIABLE_RANGE_RADIUS );
			//Where we handle what the sliding window has now left behind, keeping receivedReliables container from outgrowing twice interval radius + 1.
	}
	else
	{
		uint16_t diff = m_nextExpectedReliableID - receivedReliableID;
		if ( UnsignedLessThan( diff, RELIABLE_RANGE_RADIUS ) )
			m_receivedReliableIDs.push_back( receivedReliableID );
	}
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::HasReceivedReliable( uint16_t reliableID ) const
{
	//Is it still within our sliding window interval? i.e. reliableID >= MIN && reliableID <= MAX?
	if ( UnsignedGreaterThanOrEqual( reliableID, m_nextExpectedReliableID - RELIABLE_RANGE_RADIUS )
		 && UnsignedLessThanOrEqual( reliableID, m_nextExpectedReliableID + RELIABLE_RANGE_RADIUS ) )
	{
		//Does it also then exist in our receivedReliables?
		for each ( const uint16_t& rid in m_receivedReliableIDs )
			if ( rid == reliableID )
				return true;
	}
	return false;
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::IsReliableConfirmed( uint16_t reliableID ) const
{		
	return ( m_confirmedReliableIDs.find( reliableID ) != m_confirmedReliableIDs.end() );
}


//--------------------------------------------------------------------------------------------------------------
std::string NetConnection::GetDebugString()
{
	const int MAX_ADDRSTR_LENGTH = 256;
	char addrStr[ MAX_ADDRSTR_LENGTH ];
	NetSystem::SockAddrToString( &m_connectionInfo.address, addrStr, MAX_ADDRSTR_LENGTH );
	
	std::string connStr = Stringf
	( 
		"%c %u [%s]\t %s\t lastRecvTime[%.3fs]\t lastSentAck[%u]\t lastRecvAck[%u]\t lastConfAck[%u]",
		m_session->IsHost( this ) ? 'H' : ' ',
		m_connectionInfo.connectionIndex,
		addrStr,
		m_connectionInfo.guid,
		m_secondsSinceLastRecv,
		m_nextSentAck,
		m_lastReceivedAck,
		m_lastConfirmedAck
	);

	if ( IsMe() )
		return Stringf( "ME %s", connStr.c_str() );
	else
		return Stringf( "-- %s", connStr.c_str() );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::ConfirmAndWakeConnection()
{
	m_connectionState = CONNECTION_STATE_CONFIRMED;
	double currentTimeSeconds = GetCurrentTimeSeconds();
	m_secondsSinceLastRecv = currentTimeSeconds - m_lastRecvTimeSeconds;
	m_lastRecvTimeSeconds = currentTimeSeconds; //For next invocation.

	if ( IsConnectionBad() )
		SetAsGoodConnection();
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::UpdateConnectionInfo( const NetConnectionInfo& newInfo )
{
//	m_connectionInfo.address = newInfo.address; //See WriteConnectionInfo
	m_connectionInfo.connectionIndex = newInfo.connectionIndex;
	for ( int i = 0; i < MAX_GUID_LENGTH; i++ )
		m_connectionInfo.guid[ i ] = newInfo.guid[i];
}


//--------------------------------------------------------------------------------------------------------------
bool NetConnection::IsConnected() const
{
	return ( GetIndex() != INVALID_CONNECTION_INDEX ) && ( m_session->GetIndexedConnection( GetIndex() ) == this );
}


//--------------------------------------------------------------------------------------------------------------
void NetConnection::FlushUnreliables()
{
	//Not really going to matter for reliables, and we don't want reliables to shove out the leave msg (an unreliable).
	while ( !m_unsentUnreliables.empty() )
		ConstructAndSendPacket(); //Important to flush unreliables, else the leave message (an unreliable) isn't sent!
}
