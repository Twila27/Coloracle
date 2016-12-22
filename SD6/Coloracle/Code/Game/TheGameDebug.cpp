#include "Game/TheGameDebug.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Networking/NetSession.hpp"
#include "Engine/Math/Camera3D.hpp"
#include "Engine/Math/Camera2D.hpp"
#include "Game/Game Entities/PlayerAvatar.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC TheGameDebug* TheGameDebug::s_theGameDebug = nullptr;


//--------------------------------------------------------------------------------------------------------------
TheGameDebug::TheGameDebug()
	: m_camPosText( new TextRenderer( TEXT_PROPORTIONAL_2D, true, 100.f, (float)g_theRenderer->GetScreenHeight() - 50.f,  "" ) )
	, m_camOriText( new TextRenderer( TEXT_PROPORTIONAL_2D, true, 100.f, (float)g_theRenderer->GetScreenHeight() - 100.f, "", .25f, nullptr, Rgba( .7f, .2f, .7f ) ) )
	, m_camDirText( new TextRenderer( TEXT_PROPORTIONAL_2D, true, 100.f, (float)g_theRenderer->GetScreenHeight() - 150.f, "", .25f, nullptr, Rgba( .7f, .7f, .7f ) ) )
	, m_liveParticlesText( new TextRenderer( TEXT_PROPORTIONAL_2D, true, (float)g_theRenderer->GetScreenWidth() - 350.f, (float)g_theRenderer->GetScreenHeight() - 50.f, "" ))
	, m_spritesCulledText( new TextRenderer( TEXT_PROPORTIONAL_2D, true, (float)g_theRenderer->GetScreenWidth() - 350.f, (float)g_theRenderer->GetScreenHeight() - 150.f, "" ))
{
}


//--------------------------------------------------------------------------------------------------------------
TheGameDebug::~TheGameDebug()
{
	delete m_camPosText;
	delete m_camOriText;
	delete m_camDirText;
	delete m_liveParticlesText;
	delete m_spritesCulledText;
}


//--------------------------------------------------------------------------------------------------------------
STATIC TheGameDebug* /*CreateOrGet*/ TheGameDebug::Instance()
{
	if ( s_theGameDebug == nullptr )
		s_theGameDebug = new TheGameDebug();

	return s_theGameDebug;
}


//--------------------------------------------------------------------------------------------------------------
void TheGameDebug::HandleDebug( float deltaSeconds )
{
	RenderFramerate( deltaSeconds ); //Because debug prints slow it down, so we want to see it apart from rest.
	
	std::vector<PlayerAvatar*> avatars;
	size_t numPlayers = g_theGame->GetAvatarsOnMyConnection( avatars );
	for ( size_t avatarIndex = 0; avatarIndex < numPlayers; avatarIndex++ )
	{
		PlayerAvatar* avatar = avatars[ avatarIndex ]; 
		std::string swordColor = avatar->GetSwordColorName();
		
		g_theRenderer->DrawTextProportional2D( Vector2f( 100.f, 800.f - avatarIndex * 100.f ),
											   Stringf( "Player %u Position: %f %f", avatarIndex, avatar->GetPosition().x, avatar->GetPosition().y ) );
		g_theRenderer->DrawTextProportional2D( Vector2f( 100.f, 750.f - avatarIndex * 100.f ),
											   Stringf( "Player %u Velocity: %f %f", avatarIndex, avatar->GetVelocity().x, avatar->GetVelocity().y ) );
		g_theRenderer->DrawTextProportional2D( Vector2f( 100.f, 700.f - avatarIndex * 100.f ),
											  Stringf( "Sword Level: %d \t    Sword Color: %s", avatar->GetSwordLevel(), swordColor.c_str() ) );
	}


	//Note that this means the memory debug window won't be able to render since it's engine-side only in TheEngine::Render.
	if ( g_inDebugMode )
	{
		if ( !m_wasInDebugModeLastFrame )
		{
			m_camPosText->Enable();
			m_camOriText->Enable();
			m_camDirText->Enable();
			m_liveParticlesText->Enable();
			m_spritesCulledText->Enable();
		}

		g_theRenderer->SetBlendFunc( 0x0302, 0x0303 );

		g_theRenderer->EnableBackfaceCulling( false ); //Console needs it.
		g_theRenderer->EnableDepthTesting( false ); //Drawing this on top of everything else.

		RenderReticle(); //May later be considered part of HUD and moved to Render2D().

		RenderLeftDebugText2D();
		RenderRightDebugText2D();

		if ( g_theGame->GetGameNetSession() != nullptr )
			g_theGame->GetGameNetSession()->SessionRender();
	}
	else if ( m_wasInDebugModeLastFrame ) //Need to turn off debug text objects.
	{
		m_camPosText->Disable();
		m_camOriText->Disable();
		m_camDirText->Disable();
		m_liveParticlesText->Disable();
		m_spritesCulledText->Disable();
	}

	m_wasInDebugModeLastFrame = g_inDebugMode;
}


