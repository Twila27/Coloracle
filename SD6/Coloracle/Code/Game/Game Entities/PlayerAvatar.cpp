#include "Game/Game Entities/PlayerAvatar.hpp"
#include "Game/Game Entities/PlayerController.hpp"
#include "Engine/Input/TheInput.hpp"
#include "Game/TheGame.hpp"
#include "Engine/Math/Camera2D.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Renderer/AnimatedSprite.hpp"
#include "Engine/Physics/Forces.hpp"


//--------------------------------------------------------------------------------------------------------------
PlayerAvatar::PlayerAvatar( PlayerController* controller )
	: m_myController( controller )
	, m_swordLevel( 0 )
	, m_animationState( false )
{
	const char* colorStr = controller->GetColorString();
	Rgba tint = Rgba::WHITE;
	tint.alphaOpacity = 207;
	WorldCoords2D pos = WorldCoords2D( GetRandomFloatInRange( -5.f, 5.f ), 0.f );

	m_sprite = m_idleSprite = Sprite::Create( Stringf( "%sPlayer_Idle", colorStr ), pos, tint );
	m_idleSprite->SetLayerID( MAIN_LAYER_ID, "Main" );
	
	m_moveAnim = AnimatedSprite::Create( Stringf( "%sPlayer_Move", colorStr ), pos, tint );
	m_moveAnim->SetLayerID( MAIN_LAYER_ID, "Main" );

	m_jumpSprite = Sprite::Create( Stringf( "%sPlayer_Jump", colorStr ), pos, tint );
	m_jumpSprite->SetLayerID( MAIN_LAYER_ID, "Main" );

	m_fallSprite = Sprite::Create( Stringf( "%sPlayer_Fall", colorStr ), pos, tint );
	m_fallSprite->SetLayerID( MAIN_LAYER_ID, "Main" );

	//Probably need to do attack as separate somehow...
	State* idleState = m_animationState.CreateState( "Idle", true );
	idleState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "PlayerAvatar_StartIdle", this ) );
	TheEventSystem::Instance()->RegisterEvent< PlayerAvatar, &PlayerAvatar::StartIdle_Handler >( "PlayerAvatar_StartIdle", this );
	idleState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "PlayerAvatar_EndAnimationState", this ) ); 

	State* moveState = m_animationState.CreateState( "Move" );
	moveState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "PlayerAvatar_StartMove", this ) );
	TheEventSystem::Instance()->RegisterEvent< PlayerAvatar, &PlayerAvatar::StartMove_Handler >( "PlayerAvatar_StartMove", this );
	moveState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "PlayerAvatar_EndAnimationState", this ) );

	State* jumpState = m_animationState.CreateState( "Jump" );
	jumpState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "PlayerAvatar_StartJump", this ) );
	TheEventSystem::Instance()->RegisterEvent< PlayerAvatar, &PlayerAvatar::StartJump_Handler >( "PlayerAvatar_StartJump", this );
	jumpState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "PlayerAvatar_EndAnimationState", this ) );

	State* fallState = m_animationState.CreateState( "Fall" );
	fallState->AddStateCommand( STATE_STAGE_ENTERING, StateCommand( "PlayerAvatar_StartFall", this ) );
	TheEventSystem::Instance()->RegisterEvent< PlayerAvatar, &PlayerAvatar::StartFall_Handler >( "PlayerAvatar_StartFall", this );
	fallState->AddStateCommand( STATE_STAGE_EXITING, StateCommand( "PlayerAvatar_EndAnimationState", this ) );

	//Referenced by several states above on exit, but only need to register once:
	TheEventSystem::Instance()->RegisterEvent< PlayerAvatar, &PlayerAvatar::EndAnimationState_Handler >( "PlayerAvatar_EndAnimationState", this );

	memset( m_swordColorStack, UNUSED_SWORD_COLOR_STACK_LAYER, sizeof( m_swordColorStack[ 0 ] ) * MAX_NUM_TEAR_COUNT );
}


//--------------------------------------------------------------------------------------------------------------
PlayerAvatar::~PlayerAvatar()
{
	m_sprite = nullptr;

	//Could've exited in multiple states.
	delete m_jumpSprite;
	m_jumpSprite = nullptr;

	delete m_moveAnim;
	m_moveAnim = nullptr;

	delete m_idleSprite;
	m_idleSprite = nullptr;

	TheEventSystem::Instance()->UnregisterSubscriber< PlayerAvatar, &PlayerAvatar::StartIdle_Handler >( "PlayerAvatar_StartIdle", this );
	TheEventSystem::Instance()->UnregisterSubscriber< PlayerAvatar, &PlayerAvatar::StartMove_Handler >( "PlayerAvatar_StartMove", this );
	TheEventSystem::Instance()->UnregisterSubscriber< PlayerAvatar, &PlayerAvatar::StartJump_Handler >( "PlayerAvatar_StartJump", this );
	TheEventSystem::Instance()->UnregisterSubscriber< PlayerAvatar, &PlayerAvatar::StartFall_Handler >( "PlayerAvatar_StartFall", this );
	TheEventSystem::Instance()->UnregisterSubscriber< PlayerAvatar, &PlayerAvatar::EndAnimationState_Handler >( "PlayerAvatar_EndAnimationState", this );
}


