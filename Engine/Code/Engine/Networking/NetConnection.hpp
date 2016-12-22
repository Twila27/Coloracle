#pragma once

#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/AckBundle.hpp"
#include "Engine/Memory/ObjectPool.hpp"
#include "Engine/Networking/NetConnectionUtils.hpp"
#include <set>


//-----------------------------------------------------------------------------
#define INVALID_CONNECTION_INDEX	(0xFF)
#define DEFAULT_TICK_RATE_SECONDS	( 1.f / 20.f ) //20 ticks a second.
#define MAX_ACK_BUNDLES				(128)  //( 2 * (int) (1.f / DEFAULT_TICK_RATE_SECONDS ) ) //2 secs of memory.
	//Worked out to more with a number someone said had been suggested by instructor, so playing it safe with this.
#define RELIABLE_RANGE_RADIUS		(1000) //Defines an interval of valid acks centered on nextExpectedReliableID.
	//i.e. The other side should never send beyond nextExpectedReliableID + RELIABLE_RANGE_RADIUS. 
	//This also can't exceed half of our selected uint type range, or else we invalidate UnsignedGreaterThan (etc) functions.


//-----------------------------------------------------------------------------
class NetSession;
class NetPacket;
struct PacketHeader;


//-----------------------------------------------------------------------------
enum NetConnectionState
{
	CONNECTION_STATE_LOCAL,			//When the connection is you.
	CONNECTION_STATE_UNCONFIRMED,	//When the connection is only added.
	CONNECTION_STATE_CONFIRMED		//When the connection is added and you receive data with their NetConnectionIndex.
};


//-----------------------------------------------------------------------------
class NetConnection //Represents the traffic BOTH to AND from a person we can see.
{
public:
	NetConnection( NetSession* ns, NetConnectionIndex index, const char* guid, sockaddr_in addr );

	const NetConnectionInfo* GetConnectionInfo() const { return &m_connectionInfo; }
	void UpdateConnectionInfo( const NetConnectionInfo& );
	NetConnectionIndex GetIndex() const { return m_connectionInfo.connectionIndex; }
	void SetIndex( NetConnectionIndex index ) { m_connectionInfo.connectionIndex = index; }
	std::string GetGuidString() const { return std::string( m_connectionInfo.guid ); }
	NetSession* GetSession() const { return m_session; }
	void SetSession( NetSession* ns ) { m_session = ns; }
	bool IsMe() const;
	std::string GetDebugString();

	bool IsConnected() const;
	bool IsConnectionBad() const { return m_isABadConnection; }
	void SetAsBadConnection() { m_isABadConnection = true; }
	void SetAsGoodConnection() { m_isABadConnection = false; }
	bool IsConnectionConfirmed() const { return m_connectionState == CONNECTION_STATE_CONFIRMED; }
	void ConfirmAndWakeConnection();
	double GetSecondsSinceLastRecv() const { return m_secondsSinceLastRecv; }
	void AddSecondsSinceLastRecv( double secs ) { m_secondsSinceLastRecv += secs; }

	void SendMessageToThem( NetMessage& msg ); //Enqueues into unreliable vector or unsent-reliable queue.
	void SendMessagesToThem( NetMessage msgs[], int numMessages );
	void ConstructAndSendPacket(); //Pops from unreliables' front end, but tosses it all if out of room.
	
	bool CanProcessMessage( NetMessage& msg ) const;
	void ProcessMessage( const NetSender& from, NetMessage& msg );
	void ProcessInOrder( const NetSender& from, NetMessage& msg );
	void MarkPacketReceived( const PacketHeader& ph );
	void MarkMessageReceived( const NetMessage& msg );

	sockaddr_in GetAddressObject() const { return m_connectionInfo.address; }
	uint16_t GetNextSentAck(); //FOR PACKETS (read from PacketHeader and updated as local/never-sent AckBundles with bitfield logic).
	uint16_t GetNextSentReliableID(); //FOR MESSAGES (embedded in their header when msg.IsReliable by its definition).