//-----------------------------------------------------------------------------
void TheGameDebug::RenderFramerate( float deltaSeconds )
{
	static float currentDisplayedFPS = -1.f;
	static float secondsSinceLastFramerateUpdate = 0.f;
	static const float SECONDS_BETWEEN_FPS_UPDATES = 1.f;

	secondsSinceLastFramerateUpdate += deltaSeconds;

	if ( secondsSinceLastFramerateUpdate > SECONDS_BETWEEN_FPS_UPDATES )
	{
		currentDisplayedFPS = 1.f / deltaSeconds;
		secondsSinceLastFramerateUpdate = 0.f;
	}

	g_theRenderer->DrawTextProportional2D
	(
		Vector2f( 0.f, (float)g_theRenderer->GetScreenHeight() ),
		Stringf( "%.2f", currentDisplayedFPS ),
		.25f,
		nullptr,
		( currentDisplayedFPS < 30.f ) ? Rgba::RED : Rgba::GREEN
	);
}


//-----------------------------------------------------------------------------
void TheGameDebug::RenderReticle()
{
	Vector2f screenCenter = g_theRenderer->GetScreenCenter();

	Vector2f crosshairLeft = Vector2f( screenCenter.x - HUD_CROSSHAIR_RADIUS, screenCenter.y );
	Vector2f crosshairRight = Vector2f( screenCenter.x + HUD_CROSSHAIR_RADIUS, screenCenter.y );
	Vector2f crosshairUp = Vector2f( screenCenter.x, screenCenter.y - HUD_CROSSHAIR_RADIUS );
	Vector2f crosshairDown = Vector2f( screenCenter.x, screenCenter.y + HUD_CROSSHAIR_RADIUS );

	TODO( "Replace OpenGL constant values with encapsulations, or better yet make RenderState class for this!" );
	g_theRenderer->SetBlendFunc( GL_SRC_ALPHA, GL_ZERO ); //GL CONSTANTS.
	g_theRenderer->DrawLine( crosshairLeft, crosshairRight, Rgba(), Rgba(), HUD_CROSSHAIR_THICKNESS );
	g_theRenderer->DrawLine( crosshairUp, crosshairDown, Rgba(), Rgba(), HUD_CROSSHAIR_THICKNESS );
	g_theRenderer->SetBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
}


//-----------------------------------------------------------------------------
void TheGameDebug::RenderRightDebugText2D()
{
	m_spritesCulledText->SetText( Stringf( "# Sprites Culled: %u", SpriteRenderer::GetNumSpritesCulled() ) );
	m_liveParticlesText->SetText( Stringf( "# Live Particles: %u", SpriteRenderer::GetNumLiveParticles() ) );

	/*static TextRenderer gameState = */g_theRenderer->DrawTextMonospaced2D
	(
		Vector2f( (float)g_theRenderer->GetScreenWidth() - 350.f, (float)g_theRenderer->GetScreenHeight() - 250.f ),
		Stringf( "GameState: %s", g_theGame->GetCurrentStateName() ),
		18.f,
		Rgba::GREEN,
		nullptr,
		.65f
	);
}


//-----------------------------------------------------------------------------
void TheGameDebug::RenderLeftDebugText2D()
{	
	std::string camPosString;
	std::string camOriString;
	std::string camDirString;

	switch ( g_theGame->GetActiveCameraMode() )
	{
	case CAMERA_MODE_3D:
	{
		const Camera3D* playerCamera3D = g_theGame->GetActiveCamera3D();
		WorldCoords3D camPos = playerCamera3D->m_worldPosition;
		EulerAngles camOri = playerCamera3D->m_orientation;
		Vector3f camDir = playerCamera3D->GetForwardXYZ();

		camPosString = Stringf( "Camera Position: %f %f %f", camPos.x, camPos.y, camPos.z );
		camOriString = Stringf( "Camera Orientation: roll(%f) pitch(%f) yaw(%f)", camOri.m_rollDegrees, camOri.m_pitchDegrees, camOri.m_yawDegrees );
		camDirString = Stringf( "Camera Forward XYZ: %f %f %f", camDir.x, camDir.y, camDir.z );
		break;
	}
	case CAMERA_MODE_2D:
	{
		const Camera2D* playerCamera2D = g_theGame->GetActiveCamera2D();
		WorldCoords2D camPos = playerCamera2D->m_worldPosition;
		float camOri = playerCamera2D->m_orientationDegrees;
		Vector2f camDir = playerCamera2D->GetForwardXY();

		camPosString = Stringf( "Camera Position: %f %f", camPos.x, camPos.y );
		camOriString = Stringf( "Camera Orientation: %f (Degrees)", camOri );
		camDirString = Stringf( "Camera Forward XYZ: %f %f", camDir.x, camDir.y );
		break;
	}
	}

	m_camPosText->SetText( camPosString );
	m_camOriText->SetText( camOriString );
	m_camDirText->SetText( camDirString );
}