//--------------------------------------------------------------------------------------------------------------
AABB2f PlayerAvatar::GetBounds() const
{
	return m_sprite->GetVirtualBoundsInWorld();
}


//--------------------------------------------------------------------------------------------------------------
Vector2f PlayerAvatar::GetPosition() const
{
	return m_sprite->GetPosition();
}


//--------------------------------------------------------------------------------------------------------------
PrimaryTearColor PlayerAvatar::GetSwordColorAt( int8_t index ) const
{
	if ( index >= 0 && index < MAX_NUM_TEAR_COUNT )
		return (PrimaryTearColor)m_swordColorStack[ index ];
	else
		return NUM_PRIMARY_TEAR_COLORS;
}


//--------------------------------------------------------------------------------------------------------------
std::string PlayerAvatar::GetSwordColorName() const
{
	if ( m_swordLevel == 0 )
		return "None";

	switch ( GetSwordColor() )
	{
		case PRIMARY_TEAR_COLOR_RED: return "Red";
		case PRIMARY_TEAR_COLOR_GREEN: return "Green";
		case PRIMARY_TEAR_COLOR_BLUE: return "Blue";
	}
	
	return "Error";
}


//--------------------------------------------------------------------------------------------------------------
Rgba PlayerAvatar::GetSpriteColor() const
{
	return m_sprite->GetTint();
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::SetPosition( const Vector2f& pos )
{
	m_sprite->m_transform.m_position = pos;
	m_rigidbody.SetPosition( pos );
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::SetColor( const Rgba& newColor )
{
	m_sprite->SetTint( newColor );
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::SetColorFromPrimaryTearColor( PrimaryTearColor newColor )
{
	Rgba finalTint;
	switch ( newColor )
	{
		case PRIMARY_TEAR_COLOR_RED: finalTint = Rgba::RED; break;
		case PRIMARY_TEAR_COLOR_GREEN: finalTint = Rgba::GREEN; break;
		case PRIMARY_TEAR_COLOR_BLUE: finalTint = Rgba::BLUE; break;
		default: finalTint = Rgba::WHITE; break; //Remove tint.
	}
	finalTint.alphaOpacity = 207; //Make the player look ghostly.
	SetColor( finalTint );
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::AddSwordColor( PrimaryTearColor newColor )
{
	if ( ( m_swordLevel + 1 ) < MAX_NUM_TEAR_COUNT )
	{
		m_swordColorStack[ m_swordLevel ] = (int)newColor;
		++m_swordLevel;
		SetColorFromPrimaryTearColor( newColor );
	}
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::LoseSwordColor()
{
	if ( ( m_swordLevel - 1 ) >= 0 )
	{
		m_swordColorStack[ m_swordLevel ] = UNUSED_SWORD_COLOR_STACK_LAYER; //Sentinel value.
		--m_swordLevel;
	}
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::Update( float deltaSeconds )
{
	m_animationState.Update();

	const float PLAYER_MOVEMENT_SPEED = 450.f;
	float deltaMove = ( PLAYER_MOVEMENT_SPEED * deltaSeconds );

	bool upPressed = g_theInput->IsKeyDown( KEY_TO_MOVE_UP_2D );
	bool downPressed = g_theInput->IsKeyDown( KEY_TO_MOVE_DOWN_2D );
	bool rightPressed = g_theInput->IsKeyDown( KEY_TO_MOVE_RIGHT_2D );
	bool leftPressed = g_theInput->IsKeyDown( KEY_TO_MOVE_LEFT_2D );

//	if ( downPressed )
//		SetVelocity( -Vector2f::UNIT_Y * deltaMove );

	if ( rightPressed )
		SetVelocity( Vector2f::UNIT_X * deltaMove );

	if ( leftPressed )
		SetVelocity( -Vector2f::UNIT_X * deltaMove );

	if ( !leftPressed && !rightPressed )
		SetVelocity( GetVelocity() * Vector2f::UNIT_Y ); //Zero out horizontal velocity.
	
	if ( upPressed )
	{
		if ( !m_rigidbody.HasForces() )
			m_rigidbody.AddForce( new ConstantWindForce( 20.f, Vector3f::UNIT_Y ) );
		else
			m_rigidbody.ClearForces();
	}
	if ( downPressed )
	{
		if ( !m_rigidbody.HasForces() )
			m_rigidbody.AddForce( new GravityForce( 60.f, Vector3f::UNIT_Y * -1.f ) );
		else
			m_rigidbody.ClearForces();
	}
	if ( !upPressed && !downPressed )
	{
		m_rigidbody.ClearForces();
		SetVelocity( Vector2f( GetVelocity().x, 0.f ) ); //Kill any built-up velocity from forces.
	}

	if ( g_theInput->IsKeyDown( KEY_TO_PLAY_EXPLOSION ) || g_theInput->IsButtonDown( BUTTON_TO_PLAY_EXPLOSION ) )
	{
		GameEventPlayerExploded ev;
		ev.m_position = GetPosition();
		TheEventSystem::Instance()->TriggerEvent( "GameEvent_OnPlayerExploded", &ev );
		g_theGame->ShouldSendAttackMessage();
	}

	//--------------------------------------------------------------------------------------------------------------

	m_rigidbody.StepWithForwardEuler( .1f, deltaSeconds );
	SetPosition( m_rigidbody.GetPosition() );
	
	EnforceWorldPerimeter();
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::EnforceWorldPerimeter()
{
	//Enforcing perimeter:
	Vector2f& pos = m_sprite->m_transform.m_position;
	if ( pos.x > 15.f )
		pos.x = 15.f;
	else if ( pos.x < -15.f )
		pos.x = -15.f;
	if ( pos.y > 9.f )
		pos.y = 9.f;
	else if ( pos.y < -9.f )
		pos.y = -9.f;

	SetPosition( pos ); //Keep physics state updated with sprite.
}


//--------------------------------------------------------------------------------------------------------------
void PlayerAvatar::UpdateAnimState()
{
	const Vector2f& vel = GetVelocity();

	m_animationState.SetCurrentState( ( vel.x == 0 ) ? "Idle" : "Move" );

	if ( vel.y > 0 )
		m_animationState.SetCurrentState( "Jump" );

	if ( vel.y < 0 )
		m_animationState.SetCurrentState( "Fall" );

	//By putting this second, we get that funny ability to look like we're falling/jumping while moving left/right.

	//Flip Sprite:
	if ( vel.x < 0 )
		m_sprite->m_transform.m_scale.x = -1.f;
	else if ( vel.x > 0 )
		m_sprite->m_transform.m_scale.x = 1.f;
	//else stay the same scale for idle.
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerAvatar::StartIdle_Handler( EngineEvent* ev )
{
	if ( !ev->IsIntendedForThis( this ) )
		return SHOULD_NOT_UNSUB;

	if ( m_sprite != nullptr )
	{
		m_idleSprite->m_transform = m_sprite->m_transform;
		m_idleSprite->SetTint( m_sprite->GetTint() );
	}

	m_sprite = m_idleSprite;

	if ( !m_sprite->IsEnabled() )
		m_sprite->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerAvatar::StartMove_Handler( EngineEvent* ev )
{
	if ( !ev->IsIntendedForThis( this ) )
		return SHOULD_NOT_UNSUB;

	if ( m_sprite != nullptr )
	{
		m_moveAnim->m_transform = m_sprite->m_transform;
		m_moveAnim->SetTint( m_sprite->GetTint() );
	}
	
	m_sprite = m_moveAnim;
	m_sprite->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerAvatar::StartJump_Handler( EngineEvent* ev )
{
	if ( !ev->IsIntendedForThis( this ) )
		return SHOULD_NOT_UNSUB;

	if ( m_sprite != nullptr )
	{
		m_jumpSprite->m_transform = m_sprite->m_transform;
		m_jumpSprite->SetTint( m_sprite->GetTint() );
	}

	m_sprite = m_jumpSprite;
	m_sprite->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerAvatar::StartFall_Handler( EngineEvent* ev )
{
	if ( !ev->IsIntendedForThis( this ) )
		return SHOULD_NOT_UNSUB;

	if ( m_sprite != nullptr )
	{
		m_fallSprite->m_transform = m_sprite->m_transform;
		m_fallSprite->SetTint( m_sprite->GetTint() );
	}

	m_sprite = m_fallSprite;
	m_sprite->Enable();

	return SHOULD_NOT_UNSUB;
}


//--------------------------------------------------------------------------------------------------------------
bool PlayerAvatar::EndAnimationState_Handler( EngineEvent* ev )
{
	if ( !ev->IsIntendedForThis( this ) )
		return SHOULD_NOT_UNSUB;

	if ( m_sprite->IsEnabled() )
		m_sprite->Disable();

	return SHOULD_NOT_UNSUB;
}
