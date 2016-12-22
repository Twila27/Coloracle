#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Engine/Networking/NetConnectionUtils.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC void NetMessage::Duplicate( const NetMessage& msg, NetMessage& out_cloneMsg )
{
	out_cloneMsg.m_msgHeader = msg.m_msgHeader; //Be sure struct has no ptr if adding to it in future.
	out_cloneMsg.SetMessageDefinition( &msg.m_defn );

	int numValidBufferBytes = msg.GetTotalReadableBytes();
	for ( int byteIndex = 0; byteIndex < numValidBufferBytes; byteIndex++ )
		out_cloneMsg.m_msgData[ byteIndex ] = msg.m_msgData[ byteIndex ];

	out_cloneMsg.SetTotalReadableBytes( numValidBufferBytes );
}


//--------------------------------------------------------------------------------------------------------------
NetMessage::NetMessage( uint8_t id /*= MAX_CORE_NETMSG_TYPES */ )
	: BytePacker( MAX_MESSAGE_SIZE )
	, m_msgHeader( id )
	, m_msgData( new byte_t[ MAX_MESSAGE_SIZE ] )
	, m_lastSentTimestampMilliseconds( 0 )
{
	SetBuffer( m_msgData ); //Has to come after allocating.
}


//--------------------------------------------------------------------------------------------------------------
NetMessage::NetMessage( uint8_t id, uint16_t totalMsgSize, byte_t* msgData, size_t msgLength )
	: BytePacker( totalMsgSize )
	, m_msgHeader( id )
	, m_msgData( msgData )
	, m_lastSentTimestampMilliseconds( 0 )
{
	SetBuffer( m_msgData );
	SetTotalReadableBytes( msgLength );
	//Don't neglect to call Finalize() to set msg definition based on m_id!
}


//--------------------------------------------------------------------------------------------------------------
size_t NetMessage::GetHeaderSize( size_t sizeOfMessageLengthVariable )
{
	//Would it be better to just precompute and store it on the defn when NetSession::RegisterMessage is called?

	//Without breaking it up individually (as is the case when it's written out) struct packing becomes problematic.
	size_t size = sizeOfMessageLengthVariable + sizeof( m_msgHeader.id );

	if ( IsReliable() )
		size += sizeof( m_msgHeader.reliableID );

	if ( IsInOrder() )
		size += sizeof( m_msgHeader.sequenceID );

	return size;
}


//--------------------------------------------------------------------------------------------------------------
bool NetMessage::FinalizeMessageDefinition( NetSession* ns )
{
	if ( ns == nullptr )
		return false;

	NetMessageDefinition* defn = ns->FindDefinitionForMessageType( m_msgHeader.id );

	if ( defn == nullptr )
		return false;

	SetMessageDefinition( defn );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetMessage::WriteConnectionInfoMinusAddress( const NetConnectionInfo* info )
{
	if ( info == nullptr )
		return false;

// Commented out because not sure it's necessary and due to Endian byte-swap non-trivial to write.
//	bool successfulWrite = Write<sockaddr_in>( info->address );
//	if ( !successfulWrite )
//		return false;
		
	bool successfulWrite = WriteString( info->guid );
	if ( !successfulWrite )
		return false;

	successfulWrite = Write<uint8_t>( info->connectionIndex );
	return successfulWrite;
}


//--------------------------------------------------------------------------------------------------------------
bool NetMessage::ReadConnectionInfoMinusAddress( NetConnectionInfo* out_info ) //See write above for why minus addr.
{
	if ( out_info == nullptr )
		return false;

//	bool successfulRead = Read<sockaddr_in>( &out_info->address );
//	if ( !successfulRead )
//		return false;

	const char* guid = ReadString();
	for ( int i = 0; i < MAX_GUID_LENGTH; i++ )
		out_info->guid[ i ] = guid[ i ];

	bool successfulRead = Read<uint8_t>( &out_info->connectionIndex );
	return successfulRead;
}
