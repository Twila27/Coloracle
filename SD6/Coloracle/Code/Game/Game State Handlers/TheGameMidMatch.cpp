#include "Game/TheGame.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Game/Game Entities/PlayerAvatar.hpp"
#include "Game/Game Entities/NetObjectSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
bool TheGame::StartupMidMatch( EngineEvent* )
{
	m_background = Sprite::Create( "UnderworldBG", WorldCoords2D::ZERO );
	m_background->SetLayerID( BACKGROUND_LAYER_ID, "BG" );
	SpriteRenderer::SetLayerVirtualSize( BACKGROUND_LAYER_ID, 4, 4 );
	SpriteRenderer::SetLayerIsScrolling( BACKGROUND_LAYER_ID, true );
//	SpriteRenderer::AddLayerEffect( UI_LAYER_ID, FramebufferEffect::GetFboEffect( "FboEffect_PostProcessObama" ) );
	m_background->Enable();

	m_spawnTimer.Start();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ShutdownMidMatch( EngineEvent* )
{
	if ( IsMyConnectionHosting() )
	{
		std::vector<TeardropNPC*>::iterator enemyIter = m_teardropEnemies.begin();
		while ( enemyIter != m_teardropEnemies.end() )
		{
			NetObjectSystem::NetStopSyncForLocalObject( *enemyIter ); //Deletes the pointers and tells clients to do same.
			enemyIter = m_teardropEnemies.begin(); //By deleting the former begin(), this should re-point us where we need to be.
		}
	}

	m_teardropEnemies.clear();

	m_background->Disable();
	delete m_background;
	m_background = nullptr;

	//Make sure that ClientEndMatch sets winningPlayerIndex BEFORE SetCurrentState call to leave this state.
	if ( m_winningPlayerIndex == INVALID_PLAYER_INDEX ) //Means we quit without going to post-match state, 
		g_theGame->LeaveGame(); //and so should leave.

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::UpdateMidMatch( EngineEvent* )
{
//	float deltaSeconds = dynamic_cast<EngineEventUpdate*>( ev )->deltaSeconds;

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::RenderMidMatch( EngineEvent* )
{
	SpriteRenderer::RenderFrame();

	return SHOULD_NOT_UNSUB;
}