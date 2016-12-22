#include "Game/TheGame.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Game/Game Entities/PlayerController.hpp"


//--------------------------------------------------------------------------------------------------------------
static Sprite* s_title = nullptr;


//--------------------------------------------------------------------------------------------------------------
bool TheGame::StartupTitle( EngineEvent* )
{
	if ( m_gameSession != nullptr )
		StopGameNetSession();

	//In case we hosted in lobby, and made our own connection as host or had someone join, but then bailed:
	for each ( PlayerController*& controller in m_playerControllers )
	{
		if ( controller == nullptr )
			continue;

		delete controller; //Won't have an avatar or net object to worry about -- shouldn't have it for above case.
		controller = nullptr;
	}

	s_title = Sprite::Create( "Title_UI", WorldCoords2D( 0.f, 5.f ) );
	s_title->SetLayerID( UI_LAYER_ID, "UI" );
	SpriteRenderer::SetLayerVirtualSize( UI_LAYER_ID, 5, 5 );
	SpriteRenderer::SetLayerIsScrolling( UI_LAYER_ID, false );
//	SpriteRenderer::AddLayerEffect( UI_LAYER_ID, FramebufferEffect::GetFboEffect( "FboEffect_PostProcessObama" ) );
	s_title->Enable();

	m_background = Sprite::Create( "CharonBG", WorldCoords2D::ZERO );
	m_background->SetLayerID( BACKGROUND_LAYER_ID, "BG" );
	SpriteRenderer::SetLayerVirtualSize( BACKGROUND_LAYER_ID, 5, 5 );
	SpriteRenderer::SetLayerIsScrolling( BACKGROUND_LAYER_ID, false );
	//	SpriteRenderer::AddLayerEffect( UI_LAYER_ID, FramebufferEffect::GetFboEffect( "FboEffect_PostProcessObama" ) );
	m_background->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ShutdownTitle( EngineEvent* )
{
	s_title->Disable();
	delete s_title;
	s_title = nullptr;

	m_background->Disable();
	delete m_background;
	m_background = nullptr;

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::RenderTitle( EngineEvent* )
{
	SpriteRenderer::RenderFrame();
	
	g_theRenderer->DrawTextProportional2D
	(
		Vector2f( 100.f, (float)g_theRenderer->GetScreenHeight() - 400.f ),
		Stringf( "Press Enter for Lobby", KEY_TO_EXIT_STUFF )
	);
	
//	g_theRenderer->DrawTextProportional2D
//	(
//		Vector2f( 50.f, (float)g_theRenderer->GetScreenHeight() - 400.f ),
//		Stringf( "Press Enter for Lobby", KEY_TO_EXIT_STUFF )
//	);

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::UpdateTitle( EngineEvent* )
{
//	float deltaSeconds = dynamic_cast<EngineEventUpdate*>( ev )->deltaSeconds;

	if ( g_theInput->WasKeyPressedOnce( KEY_TO_EXIT_STUFF ) )
		g_theConsole->RunCommand( "Quit" );

	return SHOULD_NOT_UNSUB;
}
