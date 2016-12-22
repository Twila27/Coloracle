#include "Game/Game Entities/NetObject Protocols/ProtocolPlayerAvatar.hpp"
#include "Game/Game Entities/NetObjectProtocol.hpp"
#include "Game/Game Entities/PlayerAvatar.hpp"
#include "Game/Game Entities/PlayerController.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Networking/NetSession.hpp"


//--------------------------------------------------------------------------------------------------------------
void* Protocol_PlayerAvatar::OnCreate( NetMessage& msg ) const
{
	PlayerIndex controllerIndex;
	msg.Read<PlayerIndex>( &controllerIndex );
	PlayerAvatar* avatar = new PlayerAvatar( g_theGame->GetIndexedPlayerController( controllerIndex ) );
	
	Vector2f pos;
	msg.Read<Vector2f>( &pos );
	avatar->SetPosition( pos );
	avatar->GetSprite()->Enable();

	g_theGame->SetIndexedPlayerAvatar( controllerIndex, avatar );
//	g_theGame->AddToEntityList( createdObject );

	return avatar;
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::OnDestroy( NetObject* netObj ) const
{
	delete (PlayerAvatar*)netObj->syncedObject; //Ensures dtor is called.
}

	
//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::WriteToCreationMessage( NetObject* netObj, NetMessage& msg ) const
{
	PlayerAvatar* playerAvatar = (PlayerAvatar*)( netObj->syncedObject );
	PlayerController* controller = playerAvatar->GetController();
	msg.Write<PlayerIndex>( controller->GetPlayerIndex() );
	msg.Write<Vector2f>( playerAvatar->GetPosition() );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::ServerWriteUpdateToMessage( NetObject* netObj, NetMessage& msg ) const
{
	PlayerAvatar* playerAvatar = (PlayerAvatar*)( netObj->syncedObject );
	msg.Write<Vector2f>( playerAvatar->GetPosition() ); //Letting the client own this for now.
	msg.Write<Vector2f>( playerAvatar->GetVelocity() ); //Letting the client own this for now.

	int8_t swordLevel = playerAvatar->GetSwordLevel();
	msg.Write<int8_t>( swordLevel );

	for ( int8_t swordIndex = 0; swordIndex < swordLevel; swordIndex++ )
		msg.Write<PrimaryTearColor>( playerAvatar->GetSwordColorAt( swordIndex ) );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::ClientReadAndProcessUpdateFromServer( NetObject* netObj, NetMessage& msg ) const
{
	if ( g_theGame->IsMyConnectionHosting() )
		return; //Don't need the below, should already be updated.

	PlayerAvatar* playerAvatar = (PlayerAvatar*)( netObj->syncedObject );
	Vector2f positionOnServer; //Needed because non-owner clients still have to receive the owned avatar's position to update it!
	msg.Read<Vector2f>( &positionOnServer ); //Might be worth teleporting to this if we get beyond a certain range.
	Vector2f velocityOnServer;
	msg.Read<Vector2f>( &velocityOnServer ); //Still need to read these on owners to advance write head inside msg.

	NetSession* sessionRef = g_theGame->GetGameNetSession();
	if ( netObj->owningConnectionIndex != sessionRef->GetMyConnectionIndex() )
	{
		playerAvatar->SetPosition( positionOnServer );
		playerAvatar->SetVelocity( velocityOnServer ); //Use this to do client-side prediction if I get A7 clock stuff!		
	}

	int8_t swordLevel;
	msg.Read<int8_t>( &swordLevel );
	playerAvatar->SetSwordLevel( swordLevel );

	if ( swordLevel > MAX_NUM_TEAR_COUNT )
	{
		ERROR_RECOVERABLE( "Sword level past max in message!" );
		return;
	}

	for ( int8_t swordIndex = 0; swordIndex < swordLevel; swordIndex++ )
	{
		PrimaryTearColor color;
		msg.Read<PrimaryTearColor>( &color );
		playerAvatar->SetSwordColorAt( swordIndex, color );
	}

	playerAvatar->SetColorFromPrimaryTearColor( playerAvatar->GetSwordColor() );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::ClientWriteUpdateToMessage( NetObject* netObj, NetMessage& msg ) const
{
	PlayerAvatar* playerAvatar = (PlayerAvatar*)( netObj->syncedObject );
	msg.Write<Vector2f>( playerAvatar->GetPosition() );
	msg.Write<Vector2f>( playerAvatar->GetVelocity() );
}


//--------------------------------------------------------------------------------------------------------------
void Protocol_PlayerAvatar::ServerReadAndProcessUpdateFromClient( NetObject* netObj, NetMessage& msg ) const
{
	PlayerAvatar* playerAvatar = (PlayerAvatar*)( netObj->syncedObject );
	Vector2f newPos;
	msg.Read<Vector2f>( &newPos );
	playerAvatar->SetPosition( newPos );

	Vector2f newVel;
	msg.Read<Vector2f>( &newVel );
	playerAvatar->SetVelocity( newVel );
}

