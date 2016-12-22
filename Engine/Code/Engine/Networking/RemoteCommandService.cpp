#include "Engine/Networking/RemoteCommandService.hpp"


#include "Engine/EngineCommon.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Networking/tcpip/TCPListener.hpp"
#include "Engine/Networking/tcpip/TCPConnection.hpp"
#include "Engine/Networking/RemoteServiceConnection.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Engine/Core/Command.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC RemoteCommandService* RemoteCommandService::s_theService = nullptr;
static const int MAX_ADDR_STRLEN = 256;

//--------------------------------------------------------------------------------------------------------------
RemoteCommandService::RemoteCommandService()
	: m_listener( nullptr )
{
	RegisterEventHandlers();
}


//--------------------------------------------------------------------------------------------------------------
#pragma region Console Commands
static void CommandServerHost( Command& )
{
	if ( RemoteCommandService::Instance()->IsHosting() )
	{
		g_theConsole->Printf( "Cannot host, already hosting!" );
		return;
	}
	else if ( !RemoteCommandService::Instance()->IsDisconnected() )
	{
		g_theConsole->Printf( "Cannot host, already connected to a host!" );
		return;
	}


	bool isListening = RemoteCommandService::Instance()->Host();
	if ( isListening )
	{
		g_theConsole->Printf( "Listening on: " );
		RemoteCommandService::Instance()->PrintListenerAddressToConsole();
	}
	else g_theConsole->Printf( "Host() failed!" );
}


//--------------------------------------------------------------------------------------------------------------
static void CommandServerStop( Command& )
{
	if ( RemoteCommandService::Instance()->IsHosting() )
	{
		RemoteCommandService::Instance()->Disconnect();
		g_theConsole->Printf( "Stopped command server." );
	}
	else g_theConsole->Printf( "Nothing to stop, service is not currently hosting." );
}


//--------------------------------------------------------------------------------------------------------------
static void CommandServerJoin( Command& args )
{
	if ( RemoteCommandService::Instance()->IsHosting() )
	{
		g_theConsole->Printf( "Cannot join another host, already hosting." );
		return;
	}

	if ( !RemoteCommandService::Instance()->IsDisconnected() )
	{
		g_theConsole->Printf( "Cannot join another host, already connected." );
		return;
	}

	//Spec says debug name (AKA client alias?), not hostname?
	std::string hostname = args.GetArgsString();
	if ( hostname != "" )
	{
		bool success = RemoteCommandService::Instance()->Join( hostname.c_str() );
		if ( success )
		{
			g_theConsole->Printf( "Joined command server at: " );
			RemoteCommandService::Instance()->PrintConnectionsToConsole();
		}
		else g_theConsole->Printf( "Join() failed!" );
	}
	else
	{
		g_theConsole->Printf( "Incorrect arguments. Use NetListTCPAddresses to see available hostnames." );
		g_theConsole->Printf( "Usage: CommandServerJoin <HostName>" );
	}
}


//--------------------------------------------------------------------------------------------------------------
static void CommandServerLeave( Command& )
{
	if ( !RemoteCommandService::Instance()->IsHosting() )
	{
		RemoteCommandService::Instance()->Disconnect();
		g_theConsole->Printf( "Client disconnected from host." );
	}
	else g_theConsole->Printf( "Call CommandServerStop instead for the host." );
}


//--------------------------------------------------------------------------------------------------------------
static void CommandServerInfo( Command& )
{
	RemoteCommandService::Instance()->PrintServiceInstanceState();
}


//--------------------------------------------------------------------------------------------------------------
static void SendCommand( Command& args )
{
	std::string commandStr = args.GetArgsString();
	if ( commandStr != "" )
	{
		//Assuming other msg types (echo and rename) are only used internally, not via command, hence hardcoding type below:
		RemoteCommandService::Instance()->SendDataToAllConnections( MSG_TYPE_COMMAND, commandStr.c_str() ); 
	}
	else
	{
		g_theConsole->Printf( "Incorrect arguments." );
		g_theConsole->Printf( "Usage: SendCommand <CommandStr>" );
	}
}


