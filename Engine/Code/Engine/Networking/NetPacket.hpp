#pragma once


#include "Engine/EngineCommon.hpp"
#include "Engine/Memory/BytePacker.hpp"
#include "Engine/Networking/AckBundle.hpp"


//-----------------------------------------------------------------------------
#define WINDOWS_PACKET_MTU 1280 //https://msdn.microsoft.com/en-us/library/ms882717.aspx


//-----------------------------------------------------------------------------
#define MAX_UDPIP_HEADER 48 //40 for biggest possible sizeof IPv6 address + 8 for UDP header.
	//UDP header is always (src_port, dst_port, length, checksum uint16_t's).


//-----------------------------------------------------------------------------
#define MAX_PACKET_SIZE (WINDOWS_PACKET_MTU - MAX_UDPIP_HEADER) 
		//Exceeding this splits packets, which we usually never want!


//-----------------------------------------------------------------------------
class NetMessage;
class NetSession;


//-----------------------------------------------------------------------------
struct PacketHeader
{
	PacketHeader() : ack( INVALID_PACKET_ACK ) {}

	uint8_t connectionIndex; //From A3.

	//A4 -- these are set in NetConnection::SendPacket when writing packet header!
	uint16_t ack/*OfThisPacket*/; //Every packet we send out uses a sequential ack # for its #th packet.
		//KEY for unreliables: if the ack # is invalid, we do not expect an acknowledging response.
	uint16_t highestReceivedAck/*OnSendingSide*/;
	uint16_t previousReceivedAcksAsBitfield/*OnSendingSide*/; 
		//e.g. if highestAck is 3, is_set(pRAAB[15]) == whether we received an ack for #2.

	//No member for # messages, because we calculate that on send.
};


//-----------------------------------------------------------------------------
class NetPacket : public BytePacker
{
public:
	NetPacket() : BytePacker( MAX_PACKET_SIZE ), m_numberOfMessages( 0 ) { SetBuffer( m_packetBuffer ); }
	size_t GetHeaderSize() const { return sizeof( m_numberOfMessages ); }
	byte_t* GetPayloadBuffer() const { return (byte_t*)m_packetBuffer; }
	uint8_t GetTotalAddedMessages() const { return m_numberOfMessages; }

	bool ReadNumMessages() { return Read<uint8_t>( &m_numberOfMessages ); }

	bool ReadPacketHeader( PacketHeader& ph );
	bool WritePacketHeader( PacketHeader& ph );

	bool ReadMessageFromPacketBuffer( NetMessage& out_msg, NetSession* );
	bool WriteMessageToBuffer( NetMessage& in_msg, NetSession* );
	bool ValidateLength( const PacketHeader& ph, size_t expectedPacketSize );


private:
	byte_t m_packetBuffer[ MAX_PACKET_SIZE ];
	uint8_t m_numberOfMessages;
};
