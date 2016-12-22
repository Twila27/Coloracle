#pragma once

#include "Game/GameCommon.hpp"


//-----------------------------------------------------------------------------
class NetMessage;
class NetObjectProtocol;
typedef uint16_t NetObjectID;


//-----------------------------------------------------------------------------
struct NetObject //Represents our local view of a game object synced over the network.
{
	NetObjectID perObjectID; //Used to identify that specific net object instance on the receiving side.
	uint16_t lastSentUpdateNumber; //Only used by the client owner. Could mesh into lastReceived to save space if size is an issue (union to allow both names).
	uint16_t lastReceivedUpdateNumber; //Used by server/non-owner clients to ignore updates, e.g. got #3 but have #4, got 65535 but have 0 (via UnsignedComparators).

	PlayerIndex owningPlayerIndex;
	NetConnectionIndex owningConnectionIndex;

	NetObjectProtocol const* protocol;
	void* syncedObject; //The actual non-net game object this NetObject keeps in sync across network.
};


//-----------------------------------------------------------------------------
//NOTE THAT "FACTORY" IS USED LOOSELY HERE, because per NetObjectSystem::CreateNetObjectFrom, each factory doesn't instantiate a type, but recreates an entity netwide.
class NetObjectProtocol abstract //Pure virtual. Have to explicitly implement ALL of these for each data type you want in synced in the game.
{
public:
	virtual void WriteToCreationMessage( NetObject*, NetMessage& msg ) const = 0; //Called by game code when it calls NetObjectSystem::NetObjectSync.
	virtual void* OnCreate( NetMessage& msg ) const = 0; //Reads results of WriteToCreationMessage inside this.
		//Good start to a delta compression system, but we're doing something more bare and UDK-like.
	virtual void OnDestroy( NetObject* ) const = 0;
	virtual void WriteToDestroyMessage( NetMessage& msg ) const = 0;

	//Sent by hosts for ALL objects: authoritative state update.
	virtual void ServerWriteUpdateToMessage( NetObject*, NetMessage& msg ) const = 0;
	virtual void ClientReadAndProcessUpdateFromServer( NetObject*, NetMessage& msg ) const = 0;

	//Sent by clients for owned objects, those they want to influence. LEAVE AS STUB IF NOT A CLIENT-OWNED OBJECT!
	virtual void ClientWriteUpdateToMessage( NetObject*, NetMessage& msg ) const = 0;
	virtual void ServerReadAndProcessUpdateFromClient( NetObject*, NetMessage& msg ) const = 0;
};
