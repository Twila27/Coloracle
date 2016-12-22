#include "Game/TheGame.hpp"

#include "Engine/Networking/NetSession.hpp"
#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/NetSender.hpp"
#include "Engine/Networking/NetConnection.hpp"
#include "Engine/Networking/NetMessageCallbacks.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Core/TheConsole.hpp"

#include "Game/DummyNetObj.hpp"
#include "Game/Game Entities/PlayerAvatar.hpp"
#include "Game/Game Entities/TeardropNPC.hpp"
#include "Game/Game Entities/PlayerController.hpp"
#include "Game/Game Entities/NetObjectSystem.hpp"
#include "Game/Game Entities/NetObject Protocols/ProtocolPlayerAvatar.hpp"
#include "Game/Game Entities/NetObject Protocols/ProtocolTeardropNPC.hpp"


//-----------------------------------------------------------------------------
bool TheGame::IsMyConnectionHosting()
{
	return m_gameSession->IsMyConnectionHosting();
}


//--------------------------------------------------------------------------------------------------------------
static bool GetSockAddrInFromCommand( Command& args, sockaddr_in& out_addr )
{
	//Needs a full address string--IP and port #.
	std::string ipStr;
	int portNum = 0;
	bool doArgsExist = args.GetNextString( &ipStr ) && args.GetNextInt( &portNum, 0 );
	if ( !doArgsExist )
		return false;

	if ( !NetSystem::SockAddrFromStringIPv4( &out_addr, ipStr, (uint16_t)portNum ) )
		return false;

	return true;
}


