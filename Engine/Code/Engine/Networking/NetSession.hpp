#pragma once


#include "Engine/Networking/NetMessage.hpp"
#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Networking/NetConnection.hpp"
#include "Engine/Core/EngineEvent.hpp"
#include "Engine/Tools/StateMachine/StateMachine.hpp"
#include "Engine/Time/Stopwatch.hpp"


#define GAME_PORT					(4334) //Use port scanning if connecting to this fails.
#define PORT_SCAN_RANGE				(8) //Max # ports to increment up to looking for a free socket.
#define MAX_PROTOCOL_DEFNS			(256)
#define MAX_CONNECTIONS				(64) //This means concurrent connections.


//-----------------------------------------------------------------------------
class UDPSocket;
class PacketChannel;
class NetPacket;
class NetSession;
class Command;
// extern NetSession* g_theNetSession; //Needed it exposed for game-side NetSessionStart command.
// 	//Just one for now. Can actually have more with various msgtypes registered to them.
// 	//e.g. A group session for when a guild gathers for private chat system handling msgtypes unneeded when playing alone.


//-----------------------------------------------------------------------------
struct EngineEventNetworked : public EngineEvent //May have a sender or msg as needed by handlers below.
{
	EngineEventNetworked( NetConnection* conn ) : EngineEvent( "EngineEvent_Networked" ), msg( nullptr ), connection( conn ) {}
	EngineEventNetworked() : EngineEventNetworked( nullptr ) {}
	NetMessage* msg;
	NetConnection* connection;

	NetConnectionIndex GetConnectionIndex() const { return ( connection ? connection->GetIndex() : connectionIndex ); }
	void SetConnectionIndex( NetConnectionIndex idx ) { connection ? connection->SetIndex( idx ) : ( connectionIndex = idx ); }

	float GetDeltaSeconds() const { return secondsSinceLastTick; }
	void SetDeltaSeconds( float deltaSeconds ) { secondsSinceLastTick = deltaSeconds; } //Used by update logic.

private:
	NetConnectionIndex connectionIndex; //Only set/used if connection doesn't exist, e.g. after DestroyConnection.
	float secondsSinceLastTick;
};


//-----------------------------------------------------------------------------
enum JoinDenyReason : uint8_t
{
	JOIN_DENY_INCOMPATIBLE_VERSION,
	JOIN_DENY_NOT_HOST,
	JOIN_DENY_NOT_JOINABLE,
	JOIN_DENY_GAME_FULL,
	JOIN_DENY_GUID_TAKEN
};


//-----------------------------------------------------------------------------
class NetSession
{
public:
	
	NetSession( const char* sessionName );
	bool Start( uint16_t port, unsigned int numAllowedPlayers ); //Uses itself as the host, scans within above fixed range for open port.
	bool Host( const char* username );
		bool StartListening();
		bool StopListening();
	bool Join( const char* username, sockaddr_in& hostAddr );
	bool Leave();
	bool IsRunning();
	~NetSession();

	bool SessionUpdate( float deltaSeconds );
	void SessionRender();
	bool SessionShutdown_Handler( EngineEvent* );
	bool SessionClearConnections_Handler( EngineEvent* );
	bool Session_StartJoiningTimeoutStopwatch( EngineEvent* );
	bool Session_OnJoiningTimeoutStopwatchEnded( EngineEvent* );

	void RegisterMessage( uint8_t id, const char* debugName, NetMessageCallback* handler, uint32_t controlFlags, uint32_t optionFlags );
	void SendMessageDirect( const sockaddr_in& addr, NetMessage& msg );
	void SendMessagesDirect( const sockaddr_in& addr, NetMessage msgs[], int numMessages ); //Multiple messages sent in one or more packets based on MTUs.
	NetMessageDefinition* FindDefinitionForMessageType( uint8_t idIndex );
		//Note we don't take in a NetCoreMessageType or a GameMessageType, to support both enum via their base type.

	const char* GetName() const { return m_sessionName; }
	NetConnection* GetIndexedConnection( NetConnectionIndex index ) { return ( index >= MAX_CONNECTIONS ) ? nullptr : m_connections[ index ]; }
	NetConnection* FindConnectionByAddressAndPort( sockaddr_in addr ); //Port is inside sockaddr_in struct, name is just for clarity.
	NetConnection* GetMyConnection() const { return m_myConnection; }
	int8_t GetMyConnectionIndex() const { return ( m_myConnection ? m_myConnection->GetIndex() : INVALID_CONNECTION_INDEX ); }
	void GetAddressObject( sockaddr_in* out_addr ) const;
	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const;
	bool Update( float deltaSeconds ); //Unlike TCP, no checking for [dis]connections.
	void RenderDebug();

