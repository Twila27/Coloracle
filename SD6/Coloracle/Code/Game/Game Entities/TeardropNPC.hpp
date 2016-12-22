#pragma once

#include "Game/GameCommon.hpp"
#include "Engine/Physics/LinearDynamicsState2D.hpp"

//-----------------------------------------------------------------------------
class AnimatedSprite;


//-----------------------------------------------------------------------------
class TeardropNPC
{
public:
	TeardropNPC( PrimaryTearColor );
	~TeardropNPC();
	void Update( float deltaSeconds ); //Just be still for now.
	AABB2f GetBounds() const;
	PrimaryTearColor GetColor() const { return m_color; }
	Vector2f GetPosition() const;
	void SetPosition( const Vector2f& newPos );

private:
	PrimaryTearColor m_color;
	AnimatedSprite* m_sprite;
	LinearDynamicsState2D m_rigidbody;
};
