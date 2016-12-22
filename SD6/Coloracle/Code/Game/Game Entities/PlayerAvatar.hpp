#pragma once

#include "Engine/Math/Vector2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Networking/NetConnection.hpp"
#include "Engine/Physics/LinearDynamicsState2D.hpp"
#include "Engine/Tools/StateMachine/StateMachine.hpp"
#include "Game/GameCommon.hpp"


//-----------------------------------------------------------------------------
class PlayerController;
class AnimatedSprite;
class Sprite;
class EngineEvent;


//-----------------------------------------------------------------------------
class PlayerAvatar
{
public:
	PlayerAvatar( PlayerController* controller );
	~PlayerAvatar();

	PlayerController* GetController() const { return m_myController; }
	AABB2f GetBounds() const;
	Vector2f GetPosition() const;
	Vector2f GetVelocity() const { return m_rigidbody.GetVelocity(); }
	int8_t GetSwordLevel() const { return m_swordLevel; }
	PrimaryTearColor GetSwordColorAt( int8_t index ) const;
	PrimaryTearColor GetSwordColor() const { return GetSwordColorAt( m_swordLevel - 1 ); }
	std::string GetSwordColorName() const;
	Rgba GetSpriteColor() const;
	Sprite* GetSprite() { return m_sprite; }
	LinearDynamicsState2D* GetRigidbody() { return &m_rigidbody; }

	void SetSwordLevel( int8_t newLevel ) { if ( newLevel >= 0 && newLevel <= MAX_NUM_TEAR_COUNT ) m_swordLevel = newLevel; }
	void SetSwordColorAt( int8_t index, PrimaryTearColor newColor ) { if ( index < MAX_NUM_TEAR_COUNT ) m_swordColorStack[ index ] = newColor; }
	void SetPosition( const Vector2f& pos );
	void SetVelocity( const Vector2f& vel ) { m_rigidbody.SetVelocity( vel ); UpdateAnimState(); }
	void SetColor( const Rgba& newColor );
	void SetColorFromPrimaryTearColor( PrimaryTearColor newColor );
	void AddSwordColor( PrimaryTearColor newColor );
	void LoseSwordColor();

	void Update( float deltaSeconds );
	void UpdateAnimState();

private:
	bool StartIdle_Handler( EngineEvent* );
	bool StartMove_Handler( EngineEvent* );
	bool StartJump_Handler( EngineEvent* );
	bool StartFall_Handler( EngineEvent* );
	bool EndAnimationState_Handler( EngineEvent* );
	void EnforceWorldPerimeter();

	PlayerController* m_myController;
	Sprite* m_sprite; //Switches between below anims.
	StateMachine m_animationState;
	LinearDynamicsState2D m_rigidbody;

	int8_t m_swordLevel; //Use to access below array's end, since #colors == sword level.
	int8_t m_swordColorStack[ MAX_NUM_TEAR_COUNT ]; //0 is the hilt/first, last is the tip, using array for easier networking.
		//int to allow use of -1 for sentinel values.

	Sprite* m_idleSprite;
	AnimatedSprite* m_moveAnim;
	Sprite* m_jumpSprite;
	Sprite* m_fallSprite;
};
