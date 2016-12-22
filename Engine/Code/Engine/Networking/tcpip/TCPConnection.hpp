#pragma once
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/EngineCommon.hpp"


class TCPConnection
{
private:

	SOCKET m_mySocket;
	sockaddr_in m_myAddr; //Sticking to IPv4 addresses. May see a more general case net-addr struct in a later lecture. Overkill at this point.

	//There may be freezes when you call connect() with this because TCPConnection can all block during creation, if we're running on the same thread.
	//Could kick it off to a dedicated thread and run it there, like we did in SD5 with the logger.

	//Saved during ctor, if 2nd is used. (Should be the case for all but the host's TCPListener which calls 1st in its AcceptConnection.)
	uint16_t m_myPort;

public:

	TCPConnection( SOCKET sock, sockaddr_in& addr ) //SHOULD ONLY be used by TCPListener.
		: m_mySocket( sock )
		, m_myAddr( addr )
	{
	} TODO( "Make this ctor private and a friend function to TCPListener class. ONLY this ctor, services' clients use the below one." );

	TCPConnection( const char* hostNameStr, uint16_t port ); //Uses NetSystem::SocketJoin.
	~TCPConnection() { if ( IsConnected() ) Disconnect(); }

	//Send and receive are most likely to have an error message for disconnected, if any.
	size_t SendData( void const* data, size_t size );
	size_t RecvData( void* out_buffer, size_t bufferSize );

	bool IsConnected() const { return m_mySocket != INVALID_SOCKET; }
	void Disconnect() { NetSystem::CloseSocket( m_mySocket ); }
	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const;


private:
	static SOCKET SocketJoin( const char* hostAddrStr, const char* servicePortStr, sockaddr_in* out_sockAddr );
	static size_t SocketSend( bool* out_shouldDisconnect, SOCKET mySocket, void const* data, size_t const dataSize ); //SEE A1 spec.
	static size_t SocketReceive( bool* out_shouldDisconnect, SOCKET mySocket, void* buffer, size_t const bufferSize );
};
