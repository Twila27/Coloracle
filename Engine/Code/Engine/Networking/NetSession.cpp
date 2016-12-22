#include "Engine/Networking/NetSession.hpp"
#include "Engine/EngineCommon.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Engine/Core/Command.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Networking/NetMessageCallbacks.hpp"
#include "Engine/Networking/NetPacket.hpp"
#include "Engine/Networking/NetSender.hpp"
#include "Engine/Networking/PacketChannel.hpp"
#include "Engine/Core/EngineEvent.hpp"
#include "Engine/Tools/StateMachine/State.hpp"


//--------------------------------------------------------------------------------------------------------------
bool NetSession::SessionUpdate( float deltaSeconds )
{
	if ( IsRunning() )
	{
		return Update( deltaSeconds );
	}
	return false;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SessionRender()
{
	if ( IsRunning() )
		RenderDebug();
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::SessionShutdown_Handler( EngineEvent* )
{
	//Breakpoint here to see whether this wrecks everything.
	if ( IsRunning() )
	{
		if ( ( m_hostConnection != nullptr ) && IsMyConnectionHosting() )
			m_isListening = false; //StopListening() requires us to be in Connected state, but this should be Disconnected, and thus are setting it raw like this.

		TheEventSystem::Instance()->UnregisterSubscriber< NetSession, &NetSession::SessionClearConnections_Handler >( "NetSession_CleanupConnections", this );
		TheEventSystem::Instance()->UnregisterSubscriber< NetSession, &NetSession::Session_StartJoiningTimeoutStopwatch >( "NetSession_StartJoiningTimeoutStopwatch", this );
		TheEventSystem::Instance()->UnregisterSubscriber< NetSession, &NetSession::Session_OnJoiningTimeoutStopwatchEnded >( "NetSession_OnJoiningTimeoutStopwatchEnded", this );
		//TheEventSystem::Instance()->UnregisterEvent( "OnNetSessionShutdown" ); //Handled by unsubbing below.
	}

	m_sessionStateMachine.SetCurrentState( "NetSessionState_Invalid" );
	const bool SHOULD_UNSUB = true;
	return SHOULD_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::SessionClearConnections_Handler( EngineEvent* )
{
	for each ( NetConnection*& conn in m_connections )
	{
		delete conn;
		conn = nullptr; //Hence the by-ref, above.
	}

	//Only runs once by virtue of being an OnEnter command, but if we unsub it, future visits to this state won't come here.
	const bool SHOULD_UNSUB = false; 
	return SHOULD_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Session_StartJoiningTimeoutStopwatch( EngineEvent* )
{
	m_joiningStateTimeLimit.Start();

	const bool SHOULD_UNSUB = false;
	return SHOULD_UNSUB;
}
bool NetSession::Session_OnJoiningTimeoutStopwatchEnded( EngineEvent* )
{
	if ( IsInJoiningState() ) //Else proceeds as joined, without interference from this event.
	{
		LogAndShowPrintfWithTag( "NetSession", "Joining state timed out." );
		m_sessionStateMachine.SetCurrentState( "NetSessionState_Disconnected" );
	}

	const bool SHOULD_UNSUB = false;
	return SHOULD_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
NetSession::NetSession( const char* sessionName )
	: m_sessionName( sessionName )
	, m_usesTimeouts( true )
	, m_secondsSinceLastUpdate( 0.f )
	, m_secondsBetweenTicks( DEFAULT_TICK_RATE_SECONDS )
	, m_myConnection( nullptr )
	, m_hostConnection( nullptr )
	, m_isListening( false )
	, m_lastSentRequestNuonce( 0 )
	, m_numAllowedConnections( MAX_CONNECTIONS )
	, m_joiningStateTimeLimit( 15.f/*durationSeconds*/, "NetSession_OnJoiningTimeoutStopwatchEnded" )
{
	memset( m_validMessages, 0, MAX_PROTOCOL_DEFNS * sizeof( NetMessageDefinition ) );
	memset( m_connections, 0, MAX_CONNECTIONS * sizeof( NetConnection* ) );

	m_sessionStateMachine.CreateState( "NetSessionState_Invalid", true );

	State* disconnectedState = m_sessionStateMachine.CreateState( "NetSessionState_Disconnected" );
	TheEventSystem::Instance()->RegisterEvent< NetSession, &NetSession::SessionClearConnections_Handler >( "NetSession_CleanupConnections", this );
	disconnectedState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "NetSession_CleanupConnections" ) );

	State* joiningState = m_sessionStateMachine.CreateState( "NetSessionState_Joining" );
	TheEventSystem::Instance()->RegisterEvent< NetSession, &NetSession::Session_StartJoiningTimeoutStopwatch >( "NetSession_StartJoiningTimeoutStopwatch", this );
	TheEventSystem::Instance()->RegisterEvent< NetSession, &NetSession::Session_OnJoiningTimeoutStopwatchEnded >( "NetSession_OnJoiningTimeoutStopwatchEnded", this );
	joiningState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "NetSession_StartJoiningTimeoutStopwatch" ) );


	m_sessionStateMachine.CreateState( "NetSessionState_Connected" );

	RegisterMessage( NETMSG_PING, "ping", OnPingReceived, NETMSGCTRL_PROCESSED_CONNECTIONLESS, NETMSGOPT_NONE );
	RegisterMessage( NETMSG_PONG, "pong", OnPongReceived, NETMSGCTRL_PROCESSED_CONNECTIONLESS, NETMSGOPT_NONE );

	RegisterMessage( NETMSG_JOIN_REQUEST, "joinRequest", OnJoinRequestReceived, NETMSGCTRL_PROCESSED_CONNECTIONLESS, NETMSGOPT_RELIABLE );
	RegisterMessage( NETMSG_JOIN_ACCEPT, "joinAccept", OnJoinAcceptReceived, NETMSGCTRL_NONE, NETMSGOPT_RELIABLE );
	RegisterMessage( NETMSG_JOIN_DENY, "joinDeny", OnJoinDenyReceived, NETMSGCTRL_NONE, NETMSGOPT_NONE );
	RegisterMessage( NETMSG_LEAVE, "leave", OnLeaveReceived, NETMSGCTRL_NONE, NETMSGOPT_NONE );
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::HasGuid( const std::string& guid ) const
{
	for each ( NetConnection* conn in m_connections )
	{
		if ( conn == nullptr )
			continue;

		if ( conn->GetGuidString() == guid )
			return true;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------------
int NetSession::GetNumConnected() const
{
	int connCount = 0;
	for each ( NetConnection* conn in m_connections )
		if ( conn != nullptr )
			++connCount;

	return connCount;
}


//--------------------------------------------------------------------------------------------------------------
nuonce_t NetSession::GetNextRequestNuonce()
{
	m_lastSentRequestNuonce += GetRandomIntLessThan( RAND_MAX ); //Affords advancing non-predictably but deterministically.
	return m_lastSentRequestNuonce;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::Connect( NetConnection* newConn, NetConnectionIndex newIndex )
{
	/* Connection Callback Order
		1st. Host
		2nd. Me
		3rd. Others (if fully connected as in P2P netgames)
	*/

	//Doing an newConn->IsConnected() check here falls apart because CreateConnection has to call that, 
	//else NetSession::Join can't make a dummy host without using this function, which would trigger game logic before being joined in.

	newConn->SetIndex( newIndex );
	SetIndexedConnection( newIndex, newConn );

	//Here's where we'd loop all connections and tell them about the newConn if doing P2P fully-connected topology.

	//Only trigger callbacks if _my_ connection is connected:
	if ( m_myConnection->IsConnected() )
	{
		if ( newConn->IsMe() && ( newConn != m_hostConnection ) ) //Let host go first (BUT not twice, if I'm hosting).
		{
			EngineEventNetworked hostEv( m_hostConnection );
			TheEventSystem::Instance()->TriggerEvent( "OnConnectionJoined", &hostEv );
		}

		//Next, either connect myself or if !IsMe() the connection as normal:
		EngineEventNetworked newConnEv( newConn );
		TheEventSystem::Instance()->TriggerEvent( "OnConnectionJoined", &newConnEv );

		//If this was me, though, trigger (my view of) everyone else.
		if ( newConn->IsMe() )
		{
			for ( size_t i = 0; i < MAX_CONNECTIONS; i++ )
			{
				NetConnection* otherConn = m_connections[ i ];
				if ( ( otherConn != nullptr ) && ( otherConn != newConn ) && ( !IsHost( otherConn ) ) ) 
				{
					EngineEventNetworked otherConnEv( otherConn );
					TheEventSystem::Instance()->TriggerEvent( "OnConnectionJoined", &otherConnEv );
				}
			}
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::Disconnect( NetConnection* paramConn )
{
	/* Disconnect Callback Order
		1st. Others
		2nd. Me
		Last. Host (can cleanup for our view of the game)
	*/

	if ( ( paramConn == nullptr ) || !paramConn->IsConnected() )
		return;

	NetConnectionIndex paramConnIndex = paramConn->GetIndex();
	if ( paramConn != m_connections[ paramConnIndex ] )
	{
		ERROR_RECOVERABLE( "Imposter or outdated m_connections in NetSession::Disconnect!" );
		return;
	}

	if ( !m_myConnection->IsConnected() )
	{
		FinalizeDisconnect( paramConn );
		return;
	}

	bool paramIsMe = paramConn->IsMe();
	bool paramIsHost = IsHost( paramConn );

	//If we're connecting the host and they aren't me, disconnect me first.
	if ( paramIsHost && !paramIsMe )
	{
		//Host migration logic can go here if supported.
		Disconnect( m_myConnection );
		return;
	}

	if ( paramIsMe ) //Make sure (my view of) all others (!me && !host) get disconnected prior to me.
	{
		for ( size_t i = 0; i < MAX_CONNECTIONS; i++ )
		{
			NetConnection* otherConn = m_connections[ i ];
			if ( ( otherConn != nullptr ) && ( !otherConn->IsMe() ) && ( !IsHost( otherConn ) ) )
				Disconnect( otherConn );
		}
	}

	//Disconnect me/whoever this is that came in by recursion but isn't the host (that's handled last below).
	EngineEventNetworked paramConnEv( paramConn );
	TheEventSystem::Instance()->TriggerEvent( "OnConnectionLeave", &paramConnEv ); //Don't want to invalidate index until after this.
	SetIndexedConnection( paramConnIndex, nullptr );
	paramConn->SetIndex( INVALID_CONNECTION_INDEX );

	//Now disconnect myself since all others have disconnected, knowing I'm not the host, and then lastly disconnect the host after me.
	if ( paramIsMe && ( m_hostConnection != nullptr ) && !paramIsHost )
	{
		//If we're disconnecting myself:
		EngineEventNetworked hostEv( m_hostConnection );
		TheEventSystem::Instance()->TriggerEvent( "OnConnectionLeave", &hostEv );
		SetIndexedConnection( m_hostConnection->GetIndex(), nullptr ); //Else crashes trying to delete 0xddddddd where host was in SessionCleaupConnections.
		FinalizeDisconnect( m_hostConnection );
		if ( m_hostConnection != nullptr )
		{
			ERROR_RECOVERABLE( "Host should be nullptr after FinalizeDisconnect runs on it!" );
			return;
		}
	}

	FinalizeDisconnect( paramConn );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendMessageToHost( NetMessage& msg )
{
	m_hostConnection->SendMessageToThem( msg );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendToAllConnections( NetMessage& msg )
{
	for ( NetConnectionIndex connIndex = 0; connIndex < m_numAllowedConnections; connIndex++ )
	{
		NetConnection* currentConn = GetIndexedConnection( connIndex );
		if ( currentConn != nullptr )
			currentConn->SendMessageToThem( msg ); //Note this includes our own connection, even if hosting!
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::FinalizeDisconnect( NetConnection* conn )
{
	if ( ( m_hostConnection != nullptr ) && IsHost( conn ) )
		m_hostConnection = nullptr;

	conn->FlushUnreliables();

	if ( !conn->IsMe() )
		delete conn; //If conn is me, then we don't delete it--that happens ONLY when moved to d/c state (it's the CleanupConnections event, fired onEnter, see NetSession ctor). 
	else
		m_sessionStateMachine.SetCurrentState( "NetSessionState_Disconnected" );
	//(i.e. I may have a connection that never actually connected, in which case Disconnect() wouldn't be called on it, so that's why the delete has to be there.)
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendDeny( nuonce_t nuonceFromJoinRequest, JoinDenyReason reasonType, sockaddr_in toAddr )
{
	NetMessage deny( NETMSG_JOIN_DENY );
	deny.Write<nuonce_t>( nuonceFromJoinRequest ); //The reason the nuonce is popular: 
		//usually you don't know how the underlying reliability is handled, so you can't assume the rID would work here. 
		//Lets us treat reliable stuff as a black box system preventing us from assuming a unique id per reliable message.
	deny.Write<uint8_t>( reasonType );
	SendMessageDirect( toAddr, deny );
}


//--------------------------------------------------------------------------------------------------------------
static const char* GetStringForDenyReason( uint8_t enumVal )
{
	switch ( (JoinDenyReason)enumVal )
	{
	case JOIN_DENY_INCOMPATIBLE_VERSION:
		return "Incompatible session versions!";
	case JOIN_DENY_NOT_HOST:
		return "Input address was not a host!";
	case JOIN_DENY_NOT_JOINABLE:
		return "Host was not listening for joins!";
	case JOIN_DENY_GAME_FULL:
		return "Host session Was full!";
	case JOIN_DENY_GUID_TAKEN:
		return "Username had already joined host!";
	default:
		return "Unsupported deny reason!";
	}
}
void NetSession::CleanupAfterJoinRequestDenied( NetMessage& denyMsg )
{
	nuonce_t msgNuonce;
	bool success = denyMsg.Read<nuonce_t>( &msgNuonce );
	if ( !success )
		return;

	bool appliesToUs = ( msgNuonce == m_lastSentRequestNuonce ); //If its nuonce doesn't match up, this request isn't for us.
	if ( !appliesToUs )
		return;

	uint8_t reason;
	denyMsg.Read<uint8_t>( &reason );
	LogAndShowPrintfWithTag( "NetSession", "Received JoinDeny with reason: %s", GetStringForDenyReason( reason ) );

	//:'(
	DestroyConnection( 0 ); //Get rid of the dummy host we made. Sets m_conns nullptr.
	delete m_myConnection; //We still have invalid index, so can't/don't need to call above.

	m_sessionStateMachine.SetCurrentState( "NetSessionState_Disconnected" );
}


//--------------------------------------------------------------------------------------------------------------
NetSession::~NetSession()
{
	//Be sure that the shutdown event is triggered or else the below delete will be deferenced by session's update's IsRunning()!
	m_myPacketChannel->Unbind();
	delete m_myPacketChannel;
	m_myPacketChannel = nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendMessageDirect( const sockaddr_in& addr, NetMessage& msg )
{
	if ( !IsRunning() )
		return; //i.e. UDPSocket did not exist or was closed/unbound.

	if ( msg.IsReliable() )
	{
		Logger::PrintfWithTag( "NetSession", "SendMessageDirect can only send unreliables." );
		g_theConsole->Printf( "SendMessageDirect can only send unreliables." );
		return;
	}

	TODO( "Add checks to FAIL if an incoming message is either in-order, reliable, or requires a connection. This does not work for those." );

	NetPacket currentPacket;
	PacketHeader ph;
	ph.connectionIndex = GetMyConnectionIndex();
	bool writeSuccess = currentPacket.WritePacketHeader( ph );
	if ( !writeSuccess )
		return;

	const int NUM_MESSAGES_WRITTEN = 1;
	currentPacket.Write<uint8_t>( NUM_MESSAGES_WRITTEN );

	bool successfulWrite = currentPacket.WriteMessageToBuffer( msg, this );

	//Sends IMMEDIATELY, hence "Direct" in method name. Other sends need not necessarily do so.
	if ( successfulWrite )
	{
		m_myPacketChannel->SendTo( addr, currentPacket.GetPayloadBuffer(), currentPacket.GetTotalReadableBytes() );
	}
	else LogAndShowPrintfWithTag( "NetSession", "WARNING: NetPacket::WriteMessageToBuffer failed in SendMessageDirect!" );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendMessagesDirect( const sockaddr_in& addr, NetMessage msgs[], int numMessages )
{
	if ( !IsRunning() )
		return; //i.e. UDPSocket did not exist or was closed/unbound.

	if ( numMessages < 1 )
		return;

	if ( numMessages == 1 )
	{
		SendMessageDirect( addr, msgs[ 0 ] );
		return;
	}

	TODO( "Add checks to FAIL if an incoming message is either in-order, reliable, or requires a connection. This does not work for those." );

	NetPacket currentPacket;
	PacketHeader ph;
	ph.connectionIndex = GetMyConnectionIndex();
	bool writeSuccess = currentPacket.WritePacketHeader( ph );
	if ( !writeSuccess )
		return;

	ByteBufferBookmark numMsgsOffset = currentPacket.ReserveForWriting<uint8_t>( 0U ); //Not able to write the #msgs until below occurs, so skip over it for now.

	for ( int msgIndex = 0; msgIndex < numMessages; msgIndex++ )
	{
		NetMessage& msg = msgs[ msgIndex ];
		if ( msg.IsReliable() )
		{
			Logger::PrintfWithTag( "NetSession", "SendMessageDirect can only send unreliables." );
			g_theConsole->Printf( "SendMessageDirect can only send unreliables." );
			return;
		}

		bool successfulWrite = currentPacket.WriteMessageToBuffer( msg, this );
		if ( !successfulWrite ) //sendto and start anew.
		{
			currentPacket.WriteAtBookmark( numMsgsOffset, currentPacket.GetTotalAddedMessages() );
			m_myPacketChannel->SendTo( addr, currentPacket.GetPayloadBuffer(), currentPacket.GetTotalReadableBytes() ); //Dispatch filled packet.
			currentPacket = NetPacket(); //Start new packet.
		}
	}

	currentPacket.WriteAtBookmark( numMsgsOffset, currentPacket.GetTotalAddedMessages() );
	m_myPacketChannel->SendTo( addr, currentPacket.GetPayloadBuffer(), currentPacket.GetTotalReadableBytes() ); //Get that last packet sent.
}


//--------------------------------------------------------------------------------------------------------------
NetMessageDefinition* NetSession::FindDefinitionForMessageType( uint8_t idIndex )
{
	NetMessageDefinition* result = &m_validMessages[ idIndex ];
	if ( result->handler == nullptr )
		return nullptr;

	return result;
}


//--------------------------------------------------------------------------------------------------------------
NetConnection* NetSession::FindConnectionByAddressAndPort( sockaddr_in addr )
{
	for each ( NetConnection* conn in m_connections )
	{
		if ( conn == nullptr )
			continue;
		
		sockaddr_in connAddr = conn->GetAddressObject();
		if ( connAddr.sin_addr.S_un.S_addr == addr.sin_addr.S_un.S_addr )
		{
			if ( connAddr.sin_port == addr.sin_port )
				return conn;
		}
	}

	return nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::GetAddressObject( sockaddr_in* out_addr ) const
{
	m_myPacketChannel->GetAddressObject( out_addr );
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Start( uint16_t portNum, unsigned int numAllowedConnections )
{
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Invalid" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::Start!" );
		return false;
	}

	//Scanning around GAME_PORT.
	const int MAX_ADDR_STRLEN = 256;
	char nameBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::GetLocalHostName( nameBuffer, MAX_ADDR_STRLEN ); //This is -our- socket!

	uint16_t portOffset = 0;
	m_myPacketChannel = new PacketChannel();
	bool success = false;
	do
	{
		success = m_myPacketChannel->Bind( nameBuffer, portNum + portOffset );
		++portOffset;
	} 
	while ( !success && portOffset <= PORT_SCAN_RANGE ); //Try to succeed until we exceed port scan range.

	if ( success ) //Register callbacks.
	{
		SetNumAllowedConnections( numAllowedConnections );

		//We may not mind updating/rendering alongside other sessions registered to these events, but shutdowns may need to occur separately.
		if ( TheEventSystem::Instance()->IsEventRegistered( "OnNetSessionShutdown" ) )
		{
			LogAndShowPrintfWithTag( "NetSession", "Please address separating session shutdown event subscribers before using multiple sessions!" );
			return false;
		}
		TheEventSystem::Instance()->RegisterEvent<NetSession, &NetSession::SessionShutdown_Handler >( "OnNetSessionShutdown", this );

		m_sessionStateMachine.SetCurrentState( "NetSessionState_Disconnected" );
	}

	return success;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Host( const char* username )
{
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Disconnected" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::Host!" );
		return false;
	}

	//Actually make host conn for ourselves, assuming it will succeed, switch state to Connected.
	sockaddr_in to;
	m_myPacketChannel->GetAddressObject( &to );
	m_hostConnection = CreateConnection( 0, username, to ); //State switched inside this.
	if ( m_hostConnection != nullptr )
	{
		Connect( m_hostConnection, 0 );
		LogAndShowPrintfWithTag( "NetSession", "NetSession created host connection at its own socket." );
		m_sessionStateMachine.SetCurrentState( "NetSessionState_Connected" );
	}
	else
	{
		LogAndShowPrintfWithTag( "NetSession", "NetSession failed to create host connection." );
	}
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::StartListening()
{
	if ( !IsMyConnectionHosting() )
	{
		LogAndShowPrintfWithTag( "NetSession", "Cannot call StartListening on non-host!" );
		return false;
	}
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Connected" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::StartListening!" );
		return false;
	}
	m_isListening = true;
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::StopListening()
{
	if ( !IsMyConnectionHosting() )
	{
		LogAndShowPrintfWithTag( "NetSession", "Cannot call StopListening on non-host!" );
		return false;
	}
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Connected" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::StopListening!" );
		return false;
	}
	m_isListening = false;
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Join( const char* username, sockaddr_in& hostAddr )
{
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Disconnected" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::Join!" );
		return false;
	}

	//Create an empty host conn, assuming index 0.
	m_hostConnection = CreateConnection( 0, "", hostAddr );

	//Create a conn for me as the joiner.
	sockaddr_in to;
	m_myPacketChannel->GetAddressObject( &to );
	m_myConnection = CreateConnection( INVALID_CONNECTION_INDEX, username, to );

	//Send join request message to host.
	NetMessage joinRequest( NETMSG_JOIN_REQUEST );
	joinRequest.WriteString( username );
	joinRequest.Write<nuonce_t>( GetNextRequestNuonce() );
	m_hostConnection->SendMessageToThem( joinRequest );

	m_sessionStateMachine.SetCurrentState( "NetSessionState_Joining" ); //Does not happen instantly for joining.
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Leave()
{
	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Connected" ) )
	{
		LogAndShowPrintfWithTag( "NetSession", "Wrong state in NetSession::Leave!" );
		return false;
	}

	if ( IsMyConnectionHosting() )
		StopListening();

	NetMessage leaveMsg( NETMSG_LEAVE );
	leaveMsg.WriteConnectionInfoMinusAddress( m_myConnection->GetConnectionInfo() );

	//The most general way to write the below: 
		// If I'm the host - needs to tell everyone who !isMe.
		// If I'm in P2P - needs to tell everyone who !isMe.
		// If I'm the client - needs to tell host, but may as well write more general case.
	for each ( NetConnection* conn in m_connections )
	{
		if ( conn == nullptr )
			continue;

		conn->SendMessageToThem( leaveMsg );
		if ( !conn->IsMe() )
			conn->FlushUnreliables();
	}

	Disconnect( m_myConnection ); //Updates state machine to Disconnected inside here.
	return true;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const
{
	m_myPacketChannel->GetConnectionAddress( out_addrStrBuffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::Update( float deltaSeconds )
{
	bool didTickNetwork = false;

	//Unlike TCP, no checking for [dis]connections.
	ReceivePackets();

	if ( m_myConnection == nullptr )
		return DID_NOT_REACH_END;

	if ( !m_joiningStateTimeLimit.IsPaused() )
		m_joiningStateTimeLimit.Update( deltaSeconds );

	m_secondsSinceLastUpdate += deltaSeconds;
	if ( m_secondsSinceLastUpdate > m_secondsBetweenTicks )
	{
		for each ( NetConnection* cp in m_connections )
		{
			if ( cp == nullptr )
				continue;

			const double SECONDS_UNTIL_TIMEOUT = 15.0;
			const double SECONDS_UNTIL_BAD = 5.0;
			if ( cp->IsConnectionConfirmed() && !cp->IsMe() ) //Don't timeout yourself!
			{
				double timeLastSeen = cp->GetSecondsSinceLastRecv();
				cp->AddSecondsSinceLastRecv( deltaSeconds ); //Will be overwritten in ConfirmAndWake if we recv from cp.
				if ( m_usesTimeouts && ( timeLastSeen > SECONDS_UNTIL_TIMEOUT ) )
				{
					LogAndShowPrintfWithTag( "NetSession", "Disconnecting %s at connection #%u.", cp->GetGuidString().c_str(), cp->GetIndex() );
					Disconnect( cp );
					continue;
				}


				if ( timeLastSeen > SECONDS_UNTIL_BAD )
					cp->SetAsBadConnection(); //Only sent heartbeats until it responds.
			}

			EngineEventNetworked ev( cp );
			ev.SetDeltaSeconds( deltaSeconds );
			TheEventSystem::Instance()->TriggerEvent( "OnNetworkTick", &ev ); //Make sure game-side is subbed to this!
			didTickNetwork = true;

			cp->ConstructAndSendPacket(); //Could add a whole bunch over multiple frames. Our first step toward packet consolidation.
		}
	
		m_secondsSinceLastUpdate = 0.f; //May want a non-global tick rate to send packets more slowly to congested peers.
	}

	return didTickNetwork;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::SetNumAllowedConnections( int newVal )
{
	if ( ( newVal > 0 ) && ( newVal < MAX_CONNECTIONS ) )
	{
		m_numAllowedConnections = newVal;
		return true;
	}
	return false;
}


//--------------------------------------------------------------------------------------------------------------
NetConnection* NetSession::AddConnection( const std::string& joineeGuid, sockaddr_in joineeAddr )
{
	for ( NetConnectionIndex connIndex = 0; connIndex < m_numAllowedConnections; connIndex++ )
	{
		if ( GetIndexedConnection( connIndex ) != nullptr ) //Slot already filled.
			continue;

		NetConnection*& newConn = m_connections[ connIndex ];
		newConn = new NetConnection( this, connIndex, joineeGuid.c_str(), joineeAddr );
		return newConn;
	}

	return nullptr;
}


//--------------------------------------------------------------------------------------------------------------
NetConnection* NetSession::CreateConnection( NetConnectionIndex connectionIndex, const char* guid, sockaddr_in addr )
{
	//All the cartwheels around invalid index allow a joining connection to make themselves a temp until host assigns their index.
	ASSERT_OR_DIE( ( connectionIndex < MAX_CONNECTIONS ) || ( connectionIndex == INVALID_CONNECTION_INDEX ), nullptr );

	if ( connectionIndex != INVALID_CONNECTION_INDEX )
	{
		if ( GetIndexedConnection( connectionIndex ) != nullptr ) //Slot already filled.
			return nullptr;
	}

	NetConnection* newConn = new NetConnection( this, connectionIndex, guid, addr );

	if ( connectionIndex != INVALID_CONNECTION_INDEX )
		SetIndexedConnection( connectionIndex, newConn );

	if ( newConn->IsMe() )
		m_myConnection = newConn;

	return newConn;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::DestroyConnection( NetConnectionIndex connectionIndex )
{
	ASSERT_OR_DIE( connectionIndex < MAX_CONNECTIONS, nullptr );

	NetConnection* conn = GetIndexedConnection( connectionIndex );
	if ( conn  == nullptr )
		return false; //Did not exist.

	delete conn;
	SetIndexedConnection( connectionIndex, nullptr );

	return true;
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SendPacket( const sockaddr_in& targetAddr, const NetPacket& packet )
{
	this->SendTo( targetAddr, packet.GetPayloadBuffer(), packet.GetTotalReadableBytes() );
}


//--------------------------------------------------------------------------------------------------------------
size_t NetSession::SendTo( sockaddr_in const& targetAddr, void const* data, const size_t dataSize )
{
	return m_myPacketChannel->SendTo( targetAddr, data, dataSize );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SetSimulatedMinAdditionalLag( int ms )
{
	m_myPacketChannel->SetSimulatedMinAdditionalLag( ms );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SetSimulatedMaxAdditionalLag( int ms )
{
	m_myPacketChannel->SetSimulatedMaxAdditionalLag( ms );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SetSimulatedMaxAdditionalLoss( float lossPercentile01 )
{
	m_myPacketChannel->SetSimulatedMaxAdditionalLoss( lossPercentile01 );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SetSimulatedMinAdditionalLoss( float lossPercentile01 )
{
	m_myPacketChannel->SetSimulatedMinAdditionalLoss( lossPercentile01 );
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::SetSimulatedAdditionalLoss( float lossPercentile01 )
{
	m_myPacketChannel->SetSimulatedAdditionalLoss( lossPercentile01 );
}


//--------------------------------------------------------------------------------------------------------------
double NetSession::GetSimulatedMinAdditionalLag() const
{
	return m_myPacketChannel->GetSimulatedMinAdditionalLag();
}


//--------------------------------------------------------------------------------------------------------------
double NetSession::GetSimulatedMaxAdditionalLag() const
{
	return m_myPacketChannel->GetSimulatedMaxAdditionalLag();
}


//--------------------------------------------------------------------------------------------------------------
float NetSession::GetSimulatedMaxAdditionalLoss() const
{
	return m_myPacketChannel->GetSimulatedMaxAdditionalLoss();
}


//--------------------------------------------------------------------------------------------------------------
float NetSession::GetSimulatedMinAdditionalLoss() const
{
	return m_myPacketChannel->GetSimulatedMinAdditionalLoss();
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::ReceivePackets()
{
	NetPacket packet;
	NetSender from;
	from.ourSession = this;

	size_t bytesRead = m_myPacketChannel->RecvFrom( &from.sourceAddr, packet.GetPayloadBuffer(), MAX_PACKET_SIZE ); //Get packet from socket.

	while ( bytesRead > 0 ) //bytesRead == packetLength, implying we can apply some validation with it.
	{
		bool success = TryProcessPacket( packet, bytesRead, from );
		if ( !success )
			break;

		bytesRead = m_myPacketChannel->RecvFrom( &from.sourceAddr, packet.GetPayloadBuffer(), MAX_PACKET_SIZE ); //Get next packet.
	}
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::TryProcessPacket( NetPacket &packet, size_t bytesRead, NetSender &from ) //ProcessIncomingPacket in A4 notes.
{
	bool readSuccess;
	packet.SetTotalReadableBytes( bytesRead ); //How far we can legally read.

	PacketHeader ph;
	readSuccess = packet.ReadPacketHeader( ph );
	if ( !readSuccess )
		return false;

	from.sourceConnection = this->GetIndexedConnection( ph.connectionIndex ); //Can return nullptr.
	
	readSuccess = packet.ReadNumMessages(); //For next line's call.
	if ( !readSuccess )
		return false;

	bool lengthMatched = packet.ValidateLength( ph, bytesRead );
	if ( !lengthMatched )
		return false; //Don't want to mark it as received, since even the ID might be corrupt.

	NetMessage msg;
	uint8_t msgCount = packet.GetTotalAddedMessages();
//	if ( msgCount >= 8 )
//		LogAndShowPrintfWithTag( "InOrderTesting", "Received packet with 8+ messages." );
	for ( uint8_t currentMessageNum = 0; currentMessageNum < msgCount; currentMessageNum++ ) //Iterate over messages in packet.
	{
		bool successfulRead = packet.ReadMessageFromPacketBuffer( msg, this );
		if ( !successfulRead )
			break;

		if ( NetSession::CanProcessMessage( from, msg ) ) //Reliable traffic safeguards on this NetSession, e.g. vs double-processing.
		{
			if ( from.sourceConnection != nullptr )
			{
				from.sourceConnection->ProcessMessage( from, msg ); //Won't get hit for join request <= won't have a connection yet.
			}
			else
			{
				msg.Process( from ); //Connectionless. Includes join requests.

				//The one tricky part of the assignment, ELSE YOU DOUBLE-PROCESS JOINS:
				if ( msg.IsReliable() )
				{
					from.sourceConnection = FindConnectionByAddressAndPort( from.sourceAddr ); //Try to find the connection for it now. 
								//i.e. since any reliable message may be a join.
								//Do a comparison of the m_connections for this session's addresses--
								//--will now be valid following msg.Process above if it was a join 
								//(but in that case we don't really know which index it received, at this point in code).
				}
			}

			if ( from.sourceConnection != nullptr )
			{
				//Since this may have been created since last check via join requests.
				from.sourceConnection->MarkMessageReceived( msg );
			}
		}
	}

	if ( from.sourceConnection != nullptr )
	{
		from.sourceConnection->MarkPacketReceived( ph );
		from.sourceConnection->ConfirmAndWakeConnection(); //For timeout logic.
	}
	//Note we still mark it as received even if we can't process.

	return true;
}


//--------------------------------------------------------------------------------------------------------------
STATIC bool NetSession::CanProcessMessage( NetSender& from, NetMessage& msg )
{
	if ( from.sourceConnection == nullptr )
	{
		return !msg.NeedsConnection();
	}
	else
	{
		return from.sourceConnection->CanProcessMessage( msg );
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetSession::RegisterMessage( uint8_t id, const char* debugName, NetMessageCallback* handler, uint32_t controlFlags, uint32_t optionFlags )
{
	ASSERT_OR_DIE( m_sessionStateMachine.GetCurrentState() != nullptr, "Calling RegisterMessage before setting NetSession states!" );

	if ( !m_sessionStateMachine.IsInState( "NetSessionState_Invalid" ) )
	{
		Logger::PrintfWithTag( "NetSession", "WARNING: NetSession already started, cannot register more messages to it!" );
		g_theConsole->Printf( "NetSession already started, cannot register more messages to it!" );
		return;
	}

	NetMessageDefinition& ref = m_validMessages[ id ];
	ref.index = id;
	ref.debugName = debugName;
	ref.handler = handler; //Check to see if this is nullptr by the ctor? Then we can do isValid checks off that!
	ref.controlFlags = controlFlags;
	ref.optionFlags = optionFlags;
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::IsRunning()
{
	return m_myPacketChannel->IsBound();
}


//--------------------------------------------------------------------------------------------------------------
bool NetSession::IsHost( NetConnection* connToCheck ) const
{
	if ( connToCheck == nullptr )
		return false;

	return IsHost( connToCheck->GetAddressObject() );
}



//--------------------------------------------------------------------------------------------------------------
bool NetSession::IsHost( sockaddr_in addrToCheck ) const
{
	if ( m_hostConnection == nullptr )
	{
		ERROR_RECOVERABLE( "HostConnection doesn't exist on this session to be checked!" );
		return false;
	}

	sockaddr_in hostAddr = m_hostConnection->GetAddressObject();
	return NetSystem::DoSockAddrMatch( hostAddr, addrToCheck );
}
