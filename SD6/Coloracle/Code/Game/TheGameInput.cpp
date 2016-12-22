#include "Game/TheGame.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Math/Camera2D.hpp"
#include "Engine/Math/Camera3D.hpp"
#include "Engine/Input/TheInput.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/TheRenderer.hpp" //Used for piloting lights."

#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Renderer/Sprite.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC CameraMode TheGame::s_activeCameraMode = CAMERA_MODE_2D;
STATIC Camera3D* TheGame::s_playerCamera3D = new Camera3D( CAMERA3D_DEFAULT_POSITION );
STATIC Camera2D* TheGame::s_playerCamera2D = new Camera2D( CAMERA2D_DEFAULT_POSITION );
STATIC void TheGame::ToggleActiveCamType2D( Command& ) { s_playerCamera2D->m_usesPolarTranslations = !s_playerCamera2D->m_usesPolarTranslations; }
STATIC void TheGame::ToggleActiveCamType3D( Command& ) { s_playerCamera3D->m_usesPolarTranslations = !s_playerCamera3D->m_usesPolarTranslations; }
STATIC void TheGame::ToggleActiveCameraMode( Command& ) { s_activeCameraMode = ( s_activeCameraMode == CAMERA_MODE_2D ) ? CAMERA_MODE_3D : CAMERA_MODE_2D; }


//-----------------------------------------------------------------------------
const Camera3D* TheGame::GetActiveCamera3D() const
{
	return s_playerCamera3D;
}


//-----------------------------------------------------------------------------
Camera2D* TheGame::GetActiveCamera2D()
{
	return s_playerCamera2D;
}

//-----------------------------------------------------------------------------
void TheGame::UpdateFromKeyboard2D( float deltaSeconds )
{
	//Super speed handling.
	float speed = 0.f;
	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_FASTER ) )
		speed = FLYCAM_SPEED_SCALAR;
	else speed = 1.f;


	float deltaMove = ( speed * deltaSeconds );

	Vector2f camForwardXY = s_playerCamera2D->GetForwardXY();
	Vector2f camLeftXY = s_playerCamera2D->GetLeftXY();
	Vector2f& camPos = s_playerCamera2D->m_worldPosition;

	if ( s_playerCamera2D->m_usesPolarTranslations )
	{
		static float radius = camPos.CalcFloatLength();
		static float theta = atan2f( camPos.y, camPos.x );
		static float polarSpeed = 1.f;

		deltaMove *= polarSpeed;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_LEFT_2D ) )
			theta -= deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_RIGHT_2D ) )
			theta += deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_UP_2D ) )
			radius += deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_DOWN_2D ) )
			radius -= deltaMove;

		if ( g_theInput->IsKeyDown( 'Q' ) )
			polarSpeed += deltaMove;

		if ( g_theInput->IsKeyDown( 'E' ) )
			polarSpeed -= deltaMove;

		camPos = Vector2f( CosDegrees( theta ), SinDegrees( theta ) ) * radius;

		return;
	}
}


