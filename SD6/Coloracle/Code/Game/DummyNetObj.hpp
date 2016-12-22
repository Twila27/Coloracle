#pragma once

#include "Engine/Renderer/Sprite.hpp"
#include "Engine/Math/Vector2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Networking/NetConnection.hpp"

#include "Game/TheApp.hpp"


//-----------------------------------------------------------------------------
struct DummyNetObj //ObjectAS3 in notes.
{
	static DummyNetObj* DummyNetObj::Create( NetConnectionIndex index ) { return new DummyNetObj( index ); }

	DummyNetObj( uint8_t index )
		: netOwnerIndex( index )
		, sprite( Sprite::Create( "Player", WorldCoords2D( GetRandomFloatInRange( -5.f, 5.f ), GetRandomFloatInRange( -5.f, 5.f ) ), Rgba::GetRandomColor() ) )
	{
		sprite->SetLayerID( MAIN_LAYER_ID, "Main" );
		sprite->Enable();
	}
	~DummyNetObj()
	{
		sprite->Disable();
		delete sprite;
		sprite = nullptr;
	}
	Vector2f GetPosition() const { return sprite->m_transform.m_position; }
	void SetPosition( const Vector2f& pos ) { sprite->m_transform.m_position = pos; }
	void OffsetPosition( const Vector2f& pos ) { sprite->m_transform.m_position += pos; }
	uint8_t netOwnerIndex;
	Sprite* sprite;
};
