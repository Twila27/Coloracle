#pragma once
#include "Game/Game Entities/NetObjectProtocol.hpp"
#include "Engine/Memory/ObjectPool.hpp"
#include "Engine/Memory/UntrackedAllocator.hpp"
#include <map>


//-----------------------------------------------------------------------------
class NetConnection;
struct NetSender;
class NetMessage;
class PlayerController;
typedef std::pair< void*, NetObject* > LocalObjectRegistryPair;
typedef std::map< void*, NetObject* > LocalObjectRegistry;


//-----------------------------------------------------------------------------	
extern void OnNetObjectCreateReceivedFromServer( const NetSender&, NetMessage& createMsg );
extern void OnNetObjectUpdateReceivedFromServer( const NetSender&, NetMessage& updateMsg );
extern void OnNetObjectUpdateReceivedFromClient( const NetSender&, NetMessage& updateMsg );
extern void OnNetObjectDesyncDestroyReceivedFromServer( const NetSender&, NetMessage& destroyMsg );
extern void OnNetObjectDesyncReceivedFromServer( const NetSender&, NetMessage& destroyMsg );


//-----------------------------------------------------------------------------
#define MAX_NET_OBJECTS (1000) //Starting conservatively.


//-----------------------------------------------------------------------------
enum NetObjectEntityType
{
	NETOBJ_PLAYER_AVATAR,
	NETOBJ_NPC_TEARDROP,
	NUM_NETOBJ_TYPES
};


//-----------------------------------------------------------------------------
class NetObjectSystem
{
public:
	static NetObject* NetSyncObject( void* objectPointer, NetObjectEntityType, PlayerController* owningPlayer );
	static void ClientNetStopSync( NetObjectID ); //DOES NOT CALL DELETE, because delete on void* won't call dtor.
	static bool NetStopSyncForLocalObject( void* gameObjectPtr );
	static void OnNetworkTick( NetConnection* connToSendOurObjectsTo ); //Dispatches updates between host and clients.
	static NetObject* FindNetObjectByID( NetObjectID netObjectID );
	static NetObjectProtocol* FindNetObjectProtocolForEnumID( NetObjectEntityType netObjectTypeID );
	static void RegisterProtocolForEntity( NetObjectEntityType id, NetObjectProtocol* instance );
	static void ResetBaseNetObjectID();


private:
	NetObjectSystem();
	~NetObjectSystem();
	static NetObject* AllocateNetObject();

	static NetObjectSystem* /*CreateOrGet*/Instance(); //Trying an internal singleton to remove the Instance() call for other classes.
	static NetObjectSystem* s_theNetObjectSystem; //Can't do without one, or I'd lose an ideal time at which to init the object pool.

	bool RegisterNetObject( NetObject* );
	bool UnregisterNetObject( NetObject* );
	NetObject* m_netObjectRegistry[ MAX_NET_OBJECTS ];
	NetObjectProtocol* m_netObjectProtocolRegistry[ NUM_NETOBJ_TYPES ];
	
	static NetObjectID GetNextNetObjectID();
	static NetObjectID s_nextNetObjectID;
	
	void ServerSendEveryNetObjectToConnection( NetConnection* );
	void ClientSendEveryOwnedNetObjectToHost( NetConnection* );

	ObjectPool<NetObject> m_netObjectPool;
	LocalObjectRegistry s_localObjectToNetObject;
};