//-----------------------------------------------------------------------------
void TheGame::UpdateFromKeyboard3D( float deltaSeconds )
{
	//Super speed handling.
	float speed = 0.f;
	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_FASTER ) )
		speed = FLYCAM_SPEED_SCALAR;
	else speed = 1.f;


	float deltaMove = ( speed * deltaSeconds ); //speed is then in blocks per second!

	Vector3f camForwardXY = s_playerCamera3D->GetForwardXY();
	Vector3f camLeftXY = s_playerCamera3D->GetLeftXY();
	Vector3f& camPos = s_playerCamera3D->m_worldPosition;

	if ( g_theRenderer->IsPilotingLight() )
	{
		if ( g_theInput->IsKeyDown( 'I' ) )
			camPos += camForwardXY * deltaMove;

		if ( g_theInput->IsKeyDown( 'K' ) )
			camPos -= camForwardXY * deltaMove;

		if ( g_theInput->IsKeyDown( 'J' ) )
			camPos += camLeftXY * deltaMove;

		if ( g_theInput->IsKeyDown( 'L' ) )
			camPos -= camLeftXY * deltaMove;

		if ( g_theInput->IsKeyDown( 'U' ) )
			camPos.z += deltaMove; //Scaled by z-axis, so *1.0f.

		if ( g_theInput->IsKeyDown( 'O' ) )
			camPos.z -= deltaMove; //Scaled by z-axis, so *1.0f.

		return;
	}

	if ( s_playerCamera3D->m_usesPolarTranslations )
	{
		static float radius = camPos.CalcFloatLength();
		static float theta = atan2f( camPos.y, camPos.x );
		static float phi = acosf( camPos.z / radius );
		static float polarSpeed = 1.f;
		float radiusScaleAlongXY = CosDegrees( phi );

		deltaMove *= polarSpeed;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_FORWARD_3D ) )
			phi -= deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_BACKWARD_3D ) )
			phi += deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_LEFT_3D ) )
			theta -= deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_RIGHT_3D ) )
			theta += deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_UP_3D ) )
			radius += deltaMove;

		if ( g_theInput->IsKeyDown( KEY_TO_MOVE_DOWN_3D ) )
			radius -= deltaMove;

		if ( g_theInput->IsKeyDown( 'Q' ) )
			polarSpeed += deltaMove;

		if ( g_theInput->IsKeyDown( 'E' ) )
			polarSpeed -= deltaMove;

		camPos = Vector3f( radiusScaleAlongXY * CosDegrees( theta ), radiusScaleAlongXY * SinDegrees( theta ), -SinDegrees( phi ) ) * radius;

		return;
	}

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_FORWARD_3D ) )
		camPos += camForwardXY * deltaMove;

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_BACKWARD_3D ) )
		camPos -= camForwardXY * deltaMove;

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_LEFT_3D ) )
		camPos += camLeftXY * deltaMove;

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_RIGHT_3D ) )
		camPos -= camLeftXY * deltaMove;

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_UP_3D ) )
		camPos.z += deltaMove; //Scaled by z-axis, so *1.0f.

	if ( g_theInput->IsKeyDown( KEY_TO_MOVE_DOWN_3D ) )
		camPos.z -= deltaMove; //Scaled by z-axis, so *1.0f.
}


//-----------------------------------------------------------------------------
void TheGame::UpdateFromMouse2D()
{
	const float mouseSensitivityYaw = 0.044f;
//	s_playerCamera2D->m_orientationDegrees -= mouseSensitivityYaw * (float)g_theInput->GetCursorDeltaX();
}


//-----------------------------------------------------------------------------
void TheGame::UpdateFromMouse3D()
{
	//The following is basis-independent: mouse X controls yaw around camera/view up j-vector, mouse Y controls pitch around camera/view right i-vector.
	const float mouseSensitivityYaw = 0.044f;
	s_playerCamera3D->m_orientation.m_yawDegrees -= mouseSensitivityYaw * (float)g_theInput->GetCursorDeltaX();
	s_playerCamera3D->m_orientation.m_pitchDegrees += mouseSensitivityYaw * (float)g_theInput->GetCursorDeltaY();
	s_playerCamera3D->FixAndClampAngles(); //This function body, however, is is basis-dependent, as what i-j-k equals is defined by the basis.
}


//-----------------------------------------------------------------------------
void TheGame::UpdateCamera( float deltaSeconds )
{
	switch ( s_activeCameraMode )
	{
	case CAMERA_MODE_2D:
		UpdateFromKeyboard2D( deltaSeconds );
		UpdateFromMouse2D();
		s_playerCamera2D->SatisfyConstraints();
		break;
	case CAMERA_MODE_3D:
		UpdateFromKeyboard3D( deltaSeconds );
		UpdateFromMouse3D();
		break;
	}
}
