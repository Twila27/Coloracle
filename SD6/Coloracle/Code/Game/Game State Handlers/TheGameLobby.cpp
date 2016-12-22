#include "Game/TheGame.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Core/TheConsole.hpp"


//--------------------------------------------------------------------------------------------------------------
bool TheGame::RenderLobby( EngineEvent* )
{
	SpriteRenderer::RenderFrame();
	
	g_theRenderer->DrawTextProportional2D
	(
		Vector2f( 100.f, (float)g_theRenderer->GetScreenHeight() - 350.f ),
		Stringf( "(%c) Host Game \t \t (%c) Start Game", KEY_TO_HOST_GAME, KEY_TO_START_GAME ),
		.25f
	);

	g_theRenderer->DrawTextProportional2D
	(
		Vector2f( 100.f, (float)g_theRenderer->GetScreenHeight() - 400.f ),
		Stringf( "(ESC) Exit to Title", KEY_TO_EXIT_STUFF ),
		.25f
	);


	g_theRenderer->DrawTextProportional2D
	(
		Vector2f( 100.f, (float)g_theRenderer->GetScreenHeight() - 450.f ),
		Stringf( "Use NetSessionJoin to join known hosts." ),
		.25f
	);

	if ( IsMyConnectionHosting() )
	{
		g_theRenderer->DrawTextProportional2D
		(
			Vector2f( 300.f, (float)g_theRenderer->GetScreenHeight() - 50.f ),
			Stringf( "Welcome, Judge of the Dead.", m_hostSelectedGoalTearCount ),
			.25f,
			nullptr,
			Rgba::RED
		);

		g_theRenderer->DrawTextProportional2D
		(
			Vector2f( 300.f, (float)g_theRenderer->GetScreenHeight() - 100.f ),
			Stringf( "Round Tear Count Goal: %d", m_hostSelectedGoalTearCount ),
			.25f,
			nullptr,
			Rgba::YELLOW
		);

		g_theRenderer->DrawTextProportional2D
		(
			Vector2f( 300.f, (float)g_theRenderer->GetScreenHeight() - 150.f ),
			Stringf( "Use the 1-9 keys to increase goal up to max, 0 to clear.", m_hostSelectedGoalTearCount ),
			.25f,
			nullptr,
			Rgba::RED
		);
	}

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
static unsigned int s_numHostGameInvocation = 0;


//--------------------------------------------------------------------------------------------------------------
bool TheGame::UpdateLobby( EngineEvent* )
{
//	float deltaSeconds = dynamic_cast<EngineEventUpdate*>( ev )->deltaSeconds;

	if ( g_theInput->WasKeyPressedOnce( KEY_TO_HOST_GAME ) )
	{
		TODO( "Grab value out of a text field the player's typed into." );
		g_theGame->HostGame( Stringf( "Host%u", s_numHostGameInvocation++ ) );
		g_theConsole->RunCommand( "NetSessionHostStartListening" );
		g_theConsole->ShowConsole();
		g_theConsole->HidePrompt();
	}
	else if ( g_theInput->WasKeyPressedOnce( KEY_TO_JOIN_GAME ) )
	{
		if ( !IsMyConnectionHosting() )
		{
			LogAndShowPrintfWithTag( "TheGameLobby", "Cannot join, already hosting!" );
		}
		else
		{
			LogAndShowPrintfWithTag( "TheGameLobby", "Still need to implement host broadcast/discovery first!" );
		}
	}
	else if ( g_theInput->WasKeyPressedOnce( KEY_TO_START_GAME ) )
	{
		if ( !IsMyConnectionHosting() )
		{
			LogAndShowPrintfWithTag( "TheGameLobby", "You are not a host!" );
		}
		else
		{
			ServerStartMatch(); //State update will occur on msg receive.
			g_theConsole->HideConsole();
		}
	}

	if ( IsMyConnectionHosting() )
	{
		if ( g_theInput->WasKeyPressedOnce( '0' ) )
			m_hostSelectedGoalTearCount = 1;

		if ( g_theInput->WasKeyPressedOnce( '1' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 1), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '2' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 2), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '3' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 3), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '4' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 4), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '5' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 5), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '6' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 6), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '7' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 7), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '8' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 8), MAX_NUM_TEAR_COUNT );

		if ( g_theInput->WasKeyPressedOnce( '9' ) )
			m_hostSelectedGoalTearCount = GetMin( (int8_t)(m_hostSelectedGoalTearCount + 9), MAX_NUM_TEAR_COUNT );
	}

	//if ( g_theInput->WasKeyPressedOnce( 'R' ) )
	//if ( g_theInput->WasKeyPressedOnce( 'G' ) )
	//if ( g_theInput->WasKeyPressedOnce( 'B' ) )

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::StartupLobby( EngineEvent* )
{
	if ( m_gameSession != nullptr )
		StopGameNetSession();

	StartGameNetSession(); //Start a clean one.

	m_background = Sprite::Create( "LobbyBG", WorldCoords2D::ZERO );
	m_background->SetLayerID( BACKGROUND_LAYER_ID, "BG" );
	SpriteRenderer::SetLayerVirtualSize( BACKGROUND_LAYER_ID, 4, 4 );
	SpriteRenderer::SetLayerIsScrolling( BACKGROUND_LAYER_ID, false );
	//	SpriteRenderer::AddLayerEffect( UI_LAYER_ID, FramebufferEffect::GetFboEffect( "FboEffect_PostProcessObama" ) );
	m_background->Enable();

	m_hostSelectedGoalTearCount = 7;

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::ShutdownLobby( EngineEvent* ) //May be exiting to Title or to a match.
{
	m_background->Disable();
	delete m_background;
	m_background = nullptr;

	return SHOULD_NOT_UNSUB;
}
