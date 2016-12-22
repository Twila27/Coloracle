#pragma once
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/EngineCommon.hpp"


class TCPConnection;

class TCPListener
{
private:

	SOCKET m_mySocket;
	sockaddr_in m_myAddr;


public:

	TCPListener( const char* host, uint16_t port, int queueCount = 1 ); //Creates the listen socket.
	TCPListener( uint16_t port ); //Same-machine connections.

	void StopListening() { NetSystem::CloseSocket( m_mySocket ); }
	bool IsListening() { return m_mySocket != INVALID_SOCKET; }
	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const { NetSystem::SockAddrToString( &m_myAddr, out_addrStrBuffer, bufferSize ); }

	TCPConnection* AcceptConnection(); //Creates a new connection if listener queues a request.
};
