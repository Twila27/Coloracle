#include "Engine/Networking/tcpip/TCPConnection.hpp"
#include "Engine/Networking/NetSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
TCPConnection::TCPConnection( const char* hostNameStr, uint16_t port )
{
	char portBuffer[ 10 ];
	snprintf( portBuffer, strlen( portBuffer ), "%u", port ); //Alt to itoa which was said to not be thread-safe.

	m_mySocket = TCPConnection::SocketJoin( hostNameStr, portBuffer, &m_myAddr );
}


//--------------------------------------------------------------------------------------------------------------
STATIC SOCKET TCPConnection::SocketJoin( const char* hostAddrStr, const char* servicePortStr, sockaddr_in* out_sockAddr )
{
	addrinfo* addrList = NetSystem::AllocAddressesGivenHostAndPort( hostAddrStr, servicePortStr, AF_INET, SOCK_STREAM, 0 );
		//Key difference from hosts' CreateListenSocket--no AI_PASSIVE here, just 0 since we don't bind(), just send to it via ::connect().

	SOCKET mySocket = INVALID_SOCKET;
	addrinfo* addrIter = addrList;
	while ( ( addrIter != nullptr ) && ( mySocket == INVALID_SOCKET ) )
	{
		//STEP #1: MAKE SOCKET.
		mySocket = GLOBAL::socket( addrIter->ai_family, addrIter->ai_socktype, addrIter->ai_protocol );

		if ( mySocket != INVALID_SOCKET )
		{
			//STEP #2: INSTEAD OF BIND SOCKET, CALL CONNECT. This performs TCP/IP handshake with host's listen socket.
			int result = GLOBAL::connect( mySocket, addrIter->ai_addr, (int)( addrIter->ai_addrlen ) ); //Same args as ::bind() though.
				//Can block => cause frame hiccup, fine for now.

			if ( result != SOCKET_ERROR )
			{
				//STEP 3: ::listen()ing is n/a for a joining client, rest is same.
				u_long nonblocking = 1; //We set it nonblocking to work with it on our main thread.
				ioctlsocket( mySocket, FIONBIO, &nonblocking );
				//Can check error on this here as well, in lecture assumed it won't go awry here.

				ASSERT_OR_DIE( addrIter->ai_addrlen == sizeof( sockaddr_in ), nullptr );
				if ( out_sockAddr != nullptr ) //Save out the created listen socket's address, if requested.
					memcpy( out_sockAddr, addrIter->ai_addr, addrIter->ai_addrlen );
			}
			else
			{
				NetSystem::CloseSocket( mySocket );
				mySocket = INVALID_SOCKET; //Breaks while loop.
			}
		}

		addrIter = addrIter->ai_next;
	}

	NetSystem::FreeAddresses( addrList );

	return mySocket;
}


//--------------------------------------------------------------------------------------------------------------
STATIC size_t TCPConnection::SocketSend( bool* out_shouldDisconnect, SOCKET mySocket, void const* data, size_t const dataSize )
{
	*out_shouldDisconnect = false;

	if ( mySocket != INVALID_SOCKET )
	{
		//send() returns #bytes of data actually sent. SHOULD match, else err.
		int size = GLOBAL::send( mySocket, (const char*)data, (int)dataSize, 0 );
		if ( size < 0 )
		{
			int32_t error = WSAGetLastError();
			if ( NetSystem::SocketErrorShouldDisconnect( error ) )
				*out_shouldDisconnect = true;
		}
		else ASSERT_OR_DIE( (size_t)size == dataSize, nullptr );

		return (size_t)size;
	}
	else return 0U;
}


//--------------------------------------------------------------------------------------------------------------
STATIC size_t TCPConnection::SocketReceive( bool* out_shouldDisconnect, SOCKET mySocket, void* buffer, size_t const bufferSize )
{
	*out_shouldDisconnect = false;

	if ( mySocket != INVALID_SOCKET )
	{
		// recv will return amount of data read, should always be <= buffer_size
		// Also, if you send, say, 3 KB with send, recv may actually
		// end up returning multiple times (say, 1KB, 512B, and 1.5KB) because 
		// the message got broken up - so be sure your application watches for it

		int size = GLOBAL::recv( mySocket, (char*)buffer, bufferSize, 0 );
		if ( size < 0 )
		{
			int32_t error = WSAGetLastError();
			if ( NetSystem::SocketErrorShouldDisconnect( error ) )
				*out_shouldDisconnect = true;

			return 0U;
		}
		else return (size_t)size;
	}
	else return 0U;
}


//--------------------------------------------------------------------------------------------------------------
size_t TCPConnection::SendData( void const* data, size_t size )
{
	if ( !IsConnected() )
		return 0U; //Validate we're connected before sending.
	bool shouldDisconnect = false;
	return TCPConnection::SocketSend( &shouldDisconnect, m_mySocket, data, size );
}


//--------------------------------------------------------------------------------------------------------------
size_t TCPConnection::RecvData( void* out_buffer, size_t bufferSize )
{
	if ( !IsConnected() )
		return 0U; //Validate we're connected before recving.
	bool shouldDisconnect = false;
	return TCPConnection::SocketReceive( &shouldDisconnect, m_mySocket, out_buffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
void TCPConnection::GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const
{
	NetSystem::SockAddrToString( &m_myAddr, out_addrStrBuffer, bufferSize );
}
