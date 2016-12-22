#include "Engine/Networking/tcpip/TCPListener.hpp"


#include "Engine/Networking/tcpip/TCPConnection.hpp"


//--------------------------------------------------------------------------------------------------------------
TCPListener::TCPListener( const char* host, uint16_t port, int queueCount /*= 1 */ )
{
	char portBuffer[ 10 ];
	snprintf( portBuffer, strlen( portBuffer ), "%u", port ); //Alt to itoa which was said to not be thread-safe.

	m_mySocket = NetSystem::CreateListenSocket( host, portBuffer, &m_myAddr, queueCount );
}

//--------------------------------------------------------------------------------------------------------------
TCPListener::TCPListener( uint16_t port )
{
	const int MAX_ADDR_STRLEN = 256;
	char nameBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::GetLocalHostName( nameBuffer, MAX_ADDR_STRLEN );
	*this = TCPListener( nameBuffer, port );
}

//--------------------------------------------------------------------------------------------------------------
TCPConnection* TCPListener::AcceptConnection()
{
	sockaddr_in theirAddress;
	
	SOCKET theirSocket = NetSystem::AcceptConnection( m_mySocket, &theirAddress );

	if ( theirSocket == INVALID_SOCKET )
		return nullptr;

	return new TCPConnection( theirSocket, theirAddress );
}
