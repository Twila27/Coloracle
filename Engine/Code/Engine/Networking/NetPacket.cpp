#include "Engine/Networking/NetPacket.hpp"
#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/NetSession.hpp"



//--------------------------------------------------------------------------------------------------------------
bool NetPacket::WriteMessageToBuffer( NetMessage& in_msg, NetSession* ns )
{
	bool foundDefn = in_msg.FinalizeMessageDefinition( ns );
	if ( !foundDefn )
		return false;

	uint16_t totalMessageSize;
	uint16_t msgHeaderSize = (uint16_t)in_msg.GetHeaderSize( sizeof( totalMessageSize ) ); //Changes if rid/sid is also used.
	uint16_t msgPayloadSize = (uint16_t)in_msg.GetPayloadSize();
	totalMessageSize = msgHeaderSize + msgPayloadSize;

	if ( GetWritableBytes() >= totalMessageSize ) //Else too big to write the message.
	{
		Write<uint16_t>( totalMessageSize ); //Size first.
		Write<uint8_t>( in_msg.GetTypeID() ); //ID second.

		if ( in_msg.IsReliable() )
		{
			Write<uint16_t>( in_msg.GetReliableID() );

			if ( in_msg.IsInOrder() )
				Write<uint16_t>( in_msg.GetSequenceID() ); //Unreliable in-order traffic supported differently, see class spec/notes.
		}
		//End of writing message header.

		WriteForwardAlongBuffer( in_msg.GetBuffer(), msgPayloadSize ); //The payload.

		++m_numberOfMessages;
		return true;
	}
	else
	{
		return false;
	}
}


//--------------------------------------------------------------------------------------------------------------
bool NetPacket::WritePacketHeader( PacketHeader& ph )
{
	//ConnIndex first, so receiver knows who sent it and can get their pointer from their NetSession::m_connections.
	bool success = Write<NetConnectionIndex>( ph.connectionIndex );
	if ( !success )
		return false;

	success = Write<uint16_t>( ph.ack );
	if ( !success )
		return false;

	success = Write<uint16_t>( ph.highestReceivedAck );
	if ( !success )
		return false;

	success = Write<uint16_t>( ph.previousReceivedAcksAsBitfield );
	if ( !success )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetPacket::ReadMessageFromPacketBuffer( NetMessage& out_msg, NetSession* ns )
{
	uint16_t totalMessageSize;
	bool hadEnoughRoom = Read<uint16_t>( &totalMessageSize );
	if ( !hadEnoughRoom )
		return false;

	uint8_t msgType; //Not a CoreMessageType to support either that or also the GameMessageType enum.
	hadEnoughRoom = Read<uint8_t>( &msgType );
	if ( !hadEnoughRoom )
		return false;

	NetMessageDefinition* defn = ns->FindDefinitionForMessageType( msgType );
	if ( defn == nullptr )
		return false;
	out_msg.SetMessageDefinition( defn ); //Necessary to call below function.
	size_t headerSize = out_msg.GetHeaderSize( sizeof( totalMessageSize ) );
	size_t payloadSize = totalMessageSize - headerSize;

	out_msg = NetMessage( msgType, totalMessageSize, GetIoHead(), payloadSize ); //Takes a ptr to take the packet buffer in-place.

	//Handle Variable Fields (e.g. reliableID if msg.IsReliable, which requires valid msg defn to work)
	bool foundDefn = out_msg.FinalizeMessageDefinition( ns );
	if ( !foundDefn ) //Sanity check.
		return false;

	if ( out_msg.IsReliable() )
	{
		uint16_t reliableID;
		hadEnoughRoom = Read<uint16_t>( &reliableID ); //Because at this point we haven't advanced packet's ioHead since reading msgType.
		if ( !hadEnoughRoom )
			return false;
		out_msg.SetReliableID( reliableID );

		if ( out_msg.IsInOrder() )
		{
			uint16_t sequenceID;
			hadEnoughRoom = Read<uint16_t>( &sequenceID );
			if ( !hadEnoughRoom )
				return false;
			out_msg.SetSequenceID( sequenceID );
		}

		out_msg.SetBuffer( GetIoHead() ); //Moves where the message believes its payload to start, to after the rID.
	}
	//End Message Header Variable Field Handling

	this->AdvanceOffset( payloadSize ); //Since we never used Read() on it, still need to advance this offset past payload.
		//Note this is for the packet, NOT out_msg -- we will allow NetMessage::Read functions to advance over its own buffer.
		//Also note this comes AFTER variable fields in the message header.

	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetPacket::ReadPacketHeader( PacketHeader& ph )
{
	bool success = Read<NetConnectionIndex>( &ph.connectionIndex );
	if ( !success )
		return false;

	success = Read<uint16_t>( &ph.ack );
	if ( !success )
		return false;

	success = Read<uint16_t>( &ph.highestReceivedAck );
	if ( !success )
		return false;

	success = Read<uint16_t>( &ph.previousReceivedAcksAsBitfield );
	if ( !success )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetPacket::ValidateLength( const PacketHeader& ph, size_t expectedPacketSize )
{
	UNREFERENCED( ph ); //It gets compiled out here, but it's actually needed for below sizeof's--they're just compiled out, hence it seemed unreferences.

	//WARNING: keep validationOffset up to date with PacketHeader or this will likely return false negatives!
	//NOT using sizeof( PacketHeader ) because packing uint8 with uint16 makes the retval of sizeof different.
	size_t offsetBeforeEnteringFunction = sizeof( m_numberOfMessages ) //We know we're this many bytes into the packet (i.e. where m_ioOffset is) here.
		+ sizeof( ph.ack )
		+ sizeof( ph.connectionIndex )
		+ sizeof( ph.highestReceivedAck )
		+ sizeof( ph.previousReceivedAcksAsBitfield );
		//i.e. You better have made sure that ReadPacketHeader and ReadNumMessages came before the call to this function did. >:(


	size_t validationOffset = offsetBeforeEnteringFunction;
	for ( int msgIndex = 0; msgIndex < m_numberOfMessages; msgIndex++ )
	{
		uint16_t totalMessageSize;
		Read<uint16_t>( &totalMessageSize );
		AdvanceOffset( totalMessageSize - sizeof( totalMessageSize ) ); //Cross over NOT totalMessageSize but yes msg id in the header as well as the msg payload!
		validationOffset += totalMessageSize; //Need this for below, but need to use AdvanceOffset to use Read<> for endian-correct parsing. 
			//This doesn't subtract sizeof (msgSize) because Read<> doesn't advance it.
	}
	
	ResetOffset( offsetBeforeEnteringFunction ); //Get it back where it was before calling this function.

	return ( expectedPacketSize == validationOffset ); //Should be true if above loop worked and packet is valid!
}
