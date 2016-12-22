#include "Game/Game Entities/PlayerController.hpp"
#include "Engine/Networking/NetMessage.hpp"


//--------------------------------------------------------------------------------------------------------------
const char* PlayerController::GetColorString() const
{
	switch ( m_color )
	{
		case PRIMARY_TEAR_COLOR_RED: return "Red";
		case PRIMARY_TEAR_COLOR_GREEN: return "Green";
		case PRIMARY_TEAR_COLOR_BLUE: return "Blue";
		default: return "BadColor";
	}
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerController::WriteToMessage( NetMessage& out_msg )
{
	//Create nuonce only written in ClientCreatePlayerRequest, so it's not included here.
	bool success = out_msg.Write<PlayerIndex>( m_controllerIndex );
	if ( !success )
		return false;

	success = out_msg.Write<NetConnectionIndex>( m_owningConnectionIndex );
	if ( !success )
		return false;

	success = out_msg.WriteString( m_username );
	if ( !success )
		return false;

	success = out_msg.Write<PrimaryTearColor>( m_color );
	return success;
}

//--------------------------------------------------------------------------------------------------------------
bool PlayerController::ReadFromMessage( const NetMessage& playerCreationMsg )
{
	bool success = playerCreationMsg.Read<PlayerIndex>( &m_controllerIndex ); //Should be INVALID.
	if ( !success )
		return false;

	success = playerCreationMsg.Read<NetConnectionIndex>( &m_owningConnectionIndex );
	if ( !success )
		return false;

	const char* username = playerCreationMsg.ReadString();
	if ( username == nullptr )
		return false;

	SetUsername( username );

	success = playerCreationMsg.Read<PrimaryTearColor>( &m_color );
	return success;
}


//--------------------------------------------------------------------------------------------------------------
void PlayerController::SetPlayerData( const PlayerController& newPlayerData )
{
	m_owningConnectionIndex = newPlayerData.GetOwningConnectionIndex();
	this->SetPlayerIndex( newPlayerData.GetPlayerIndex() );
	this->SetUsername( newPlayerData.m_username );
	this->SetPrimaryTearColor( newPlayerData.m_color );
	//Not updating creation nuonce.
}
