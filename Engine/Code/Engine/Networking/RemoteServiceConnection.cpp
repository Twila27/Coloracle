#include "Engine/Networking/RemoteServiceConnection.hpp"


#include "Engine/Networking/tcpip/TCPConnection.hpp"
#include "Engine/Core/TheEventSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
RemoteServiceConnection::~RemoteServiceConnection()
{
	delete m_tcpConnection;
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteServiceConnection::IsConnected() const
{
	return ( m_tcpConnection != nullptr ) && ( m_tcpConnection->IsConnected() );
}


//--------------------------------------------------------------------------------------------------------------
void RemoteServiceConnection::SendData( const byte_t msgTypeID, const char* msgData )
{
	//THIS WILL NOT WORK FOR WIDE (L or T) CHARS. Format is standardized to allow classwide communications.

	m_tcpConnection->SendData( &msgTypeID, 1 ); //Sends 1 byte over socket, since sizeof( byte_t ) == sizeof( char ). 
									 //See header's const byte_t variables for types.

									 //Can check to see if the msg arg is nullptr and wrap the whole block in that check if wanted, but we'll assume it never is.
	m_tcpConnection->SendData( msgData, strlen( msgData) ); //Since strlen(msg) == msg's #bytes.

	char nil = NULL;
	m_tcpConnection->SendData( &nil, 1 ); //Message end-terminator, only afforded if all we send/recv are strings.
}


//--------------------------------------------------------------------------------------------------------------
void RemoteServiceConnection::RecvData()
{
	if ( !IsConnected() )
		return;

	const size_t BUFFER_SIZE = 1 KB;
	byte_t buffer[ BUFFER_SIZE ];

	size_t read = m_tcpConnection->RecvData( buffer, BUFFER_SIZE );

	while ( read > 0 ) //Only 0 when no bytes were received, else == msg length.
	{
		for ( size_t byteIndex = 0; byteIndex < read; ++byteIndex )
		{
			char c = buffer[ byteIndex ];

			m_nextMessage.push_back( c ); //Not 100% secure this way.
										  //i.e. Not cleared between iterations, so will wait on a terminator even between packets.

			if ( c == NULL ) //Our message terminator.
			{
				EngineEventOnConnectionMessage onConnectionMessage;
				onConnectionMessage.m_msgTypeID = m_nextMessage[ 0 ]; //First byte in buffer is type ID.
				onConnectionMessage.m_msgData = &m_nextMessage[ 1 ]; //All bytes AFTER the first.

				TheEventSystem::Instance()->TriggerEvent( "RemoteCommandService_OnConnectionMessage", &onConnectionMessage );

				m_nextMessage.clear(); //To receive next null-terminated message.
			}
		}

		read = m_tcpConnection->RecvData( buffer, BUFFER_SIZE );
	}
}


//--------------------------------------------------------------------------------------------------------------
void RemoteServiceConnection::Disconnect()
{
	if ( m_tcpConnection != nullptr )
		m_tcpConnection->Disconnect();

	delete m_tcpConnection;
	m_tcpConnection = nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void RemoteServiceConnection::GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize )
{
	m_tcpConnection->GetConnectionAddress( out_addrStrBuffer, bufferSize );
}