//--------------------------------------------------------------------------------------------------------------
static void OnBoomReceived( const NetSender&, NetMessage& msg )
{
	if ( strcmp( g_theGame->GetCurrentStateName(), "TheGameState_MidMatch" ) != 0 )
		return; //Don't process otherwise.

	GameEventPlayerExploded ev;
	msg.Read<float>( &ev.m_position.x );
	msg.Read<float>( &ev.m_position.y );
	g_theGame->OnPlayerExploded_Handler( &ev );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionPingsInOrder( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	NetMessage msg( NETMSG_PING_INORDER );
	std::string optMsgStr;

	sockaddr_in to;
	bool success = GetSockAddrInFromCommand( args, to );
	if ( !success )
		goto badArgs;

	//Check for optional message argument.
	args.GetNextString( &optMsgStr ); //Reusing the IP string var to hold the optional string.
	msg.WriteString( optMsgStr.c_str() ); //Doesn't matter if it has none. 
		//i.e. Our functions will just write empty string or null (as 0xFF).
	
	NetConnection* conn = g_theGame->GetGameNetSession()->FindConnectionByAddressAndPort( to );
	if ( conn == nullptr )
	{
		g_theConsole->Printf( "Failed to find connection, call NetSessionCreateConnection first!" );
		return;
	}
	else
	{
		NetMessage msgs[ 8 ] = { msg, msg, msg, msg, msg, msg, msg, msg };
		conn->SendMessagesToThem( msgs, 8 );
		g_theConsole->Printf( "Pings sent." );
	}
	return;

badArgs:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage: NetSessionPingsInOrder <ip> <port#> [messageStr]." ); //e.g. NetSessionPing 192.168.1.65 4334 myMessage
	return;
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionPing( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	sockaddr_in to;
	bool success = GetSockAddrInFromCommand( args, to );
	if ( !success )
	{
		g_theConsole->Printf( "Incorrect arguments." );
		g_theConsole->Printf( "Usage: NetSessionPing <ip> <port#> [messageStr]." ); //e.g. NetSessionPing 192.168.1.65 4334 myMessage
	}


	//Check for optional message argument.
	std::string optMsgStr;
	args.GetNextString( &optMsgStr ); //Reusing the IP string var to hold the optional string.
	NetMessage msg( NETMSG_PING );
	msg.WriteString( optMsgStr.c_str() ); //Doesn't matter if it has none. 
		//i.e. Our functions will just write empty string or null (as 0xFF).

	g_theGame->GetGameNetSession()->SendMessageDirect( to, msg );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionPingPingPingPing( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	//Needs a full address string--IP and port #.
	sockaddr_in to;
	bool success = GetSockAddrInFromCommand( args, to );
	if ( !success )
	{
		g_theConsole->Printf( "Incorrect arguments." );
		g_theConsole->Printf( "Usage: NetSessionPingPingPingPing <ip> <port#> [messageStr]." ); //e.g. NetSessionPing 192.168.1.65 4334 myMessage
	}

	//Check for optional message argument.
	std::string optMsgStr;
	args.GetNextString( &optMsgStr ); //Reusing the IP string var to hold the optional string.
	NetMessage msg( NETMSG_PING );
	msg.WriteString( optMsgStr.c_str() ); //Doesn't matter if it has none. 
		//i.e. Our functions will just write empty string or null (as 0xFF).

	NetMessage msgs[ 4 ] = { msg, msg, msg, msg };
	g_theGame->GetGameNetSession()->SendMessagesDirect( to, msgs, 4 );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSimLag( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	NetSession* sessionRef = g_theGame->GetGameNetSession();

	int minAdditionalLagMilliseconds;
	bool didArgExist = args.GetNextInt( &minAdditionalLagMilliseconds, 0 );
	if ( !didArgExist )
		goto badArgs;

	int maxAdditionalLagMilliseconds;
	didArgExist = args.GetNextInt( &maxAdditionalLagMilliseconds, 0 );
	if ( didArgExist && ( minAdditionalLagMilliseconds > maxAdditionalLagMilliseconds ) ) //Swap if they were written out of order.
	{
		int temp = maxAdditionalLagMilliseconds;
		maxAdditionalLagMilliseconds = minAdditionalLagMilliseconds;
		minAdditionalLagMilliseconds = temp;
	}

	sessionRef->SetSimulatedMinAdditionalLag( minAdditionalLagMilliseconds );
	if ( didArgExist )
		sessionRef->SetSimulatedMaxAdditionalLag( maxAdditionalLagMilliseconds );

	double actualMin = sessionRef->GetSimulatedMinAdditionalLag();
	double actualMax = sessionRef->GetSimulatedMaxAdditionalLag();
	g_theConsole->Printf( "NetSession now using an additional lag of min %.2f s, max %.2f s.", actualMin, actualMax );
	Logger::PrintfWithTag( "NetSession", "NetSession now using an additional lag of min %.2f s, max %.2f s.", actualMin, actualMax );
	return;

badArgs:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage: NetSimLag <int minAdditionalLagMilliseconds> [int maxAddedLagMs]" );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSimLoss( Command& arg )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	NetSession* sessionRef = g_theGame->GetGameNetSession();

	float firstPercentile01;
	bool didArgExist = arg.GetNextFloat( &firstPercentile01, -1.f );
	if ( !didArgExist || ( firstPercentile01 < 0.f ) || ( firstPercentile01 > 1.f ) )
		goto badArg;

	float secondPercentile01;
	didArgExist = arg.GetNextFloat( &secondPercentile01, -1.f );

	if ( !didArgExist ) //User only wants Usage 1.
	{
		sessionRef->SetSimulatedAdditionalLoss( firstPercentile01 );
		goto feedback;
	}

	//At this point, we know they're at least trying to use Usage 2.
	if ( ( secondPercentile01 < 0.f ) || ( secondPercentile01 > 1.f ) )
		goto badArg;

	if ( firstPercentile01 > secondPercentile01 ) //Out of order, so swap args.
	{
		float temp = firstPercentile01;
		firstPercentile01 = secondPercentile01;
		secondPercentile01 = temp;
	}

	sessionRef->SetSimulatedMinAdditionalLoss( firstPercentile01 );
	sessionRef->SetSimulatedMaxAdditionalLoss( secondPercentile01 );

feedback:
	float actualLossMin = sessionRef->GetSimulatedMinAdditionalLoss();
	float actualLossMax = sessionRef->GetSimulatedMaxAdditionalLoss();
	g_theConsole->Printf( "NetSession now using a packet drop rate of %f min, %f max.", actualLossMin, actualLossMax );
	Logger::PrintfWithTag( "NetSession", "NetSession now using a packet drop rate of %f min, %f max.", actualLossMin, actualLossMax );
	return;

badArg:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage 1: NetSimLoss <float lossPercentage01>" );
	g_theConsole->Printf( "Usage 2: NetSimLoss <min> <max>" );
}


//--------------------------------------------------------------------------------------------------------------
static void NetToggleTimeouts( Command& )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	NetSession* sessionRef = g_theGame->GetGameNetSession();

	bool resultValue = sessionRef->ToggleTimeouts();

	char* resultStr = resultValue ? "now" : "stopped";
	Logger::PrintfWithTag( "NetSession", "NetSession %s using timeouts.", resultStr );
	g_theConsole->Printf( "NetSession %s using timeouts.", resultStr );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionStart( Command& )
{
	if ( g_theGame->GetGameNetSession() != nullptr )
	{
		Logger::PrintfWithTag( "NetSession", "WARNING: NetSession already started!" );
		g_theConsole->Printf( "NetSession already started!" );
		return;
	}

	g_theGame->StartGameNetSession();
}


//-----------------------------------------------------------------------------
static void OnPlayerRequestReceivedFromClient( const NetSender& from, NetMessage& playerRequestMsg )
{
	if ( !g_theGame->IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Client should never receive creation request!" );
		return;
	}

	nuonce_t createNuonce;
	playerRequestMsg.Read<nuonce_t>( &createNuonce );
	//For now, assuming requests (requested only once) always succeed.

	//[A6 spec has game-specific code here to force one player per connection here.]

	PlayerController* requestedController = new PlayerController( createNuonce );
	requestedController->ReadFromMessage( playerRequestMsg );
	g_theGame->ServerRegisterIndexForPlayer( requestedController );

	//Temporary Hack: forcing player to the unique roles here until I have time to create a separate selection menu.
	PlayerIndex newIndex = requestedController->GetPlayerIndex();
	if ( newIndex >= NUM_PRIMARY_TEAR_COLORS )
	{
		ERROR_RECOVERABLE( "Unexpected PlayerIndex in OnPlayerRequestReceivedFromClient!" );
	}
	else
	{
		requestedController->SetPrimaryTearColor( (PrimaryTearColor)newIndex );
	}

	NetMessage playerJoinedNotify( NETMSG_GAME_PLAYER_JOINED_SFS );
	playerJoinedNotify.Write<nuonce_t>( createNuonce );
	requestedController->WriteToMessage( playerJoinedNotify );

	from.ourSession->SendToAllConnections( playerJoinedNotify );
}


//--------------------------------------------------------------------------------------------------------------
static void OnPlayerJoinedReceivedFromServer( const NetSender& from, NetMessage& playerJoinedMsg )
{
	//This new player may or may not be my own.
	nuonce_t playerCreationNuonce;
	playerJoinedMsg.Read<nuonce_t>( &playerCreationNuonce );

	//We need a local temp player made here to be able to get out the NetConnectionIndex from the message.
	PlayerController controllerData( playerCreationNuonce );
	controllerData.ReadFromMessage( playerJoinedMsg );

	//Figure out whether this was our player request or not:
	PlayerController* controller = nullptr;
	bool wasMyRequest = ( controllerData.GetOwningConnectionIndex() == from.ourSession->GetMyConnectionIndex() );
	if ( wasMyRequest )
	{
		controller = g_theGame->PopUnconfirmedPlayerForNuonce( playerCreationNuonce );
		if ( controller == nullptr )
		{
			ERROR_RECOVERABLE( "It was our connection index in the message, but we didn't request a player?" );
			//This case will be normal if the host can create players for the clients without request.
			return;
		}
	}
	else //Means this is just the host notifying us of other existing players we need to create.
	{
		controller = new PlayerController( playerCreationNuonce );
	}

	controller->SetPlayerData( controllerData );

	//POINT AT WHICH SOMEONE ACTUALLY JOINS, CALLBACKS (like for when we actually connected) HERE:
	g_theGame->ClientRegisterPlayerAsIndex( controller, controllerData.GetPlayerIndex() ); //Will return false for a non-dedicated host, but that's fine.
}


//--------------------------------------------------------------------------------------------------------------
void OnPlayerUpdateReceivedFromServer( const NetSender&, NetMessage& playerUpdateMsg )
{	
	//We need a local temp player made here to be able to get out the NetConnectionIndex from the message.
	PlayerController controllerData;
	controllerData.ReadFromMessage( playerUpdateMsg );

	//ASSUMING THAT THE PLAYER INDEX DOESN'T CHANGE UNTIL DELETION:
	PlayerController* controller = g_theGame->GetIndexedPlayerController( controllerData.GetPlayerIndex() );
	if ( controller == nullptr )
	{
		ERROR_RECOVERABLE( "Asked to update a player we haven't received a JOINED_SFS to create from!" );
		return;
	}
	controller->SetPlayerData( controllerData );
}


//--------------------------------------------------------------------------------------------------------------
void OnPlayerUpdateReceivedFromClient( const NetSender& from, NetMessage& playerUpdateMsg )
{
	if ( !from.ourSession->IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Client should never receive player update from another client!" );
		return;
	}

	//Disseminate to all connections.
	NetMessageDefinition* defn = from.ourSession->FindDefinitionForMessageType( NETMSG_GAME_PLAYER_UPDATE_SFS );
	ASSERT_RETURN( defn );
	playerUpdateMsg.SetMessageDefinition( defn );
	from.ourSession->SendToAllConnections( playerUpdateMsg );
}


//--------------------------------------------------------------------------------------------------------------
void OnPlayerLeaveReceivedFromServer( const NetSender&, NetMessage& playerLeftMsg )
{
	PlayerController playerData;
	playerData.ReadFromMessage( playerLeftMsg );
	g_theGame->DestroyPlayer( playerData.GetPlayerIndex() );
}


//--------------------------------------------------------------------------------------------------------------
void OnMatchStartReceived( const NetSender&, NetMessage& )
{
	if ( strcmp( g_theGame->GetCurrentStateName(), "TheGameState_Lobby" ) != 0 )
		return; //Don't process otherwise.

	//This gets sent to all connections the host has, including its own.
//	LogAndShowPrintfWithTag( "NetSession", "OnMatchStartReceived" );
	
	g_theGame->ClientStartMatch(); //This causes the background to change, and game logic to switch gears.
		//Note we do this BEFORE making avatars appear. Should do likewise with spawning enemies AFTER avatars.

	//Create an avatar for every player controller that's been requested and created since a connection joined.
	if ( g_theGame->IsMyConnectionHosting() )
		g_theGame->ServerCreateAndSendPlayerAvatars();
}


//--------------------------------------------------------------------------------------------------------------
void OnMatchEndReceived( const NetSender&, NetMessage& matchEndMessage )
{
	if ( strcmp( g_theGame->GetCurrentStateName(), "TheGameState_MidMatch" ) != 0 )
		return; //Don't process otherwise.

//	LogAndShowPrintfWithTag( "NetSession", "OnMatchEndReceived" );

	PlayerIndex winningPlayerIndex;
	matchEndMessage.Read<PlayerIndex>( &winningPlayerIndex );

	g_theGame->ClientEndMatch( winningPlayerIndex );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::StartGameNetSession()
{
	m_gameSession = new NetSession( "GameSession" );
	m_gameSession->RegisterMessage( NETMSG_GAME_BOOM, "Game_Boom", OnBoomReceived, NETMSGCTRL_NONE, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_PING_INORDER, "Ping_InOrder", OnPingReceived, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );

	m_gameSession->RegisterMessage( NETMSG_GAME_MATCH_STARTED, "Game_MatchStarted", OnMatchStartReceived, NETMSGCTRL_NONE, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_MATCH_ENDED, "Game_MatchEnded", OnMatchEndReceived, NETMSGCTRL_NONE, NETMSGOPT_RELIABLE );

	m_gameSession->RegisterMessage( NETMSG_GAME_PLAYER_REQUEST_SFC, "Game_Player_Request", OnPlayerRequestReceivedFromClient, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_PLAYER_JOINED_SFS, "Game_Player_Joined", OnPlayerJoinedReceivedFromServer, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_PLAYER_UPDATE_SFS, "Game_Player_ServerUpdate", OnPlayerUpdateReceivedFromServer, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_PLAYER_UPDATE_SFC, "Game_Player_ClientUpdate", OnPlayerUpdateReceivedFromClient, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_PLAYER_LEAVE_SFS, "Game_Player_ServerLeave", OnPlayerLeaveReceivedFromServer, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );

	//Whereas PlayerController updates were reliable, NetObjUpdates (INCLUDING PlayerAvatar) are unreliable.
	m_gameSession->RegisterMessage( NETMSG_GAME_NETOBJ_CREATE_SFS, "Game_NetObj_Create", OnNetObjectCreateReceivedFromServer, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );
	m_gameSession->RegisterMessage( NETMSG_GAME_NETOBJ_UPDATE_SFS, "Game_NetObj_ServerUpdate", OnNetObjectUpdateReceivedFromServer, NETMSGCTRL_NONE, NETMSGOPT_NONE );
	m_gameSession->RegisterMessage( NETMSG_GAME_NETOBJ_UPDATE_SFC, "Game_NetObj_ClientUpdate", OnNetObjectUpdateReceivedFromClient, NETMSGCTRL_NONE, NETMSGOPT_NONE );
	m_gameSession->RegisterMessage( NETMSG_GAME_NETOBJ_DESYNC_SFS, "Game_NetObj_Desync", OnNetObjectDesyncReceivedFromServer, NETMSGCTRL_PROCESSED_INORDER, NETMSGOPT_RELIABLE );

	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::OnConnectionJoined_Handler >( "OnConnectionJoined", g_theGame );
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::OnConnectionLeave_Handler >( "OnConnectionLeave", g_theGame );
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::OnNetworkTick_Handler >( "OnNetworkTick", g_theGame );

	if ( m_gameSession->Start( GAME_PORT, MAX_NUM_PLAYERS ) )
	{
		char addrBuffer[ MAX_ADDR_STRLEN ];
		m_gameSession->GetConnectionAddress( addrBuffer, MAX_ADDR_STRLEN );

		Logger::PrintfWithTag( "NetSession", "NetSession opened socket at address %s.", addrBuffer );
		g_theConsole->Printf( "NetSession opened socket at address %s.", addrBuffer );
	}
	else
	{
		Logger::PrintfWithTag( "NetSession", "NetSession failed to open a socket." );
		g_theConsole->Printf( "NetSession failed to open a socket." );
		delete m_gameSession;
		m_gameSession = nullptr;
	}
}


//--------------------------------------------------------------------------------------------------------------
static void NetGameStart( Command& ) //Begin the game with connected players.
{
	if ( g_theGame->GetGameNetSession() == nullptr )
	{
		LogAndShowPrintfWithTag( "NetSession", "Call NetSessionStart and NetSessionHost first!" );
		return;
	}

	if ( !g_theGame->IsMyConnectionHosting() )
	{
		LogAndShowPrintfWithTag( "NetSession", "Need to be host to start the game!" );
		return;
	}

	g_theGame->ServerStartMatch();
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionCreateConnection( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	std::string ipStr, guidStr; //Have to define before gotos.

	int assignedIndex;
	bool doesConnIndexExist = args.GetNextInt( &assignedIndex, -1 );
	if ( !doesConnIndexExist )
		goto badArgs;

	//Needs a full address string--IP and port #.
	sockaddr_in to; //Not using the helper function because it wants to print out each part.
	int portNum = 0;
	bool doArgsExist = args.GetNextString( &ipStr ) && args.GetNextInt( &portNum, 0 );
	if ( !doArgsExist )
		goto badArgs;

	if ( !NetSystem::SockAddrFromStringIPv4( &to, ipStr, (uint16_t)portNum ) )
		goto badArgs;

	bool doesGuidExist = args.GetNextString( &guidStr );
	if ( !doesGuidExist )
		goto badArgs;

	char guidBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::GetLocalHostName( guidBuffer, MAX_ADDR_STRLEN );
	static unsigned int numInvocation = 0;
	snprintf( guidBuffer, MAX_ADDR_STRLEN, "%s%u_%s", guidBuffer, numInvocation++, guidStr.c_str() );
	NetConnection* conn = g_theGame->GetGameNetSession()->CreateConnection( (NetConnectionIndex)assignedIndex, guidBuffer, to );
	if ( conn != nullptr )
	{
		LogAndShowPrintfWithTag( "NetSession", "NetSession created connection to %s:%d.", ipStr.c_str(), portNum );

		EngineEventNetworked ev;
		ev.connection = conn;
		TheEventSystem::Instance()->TriggerEvent( "OnConnectionJoined", &ev );
	}
	else
	{
		LogAndShowPrintfWithTag( "NetSession", "NetSession failed to create connection to %s:%d.", ipStr.c_str(), portNum );
	}

	return;

badArgs:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage: NetSessionCreateConnection <0-%d ConnIndex> <ip> <port#> <guidStr>", MAX_CONNECTIONS ); //e.g. NetSessionPing 192.168.1.65 4334 myMessage
	return;
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionDestroyConnection( Command& args )
{
	if ( !g_theGame->IsGameSessionRunning() )
		return;

	int indexToDestroy;
	bool doesConnIndexExist = args.GetNextInt( &indexToDestroy, -1 );
	if ( !doesConnIndexExist )
		goto badArgs;

	bool didConnExist = g_theGame->GetGameNetSession()->DestroyConnection( (NetConnectionIndex)indexToDestroy );
	if ( didConnExist )
	{
		Logger::PrintfWithTag( "NetSession", "NetSession removed connection %u.", indexToDestroy );
		g_theConsole->Printf( "NetSession removed connection %u.", indexToDestroy );

		EngineEventNetworked ev;
		ev.SetConnectionIndex( (NetConnectionIndex)indexToDestroy );
		TheEventSystem::Instance()->TriggerEvent( "OnConnectionLeave", &ev );
	}
	else
	{
		Logger::PrintfWithTag( "NetSession", "NetSession does not have connection %u.", indexToDestroy );
		g_theConsole->Printf( "NetSession does not have connection %u.", indexToDestroy );
	}

	return;

badArgs:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage: NetSessionDestroyConnection <0-%d ConnIndex>", MAX_CONNECTIONS ); //e.g. NetSessionPing 192.168.1.65 4334 myMessage
	return;
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionHost( Command& args )
{
	g_theGame->HostGame( args.GetArgsString() );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionHostStartListening( Command& )
{
	g_theGame->StartHostListen();
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionHostStopListening( Command& )
{
	g_theGame->StopHostListen();
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionJoin( Command& args )
{
	std::string out_username;
	bool success = args.GetNextString( &out_username );
	if ( !success )
		goto badArgs;

	sockaddr_in to;
	success = GetSockAddrInFromCommand( args, to );
	if ( !success )
		goto badArgs;

	g_theGame->JoinGame( out_username, to );
	return;

badArgs:
	g_theConsole->Printf( "Incorrect arguments." );
	g_theConsole->Printf( "Usage: NetSessionJoin <usernameStr> <ipStr> <port#>" );
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionLeave( Command& )
{
	g_theGame->LeaveGame();
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::HostGame( const std::string& username )
{
	if ( m_gameSession == nullptr )
	{
		LogAndShowPrintfWithTag( "GameNetSession", "Call NetSessionStart first!" );
	}
	else
	{
		m_gameSession->Host( username.c_str() );
		m_lastSentCreationNuonce = 0; //Start back over to be in sync with incoming clients.
	}
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::StartHostListen()
{
	if ( m_gameSession == nullptr )
	{
		LogAndShowPrintfWithTag( "GameNetSession", "Call NetSessionStart first!" );
	}
	else
	{
		m_gameSession->StartListening();
	}
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::StopHostListen()
{
	if ( m_gameSession == nullptr )
	{
		LogAndShowPrintfWithTag( "GameNetSession", "Call NetSessionStart first!" );
	}
	else
	{
		m_gameSession->StopListening();
	}
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::JoinGame( const std::string& username, sockaddr_in& hostAddr )
{
	if ( m_gameSession == nullptr )
	{
		LogAndShowPrintfWithTag( "GameNetSession", "Call NetSessionStart first!" );
	}
	else
	{
		m_gameSession->Join( username.c_str(), hostAddr );
		m_lastSentCreationNuonce = 0; //Start back over to be in sync with incoming host.
	}
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::LeaveGame()
{
	if ( m_gameSession == nullptr )
	{
		LogAndShowPrintfWithTag( "GameNetSession", "Call NetSessionStart first!" );
	}
	else
	{
		m_gameSession->Leave();
		NetObjectSystem::ResetBaseNetObjectID(); //See comments inside.
	}
}


//--------------------------------------------------------------------------------------------------------------
static void NetSessionStop( Command& )
{
	g_theGame->StopGameNetSession();
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::StopGameNetSession()
{
	if ( m_gameSession == nullptr )
	{
		Logger::PrintfWithTag( "NetSession", "WARNING: NetSession doesn't exist to be stopped!" );
		g_theConsole->Printf( "NetSession doesn't exist to be stopped!" );
		return;
	}
	if ( !m_gameSession->IsRunning() )
	{
		Logger::PrintfWithTag( "NetSession", "WARNING: NetSession isn't running to be stopped!" );
		g_theConsole->Printf( "NetSession isn't running to be stopped!" );
		return;
	}

	TheEventSystem::Instance()->TriggerEvent( "OnNetSessionShutdown" );
	TheEventSystem::Instance()->UnregisterEvent( "OnNetSessionShutdown" );
	TheEventSystem::Instance()->UnregisterEvent( "OnConnectionJoined" );
	TheEventSystem::Instance()->UnregisterEvent( "OnConnectionLeave" );
	TheEventSystem::Instance()->UnregisterEvent( "OnNetworkTick" );

	Logger::PrintfWithTag( "NetSession", "NetSession stopped." );
	g_theConsole->Printf( "NetSession stopped." );

	delete m_gameSession;
	m_gameSession = nullptr;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::IsGameSessionRunning()
{
	bool result = ( g_theGame->GetGameNetSession() != nullptr ) && g_theGame->GetGameNetSession()->IsRunning();
	if ( !result )
	{
		Logger::PrintfWithTag( "NetSession", "WARNING: NetSession needs to be started and running first!" );
		g_theConsole->Printf( "NetSession needs to be started and running first!" );
	}
	return result;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::RegisterConsoleCommands_Networking()
{
	//SD6 A2
	g_theConsole->RegisterCommand( "NetSessionStart", NetSessionStart );
	g_theConsole->RegisterCommand( "NetSessionStop", NetSessionStop );
	g_theConsole->RegisterCommand( "NetSessionPing", NetSessionPing );
	g_theConsole->RegisterCommand( "NetSessionPingPingPingPing", NetSessionPingPingPingPing );

	//SD6 A3
	g_theConsole->RegisterCommand( "NetSessionCreateConnection", NetSessionCreateConnection );
	g_theConsole->RegisterCommand( "NetSessionDestroyConnection", NetSessionDestroyConnection );
	g_theConsole->RegisterCommand( "NetSimLag", NetSimLag );
	g_theConsole->RegisterCommand( "NetSimLoss", NetSimLoss );

	//SD6 A4
	g_theConsole->RegisterCommand( "NetSessionToggleTimeouts", NetToggleTimeouts );

	//SD6 A5
	g_theConsole->RegisterCommand( "NetSessionPingsInOrder", NetSessionPingsInOrder );
	g_theConsole->RegisterCommand( "NetSessionHost", NetSessionHost );
	g_theConsole->RegisterCommand( "NetSessionHostStartListening", NetSessionHostStartListening );
	g_theConsole->RegisterCommand( "NetSessionHostStopListening", NetSessionHostStopListening );
	g_theConsole->RegisterCommand( "NetSessionJoin", NetSessionJoin );
	g_theConsole->RegisterCommand( "NetSessionLeave", NetSessionLeave );

	//SD6 A6
	g_theConsole->RegisterCommand( "NetGameStart", NetGameStart );
	g_theConsole->RegisterCommand( "NetStartGame", NetGameStart );
}


//-----------------------------------------------------------------------------
bool TheGame::OnConnectionJoined_Handler( EngineEvent* eventContext )
{
	NetConnection* conn = dynamic_cast<EngineEventNetworked*>( eventContext )->connection;

	//This way even the host must request a player, practices writing singleplayer as multiplayer:
	if ( conn->IsMe() )
		ClientCreatePlayerRequest( conn ); //See processing in OnPlayerCreationRequestReceived.

	//If someone else joins, the server host needs to send them a PLAYER_CREATE for every player currently in the game.
	NetSession* session = conn->GetSession(); //Use this to support more than one session?
	if ( session->IsMyConnectionHosting() )
		g_theGame->SendEveryPlayerControllerToConnection( conn );
		
	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::SendEveryPlayerControllerToConnection( NetConnection* conn )
{
	for ( PlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PLAYERS; playerIndex++ )
	{
		PlayerController* currentController = GetIndexedPlayerController( playerIndex );
		if ( currentController == nullptr )
			continue;

		NetMessage playerJoinedNotify( NETMSG_GAME_PLAYER_JOINED_SFS );
		playerJoinedNotify.Write<nuonce_t>( currentController->GetCreationNuonce() );
		currentController->WriteToMessage( playerJoinedNotify );
		conn->SendMessageToThem( playerJoinedNotify );
	}
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::DestroyPlayer( PlayerIndex destroyedPlayer )
{
	ASSERT_OR_DIE( destroyedPlayer < MAX_NUM_PLAYERS, nullptr );

	PlayerController* theirController = GetIndexedPlayerController( destroyedPlayer );
	if ( theirController != nullptr )
		delete theirController;
	SetIndexedPlayerController( destroyedPlayer, nullptr );

	if ( m_playerAvatars[ destroyedPlayer ] != nullptr )
		NetObjectSystem::NetStopSyncForLocalObject( m_playerAvatars[ destroyedPlayer ] ); //Deletes avatar in here.

	SetIndexedPlayerAvatar( destroyedPlayer, nullptr ); 
		//All will have called delete in NetStopSync (called by SFS msg's handler for clients).
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::OnConnectionLeave_Handler( EngineEvent* eventContext )
{
	NetConnectionIndex connIndex = dynamic_cast<EngineEventNetworked*>( eventContext )->GetConnectionIndex();
	
	if ( IsMyConnectionHosting() )
	{
		std::vector<PlayerController*> leavingPlayers;
		size_t numPlayers = GetControllersOnConnection( leavingPlayers, m_gameSession->GetIndexedConnection( connIndex ) );
		for ( size_t controllerIndex = 0; controllerIndex < numPlayers; controllerIndex++ )
		{
			NetMessage playerLeftMsg( NETMSG_GAME_PLAYER_LEAVE_SFS );
			leavingPlayers[ controllerIndex ]->WriteToMessage( playerLeftMsg );
			m_gameSession->SendToAllConnections( playerLeftMsg );
			DestroyPlayer( leavingPlayers[ controllerIndex ]->GetPlayerIndex() );
		}
	}
	else if ( m_gameSession->GetMyConnectionIndex() == connIndex )
	{
		//If I'm leaving, even if not the host, I don't affect others anymore, so I'm cleaning myself up.
		for ( size_t controllerIndex = 0; controllerIndex < MAX_NUM_PLAYERS; controllerIndex++ )
		{
			if ( m_playerControllers[ controllerIndex ] != nullptr )
				DestroyPlayer( m_playerControllers[ controllerIndex ]->GetPlayerIndex() );
		}
	}

	//-----------------------------------------------------------------------------

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
size_t TheGame::GetControllersOnConnection( std::vector<PlayerController*>& out_players, NetConnection* conn )
{
	if ( conn == nullptr )
		return 0U;

	NetConnectionIndex owningConnIndex = conn->GetIndex();
	for each ( PlayerController* controller in m_playerControllers )
	{
		if ( controller == nullptr )
			continue;

		if ( controller->GetOwningConnectionIndex() == owningConnIndex )
			out_players.push_back( controller );
	}

	return out_players.size();
}


//--------------------------------------------------------------------------------------------------------------
size_t TheGame::GetAvatarsOnConnection( std::vector<PlayerAvatar*>& out_players, NetConnection* conn )
{
	if ( conn == nullptr )
		return 0U;

	std::vector<PlayerController*> out_controllers;
	GetControllersOnConnection( out_controllers, conn );
	for each ( PlayerController* controller in out_controllers )
	{
		PlayerAvatar* avatar = GetIndexedPlayerAvatar( controller->GetPlayerIndex() );
		if ( avatar != nullptr )
			out_players.push_back( avatar );
	}

	return out_players.size();
}


//--------------------------------------------------------------------------------------------------------------
size_t TheGame::GetAvatarsOnMyConnection( std::vector<PlayerAvatar*>& out_players )
{
	if ( m_gameSession == nullptr )
		return 0U;

	return GetAvatarsOnConnection( out_players, m_gameSession->GetMyConnection() );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ServerStartMatch()
{
	//[Post-lobby pre-match mode logic might be best to put here.]
	g_theConsole->RunCommand( "NetSessionHostStopListening" );

	NetMessage matchStartMsg( NETMSG_GAME_MATCH_STARTED );
	m_gameSession->SendToAllConnections( matchStartMsg );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ClientEndMatch( PlayerIndex winningPlayerIndex )
{
	m_winningPlayerIndex = winningPlayerIndex; 
	m_gameStateMachine.SetCurrentState( "TheGameState_PostMatch" );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ServerUpdateAuthoritativeSimulation( float deltaSeconds ) //Does not simulate non-game-affecting particles, or rag-dolls, etc.
{
	//Assume that all the client-owned objects local to server's simulation have been updated by their NetObject updates.

	PlayerAvatar* winner = CheckWinCondition();
	if ( winner != nullptr ) //Because we'd prefer to have a win registered before an enemy hitting us.
	{
		NetMessage matchEndMessage( NETMSG_GAME_MATCH_ENDED );
		matchEndMessage.Write<PlayerIndex>( winner->GetController()->GetPlayerIndex() );
		m_gameSession->SendToAllConnections( matchEndMessage );
	}
	UpdateWorld( deltaSeconds ); //No non-debug rendering nor audio!
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ClientUpdateUnauthoritativeSimulation( float deltaSeconds )
{
	for each ( TeardropNPC* enemy in m_teardropEnemies )
		enemy->Update( deltaSeconds ); //Animation.
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ConnectionHasPlayers()
{
	//See PlayerCreate for ideas on how to hook these two together.
	for ( PlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PLAYERS; playerIndex++ )
		if ( GetIndexedPlayerController( playerIndex ) != nullptr )
			return true;

	return false;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ClientUpdateOwnedPlayers( float deltaSeconds )
{
	std::vector<PlayerAvatar*> avatarsOnConnection;
	size_t numPlayers = GetAvatarsOnConnection( avatarsOnConnection, m_gameSession->GetMyConnection() ); 
	if ( numPlayers == 0 )
		return false; //May find none on first pass, exit cleanly if so.

	for ( size_t avatarIndex = 0; avatarIndex < numPlayers; avatarIndex++ )
	{
		PlayerAvatar* avatar = avatarsOnConnection[ avatarIndex ]; 

		//NEED TO LERP BY A PORTION OF THE DIRECTION * MY CHOSEN CAM SPEED OF THE DIFFERENCE BETWEEN CAMPOS AND PLAYERPOS.
		if ( avatarIndex == 0 ) //Using the Sonic/Tails kind of camera that only follows player 0.
		{
			const float FOLLOW_CAM_ARM_LENGTH_SQUARED = 4.f;
			Vector2f& camPos = s_playerCamera2D->m_worldPosition;
			const Vector2f& playerPos = avatar->GetPosition();
			Vector2f displacement = playerPos - camPos;

			if ( displacement.CalcFloatLengthSquared() > FOLLOW_CAM_ARM_LENGTH_SQUARED )
			{
				s_playerCamera2D->m_worldPosition = Lerp( camPos, playerPos, deltaSeconds ); //Keeps camera moving with player.
			}
		}

		avatar->Update( deltaSeconds ); //Clients own their own players, so e.g. movement update here.
	}

	EngineEventUpdate ev( deltaSeconds ); //To render FPS.
	TheEventSystem::Instance()->TriggerEvent( "TheGame::ClientRender2D", &ev );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
nuonce_t TheGame::GetNextCreationNuonce()
{
	++m_lastSentCreationNuonce; //Needs to be consistent between hosts and clients.
	return m_lastSentCreationNuonce;
}


//--------------------------------------------------------------------------------------------------------------
PlayerController* TheGame::PopUnconfirmedPlayerForNuonce( nuonce_t playerCreationNuonce )
{
	PlayerController* popped = nullptr;
	int index;
	for ( index = 0; index < (int)m_unconfirmedPlayers.size(); index++ )
	{
		PlayerController* controller = m_unconfirmedPlayers[ index ];
		if ( controller->GetCreationNuonce() == playerCreationNuonce )
		{
			popped = controller;
			break;
		}
	}

	if ( index == (int)m_unconfirmedPlayers.size() )
		return nullptr;

	m_unconfirmedPlayers.erase( m_unconfirmedPlayers.begin() + index );
	return popped;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ServerRegisterIndexForPlayer( PlayerController* newController )
{
	//PlayerIndex assign based on the first whole we find in existing players array.
	for ( PlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PLAYERS; playerIndex++ )
	{
		if ( GetIndexedPlayerController( playerIndex ) != nullptr ) //Slot already filled.
			continue;

		newController->SetPlayerIndex( playerIndex ); 
		m_playerControllers[ playerIndex ] = newController;
		return true;
	}
	return false;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ClientRegisterPlayerAsIndex( PlayerController* controller, PlayerIndex serverAssignedIndex )
{
	if ( GetIndexedPlayerController( serverAssignedIndex ) != nullptr ) //Slot already filled.
		return false;

	m_playerControllers[ serverAssignedIndex ] = controller;
	return true;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ClientCreatePlayerRequest( NetConnection* conn )
{
	//May still enter here if we are the host, i.e. in the case of a non-dedicated server, so not doing a conn != host check.

	//Pack into msg any info needed to recreate the PlayerController on other clients. 
	PlayerController* controller = new PlayerController( conn->GetIndex(), GetNextCreationNuonce() );
		//So we make one ourselves to send on, and check vs creationNuonce to not double-create it--but we still need a host-assigned index (etc).
	controller->SetUsername( conn->GetGuidString() ); //Add the %d with invocation # if we do multiple players to tell them apart.
	m_unconfirmedPlayers.push_back( controller );

	NetMessage creationRequestMsg( NETMSG_GAME_PLAYER_REQUEST_SFC );
	creationRequestMsg.Write<nuonce_t>( controller->GetCreationNuonce() );
	controller->WriteToMessage( creationRequestMsg );

	m_gameSession->SendMessageToHost( creationRequestMsg );
}


//--------------------------------------------------------------------------------------------------------------
PlayerAvatar* TheGame::ServerCreateAvatarForPlayer( PlayerController* controller )
{
	PlayerAvatar* avatar = new PlayerAvatar( controller ); //Host creates the object.
	m_playerAvatarNetObjects[ controller->GetPlayerIndex() ] = NetObjectSystem::NetSyncObject( avatar, NETOBJ_PLAYER_AVATAR, controller ); 
		//Clients create it on their side from creation message fired in NetSyncObject. TheGame tracks it for unsync and delete on leave.
	return avatar;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::ServerCreateAndSendPlayerAvatars()
{
	if ( !IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Only host should trigger player avatar creation!" );
		return;
	}

	for ( PlayerIndex playerIndex = 0; playerIndex < MAX_NUM_PLAYERS; playerIndex++ )
	{
		PlayerController* controller = GetIndexedPlayerController( playerIndex );
		if ( controller == nullptr )
			continue; //No player joined for that index.

					  //Note the below only assigned them on server side. NetObjectSystem's logic is what remakes them elsewhere.
		m_playerAvatars[ playerIndex ] = ServerCreateAvatarForPlayer( controller );
	}
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::OnNetworkTick_Handler( EngineEvent* eventContext )
{
	//Called for every connection we can see, to update them for what objects we own. For server-client, host sees all, but client only sees self and host.
	
	//Assume net session process incoming has occurred prior to this function call.

	EngineEventNetworked* ev = dynamic_cast<EngineEventNetworked*>( eventContext ); //Does this only get hit in certain states?
	NetConnection* conn = ev->connection;

	//In A3, we didn't want to tell our object where it was at. Do we now?
//	if ( conn->IsMe() )
//		return false; //Don't need to tell myself where my object's at.

	float deltaSeconds = ev->GetDeltaSeconds();

	if ( strcmp( m_gameStateMachine.GetCurrentState()->GetName(), "TheGameState_MidMatch" ) != 0 )
		return SHOULD_NOT_UNSUB;

	if ( m_gameSession->IsMyConnectionHosting() )
		ServerUpdateAuthoritativeSimulation( deltaSeconds );
	if ( ConnectionHasPlayers() ) //Not an else clause, can be both.
	{
		ClientUpdateUnauthoritativeSimulation( deltaSeconds );
		m_didClientRender = ClientUpdateOwnedPlayers( deltaSeconds ); //Only small client-owned or predictive-logic game events processed here.
	}

	NetObjectSystem::OnNetworkTick( conn );

	//Do not do updating code here, you'll flood the socket if you do this asap -- we send on a fixed tick.
	//This is the last chance you have to send info until next tick.

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::InitNetObjectSystem()
{
	NetObjectSystem::RegisterProtocolForEntity( NETOBJ_PLAYER_AVATAR, new Protocol_PlayerAvatar() );
	NetObjectSystem::RegisterProtocolForEntity( NETOBJ_NPC_TEARDROP, new Protocol_TeardropNPC() );
}
