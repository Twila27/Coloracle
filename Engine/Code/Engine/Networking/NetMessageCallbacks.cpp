#include "Engine/Networking/NetMessageCallbacks.hpp"
#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Networking/NetSender.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Engine/Core/TheConsole.hpp"


//--------------------------------------------------------------------------------------------------------------
void OnPingReceived( const NetSender& sender, NetMessage& msg )
{
	const char* str = msg.ReadString();

	const int MAX_ADDR_STRLEN = 256;
	char addrStrBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::SockAddrToString( &sender.sourceAddr, addrStrBuffer, MAX_ADDR_STRLEN );
	const char* optionalMessage = ( str != nullptr ) ? str : "null";
	LogAndShowPrintfWithTag( "NetSession", "Ping received from %s. [%s]", addrStrBuffer, optionalMessage );

	char ourAddrStrBuffer[ MAX_ADDR_STRLEN ];
	sender.ourSession->GetConnectionAddress( ourAddrStrBuffer, MAX_ADDR_STRLEN );
	if ( strcmp( ourAddrStrBuffer, addrStrBuffer ) == 0 )
	{
		LogAndShowPrintfWithTag( "NetSession", "Since ping came from self, not sending pong." );
		return;
	}
	else
	{
		NetMessage pong( NETMSG_PONG );
		LogAndShowPrintfWithTag( "InOrderTesting", "Sending pong." );
		sender.ourSession->SendMessageDirect( sender.sourceAddr, pong );
	}
}


//--------------------------------------------------------------------------------------------------------------
void OnPongReceived( const NetSender& sender, NetMessage& )
{
	const int MAX_ADDR_STRLEN = 256;
	char addrStrBuffer[ MAX_ADDR_STRLEN ];
	NetSystem::SockAddrToString( &sender.sourceAddr, addrStrBuffer, MAX_ADDR_STRLEN );
	LogAndShowPrintfWithTag( "NetSession", "Pong received from %s.", addrStrBuffer );
}


//--------------------------------------------------------------------------------------------------------------
static bool DoesVersionMatch() { return true; }
void OnJoinRequestReceived( const NetSender& from, NetMessage& msg )
{
	NetSession* ourSession = from.ourSession;
	if ( ourSession == nullptr )
	{
		ERROR_RECOVERABLE( "from.ourSession should be set in NetSession before calling msg.Process!" );
		return;
	}
	const char* joineeGuid = msg.ReadString();
	nuonce_t requestNuonce;
	bool success = msg.Read<nuonce_t>( &requestNuonce );
	if ( !success )
		return; //This really shouldn't ever happen.

	if ( !DoesVersionMatch() )
	{
		ourSession->SendDeny( requestNuonce, JOIN_DENY_INCOMPATIBLE_VERSION, from.sourceAddr );
		return;
	}

	NetConnection* me = ourSession->GetMyConnection();
	if ( !ourSession->IsHost( me ) ) //Only host allowed to process join requests.
	{
		ourSession->SendDeny( requestNuonce, JOIN_DENY_NOT_HOST, from.sourceAddr );
		return;
	}

	if ( !ourSession->IsListening() ) //Tends not to happen for a join-in-progress game.
	{
		ourSession->SendDeny( requestNuonce, JOIN_DENY_NOT_JOINABLE, from.sourceAddr ); //May have room, but not accept()ing.
		return;
		//Thought it was listening when you got the response, e.g. the game had 7/8 players, 
		//but by the time you hit "Join" it was 8/8 on their side.
	}

	//May want to stay listening until you start your game, people may come and go as you set up.
	if ( ourSession->IsFull() )
	{
		ourSession->SendDeny( requestNuonce, JOIN_DENY_GAME_FULL, from.sourceAddr );
		return;
	}

	if ( ourSession->HasGuid( joineeGuid ) )
	{
		ourSession->SendDeny( requestNuonce, JOIN_DENY_GUID_TAKEN, from.sourceAddr );
		return;
	}

	NetConnection* newConn = ourSession->AddConnection( joineeGuid, from.sourceAddr );
	if ( newConn == nullptr )
	{
		ERROR_RECOVERABLE( nullptr );
		return;
	}

	NetMessage accept( NETMSG_JOIN_ACCEPT );
	accept.WriteConnectionInfoMinusAddress( ourSession->GetHostInfo() );
	accept.WriteConnectionInfoMinusAddress( newConn->GetConnectionInfo() );
	newConn->SendMessageToThem( accept );
	ourSession->Connect( newConn, newConn->GetIndex() ); //Added on top of notes, did not see this in class...
	LogAndShowPrintfWithTag( "NetSession", "Connected to incoming client with username %s.", newConn->GetGuidString().c_str() );
}


//--------------------------------------------------------------------------------------------------------------
void OnJoinAcceptReceived( const NetSender& from, NetMessage& msg )
{
	NetSession* ourSession = from.ourSession;

	if ( ourSession == nullptr )
	{
		ERROR_RECOVERABLE( "from.ourSession should be set in NetSession before calling msg.Process!" );
		return;
	}

	if ( !ourSession->IsInJoiningState() )
		return;

	NetConnectionInfo hostInfo; //Prior to here we just had a dummy, only knew their address.
	NetConnectionInfo myAssignedInfo;
	msg.ReadConnectionInfoMinusAddress( &hostInfo );
	msg.ReadConnectionInfoMinusAddress( &myAssignedInfo );

	NetConnection* host = ourSession->GetHostConnection();
	host->UpdateConnectionInfo( hostInfo );

	NetConnection* me = ourSession->GetMyConnection();
	ourSession->Connect( me, myAssignedInfo.connectionIndex );

	ourSession->SetSessionStateConnected();
	LogAndShowPrintfWithTag( "NetSession", "Connected to host %s.", host->GetGuidString().c_str() );
}


//--------------------------------------------------------------------------------------------------------------
void OnJoinDenyReceived( const NetSender& from, NetMessage& msg )
{
	NetSession* ourSession = from.ourSession;

	if ( ourSession == nullptr )
	{
		ERROR_RECOVERABLE( "from.ourSession should be set in NetSession before calling msg.Process!" );
		return;
	}

	if ( !ourSession->IsInJoiningState() )
		return;

	ourSession->CleanupAfterJoinRequestDenied( msg );
}



//--------------------------------------------------------------------------------------------------------------
void OnLeaveReceived( const NetSender& from, NetMessage& )
{
	LogAndShowPrintfWithTag( "NetSession", "Received leave message from username %s.", from.sourceConnection->GetGuidString().c_str() );
	from.ourSession->Disconnect( from.sourceConnection );
}
