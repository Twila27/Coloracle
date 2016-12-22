#include "Game/Game Entities/NetObjectSystem.hpp"
#include "Game/Game Entities/NetObjectProtocol.hpp"
#include "Game/Game Entities/PlayerController.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Networking/NetSender.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Game/GameCommon.hpp"


//--------------------------------------------------------------------------------------------------------------
#define INITIAL_NEXT_OBJECT_ID (1)
STATIC NetObjectSystem* NetObjectSystem::s_theNetObjectSystem = nullptr;
STATIC NetObjectID NetObjectSystem::s_nextNetObjectID = INITIAL_NEXT_OBJECT_ID;


//--------------------------------------------------------------------------------------------------------------
NetObjectSystem::NetObjectSystem()
{
	m_netObjectPool.Init( MAX_NET_OBJECTS );
	memset( m_netObjectRegistry, 0, MAX_NET_OBJECTS * sizeof( NetObject* ) );
	memset( m_netObjectProtocolRegistry, 0, NUM_NETOBJ_TYPES * sizeof( NetObjectProtocol* ) );
}


//--------------------------------------------------------------------------------------------------------------
NetObjectSystem::~NetObjectSystem()
{
	for each ( NetObject*& netObj in m_netObjectRegistry )
	{
		if ( netObj != nullptr )
		{
			ClientNetStopSync( netObj->perObjectID );
			netObj = nullptr;
		}
	}

	for each ( NetObjectProtocol*& protocol in m_netObjectProtocolRegistry )
	{
		if ( protocol != nullptr ) 
		{
			delete protocol;
			protocol = nullptr;
		}
	}

}


//--------------------------------------------------------------------------------------------------------------
void OnNetObjectUpdateReceivedFromServer( const NetSender&, NetMessage& updateMsg )
{
	NetObjectID id;
	updateMsg.Read<NetObjectID>( &id );

	uint16_t updateNumber;
	updateMsg.Read<uint16_t>( &updateNumber );

	NetObject* netObject = NetObjectSystem::FindNetObjectByID( id );
	if ( netObject != nullptr ) 
	{
		if ( UnsignedGreaterThanOrEqual( updateNumber, netObject->lastReceivedUpdateNumber ) ) //cf. ServerSendEveryNetObjectToConnection's paragraph.
		{
			//Short version: the OrEqual case == non-authoritative host-prediction updates to keep from lagging behind an unresponsive client owner.

			netObject->lastReceivedUpdateNumber = updateNumber; //May be more than just lastReceived+1, if we're behind.
			netObject->protocol->ClientReadAndProcessUpdateFromServer( netObject, updateMsg );
		}
	}
	else
	{
//		ERROR_RECOVERABLE( "Failed to find NetObject for id!" );
		return;
	}
}


//--------------------------------------------------------------------------------------------------------------
void OnNetObjectUpdateReceivedFromClient( const NetSender& from, NetMessage& updateMsg )
{
	if ( !from.ourSession->IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Only host/server should receive an update from a client!" );
		return;
	}

	NetObjectID id;
	updateMsg.Read<NetObjectID>( &id );

	uint16_t updateNumber;
	updateMsg.Read<uint16_t>( &updateNumber );

	NetObject* netObject = NetObjectSystem::FindNetObjectByID( id );
	if ( netObject != nullptr ) 
	{
		if ( netObject->owningConnectionIndex == from.ourSession->GetMyConnectionIndex() )
			return; //Else a non-dedicated host will double-update its owned objects!

		if ( UnsignedGreaterThan( updateNumber, netObject->lastReceivedUpdateNumber ) ) //cf. ServerSendEveryNetObjectToConnection's paragraph.
		{
			netObject->lastReceivedUpdateNumber = updateNumber; //May be more than just lastReceived+1, if we're behind.
			netObject->protocol->ServerReadAndProcessUpdateFromClient( netObject, updateMsg );
		}
	}
	else
	{
		ERROR_RECOVERABLE( "Failed to find NetObject for id!" );
		return;
	}
}


