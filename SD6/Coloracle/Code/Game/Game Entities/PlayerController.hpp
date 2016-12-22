#pragma once
#include "Game/GameCommon.hpp"


//-----------------------------------------------------------------------------
class NetMessage;
#define MAX_GUID_LENGTH (32)


//-----------------------------------------------------------------------------
class PlayerController //Called a conceptual player in class, distinct from the pawn it controls (see PlayerAvatar).
{
/* Any time you change your Player( Conceptual ) state,
	First, make sure it's yourself -- ONLY the Host can change non-self entities.
	Second, do your update.
	Third, dirty your connection which goes on as a PLAYER_UPDATE msg of yourself( controller ) to the Host, who fans that out to every other connection.
*/
public:
	PlayerController()
		: m_controllerIndex( INVALID_PLAYER_INDEX )
	{
		memset( m_username, -1, sizeof( m_username[ 0 ] ) * MAX_GUID_LENGTH );
	}
	PlayerController( nuonce_t creationNuonce )
		: m_controllerIndex( INVALID_PLAYER_INDEX )
		, m_createNuonce( creationNuonce ) //Else FindByNuonce goes awry.
	{
		memset( m_username, -1, sizeof( m_username[ 0 ] ) * MAX_GUID_LENGTH );
	}
	PlayerController( NetConnectionIndex owningConnectionIndex, nuonce_t creationNuonce, PlayerIndex controllerIndex = INVALID_PLAYER_INDEX )
		: m_owningConnectionIndex( owningConnectionIndex )
		, m_controllerIndex( controllerIndex )
		, m_createNuonce( creationNuonce )
	{
		memset( m_username, -1, sizeof( m_username[ 0 ] ) * MAX_GUID_LENGTH );
	}
	PlayerIndex GetPlayerIndex() const { return m_controllerIndex; }
	NetConnectionIndex GetOwningConnectionIndex() const { return m_owningConnectionIndex; }
	const char* GetColorString() const;
	nuonce_t GetCreationNuonce() const { return m_createNuonce; }
	void GetUsername( char out_guid[ MAX_GUID_LENGTH ] ) const { strcpy_s( out_guid, MAX_GUID_LENGTH, m_username ); }
	void SetUsername( const std::string& guid ) { strcpy_s( m_username, MAX_GUID_LENGTH, guid.c_str() );  }
	void SetPrimaryTearColor( PrimaryTearColor color ) { m_color = color; }
	void SetPlayerIndex( PlayerIndex newVal ) { m_controllerIndex = newVal; }
	void SetPlayerData( const PlayerController& newPlayerData );
	bool WriteToMessage( NetMessage& out_msg );
	bool ReadFromMessage( const NetMessage& playerCreationMsg );


private:

	//Assigned from the server host, must-have:
	PlayerIndex m_controllerIndex;
	NetConnectionIndex m_owningConnectionIndex;
	nuonce_t m_createNuonce;


	//
	char m_username[ MAX_GUID_LENGTH ];

	//
	PrimaryTearColor m_color; //Because like team ID, this stays for the entire round.
//	NetSessionReadyState m_readyState; //e.g. In lobby or not.
//	NetGameSessionModeState m_gameModeState; //e.g. In pre-match, mid-match, or post-match?
};
