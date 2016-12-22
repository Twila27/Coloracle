#pragma once


#include <vector>
#include "Engine/EngineCommon.hpp"
#include "Engine/Core/EngineEvent.hpp"


class TCPConnection;


//-----------------------------------------------------------------------------
struct EngineEventOnConnectionMessage : public EngineEvent
{
	EngineEventOnConnectionMessage() : EngineEvent( "EngineEvent_OnConnectionMessage" ) {}
	byte_t m_msgTypeID;
	const char* m_msgData;
};


//-----------------------------------------------------------------------------
class RemoteServiceConnection
{
public:

	RemoteServiceConnection( TCPConnection* tcpConnection ) : m_tcpConnection( tcpConnection ) {}
	~RemoteServiceConnection();

	bool IsConnected() const;
	void SendData( const byte_t msgTypeID, const char* msgData );
	void RecvData();
	void Disconnect();
	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize );

private:

	TCPConnection* m_tcpConnection;
	std::vector<char> m_nextMessage; //Built dynamically to keep from having to resize if a novel's sent.
};