	NetConnection* CreateConnection( NetConnectionIndex connectionIndex, const char* guid, sockaddr_in addr );
	bool DestroyConnection( NetConnectionIndex connectionIndex );

	void SendPacket( const sockaddr_in& targetAddr, const NetPacket& packet );

	double GetSimulatedMaxAdditionalLag() const;
	double GetSimulatedMinAdditionalLag() const;
	float GetSimulatedMaxAdditionalLoss() const;
	float GetSimulatedMinAdditionalLoss() const;

	void SetSimulatedMaxAdditionalLag( int ms );
	void SetSimulatedMinAdditionalLag( int ms );
	void SetSimulatedMaxAdditionalLoss( float lossPercentile01 );
	void SetSimulatedMinAdditionalLoss( float lossPercentile01 );
	void SetSimulatedAdditionalLoss( float lossPercentile01 );
	bool ToggleTimeouts() { m_usesTimeouts = !m_usesTimeouts; return m_usesTimeouts; }

	bool SetNumAllowedConnections( int newVal ); //Can't exceed but can go lower than MAX_CONNECTIONS.
	bool SetSessionStateConnected() { return m_sessionStateMachine.SetCurrentState( "NetSessionState_Connected" ); }
	NetConnection* AddConnection( const std::string& joineeGuid, sockaddr_in joineeAddr );
	bool IsHost( NetConnection* connToCheck ) const;
	bool IsMyConnectionHosting() const { return IsHost( m_myConnection ); }
	bool IsInJoiningState() const { return m_sessionStateMachine.IsInState( "NetSessionState_Joining" ); }
	bool IsListening() const { return m_isListening; }
	bool IsFull() const { return GetNumConnected() > m_numAllowedConnections; }
	bool HasGuid( const std::string& guid ) const;
	int GetNumConnected() const;
	const NetConnectionInfo* GetHostInfo() const { return ( m_hostConnection ? m_hostConnection->GetConnectionInfo() : nullptr ); }
	void SendDeny( nuonce_t nuonceFromJoinRequest, JoinDenyReason reason, sockaddr_in toAddr );
	NetConnection* GetHostConnection() { return m_hostConnection; }
	void CleanupAfterJoinRequestDenied( NetMessage& denyMsg );
	nuonce_t GetNextRequestNuonce();

	void Connect( NetConnection* newConn, NetConnectionIndex newIndex ); //Calls game-side callbacks for new joins. Respects connect order.
	void Disconnect( NetConnection* conn ); //Calls game-side cleanup callbacks for leaving. Respects disconnect order.

	void SendMessageToHost( NetMessage& );
	void SendToAllConnections( NetMessage& );


private:	
	void FinalizeDisconnect( NetConnection* conn );
	size_t SendTo( const sockaddr_in& targetAddr, void const* data, const size_t dataSize );
	void SetIndexedConnection( NetConnectionIndex index, NetConnection* conn ) { if ( index < MAX_CONNECTIONS ) m_connections[ index ] = conn; }
	static bool CanProcessMessage( NetSender& from, NetMessage& msg );

	void ReceivePackets(); //Like CheckForMessages in A1's RemoteCommandService.hpp.
	bool TryProcessPacket( NetPacket &packet, size_t bytesRead, NetSender &from );

	bool IsHost( sockaddr_in addrToCheck ) const;

	StateMachine m_sessionStateMachine;
	PacketChannel* m_myPacketChannel; //Only one per session. Unlike TCP, in UDP everybody's megaphoning via their own socket.
		//When switching between PacketChannel and UDPSocket, don't forget to also change instantiation in NetSession::Start().
	
	NetMessageDefinition m_validMessages[ MAX_PROTOCOL_DEFNS ]; //These form the "protocol" for this session. It will ignore other messages.
		//Note message type IDs are used as indices into this array to prevent duplicates and optimize Find() above.

	bool m_usesTimeouts;
	NetConnection* m_hostConnection; //This way we don't assume host connIndex 0, as is false in the case of P2P game host migration.
	NetConnection* m_myConnection;
	NetConnection* m_connections[ MAX_CONNECTIONS ];

	float m_secondsBetweenTicks; //i.e. network update ticks.
	float m_secondsSinceLastUpdate;

	const char* m_sessionName;
	int m_numAllowedConnections;
	bool m_isListening;
	nuonce_t m_lastSentRequestNuonce;
	Stopwatch m_joiningStateTimeLimit;
};
