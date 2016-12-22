#include "Game/Game Entities/NetObject Protocols/ProtocolTeardropNPC.hpp"
#include "Game/Game Entities/TeardropNPC.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Networking/NetMessage.hpp"


//--------------------------------------------------------------------------------------------------------------
void* Protocol_TeardropNPC::OnCreate( NetMessage& msg ) const
{
	PrimaryTearColor color;
	msg.Read<PrimaryTearColor>( &color );
	TeardropNPC* newEnemy = new TeardropNPC( color );

	Vector2f position;
	msg.Read<WorldCoords2D>( &position );
	newEnemy->SetPosition( position );

	g_theGame->AddEnemy( newEnemy );

	return newEnemy;
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_TeardropNPC::OnDestroy( NetObject* netObj ) const
{
	TeardropNPC* enemy = (TeardropNPC*)netObj->syncedObject;
	g_theGame->RemoveEnemy( enemy );
	delete enemy; //Ensures dtor is called.
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_TeardropNPC::WriteToCreationMessage( NetObject* netObj, NetMessage& msg ) const
{
	TeardropNPC* enemy = (TeardropNPC*)( netObj->syncedObject );

	msg.Write<PrimaryTearColor>( enemy->GetColor() );
	msg.Write<Vector2f>( enemy->GetPosition() );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_TeardropNPC::ServerWriteUpdateToMessage( NetObject* netObj, NetMessage& msg ) const
{
	TeardropNPC* enemy = (TeardropNPC*)( netObj->syncedObject );
	msg.Write<Vector2f>( enemy->GetPosition() );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_TeardropNPC::ClientReadAndProcessUpdateFromServer( NetObject* netObj, NetMessage& msg ) const
{
	TeardropNPC* enemy = (TeardropNPC*)( netObj->syncedObject );
	Vector2f newPos;
	msg.Read<Vector2f>( &newPos );
	enemy->SetPosition( newPos );
}