	void FlushUnreliables();


private:
	bool WasAckReceived( uint16_t ackValue ); //What uses this? Was part of the interview question, but unconnected during lecture.
	bool HasReceivedReliable( uint16_t reliableID ) const;
	bool IsReliableConfirmed( uint16_t reliableID ) const;
	AckBundle* CreateAckBundle( uint16_t packetAck ); //May recycle old ones.

	void QueueReliable( NetMessage& msg );
	void QueueUnreliable( NetMessage& msg );
	uint8_t ResendSentReliables( NetPacket& packet, AckBundle* bundle );
	uint8_t SendUnsentReliables( NetPacket& packet, AckBundle* bundle );
	uint8_t SendUnreliables( NetPacket& packet );
	
	void MarkReliableReceived( uint16_t receivedReliableID );
	void RemoveAllReliableIDsLessThan( uint16_t newCutoffID );

	//Below are called upon by MarkPacketReceived, but not MarkMessageReceived. Yeah.
		void UpdateHighestReceivedAck( uint16_t newAckValue );
		void ProcessConfirmedAcks( uint16_t packetHighestAckReceived, uint16_t packetPreviousAcksBitfield );
		void ConfirmAck( uint16_t ack );
			void MarkReliableConfirmed( uint16_t rid );
			AckBundle* FindBundle( uint16_t ackID );

	NetSession* m_session; //Who made this connection.
	NetConnectionState m_connectionState;
	NetConnectionInfo m_connectionInfo; //Connection identifiers.
	bool m_isABadConnection; //Haven't heard from them in over a time limit set in NetSession::Update.
	double m_lastRecvTimeSeconds;
	double m_lastSendTimeSeconds;
	double m_secondsSinceLastRecv;
	double m_secondsSinceLastSend;

	//-----------------------------------------------------------------------------//Ack Records (Cannot send reliables without one.)

	//----//Sending Acks
	uint16_t m_nextSentAck; //For sending side, increased with every NetConnection::SendPacket call to NC::GetNextAck.
	AckBundle m_ackBundles[ MAX_ACK_BUNDLES ];

	//----//Receiving Acks
	uint16_t m_lastReceivedAck; //For debug window.
	uint16_t m_lastConfirmedAck; //For debug window.
	uint16_t m_highestReceivedAck; //For receiving side.
	uint16_t m_previousReceivedAcksAsBitfield; //For receiving side.

	//-----------------------------------------------------------------------------//Messages
	
	//----//Sending Unreliable Traffic
	ObjectPool< NetMessage > m_unreliablesPool;
	std::vector< NetMessage* > m_unsentUnreliables;

	//----//Sending Reliable Traffic (IDs)
	uint16_t m_nextSentReliableID;
	uint16_t m_oldestUnconfirmedReliableID; //We have to hear back from the other side via ??? before incrementing this.
	std::set<uint16_t, UnsignedComparator> m_confirmedReliableIDs; //Confirmed == the sender's side, below received == the recv's side.

	//----//Sending Reliable Traffic
	ObjectPool< NetMessage > m_reliablesPool;
	std::queue< NetMessage* > m_unsentReliables;
	std::queue< NetMessage* > m_sentReliables; //NetMessage receives no reliableID until it migrates from m_unsent to here.
	
	//----//Receiving Reliable Traffic (IDs)
	std::vector<uint16_t> m_receivedReliableIDs;
	uint16_t m_nextExpectedReliableID; //We can tolerate this being exceeded up to this + MAX_RELIABLE_RANGE.
		//Works like the ack -- nextExpectedReliableID is one above our oldest one we could possibly have (i.e. sliding window upper bound).

	//-----------------------------------------------------------------------------//In-Order Channels (for now just one)
	InOrderChannelData m_channelData;
};
