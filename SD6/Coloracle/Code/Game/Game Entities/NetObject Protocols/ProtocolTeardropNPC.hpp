#pragma once


#include "Game/Game Entities/NetObjectProtocol.hpp"


class Protocol_TeardropNPC : public NetObjectProtocol
{
public:
	virtual void WriteToCreationMessage( NetObject*, NetMessage& ) const override;
	virtual void* OnCreate( NetMessage& ) const override;
	virtual void OnDestroy( NetObject* ) const override;
	virtual void WriteToDestroyMessage( NetMessage& ) const override {}

	virtual void ServerWriteUpdateToMessage( NetObject*, NetMessage& ) const override;
	virtual void ClientReadAndProcessUpdateFromServer( NetObject*, NetMessage& ) const override;

	//Not a client-owned object, hence these are stubs.
	virtual void ClientWriteUpdateToMessage( NetObject*, NetMessage& ) const override {}
	virtual void ServerReadAndProcessUpdateFromClient( NetObject*, NetMessage& ) const override {}
};
