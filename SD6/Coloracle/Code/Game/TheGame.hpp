#pragma once
#include "Engine/Core/EngineEvent.hpp"
#include "Game/DummyNetObj.hpp"
#include "Engine/Tools/StateMachine/StateMachine.hpp"
#include "Engine/Time/Stopwatch.hpp"


//-----------------------------------------------------------------------------
class TheGame;
class Camera2D;
class Camera3D;
class Command;
class Sprite;
class ParticleSystem;
class PlayerController;
class PlayerAvatar;
struct NetObject;
class TextRenderer;
class TeardropNPC;


//-----------------------------------------------------------------------------
extern TheGame* g_theGame;


//-----------------------------------------------------------------------------
enum CameraMode
{
	CAMERA_MODE_2D,
	CAMERA_MODE_3D
};


//-----------------------------------------------------------------------------
struct GameEventPlayerExploded : public EngineEvent
{
	GameEventPlayerExploded() : EngineEvent( "GameEvent_OnPlayerExploded" ) {}
	WorldCoords2D m_position;
};


//-----------------------------------------------------------------------------
class TheGame
{
public:

	TheGame();
	~TheGame();

	void Startup();
	void Shutdown();
	
	void Update( float deltaSeconds );

	void RegisterConsoleCommands_General();
	void RegisterConsoleCommands_Networking();

	bool OnConnectionJoined_Handler( EngineEvent* );
	bool OnUpdateReceived_Handler( EngineEvent* );
	bool OnConnectionLeave_Handler( EngineEvent* );
	bool OnNetworkTick_Handler( EngineEvent* );

	bool OnSpawnTimerEnded_Handler( EngineEvent* eventContext );
	bool OnPlayerExploded_Handler( EngineEvent* );

	bool ClientRender2D( EngineEvent* );
	bool Render3D( EngineEvent* );
	bool Render2D( EngineEvent* );
	bool RenderDebug3D( EngineEvent* );
	bool RenderDebug2D( EngineEvent* );

	const Camera3D* GetActiveCamera3D() const;
	Camera2D* GetActiveCamera2D();
	static void ToggleActiveCamType3D( Command& );
	static void ToggleActiveCamType2D( Command& );
	static void ToggleActiveCameraMode( Command& );
	
	bool IsGameSessionRunning();
	bool IsMyConnectionHosting();
	NetSession* GetGameNetSession() { return m_gameSession; }
	void StartGameNetSession();
	void StopGameNetSession();
	void HostGame( const std::string& username );
		void StartHostListen();
		void StopHostListen();
	void JoinGame( const std::string& username, sockaddr_in& hostAddr );
	void LeaveGame();

	size_t GetControllersOnConnection( std::vector<PlayerController*>& out_players, NetConnection* );
	size_t GetAvatarsOnConnection( std::vector<PlayerAvatar*>& out_players, NetConnection* );
	size_t GetAvatarsOnMyConnection( std::vector<PlayerAvatar*>& out_players );
	PlayerController* PopUnconfirmedPlayerForNuonce( nuonce_t playerCreationNuonce );
	PlayerController* GetIndexedPlayerController( PlayerIndex index ) { return ( index >= MAX_NUM_PLAYERS ) ? nullptr : m_playerControllers[ index ]; }
	PlayerAvatar* GetIndexedPlayerAvatar( PlayerIndex index ) { return ( index >= MAX_NUM_PLAYERS ) ? nullptr : m_playerAvatars[ index ]; }
	void SetIndexedPlayerAvatar( PlayerIndex index, PlayerAvatar* player ) { if ( index < MAX_NUM_PLAYERS ) m_playerAvatars[ index ] = player; }
	bool ConnectionHasPlayers();
	bool ServerRegisterIndexForPlayer( PlayerController* newController );
	bool ClientRegisterPlayerAsIndex( PlayerController* controller, PlayerIndex serverAssignedIndex );
	void DestroyPlayer( PlayerIndex destroyedPlayer );

