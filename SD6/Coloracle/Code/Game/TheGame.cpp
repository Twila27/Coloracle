#include "Game/TheGame.hpp"

#include "Engine/Math/MathUtils.hpp"
#include "Engine/Physics/LinearDynamicsState3D.hpp"
#include "Engine/Input/TheInput.hpp"
#include "Engine/Audio/TheAudio.hpp"
#include "Engine/Core/Command.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Math/Camera3D.hpp"
#include "Engine/Math/Camera2D.hpp"

#include "Engine/Renderer/Sprite.hpp"
#include "Engine/Renderer/SpriteResource.hpp"
#include "Engine/Renderer/SpriteRenderer.hpp"
#include "Engine/Renderer/FramebufferEffect.hpp"
#include "Engine/Renderer/AnimatedSprite.hpp"

#include "Engine/Renderer/Particles/ParticleSystem.hpp"
#include "Engine/Renderer/Particles/ParticleEmitter.hpp"
#include "Engine/Renderer/Particles/ParticleSystemDefinition.hpp"
#include "Engine/Renderer/Particles/ParticleEmitterDefinition.hpp"

#include "Game/GameCommon.hpp"
#include "Game/TheGameDebug.hpp"
#include "Game/TheGameNetworking.cpp"
#include "Game/Game Entities/TeardropNPC.hpp"
#include "Game/Game Entities/NetObjectSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
TheGame* g_theGame = nullptr;


//--------------------------------------------------------------------------------------------------------------
void TheGame::RegisterConsoleCommands_General()
{
	//AES A1
	g_theConsole->RegisterCommand( "ToggleActiveCameraMode", TheGame::ToggleActiveCameraMode );
	g_theConsole->RegisterCommand( "TogglePolarTranslations2D", TheGame::ToggleActiveCamType2D );
	g_theConsole->RegisterCommand( "TogglePolarTranslations3D", TheGame::ToggleActiveCamType3D );
}


//--------------------------------------------------------------------------------------------------------------
TheGame::TheGame()
	: m_gameSession( nullptr )
	, m_shouldSendAttackMessage( false )
	, m_lastSentCreationNuonce( 0 )
	, m_hostSelectedGoalTearCount( -1 )
	, m_winningPlayerIndex( INVALID_PLAYER_INDEX )
	, m_spawnTimer( MAX_SECONDS_BETWEEN_SPAWNS, "GameEvent_OnSpawnTimerEnded" )
{
}


