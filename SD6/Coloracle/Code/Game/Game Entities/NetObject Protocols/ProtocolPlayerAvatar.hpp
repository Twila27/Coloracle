#pragma once


#include "Game/Game Entities/NetObjectProtocol.hpp"


class Protocol_PlayerAvatar : public NetObjectProtocol //See parent class for details.
{
public:
	virtual void WriteToCreationMessage( NetObject*, NetMessage& msg ) const override;
	virtual void* OnCreate( NetMessage& msg ) const override;
	virtual void OnDestroy( NetObject* ) const override;
	virtual void WriteToDestroyMessage( NetMessage& ) const override {}

	virtual void ServerWriteUpdateToMessage( NetObject*, NetMessage& msg ) const override;
	virtual void ClientReadAndProcessUpdateFromServer( NetObject*, NetMessage& msg ) const override;

	//Because this is a client-owned object, these will actually be non-stubs:
	virtual void ClientWriteUpdateToMessage( NetObject*, NetMessage& msg ) const override;
	virtual void ServerReadAndProcessUpdateFromClient( NetObject*, NetMessage& msg ) const override;
};
