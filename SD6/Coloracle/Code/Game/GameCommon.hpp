#pragma once


//-----------------------------------------------------------------------------
#include "Engine/EngineCommon.hpp"
#include "Engine/Renderer/DebugRenderCommand.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Math/Vector3.hpp"
#include "Engine/Math/Matrix4x4.hpp"

#include "Engine/Input/TheInput.hpp"


//-----------------------------------------------------------------------------
#define STATIC


//-----------------------------------------------------------------------------
//Gameplay
const int8_t MAX_NUM_TEAR_COUNT = 16;
const int MAX_ALIVE_ENEMIES = 16;
const float MAX_SECONDS_BETWEEN_SPAWNS = 3.f;
const int UNUSED_SWORD_COLOR_STACK_LAYER = -1;
enum PrimaryTearColor : int8_t
{
	PRIMARY_TEAR_COLOR_RED,
	PRIMARY_TEAR_COLOR_GREEN,
	PRIMARY_TEAR_COLOR_BLUE,
	NUM_PRIMARY_TEAR_COLORS
};


//-----------------------------------------------------------------------------
//Operating System
static const char* g_appName = "Client-Server Game (SD6 A6) by Benjamin Gibson";


//-----------------------------------------------------------------------------
//Networking
typedef uint8_t PlayerIndex;
#define INVALID_PLAYER_INDEX (0xFF)
#define MAX_NUM_PLAYERS (3)
static const int MAX_ADDR_STRLEN = 256;
enum GamePackets : uint8_t 
{
	NETMSG_GAME_BOOM = NetCoreMessageType::MAX_CORE_NETMSG_TYPES, //Start off at the end of the core messages.
	NETMSG_PING_INORDER,

	NETMSG_GAME_MATCH_STARTED,
	NETMSG_GAME_MATCH_ENDED,

	//SFS = SentFromServer (server should NEVER receive one), 
	//SFC = SentFromClient (client should NEVER receive one).
	NETMSG_GAME_PLAYER_REQUEST_SFC,
	NETMSG_GAME_PLAYER_JOINED_SFS,
	NETMSG_GAME_PLAYER_UPDATE_SFS,
	NETMSG_GAME_PLAYER_UPDATE_SFC,
	NETMSG_GAME_PLAYER_LEAVE_SFS,

	NETMSG_GAME_NETOBJ_CREATE_SFS,
	NETMSG_GAME_NETOBJ_UPDATE_SFC,
	NETMSG_GAME_NETOBJ_UPDATE_SFS,
	NETMSG_GAME_NETOBJ_DESYNC_SFS
};


//-----------------------------------------------------------------------------
//Entity System -- for now, not going to add GameState to the base project.
enum EntityType
{
	//Fill out via e.g. ENTITY_TYPE_PLAYER/ITEM/ENEMY/BULLET/NPC.
	NUM_ENTITY_TYPES 
};
typedef int EntityID;


//-----------------------------------------------------------------------------
//Naming Keys
static const char KEY_TO_MOVE_FASTER		= VK_SHIFT;

static const char KEY_TO_MOVE_FORWARD_3D	= 'W';
static const char KEY_TO_MOVE_BACKWARD_3D	= 'S';
static const char KEY_TO_MOVE_LEFT_3D		= 'A';
static const char KEY_TO_MOVE_RIGHT_3D		= 'D';
static const char KEY_TO_MOVE_UP_3D			= VK_SPACE;
static const char KEY_TO_MOVE_DOWN_3D		= 'X';

static const char KEY_TO_MOVE_LEFT_2D		= 'A';
static const char KEY_TO_MOVE_RIGHT_2D		= 'D';
static const char KEY_TO_MOVE_UP_2D			= 'W';
static const char KEY_TO_MOVE_DOWN_2D		= 'S';

static const char KEY_TO_PLAY_EXPLOSION		= VK_SPACE;

static const char KEY_TO_HOST_GAME			= 'H';
static const char KEY_TO_JOIN_GAME			= 'J';
static const char KEY_TO_START_GAME			= 'S'; 
static const char KEY_TO_ENTER_STUFF		= VK_ENTER;
static const char KEY_TO_EXIT_STUFF			= VK_ESCAPE;


//-----------------------------------------------------------------------------
//Naming Buttons
static const ControllerButtons BUTTON_TO_MOVE_FASTER	= ControllerButtons::A_BUTTON;
static const ControllerButtons BUTTON_TO_MOVE_UP		= ControllerButtons::Y_BUTTON;
static const ControllerButtons BUTTON_TO_MOVE_DOWN		= ControllerButtons::X_BUTTON;
static const ControllerButtons BUTTON_TO_PLAY_EXPLOSION	= ControllerButtons::B_BUTTON;


//-----------------------------------------------------------------------------
//Camera
static const float FLYCAM_SPEED_SCALAR = 8.f;
static const WorldCoords3D CAMERA3D_DEFAULT_POSITION = WorldCoords3D( -5.f, 0.f, 0.f );
static const WorldCoords2D CAMERA2D_DEFAULT_POSITION = WorldCoords2D::ZERO;


//-----------------------------------------------------------------------------
//HUD

static const Vector2f HUD_BOTTOM_LEFT_POSITION = Vector2f( 100.f, 27.f ); //In from left, up from bottom of screen.
static const float HUD_CROSSHAIR_RADIUS = 20.f;
static const float HUD_CROSSHAIR_THICKNESS = 4.f;


//-----------------------------------------------------------------------------
//Lighting

static const unsigned int LIGHTS_IN_SCENE_MAX = 2;


//-----------------------------------------------------------------------------
//Rendering
static const RenderLayerID UI_LAYER_ID = 1000;
static const RenderLayerID MAIN_LAYER_ID = 0;
static const RenderLayerID VFX_LAYER_ID = -100; //To not obscure player!
static const RenderLayerID BACKGROUND_LAYER_ID = -1000;