//--------------------------------------------------------------------------------------------------------------
static void BroadcastCommand( Command& args )
{
	std::string commandStr = args.GetArgsString();
	if ( commandStr != "" )
	{
		//Assuming other msg types are only used internally, not via command, hence hardcoding type below:
		RemoteCommandService::Instance()->SendDataToAllConnections( MSG_TYPE_COMMAND, commandStr.c_str() );
		g_theConsole->RunCommand( commandStr ); //Runs it on all other connections first, then ourselves.
	}
	else
	{
		g_theConsole->Printf( "Incorrect arguments." );
		g_theConsole->Printf( "Usage: BroadcastCommand <CommandStr>" );
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC void RemoteCommandService::RegisterConsoleCommands()
{
	g_theConsole->RegisterCommand( "CommandServerHost", CommandServerHost );
	g_theConsole->RegisterCommand( "CommandServerStop", CommandServerStop );
	g_theConsole->RegisterCommand( "CommandServerJoin", CommandServerJoin );
	g_theConsole->RegisterCommand( "CommandServerLeave", CommandServerLeave );
	g_theConsole->RegisterCommand( "CommandServerInfo", CommandServerInfo );
	g_theConsole->RegisterCommand( "SendCommand", SendCommand );
	g_theConsole->RegisterCommand( "BroadcastCommand", BroadcastCommand );
}
#pragma endregion


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::RegisterEventHandlers()
{
	TheEventSystem::Instance()->RegisterEvent< RemoteCommandService, &RemoteCommandService::OnConnectionMessage_Handler >( "RemoteCommandService_OnConnectionMessage", this );
	TheEventSystem::Instance()->RegisterEvent< RemoteCommandService, &RemoteCommandService::OnServiceJoin_Handler >( "RemoteCommandService_OnServiceJoin", this );
	TheEventSystem::Instance()->RegisterEvent< RemoteCommandService, &RemoteCommandService::OnServiceLeave_Handler >( "RemoteCommandService_OnServiceLeave", this );
}


//--------------------------------------------------------------------------------------------------------------
STATIC RemoteCommandService* RemoteCommandService::Instance()
{
	if ( s_theService == nullptr )
		s_theService = new STATIC RemoteCommandService();

	return s_theService;
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::IsDisconnected() const
{
	if ( IsHosting() )
		return !m_listener->IsListening();
	else if ( !m_connections.empty() ) //For client, check front if it exists.
		return !m_connections[ 0 ]->IsConnected();

	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::Host()
{
	char nameBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::GetLocalHostName( nameBuffer, MAX_ADDR_STRLEN );
	m_listener = new TCPListener( nameBuffer, COMMAND_SERVICE_PORT );
	return m_listener->IsListening();
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::Join( const char* hostName )
{
	TCPConnection* connectionToHost = new TCPConnection( hostName, COMMAND_SERVICE_PORT );
	if ( !connectionToHost->IsConnected() )
		return false;

	RemoteServiceConnection* remoteServiceConnectionWrapper = new RemoteServiceConnection( connectionToHost );
	m_connections.push_back( remoteServiceConnectionWrapper );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::SendDataToAllConnections( byte_t msgTypeID, const char* msgStr )
{
	if ( IsDisconnected() )
		return;

	for each ( RemoteServiceConnection* conn in m_connections )
		conn->SendData( msgTypeID, msgStr );
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::CheckForConnection() //Only used by host.
{
	if ( !IsHosting() )
		return;

	TCPConnection* connectionToClient = m_listener->AcceptConnection();
	if ( connectionToClient == nullptr )
		return;

	RemoteServiceConnection* remoteServiceConnectionWrapper = new RemoteServiceConnection( connectionToClient );
	m_connections.push_back( remoteServiceConnectionWrapper );

	EngineEventOnServerJoin ev;
	connectionToClient->GetConnectionAddress( ev.addrStr, EngineEventOnServerJoin::MAX_ADDR_STRLEN );
	TheEventSystem::Instance()->TriggerEvent( "RemoteCommandService_OnServiceJoin", &ev );
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::CheckForMessages() //Only used by host.
{
	for each ( RemoteServiceConnection* conn in m_connections )
		conn->RecvData(); //Fires a message inside it.
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::CheckForDisconnects() //Only used by host.
{
	for each ( RemoteServiceConnection* conn in m_connections )
	{
		if ( !conn->IsConnected() )
		{
			conn->Disconnect();
			TheEventSystem::Instance()->TriggerEvent( "RemoteCommandService_OnServiceLeave" );
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::Disconnect()
{
	if ( IsHosting() )
	{
		SendDataToAllConnections( MSG_TYPE_COMMAND, "CommandServerLeave" );
		m_listener->StopListening();
		delete m_listener;
		m_listener = nullptr;
	}

	for each ( RemoteServiceConnection* conn in m_connections )
	{
		conn->Disconnect();
		delete conn;
	}
	
	m_connections.clear();
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::PrintServiceInstanceState()
{
	if ( IsHosting() )
	{
		g_theConsole->Printf( "Command Service State: SERVER" );

		g_theConsole->Printf( "Host Connection Address: " );
		PrintListenerAddressToConsole();

		g_theConsole->Printf( "Client Connection Addresses: " );
		PrintConnectionsToConsole();
	}
	else if ( !IsDisconnected() )
	{
		g_theConsole->Printf( "Command Service State: CLIENT" );

		g_theConsole->Printf( "Host Connection Address: " );
		PrintConnectionsToConsole();
	}
	else g_theConsole->Printf( "Command Service State: NONE" );
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::OnConnectionMessage_Handler( EngineEvent* eventContext )
{
	EngineEventOnConnectionMessage* messageData = dynamic_cast<EngineEventOnConnectionMessage*>( eventContext );

	switch ( messageData->m_msgTypeID )
	{
		case MSG_TYPE_COMMAND:
			g_theConsole->Printf( "Running Command: %s", messageData->m_msgData );
			g_theConsole->RunCommand( messageData->m_msgData );
			break;
		case MSG_TYPE_ECHO:
			g_theConsole->Printf( "Echo Message: %s", messageData->m_msgData );
			break;
		case MSG_TYPE_RENAME:
			g_theConsole->Printf( "Unimplemented!" );
			break;
	}

	const bool SHOULD_UNSUB_TO_EVENT = false;
	return SHOULD_UNSUB_TO_EVENT;
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::OnServiceJoin_Handler( EngineEvent* eventContext )
{
	const char* addrStr = dynamic_cast<EngineEventOnServerJoin*>( eventContext )->addrStr;
	g_theConsole->Printf( "You connected to %s.", addrStr );

	const bool SHOULD_UNSUB_TO_EVENT = false;
	return SHOULD_UNSUB_TO_EVENT;
}


//--------------------------------------------------------------------------------------------------------------
bool RemoteCommandService::OnServiceLeave_Handler( EngineEvent* )
{
	char hostAddrBuffer[ MAX_ADDR_STRLEN ];
	m_connections[ 0 ]->GetConnectionAddress( hostAddrBuffer, MAX_ADDR_STRLEN );
	g_theConsole->Printf( "You disconnected from %s.", hostAddrBuffer );

	const bool SHOULD_UNSUB_TO_EVENT = false;
	return SHOULD_UNSUB_TO_EVENT;
}


//--------------------------------------------------------------------------------------------------------------
RemoteCommandService::~RemoteCommandService()
{
	for each ( RemoteServiceConnection* conn in m_connections )
		delete conn;
	m_connections.clear();

	if ( !IsDisconnected() )
		m_listener->StopListening();
	delete m_listener;
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::PrintListenerAddressToConsole()
{
	char addrStrBuffer[ MAX_ADDR_STRLEN ];
	m_listener->GetConnectionAddress( addrStrBuffer, MAX_ADDR_STRLEN );
	g_theConsole->Printf( "%s", addrStrBuffer );
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::PrintConnectionsToConsole()
{
	if ( m_connections.empty() )
		g_theConsole->Printf( "No connections." );

	char addrBuffer[ MAX_ADDR_STRLEN ];
	for each ( RemoteServiceConnection* conn in m_connections )
	{
		conn->GetConnectionAddress( addrBuffer, MAX_ADDR_STRLEN );
		g_theConsole->Printf( "%s", addrBuffer );
	}
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::Update()
{
	CheckForConnection(); 
	CheckForMessages(); 
	CheckForDisconnects();
}


//--------------------------------------------------------------------------------------------------------------
void RemoteCommandService::Autoconnect()
{
	ASSERT_OR_DIE( RemoteCommandService::s_theService == nullptr, nullptr ); //Should only be called at the beginning of the program right now.

//	g_theConsole->ShowConsole();

	char nameBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::GetLocalHostName( nameBuffer, MAX_ADDR_STRLEN );
	bool didJoin = RemoteCommandService::Instance()->Join( nameBuffer ); //Note this means NetSystem::Startup must come first!
	if ( didJoin )
	{
		g_theConsole->Printf( "Joined command server at: " );
		RemoteCommandService::Instance()->PrintConnectionsToConsole();
		return; //Only try to host if join fails.
	}
	else g_theConsole->Printf( "Join() failed!" );

	bool isListening = RemoteCommandService::Instance()->Host();
	if ( isListening )
	{
		g_theConsole->Printf( "Acting as Host. Listening on: " );
		RemoteCommandService::Instance()->PrintListenerAddressToConsole();
	}
	else g_theConsole->Printf( "Host() failed!" );
}
