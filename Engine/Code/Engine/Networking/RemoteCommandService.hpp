#pragma once


#include <vector>
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Core/EngineEvent.hpp"


#define COMMAND_SERVICE_PORT 4325


class TCPListener;
class RemoteServiceConnection;


//-----------------------------------------------------------------------------
const byte_t MSG_TYPE_COMMAND = 1; //Executes received message as command.
const byte_t MSG_TYPE_ECHO = 2; //Just prints, doesn't execute.
const byte_t MSG_TYPE_RENAME = 3; //Suggested to allow renaming client, rather than just showing their address.
	//(Not too important until 2+ clients need to be distinguished.)


//-----------------------------------------------------------------------------
struct EngineEventOnServerJoin : public EngineEvent
{
	EngineEventOnServerJoin() : EngineEvent( "EngineEvent_OnServiceJoin" ) {}
	static const int MAX_ADDR_STRLEN = 256;
	char addrStr[ MAX_ADDR_STRLEN ];
};


//-----------------------------------------------------------------------------
class RemoteCommandService //One who runs the service => the server.
{
	//Could split up into RCHost and RCClient, but what we do either way will be the same so we'll put them together.
	//Will require some branching, but that's it.

public:

	static RemoteCommandService* /*CreateOrGet*/Instance();
	static void RegisterConsoleCommands(); //Called in NetSystem::Startup.
	static void Autoconnect(); //Attempts to auto-join and hosts if that fails.
	~RemoteCommandService();

	bool Host(); //Uses GetLocalHostName() for IP and Uses above macro for port. Creates m_listener, whose ctor binds() and opens listen() socket.
	bool Join( const char* hostName ); //Uses above macro for port. Creates a TCPConn* (2nd ctor) adding it to m_conn list (not using accept).
	void Disconnect();
	bool IsDisconnected() const;
	bool IsHosting() const { return m_listener != nullptr; } //Join() doesn't create one.

	void PrintServiceInstanceState();
	void PrintListenerAddressToConsole();
	void PrintConnectionsToConsole();

	//Application layer, common to host and client (why we're combining them).
	void SendDataToAllConnections( byte_t msgTypeID, const char* msgStr );
	void Update();


private:

	void CheckForConnection(); //Triggers onConnectionJoin below, after calling m_listener.accept().
	void CheckForMessages(); //Triggers onMessage below if found one (recv returned nonzero value).
	void CheckForDisconnects(); //Triggers onConnectionLeave if found one, after calling disconnect() and removing it from m_connections.
	void RegisterEventHandlers();

	bool OnConnectionMessage_Handler( EngineEvent* );
	bool OnServiceJoin_Handler( EngineEvent* );
	bool OnServiceLeave_Handler( EngineEvent* );

	RemoteCommandService();
	static RemoteCommandService* s_theService;

	std::vector<RemoteServiceConnection*> m_connections; //Will only have one of these if you're a client, more if you're a host.
	TCPListener* m_listener; //Will not have one of these if you're a client. 
		//Again, it's because we're blending host/client together.
};