	void ServerStartMatch();
	void ServerCreateAndSendPlayerAvatars();
	void ClientStartMatch() { m_winningPlayerIndex = INVALID_PLAYER_INDEX; m_gameStateMachine.SetCurrentState( "TheGameState_MidMatch" ); }
	void ClientEndMatch( PlayerIndex winningPlayer );

	bool AddEnemy( TeardropNPC* newEnemy );
	bool RemoveEnemy( TeardropNPC* existingEnemy );
	void ShouldSendAttackMessage() { m_shouldSendAttackMessage = true; }
	StateNameStr GetCurrentStateName() const { return m_gameStateMachine.GetCurrentState()->GetName(); }
	CameraMode GetActiveCameraMode() const { return s_activeCameraMode; }


private:

	NetSession* m_gameSession;
	static CameraMode s_activeCameraMode;

	void RegisterEvents();

	void InitGameStates();
	void InitTitleState();
	void InitLobbyState();

	void InitSceneAudio();

	void SetupParticleSystems();

	void InitSpriteRenderer();
	void InitAndRegisterSprites();
	
	void InitNetObjectSystem();

	void InitSceneRendering();
	void CreateShaderPrograms();
	void CreateUniformValues();
	void CreateMeshRenderers();
	void CreateFBOs();
	void CreateSceneLights();

	bool RenderTitle( EngineEvent* );
	bool RenderLobby( EngineEvent* );
	bool RenderMidMatch( EngineEvent* );
	bool RenderPostMatch( EngineEvent* );

	bool StartupTitle( EngineEvent* );
	bool ShutdownTitle( EngineEvent* );
	bool StartupLobby( EngineEvent* );
	bool ShutdownLobby( EngineEvent* );
	bool StartupMidMatch( EngineEvent* );
	bool ShutdownMidMatch( EngineEvent* );
	bool StartupPostMatch( EngineEvent* );
	bool ShutdownPostMatch( EngineEvent* );

	bool UpdateTitle( EngineEvent* );
	bool UpdateLobby( EngineEvent* );
	bool UpdateMidMatch( EngineEvent* );
	PlayerAvatar* CheckWinCondition();
	void UpdateWorld( float deltaSeconds );
	bool UpdatePostMatch( EngineEvent* );
	void UpdateAudioListener();
	void UpdateCamera( float deltaSeconds );
	void UpdateFromKeyboard2D( float deltaSeconds );
	void UpdateFromMouse2D();
	void UpdateFromKeyboard3D( float deltaSeconds );
	void UpdateFromMouse3D();

	void ServerUpdateAuthoritativeSimulation( float deltaSeconds );
	void ClientUpdateUnauthoritativeSimulation( float deltaSeconds );
	PlayerAvatar* ServerCreateAvatarForPlayer( PlayerController* );
	bool ClientUpdateOwnedPlayers( float deltaSeconds );
	void ClientCreatePlayerRequest( NetConnection* );
	void SetIndexedPlayerController( PlayerIndex index, PlayerController* player ) { if ( index < MAX_NUM_PLAYERS ) m_playerControllers[ index ] = player; }
	nuonce_t GetNextCreationNuonce();
	void SendEveryPlayerControllerToConnection( NetConnection* );

	int8_t GetHighestPlayerSwordLevel();
	bool HandleCollision( PlayerAvatar* avatar, TeardropNPC* enemy );

	PlayerController* m_playerControllers[ MAX_NUM_PLAYERS ];
	PlayerAvatar* m_playerAvatars[ MAX_NUM_PLAYERS ];
	NetObject* m_playerAvatarNetObjects[ MAX_NUM_PLAYERS ];
	nuonce_t m_lastSentCreationNuonce;

	static Camera3D* s_playerCamera3D;
	static Camera2D* s_playerCamera2D;

	StateMachine m_gameStateMachine;
	Sprite* m_background;
	bool m_didClientRender;
	std::vector<PlayerController*> m_unconfirmedPlayers;

	int8_t m_hostSelectedGoalTearCount;
	bool m_shouldSendAttackMessage;
	PlayerIndex m_winningPlayerIndex;
	std::vector<TeardropNPC*> m_teardropEnemies;
	Stopwatch m_spawnTimer;
};
