#pragma once


#include "Engine/EngineCommon.hpp"
#include "Engine/Memory/BytePacker.hpp"
#include "Engine/Memory/BitUtils.hpp"


//-----------------------------------------------------------------------------
#define MAX_MESSAGE_SIZE 1 KB


//-----------------------------------------------------------------------------
enum NetMessageControl : uint32_t //Describes the "what" needed to process a message, cf. NetSession::CanProcessMessage.
{
	NETMSGCTRL_NONE = 0,
	NETMSGCTRL_PROCESSED_CONNECTIONLESS = (1 << 0), //Does not use the NetConnection => tolerates invalid connection index in packet.
	NETMSGCTRL_PROCESSED_INORDER = (1 << 1),
	NUM_NETMSGCTRLS
};


//-----------------------------------------------------------------------------
enum NetMessageOption : uint32_t //Describes "how" a message needs to be sent/received, cf. NetSession::CanProcessMessage.
{
	NETMSGOPT_NONE = 0,
	NETMSGOPT_RELIABLE = (1 << 0), 
	NUM_NETMSGOPTS
};


//-----------------------------------------------------------------------------
struct NetSender;
class NetMessage;
class NetSession;
struct NetConnectionInfo;
typedef void( NetMessageCallback )( const NetSender& sender, NetMessage& msg );


//-----------------------------------------------------------------------------
struct NetMessageDefinition //Created by calls to NetSession::RegisterMessage.
{
	uint8_t index;
	const char* debugName;
	NetMessageCallback* handler;
	uint32_t controlFlags; //cf. NetMessageControl above. The "what" to send. Used while receiving, as parity bits.
	uint32_t optionFlags; //e.g. RELIABLE -- The "how" to send. Used while sending.
};


//-----------------------------------------------------------------------------
struct MessageHeader //WARNING: DOES NOT INCLUDE THE MESSAGE LENGTH (hence GetHeaderSize's param).
{
	MessageHeader() {}
	MessageHeader( uint8_t msgTypeId, uint16_t msgReliableID = 0, uint16_t msgSequenceID = 0 )
		: id( msgTypeId )
		, reliableID( msgReliableID )
		, sequenceID( msgSequenceID )
	{
	}
	uint8_t id;//Confer SD6 A1's MSG_TYPE_xxx constants. Will never exceed 256 as restricted by above enum type.
		//Note we store it as uint8_t rather than a NetCoreMessageType or a GameMessageType, to support both.

	uint16_t reliableID; //A4: For traffic sent reliably, uses this to confirm as having received.
	uint16_t sequenceID; //A5: For traffic processed in-order, uses this to tell when it needs to wait on or progress processing.
};


//-----------------------------------------------------------------------------
class NetMessage : public BytePacker
{
public:
	static void Duplicate( const NetMessage& msg, NetMessage& out_cloneMsg );

	NetMessage( uint8_t id = NO_MSG_TYPE_ID ); //Supports either core engine-side or game-side message type enums.
	NetMessage( uint8_t id, uint16_t totalMsgSize, byte_t* msgData, size_t msgLength );
	
	bool NeedsConnection() const { return ( GET_BIT_AT_BITFIELD_WITHOUT_INDEX_MASKED( m_defn.controlFlags, NETMSGCTRL_PROCESSED_CONNECTIONLESS ) == 0 ); }
	bool IsInOrder() const { return ( GET_BIT_AT_BITFIELD_WITHOUT_INDEX_MASKED( m_defn.controlFlags, NETMSGCTRL_PROCESSED_INORDER ) != 0 ); }
	bool IsReliable() const { return ( GET_BIT_AT_BITFIELD_WITHOUT_INDEX_MASKED( m_defn.optionFlags, NETMSGOPT_RELIABLE ) != 0 ); }
	void Process( const NetSender& from ) { m_defn.handler( from, *this/*Won't copy because handler is pass-by-ref, not by-val.*/ ); }

	uint8_t GetTypeID() const { return m_msgHeader.id; }
	uint16_t GetReliableID() const { return m_msgHeader.reliableID; }
	uint16_t GetSequenceID() const { return m_msgHeader.sequenceID; }
	size_t GetHeaderSize( size_t sizeOfMessageLengthVariable );
	size_t GetPayloadSize() const { return GetTotalReadableBytes(); } //Payload size == how far we've written into the BytePacker'd buffer.
	byte_t* GetMessageBuffer() { return m_msgData; }
	NetMessageDefinition GetMessageDefinition() const { return m_defn; }
	uint32_t GetTimestamp() const { return m_lastSentTimestampMilliseconds; }

	bool FinalizeMessageDefinition( NetSession* );
	void SetTimestamp( uint32_t ms ) { m_lastSentTimestampMilliseconds = ms; }
	void SetReliableID( uint16_t rid ) { m_msgHeader.reliableID = rid; }
	void SetSequenceID( uint16_t sid ) { m_msgHeader.sequenceID = sid; }
	void SetMessageDefinition( const NetMessageDefinition* defn ) { m_defn = *defn; }

	bool WriteConnectionInfoMinusAddress( const NetConnectionInfo* );
	bool ReadConnectionInfoMinusAddress( NetConnectionInfo* );


private:

	byte_t* m_msgData;
	MessageHeader m_msgHeader;
	NetMessageDefinition m_defn;

	uint32_t m_lastSentTimestampMilliseconds; //If reliable, time since this was last attempted to be sent.
};