//--------------------------------------------------------------------------------------------------------------
TheGame::~TheGame()
{
	delete s_playerCamera2D;
	delete s_playerCamera3D;

	delete m_gameSession; //In case we forgot to call the stop command.
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::Update( float deltaSeconds )
{
	//Have to disseminate events more manually, StateCommands really only do fixed-value event arguments unlike deltaSeconds.
	EngineEventUpdate updateEv( deltaSeconds );
	StateNameStr currentStateName = m_gameStateMachine.GetCurrentState()->GetName();
	if ( strcmp( currentStateName, "TheGameState_Title" ) == 0 )
		TheEventSystem::Instance()->TriggerEvent( "TheGame_UpdateTitle", &updateEv );
	else if ( strcmp( currentStateName, "TheGameState_Lobby" ) == 0 )
		TheEventSystem::Instance()->TriggerEvent( "TheGame_UpdateLobby", &updateEv );
	else if ( strcmp( currentStateName, "TheGameState_MidMatch" ) == 0 )
		TheEventSystem::Instance()->TriggerEvent( "TheGame_UpdateMidMatch", &updateEv );
	else if ( strcmp( currentStateName, "TheGameState_PostMatch" ) == 0 )
		TheEventSystem::Instance()->TriggerEvent( "TheGame_UpdatePostMatch", &updateEv );

	m_gameStateMachine.Update(); //If this comes first, exiting back to title in here will thereafter instantly trigger game exit in UpdateTitle.

	UpdateCamera( deltaSeconds );
	UpdateAudioListener();

	SpriteRenderer::Update( deltaSeconds ); //Culling should be handled by TheRenderer itself, see code review. What about animation updates?

	//How else would we handle client rendering themselves before the session's up and going and connected and in a game?
	if ( m_gameSession == nullptr )
	{
		EngineEventUpdate ev( deltaSeconds ); //To render FPS.
		TheEventSystem::Instance()->TriggerEvent( "TheGame::ClientRender2D", &ev ); //Session doesn't exist yet to trigger this render function.
	}
	else
	{
		m_didClientRender = false; //Re-zeroing.
		bool didTickNetwork = m_gameSession->SessionUpdate( deltaSeconds ); //May or may not set did client render true, cf. ClientUpdatePlayers.
		if ( !didTickNetwork || !m_didClientRender )
		{
			EngineEventUpdate ev( deltaSeconds ); //To render FPS.
			TheEventSystem::Instance()->TriggerEvent( "TheGame::ClientRender2D", &ev ); //SessionUpdate didn't get to ClientRender trigger yet.
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
PlayerAvatar* TheGame::CheckWinCondition()
{
	for each ( PlayerAvatar* avatar in m_playerAvatars )
		if ( ( avatar != nullptr ) && ( avatar->GetSwordLevel() >= m_hostSelectedGoalTearCount ) )
			return avatar;

	return nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::UpdateWorld( float deltaSeconds )
{
	if ( !IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Should not be here unless you're hosting!" );
		return;
	}

	for ( std::vector<TeardropNPC*>::iterator enemyIter = m_teardropEnemies.begin(); enemyIter != m_teardropEnemies.end(); )
	{
		TeardropNPC* enemy = *enemyIter;
		enemy->Update( deltaSeconds );

		AABB2f enemyBounds = enemy->GetBounds();
		bool enemyDied = false;
		for each ( PlayerAvatar* avatar in m_playerAvatars )
		{
			if ( avatar == nullptr )
				continue;
			
			if ( DoAABBsOverlap( enemyBounds, avatar->GetBounds() ) )
				enemyDied = HandleCollision( avatar, enemy );

			if ( enemyDied )
				break;
		}

		if ( enemyDied )
		{
			enemyIter = m_teardropEnemies.erase( enemyIter );
			NetObjectSystem::NetStopSyncForLocalObject( enemy );
		}
		else ++enemyIter;
	}	

	m_spawnTimer.Update( deltaSeconds );
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::HandleCollision( PlayerAvatar* avatar, TeardropNPC* enemy )
{
	const bool SHOULD_NOT_REMOVE_ENEMY = false;
	const bool SHOULD_INDEED_REMOVE_ENEMY = true;

	//Attacks pass through enemies if they match your sword color, and you take damage.
	if ( enemy->GetColor() == avatar->GetSwordColor() )
	{
		avatar->LoseSwordColor();
		return SHOULD_NOT_REMOVE_ENEMY;
	}
	else //Otherwise, you add their color to your sword.
	{
		avatar->AddSwordColor( enemy->GetColor() );
		return SHOULD_INDEED_REMOVE_ENEMY;
	}
}


//--------------------------------------------------------------------------------------------------------------
int8_t TheGame::GetHighestPlayerSwordLevel()
{
	int8_t currentHighestLevel = -1;

	for each ( PlayerAvatar* avatar in m_playerAvatars )
	{
		if ( avatar == nullptr )
			continue;

		int8_t avatarSwordLevel = avatar->GetSwordLevel();
		if ( avatarSwordLevel > currentHighestLevel )
			currentHighestLevel = avatarSwordLevel;
	}

	return currentHighestLevel;
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::UpdateAudioListener()
{
	//Update listener in positional audio subsystem (VR or not).
	const Vector3f& camPos = s_playerCamera3D->m_worldPosition;
	FMOD_3D_ATTRIBUTES attributes = { { 0 } };
	attributes.position.x = camPos.x;
	attributes.position.y = camPos.y;
	attributes.position.z = camPos.z;
	attributes.up.x = 0.f;
	attributes.up.y = 0.f;
	attributes.up.z = 1.f;
	attributes.forward.x = 1.f;
	attributes.forward.y = 0.f;
	attributes.forward.z = 0.f;
	g_theAudio->SetListenerAttributes( 0, &attributes );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::InitGameStates()
{
	//Note that dynamic-argument events (like Updates' deltaSeconds) are registered here but not tied to state logic.
	//Instead, calls to these are manually handled in TheGame::Update. Render calls in TheGame::Render to not interleave them with state logic.

	State* titleState = m_gameStateMachine.CreateState( "TheGameState_Title", true );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::StartupTitle >( "TheGame_StartupTitle", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::UpdateTitle >( "TheGame_UpdateTitle", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::RenderTitle >( "TheGame_RenderTitle", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::ShutdownTitle >( "TheGame_ShutdownTitle", this );
	titleState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "TheGame_StartupTitle" ) );
	titleState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "TheGame_ShutdownTitle" ) );

	State* lobbyState = m_gameStateMachine.CreateState( "TheGameState_Lobby" );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::StartupLobby >( "TheGame_StartupLobby", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::UpdateLobby >( "TheGame_UpdateLobby", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::RenderLobby >( "TheGame_RenderLobby", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::ShutdownLobby >( "TheGame_ShutdownLobby", this );
	lobbyState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "TheGame_StartupLobby" ) );
	lobbyState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "TheGame_ShutdownLobby" ) );

//	m_gameStateMachine.CreateState( "TheGameState_PreMatch" ); // Where you'll choose your tear?
	State* playState = m_gameStateMachine.CreateState( "TheGameState_MidMatch" );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::StartupMidMatch >( "TheGame_StartupMidMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::UpdateMidMatch >( "TheGame_UpdateMidMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::RenderMidMatch >( "TheGame_RenderMidMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::ShutdownMidMatch >( "TheGame_ShutdownMidMatch", this );
	playState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "TheGame_StartupMidMatch" ) );
	playState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "TheGame_ShutdownMidMatch" ) );

	State* winState = m_gameStateMachine.CreateState( "TheGameState_PostMatch" );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::StartupPostMatch >( "TheGame_StartupPostMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::UpdatePostMatch >( "TheGame_UpdatePostMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::RenderPostMatch >( "TheGame_RenderPostMatch", this );
	TheEventSystem::Instance()->RegisterEvent<TheGame, &TheGame::ShutdownPostMatch >( "TheGame_ShutdownPostMatch", this );
	winState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "TheGame_StartupPostMatch" ) );
	winState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "TheGame_ShutdownPostMatch" ) );
	
	// WARNING: PLEASE REMEMBER TO ADD UPDATE EVENT TO THE THEGAME::UPDATE AND RENDER EVENT TO THEGAMERENDERER::CLIENTRENDER2D!

	titleState->AddInputTransition( KEY_TO_ENTER_STUFF, lobbyState );
	lobbyState->AddInputTransition( KEY_TO_EXIT_STUFF, titleState );
	playState->AddInputTransition( KEY_TO_EXIT_STUFF, lobbyState );
	winState->AddInputTransition( KEY_TO_EXIT_STUFF, lobbyState );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::RegisterEvents()
{
	//Here for an example of what to do in a future game project, if using this one for a future base:
/*
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::Render2D >( "TheGame::Render2D", this );
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::Render3D >( "TheGame::Render3D", this );
*/
	//This way TheEngine can just shout off at these string IDs which may be unregistered, and fail silently just fine.
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::RenderDebug2D >( "TheGame::RenderDebug2D", this );
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::RenderDebug3D >( "TheGame::RenderDebug3D", this );

	//Meanwhile, game code can pick whether it wants to use Render2D/Render3D or not, as in this SD6 netgame:
	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::ClientRender2D >( "TheGame::ClientRender2D", this );

	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::OnPlayerExploded_Handler >( "GameEvent_OnPlayerExploded", this );

	TheEventSystem::Instance()->RegisterEvent< TheGame, &TheGame::OnSpawnTimerEnded_Handler >( "GameEvent_OnSpawnTimerEnded", this );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::Startup()
{
	memset( m_playerControllers, 0, MAX_NUM_PLAYERS * sizeof( PlayerController* ) );
	memset( m_playerAvatars, 0, MAX_NUM_PLAYERS * sizeof( PlayerAvatar* ) );
	memset( m_playerAvatarNetObjects, 0, MAX_NUM_PLAYERS * sizeof( NetObject* ) );

	RegisterEvents();

	InitGameStates();

	RegisterConsoleCommands_General();
	RegisterConsoleCommands_Networking();

	InitSceneRendering();

	InitSpriteRenderer();

	InitSceneAudio();

	InitNetObjectSystem();
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::InitSceneAudio()
{
	//Refer to DFS1 project for loading sound banks from FMOD Studio.
//	SoundID rawSoundTest = g_theAudio->CreateOrGetSound( "Data/Audio/beek-blue_slide.it" ); //.wav, etc. also supported.
//	g_theAudio->PlaySound( rawSoundTest );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::SetupParticleSystems()
{
	//-----------------------------------------------------------------------------
	ParticleSystemDefinition* sparksSystem = ParticleSystem::Register( "Spark" ); //e.g. for where a bullet hits.

	//Add an emitter to it
	ParticleEmitterDefinition* sparksEmitter = sparksSystem->AddEmitter( ParticleEmitterDefinition::Create( "Cross Bullet", false ) );
	sparksEmitter->SetBlendState( PARTICLE_BLEND_STATE_ALPHA_ADDITIVE );
	sparksEmitter->m_initialSpawnCount = 10;
	sparksEmitter->m_secondsPerSpawn = 0.f;
	sparksEmitter->m_initialVelocity = Interval<Vector2f>( Vector2f( -5.5f ), Vector2f( 5.5f ) );
	sparksEmitter->m_lifetimeSeconds = Interval<float>( 1.f, 1.5f );
	sparksEmitter->m_initialScale = Interval<Vector2f>( Vector2f( 2.5f ), Vector2f( 2.7f ) );
	sparksEmitter->m_tint = Rgba( .1f, .8f, .1f, .2f ); //TRANSLUCENT GREEN SPARKS (positive feedback that they died).
	ASSERT_OR_DIE( !sparksEmitter->IsLooping(), nullptr );

	//-----------------------------------------------------------------------------
	ParticleSystemDefinition* boomSystem = ParticleSystem::Register( "Explosion" );

	//Add an emitter to it
	ParticleEmitterDefinition* boomEmitter = boomSystem->AddEmitter( ParticleEmitterDefinition::Create( "boom_particle", true ) );
	boomEmitter->SetBlendState( PARTICLE_BLEND_STATE_ALPHA_ADDITIVE );
	boomEmitter->m_initialSpawnCount = 100;
	boomEmitter->m_initialVelocity = Interval<Vector2f>( Vector2f( -5.5f ), Vector2f( 5.5f ) );
	boomEmitter->m_secondsPerSpawn = 0.f;
	boomEmitter->m_lifetimeSeconds = Interval<float>( 1.f, 1.5f );
	boomEmitter->m_initialScale = Interval<Vector2f>( Vector2f( .5f ), Vector2f( .7f ) );
	boomEmitter->m_tint = Rgba( .8f, .8f, .1f, .2f ); //TRANSLUCENT YELLOW EXPLOSION.
	ASSERT_OR_DIE( !boomEmitter->IsLooping(), nullptr );

	//-----------------------------------------------------------------------------
	ParticleSystemDefinition* smokeSystem = ParticleSystem::Register( "Smoke" ); //e.g. left post-explosion.

	//Add an emitter to it
	ParticleEmitterDefinition* smokeEmitter = smokeSystem->AddEmitter( ParticleEmitterDefinition::Create( "soft_particle", false ) );
	smokeEmitter->SetBlendState( PARTICLE_BLEND_STATE_ALPHA );
	smokeEmitter->m_initialSpawnCount = 10;
	smokeEmitter->m_secondsPerSpawn = .2f;
	smokeEmitter->m_lifetimeSeconds = Interval<float>( .75f, 1.f );
	smokeEmitter->m_initialVelocity = Interval<Vector2f>( Vector2f( -1.5f ), Vector2f( 1.5f ) );
	smokeEmitter->m_initialScale = Interval<Vector2f>( Vector2f( .2f ), Vector2f( .4f ) );
	smokeEmitter->m_tint = Rgba( .4f, .4f, .42f ); //GRAY SMOKE.
	ASSERT_OR_DIE( smokeEmitter->IsLooping(), nullptr );
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::InitSpriteRenderer()
{
	SpriteRenderer::LoadAllSpriteResources();

	SpriteRenderer::Startup( s_playerCamera2D, Rgba::DARK_GRAY, true );

	SpriteRenderer::SetDefaultVirtualSize( 15 );
	/*
		Suggested to make 1 unit == size of the player's ship sprite.
		HOWEVER, do NOT base virtual or import size on an asset--these should both be decided prior to making assets/game.
		Nuclear Throne example:
			- 240p import size, implying a pixel is 1/240th of the screen (or there are 240 rows of pixels in the window).
			- 15 virtual size, implying a 15x15 grid of virtual units is what the screen can see in total.
			-> Therefore, a single virtual unit takes up 240/15 px == 16 (by 16) px on screen.
			-> Therefore, the average size of a sprite, e.g. the player sprite, should also be 16x16 px.
	*/
	SpriteRenderer::SetImportSize( 240 ); // # vertical lines of resolution.

	SpriteRenderer::UpdateAspectRatio( (float)g_theRenderer->GetScreenWidth(), (float)g_theRenderer->GetScreenHeight() ); //SET THIS at any moment the resolution is changed in the options.

	SetupParticleSystems();

	//InitAndRegisterSprites();
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::InitAndRegisterSprites()
{
	Sprite* spriteTest = Sprite::Create( "Test", WorldCoords2D::ZERO );
	spriteTest->SetLayerID( MAIN_LAYER_ID );
	spriteTest->Enable();

	AnimatedSprite* changingBullet = AnimatedSprite::Create( "Expanding Bullet", WorldCoords2D( -5.f, 0.f ) );
	changingBullet->SetLayerID( MAIN_LAYER_ID );
	changingBullet->Enable();
}


//--------------------------------------------------------------------------------------------------------------
void TheGame::Shutdown()
{
	TheGameDebug::Shutdown();
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::OnPlayerExploded_Handler( EngineEvent* eventContext )
{
	WorldCoords2D blastPosition = dynamic_cast<GameEventPlayerExploded*>( eventContext )->m_position;
	ParticleSystem::Play( "Explosion", VFX_LAYER_ID, blastPosition );

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::OnSpawnTimerEnded_Handler( EngineEvent* )
{
	if ( !IsMyConnectionHosting() )
	{
		ERROR_RECOVERABLE( "Only host should be in here!" );
		return SHOULD_NOT_UNSUB;
	}

	static int lastTearColorUsed = GetRandomIntLessThan( (int)NUM_PRIMARY_TEAR_COLORS );
	if ( m_teardropEnemies.size() + 1 <= MAX_ALIVE_ENEMIES ) //Cap on spawns that doesn't prevent timer from restarting (and if we took damage, slowing spawn delay).
	{
		TeardropNPC* newEnemy = new TeardropNPC( (PrimaryTearColor)lastTearColorUsed );
		NetObjectSystem::NetSyncObject( newEnemy, NETOBJ_NPC_TEARDROP, nullptr );
		//No owner. Not in ctor to decouple logic.

		m_teardropEnemies.push_back( newEnemy ); //ACTUAL SPAWNING.

		//Since the colors are RGB, pick one of the two not used last time.
		int enumStep = GetRandomChance( .5f ) ? 1 : 2; //Walk 1 or 2 down the enum (with below wrapping) from current value.
		lastTearColorUsed = WrapNumberWithinCircularRange( lastTearColorUsed + enumStep, 0, NUM_PRIMARY_TEAR_COLORS );
	}

	//Grab the highest player's sword level and use it to shrink enemy spawn times.
	int8_t highestSwordLevel = GetHighestPlayerSwordLevel();
	m_spawnTimer.SetEndTimeSeconds( MAX_SECONDS_BETWEEN_SPAWNS * ( m_hostSelectedGoalTearCount - highestSwordLevel ) / m_hostSelectedGoalTearCount );
	m_spawnTimer.Start();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::AddEnemy( TeardropNPC* newEnemy )
{
	if ( m_teardropEnemies.size() + 1 > MAX_ALIVE_ENEMIES )
		return false;

	m_teardropEnemies.push_back( newEnemy );
	return true;
}


//--------------------------------------------------------------------------------------------------------------
bool TheGame::RemoveEnemy( TeardropNPC* existingEnemy )
{
	unsigned int index;
	for ( index = 0; index < m_teardropEnemies.size(); index++ )
		if ( m_teardropEnemies[ index ] == existingEnemy )
			break;

	if ( index == m_teardropEnemies.size() )
		return false;
	
	m_teardropEnemies.erase( m_teardropEnemies.begin() + index );
	return true;
}
