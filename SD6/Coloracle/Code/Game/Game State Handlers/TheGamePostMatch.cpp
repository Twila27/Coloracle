#include "Game/TheGame.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Game/Game Entities/PlayerAvatar.hpp"
#include "Game/Game Entities/PlayerController.hpp"
#include "Game/Game Entities/NetObjectSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
bool TheGame::StartupPostMatch( EngineEvent* )
{
	m_background = Sprite::Create( "PostMatchBG", WorldCoords2D::ZERO );
	m_background->SetLayerID( BACKGROUND_LAYER_ID, "BG" );
	SpriteRenderer::SetLayerVirtualSize( BACKGROUND_LAYER_ID, 4, 4 );
	SpriteRenderer::SetLayerIsScrolling( BACKGROUND_LAYER_ID, false );
	m_background->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ShutdownPostMatch( EngineEvent* )
{
	m_background->Disable();
	delete m_background;
	m_background = nullptr;

	g_theGame->LeaveGame();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::UpdatePostMatch( EngineEvent* ev )
{
	float deltaSeconds = dynamic_cast<EngineEventUpdate*>( ev )->deltaSeconds;
	PlayerAvatar* winner = m_playerAvatars[ m_winningPlayerIndex ];
	if ( m_winningPlayerIndex < MAX_NUM_PLAYERS && ( winner != nullptr ) && ( winner->GetController()->GetOwningConnectionIndex() == m_gameSession->GetMyConnectionIndex() ) )
		winner->Update( deltaSeconds ); //Don't update unless it's owned by us.

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::RenderPostMatch( EngineEvent* )
{
	SpriteRenderer::RenderFrame();

	//Winner may have left on this screen.
	PlayerAvatar* winner = m_playerAvatars[ m_winningPlayerIndex ];
	if ( m_winningPlayerIndex < MAX_NUM_PLAYERS && ( winner != nullptr ) )
	{
		char username[ MAX_GUID_LENGTH ];
		winner->GetController()->GetUsername( username );
		g_theRenderer->DrawTextProportional2D
		(
			Vector2f( 100.f, (float)g_theRenderer->GetScreenHeight() - 350.f ),
			Stringf( "Player %s is the winner!", username ),
			.80f,
			nullptr,
			Rgba::YELLOW
		);

		winner->SetColor( Rgba::YELLOW ); //Add winner tint.

		for each ( PlayerAvatar* avatar in m_playerAvatars )
		{
			if ( avatar == nullptr || avatar == winner )
				continue;

			const Rgba& tint = avatar->GetSpriteColor();
			Vector4f tintAsFloats = tint.GetAsFloats();
			tintAsFloats.w -= ( tintAsFloats.w - .01f < 0.f ) ? 0.f : .01f; //Lowering alpha to fade out non-winners.
			avatar->SetColor( tintAsFloats );
		}
	}
	return SHOULD_NOT_UNSUB;
}
