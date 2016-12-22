#include "Game/Game Entities/TeardropNPC.hpp"
#include "Engine/Renderer/AnimatedSprite.hpp"
#include "Engine/Renderer/Rgba.hpp"
#include "Engine/Math/MathUtils.hpp"


//--------------------------------------------------------------------------------------------------------------
TeardropNPC::TeardropNPC( PrimaryTearColor color )
	: m_color( color )
{
	Rgba spriteTint;
	switch ( color )
	{
		case PRIMARY_TEAR_COLOR_RED: spriteTint = Rgba::RED * Rgba::GRAY;  break;
		case PRIMARY_TEAR_COLOR_GREEN: spriteTint = Rgba::GREEN * Rgba::GRAY;  break;
		case PRIMARY_TEAR_COLOR_BLUE: spriteTint = Rgba::BLUE * Rgba::GRAY;  break;
	}

	float directionX = GetRandomChance( .5 ) ? -1.f : 1.f;
	float directionY = GetRandomChance( .5 ) ? -1.f : 1.f;
	m_sprite = AnimatedSprite::Create(
		"Slime",
		WorldCoords2D( GetRandomFloatInRange( 3.1f, 13.1f ) * directionX, GetRandomFloatInRange( 3.1f, 7.2f ) * directionY ),
		spriteTint );
	m_sprite->SetLayerID( MAIN_LAYER_ID, "Main" );
	m_sprite->Enable();
}


//--------------------------------------------------------------------------------------------------------------
TeardropNPC::~TeardropNPC()
{
	m_sprite->Disable();
	delete m_sprite;
	m_sprite = nullptr;
}


//--------------------------------------------------------------------------------------------------------------
void TeardropNPC::Update( float deltaSeconds )
{
	static float lerpFactor = 0.f;
	static bool negateDirection = true;


	if ( lerpFactor + deltaSeconds > 1.f )
		negateDirection = true;
	else if ( lerpFactor - deltaSeconds < 0.f )
		negateDirection = false;

	lerpFactor += deltaSeconds * ( negateDirection ? -1.f : 1.f );

	Vector2f& scale = m_sprite->m_transform.m_scale;
	scale = Lerp( Vector2f( 0.9f ), Vector2f( 1.1f ), lerpFactor );
}


//--------------------------------------------------------------------------------------------------------------
AABB2f TeardropNPC::GetBounds() const
{
	return m_sprite->GetVirtualBoundsInWorld();
}


//--------------------------------------------------------------------------------------------------------------
Vector2f TeardropNPC::GetPosition() const
{
	return m_sprite->m_transform.m_position;
}


//--------------------------------------------------------------------------------------------------------------
void TeardropNPC::SetPosition( const Vector2f& pos )
{
	m_sprite->m_transform.m_position = pos;
	m_rigidbody.SetPosition( pos );
}