//--------------------------------------------------------------------------------------------------------------
void OnNetObjectDesyncReceivedFromServer( const NetSender&, NetMessage& destroyMsg )
{
	if ( g_theGame->IsMyConnectionHosting() )
		return; //Server handles thmselves in ServerNetStopSyncForLocalObject.

	NetObjectID id;
	destroyMsg.Read<NetObjectID>( &id );
	NetObjectSystem::ClientNetStopSync( id );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetObjectSystem::ClientNetStopSync( NetObjectID netObjID )
{
	//This is the receiving side, server should use ServerNetStopSyncForLocalObject.
	NetObject* netObject = NetObjectSystem::FindNetObjectByID( netObjID );
	if ( netObject != nullptr )
	{
		netObject->protocol->OnDestroy( netObject );
		Instance()->UnregisterNetObject( netObject );
	}
	else
	{
		ERROR_RECOVERABLE( "Failed to find NetObject for id!" );
		return;
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC bool NetObjectSystem::NetStopSyncForLocalObject( void* gameObjectPtr )
{
	if ( gameObjectPtr == nullptr )
		return false;

	NetSession* sessionRef = g_theGame->GetGameNetSession();
	if ( sessionRef == nullptr )
	{
		ERROR_RECOVERABLE( "Called NetStopSyncForLocalObject before creating game session!" );
		return false;
	}


	//Have to clone the UnregisterNetObject code here to get netObj from gameObjectPtr.
	NetObjectSystem* sys = Instance();
	LocalObjectRegistry& registry = sys->s_localObjectToNetObject;
	LocalObjectRegistry::iterator found = registry.find( gameObjectPtr );
	if ( found == registry.end() )
		return false;

	NetObject* netObj = found->second;
	NetObjectID netObjID = netObj->perObjectID;

	if ( g_theGame->IsMyConnectionHosting() )
	{
		NetMessage destroyMsg( NETMSG_GAME_NETOBJ_DESYNC_SFS ); //We want to destroy it down below so its dtor gets hit.
		destroyMsg.Write<NetObjectID>( netObjID );
		netObj->protocol->WriteToDestroyMessage( destroyMsg );
		sessionRef->SendToAllConnections( destroyMsg );
	}

	netObj->protocol->OnDestroy( netObj );
	Instance()->UnregisterNetObject( netObj );

	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool NetObjectSystem::UnregisterNetObject( NetObject* netObj )
{
	NetObjectSystem* sys = Instance();
	LocalObjectRegistry& registry = sys->s_localObjectToNetObject;
	LocalObjectRegistry::iterator found = registry.find( netObj->syncedObject );
	if ( found == registry.end() )
		return false;

	registry.erase( found );

	NetObjectID netObjID = netObj->perObjectID;
	sys->m_netObjectRegistry[ netObjID ] = nullptr;
	sys->m_netObjectPool.Delete( netObj );

	return true;
}


//--------------------------------------------------------------------------------------------------------------
void OnNetObjectCreateReceivedFromServer( const NetSender&, NetMessage& msg )
{
	if ( !g_theGame->ConnectionHasPlayers() )
	{
		ERROR_RECOVERABLE( "OnNetObjectCreateRequestReceived must run on a connection that has a player!" );
		return;
	}

	NetObjectID netObjectID;
	bool successfulRead = msg.Read<NetObjectID>( &netObjectID );
	if ( !successfulRead )
		return;

	NetObjectEntityType	netObjectTypeID;
	successfulRead = msg.Read<NetObjectEntityType>( &netObjectTypeID );
	if ( !successfulRead )
		return;

	if ( NetObjectSystem::FindNetObjectByID( netObjectID ) != nullptr ) //Don't recreate existing, e.g. extra avatar for non-dedicated host.
		return;

	bool hasPlayer;
	successfulRead = msg.Read<bool>( &hasPlayer );
	PlayerController controllerData; //Because we at least need to write default invalid values even if we didn't get an owningPlayer.
	if ( hasPlayer )
		controllerData.ReadFromMessage( msg ); //Else it threw tantrums trying to strcpy out the username.
	
	NetObjectProtocol* protocol = NetObjectSystem::FindNetObjectProtocolForEnumID( netObjectTypeID );
	void* gameObject = protocol->OnCreate( msg ); //Results of WriteToCreationMessage accessed in here.
	NetObjectSystem::NetSyncObject( gameObject, netObjectTypeID, ( hasPlayer ? &controllerData : nullptr ) ); //Else, we have nothing to use to update gameObject by in UpdateReceivedFromServer!
}


//--------------------------------------------------------------------------------------------------------------
STATIC NetObjectSystem* NetObjectSystem::Instance()
{
	if ( s_theNetObjectSystem == nullptr )
		s_theNetObjectSystem = new STATIC NetObjectSystem();

	return s_theNetObjectSystem;
}


//--------------------------------------------------------------------------------------------------------------
STATIC NetObject* NetObjectSystem::AllocateNetObject()
{
	NetObject* newNetObject = NetObjectSystem::Instance()->m_netObjectPool.Allocate();

	return newNetObject;
}


//--------------------------------------------------------------------------------------------------------------
NetObjectID NetObjectSystem::GetNextNetObjectID()
{
	//Need to make sure it's not in use, we don't mind it overflowing and wrapping around.
	while ( FindNetObjectByID( s_nextNetObjectID ) != nullptr )
	{
		++s_nextNetObjectID; //Increment until we find an unused one.
		if ( s_nextNetObjectID == MAX_NET_OBJECTS )
			s_nextNetObjectID = 0;
	}

	return s_nextNetObjectID++; //We know it's probably used once we return it, so ++ it now to save on a Find in the next invocation.
}


//--------------------------------------------------------------------------------------------------------------
bool NetObjectSystem::RegisterNetObject( NetObject* netObject )
{
	s_localObjectToNetObject.insert( LocalObjectRegistryPair( netObject->syncedObject, netObject ) );

	NetObject*& currentOccupant = m_netObjectRegistry[ netObject->perObjectID ];
	bool wasVacant = ( currentOccupant == nullptr );
		
	if ( wasVacant ) //Not allowing overwriting for now.
		currentOccupant = netObject;
	else
		ERROR_AND_DIE( "Need to write in a way to remove NetObjects from the registry!" );

	return wasVacant;
}


//--------------------------------------------------------------------------------------------------------------
STATIC NetObject* NetObjectSystem::NetSyncObject( void* objectPointer, NetObjectEntityType netObjectTypeID, PlayerController* owningPlayer )
{
	//For now, called by the server only, but can be client-called in future in certain scenarios.
	NetSession* sessionRef = g_theGame->GetGameNetSession();
	if ( sessionRef == nullptr )
	{
		ERROR_RECOVERABLE( "Called NetSyncObject before creating game session!" );
		return nullptr;
	}

	//Make sure such a protocol exists.
	NetObjectProtocol const* protocol = FindNetObjectProtocolForEnumID( netObjectTypeID );
	if ( protocol == nullptr )
	{
		ERROR_RECOVERABLE( "No protocol found!" );
		return nullptr;
	}

	NetObject* newNetObj = AllocateNetObject();
	newNetObj->protocol = protocol;
	newNetObj->owningPlayerIndex = owningPlayer ? owningPlayer->GetPlayerIndex() : INVALID_PLAYER_INDEX;
	newNetObj->owningConnectionIndex = owningPlayer ? owningPlayer->GetOwningConnectionIndex() : INVALID_CONNECTION_INDEX;
	newNetObj->syncedObject = objectPointer;
	newNetObj->perObjectID = GetNextNetObjectID();
	newNetObj->lastSentUpdateNumber = 0;
	newNetObj->lastReceivedUpdateNumber = 0;

	Instance()->RegisterNetObject( newNetObj );

	if ( sessionRef->IsMyConnectionHosting() )
	{
		NetMessage creationMsg( NETMSG_GAME_NETOBJ_CREATE_SFS );
		creationMsg.Write<NetObjectID>( newNetObj->perObjectID );
		creationMsg.Write<NetObjectEntityType>( netObjectTypeID );

		bool hasPlayer = ( owningPlayer != nullptr );
		creationMsg.Write<bool>( hasPlayer );
		PlayerController controllerData; //Because we at least need to write default invalid values even if we didn't get an owningPlayer.
		if ( owningPlayer != nullptr )
		{
			controllerData.SetPlayerData( *owningPlayer );
			controllerData.WriteToMessage( creationMsg );
		}
		
		protocol->WriteToCreationMessage( newNetObj, creationMsg ); //Overridden for the particular needs of this object type.
		sessionRef->SendToAllConnections( creationMsg ); //Disseminate the new sync object netwide. These trigger OnCreate in the protocol.
	}
	return newNetObj;
}


//--------------------------------------------------------------------------------------------------------------
void NetObjectSystem::ServerSendEveryNetObjectToConnection( NetConnection* connToSendTo )
{
//	NetConnectionIndex connIndex = connToSendTo->GetIndex(); //Ignore objects owned by clients for now, else player movement jigs everywhere.
	for each ( NetObject* netObj in m_netObjectRegistry )
	{
		if ( netObj == nullptr )
			continue; 

//		if ( netObj->owningConnectionIndex == connIndex ) //PlayerAvatars now need to be updated by both client owner and server (authoritative sim stuff).
//			continue;

		NetMessage updateFromServerMsg( NETMSG_GAME_NETOBJ_UPDATE_SFS );
		updateFromServerMsg.Write<NetObjectID>( netObj->perObjectID );
		updateFromServerMsg.Write<uint16_t>( netObj->lastReceivedUpdateNumber ); 
			//Unlike ClientSendEveryOwnedNetObjectToHost which can just ++, 
			//we as server don't want to up the updateNumber here until we've received a new authoritative client-owner update.
			//i.e. We get Update1, and send our prediction of that out to all connections as Update1.
			//But until we get Update2 from the owner, we continue sending Update1 repeatedly, despite the different data (more host predictions).
			//This is why OnNetObjectUpdateReceivedFromServer uses UnsignedGreaterThanOrEqual, while FromClient just uses UnsignedGreaterThan.
			//Note that to this end we skip sending host updates to the client owner via netObj->owningConnectionIndex == connIndex check above.
		netObj->protocol->ServerWriteUpdateToMessage( netObj, updateFromServerMsg );
		connToSendTo->SendMessageToThem( updateFromServerMsg );
	}
}


//--------------------------------------------------------------------------------------------------------------
void NetObjectSystem::ClientSendEveryOwnedNetObjectToHost( NetConnection* connToSendTo )
{
	if ( !connToSendTo || !connToSendTo->GetSession() || !connToSendTo->GetSession()->GetHostConnection() )
		return;

	NetConnectionIndex myConnIndex = connToSendTo->GetSession()->GetMyConnectionIndex();
	for each ( NetObject* netObj in m_netObjectRegistry )
	{
		if ( ( netObj == nullptr ) || ( netObj->owningConnectionIndex != myConnIndex ) )
			continue; 

		NetMessage updateFromClientMsg( NETMSG_GAME_NETOBJ_UPDATE_SFC );
		updateFromClientMsg.Write<NetObjectID>( netObj->perObjectID );
		updateFromClientMsg.Write<uint16_t>( ++netObj->lastSentUpdateNumber );
		netObj->protocol->ClientWriteUpdateToMessage( netObj, updateFromClientMsg );
		connToSendTo->GetSession()->SendMessageToHost( updateFromClientMsg );
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetObjectSystem::OnNetworkTick( NetConnection* connToSendTo ) //Dispatches updates between host and clients.
{
	//IMPORTANT: Called for every connection we can see, to update them for what objects we own. 
		//For server-client, host sees all, but client only sees self and host.

	NetSession* sessionRef = g_theGame->GetGameNetSession();

	if ( ( sessionRef != nullptr ) && sessionRef->IsMyConnectionHosting() )
		Instance()->ServerSendEveryNetObjectToConnection( connToSendTo ); //If I'm the host, send updates for all objects to whoever tickedConnection is.

	Instance()->ClientSendEveryOwnedNetObjectToHost( connToSendTo );
	
	//OPTIMIZATION:
	//Every obj we make will send updates every single frame, which is unoptimal.
	//When we get to that point, we can start adding more checks to only update those that are the oldest.
}


//--------------------------------------------------------------------------------------------------------------
STATIC NetObject* NetObjectSystem::FindNetObjectByID( NetObjectID netObjectID )
{
	if ( netObjectID >= MAX_NET_OBJECTS )
	{
		ERROR_RECOVERABLE( "Walked out of array bounds!" );
		return nullptr;
	}
	return Instance()->m_netObjectRegistry[ netObjectID ];
}


//--------------------------------------------------------------------------------------------------------------
STATIC NetObjectProtocol* NetObjectSystem::FindNetObjectProtocolForEnumID( NetObjectEntityType netObjectTypeID )
{
	return Instance()->m_netObjectProtocolRegistry[ netObjectTypeID ];
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetObjectSystem::RegisterProtocolForEntity( NetObjectEntityType id, NetObjectProtocol* instance )
{
	Instance()->m_netObjectProtocolRegistry[ id ] = instance;
}


//--------------------------------------------------------------------------------------------------------------
void NetObjectSystem::ResetBaseNetObjectID()
{
	s_nextNetObjectID = INITIAL_NEXT_OBJECT_ID;
	//I suspect this causes the IDs sent between server/clients to get out of sync when not reset between games.
	//e.g. Host starts a game and leaves it, incrementing nextID to 5. Its next game starts player at 6,
	//But if a client joins it then, it'll sync the object from the initial value instead.
	//Alternatively, could just send the ID with the object create message... actually no, it checks to see if a prior object exists there.
	//So if the host had that earlier player of its own at 6, but another client had hosted, left, and joined our host that could conflict.
}
