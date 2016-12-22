#include "Engine/Physics/Forces.hpp"
#include "Engine/Physics/LinearDynamicsState2D.hpp"
#include "Engine/Physics/LinearDynamicsState3D.hpp"
#include "Engine/Math/MathUtils.hpp"


//--------------------------------------------------------------------------------------------------------------
Vector2f GravityForce::CalcForceForStateAndMass( const LinearDynamicsState2D* lds, float mass ) const
{
	return CalcDirectionForState( lds ) * CalcMagnitudeForState( lds ) * mass;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f GravityForce::CalcForceForStateAndMass( const LinearDynamicsState3D* lds, float mass ) const
{
	return CalcDirectionForState( lds ) * CalcMagnitudeForState( lds ) * mass;
}


//--------------------------------------------------------------------------------------------------------------
Vector2f ConstantWindForce::CalcForceForStateAndMass( const LinearDynamicsState2D* lds, float /*mass*/ ) const
{
	Vector2f windVector = CalcDirectionForState( lds ) * CalcMagnitudeForState( lds );
	Vector2f undampedWindForce = lds->GetVelocity() - windVector;

	return undampedWindForce * -m_dampedness;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f ConstantWindForce::CalcForceForStateAndMass( const LinearDynamicsState3D* lds, float /*mass*/ ) const
{
	Vector3f windVector = CalcDirectionForState( lds ) * CalcMagnitudeForState( lds );
	Vector3f undampedWindForce = lds->GetVelocity() - windVector;

	return undampedWindForce * -m_dampedness;
}


//--------------------------------------------------------------------------------------------------------------
float WormholeForce::CalcMagnitudeForState( const LinearDynamicsState2D* lds ) const
{
	return m_magnitude * lds->GetPosition().CalcFloatLength(); //MAGIC: Further from origin you move == stronger wind.
}


//--------------------------------------------------------------------------------------------------------------
float WormholeForce::CalcMagnitudeForState( const LinearDynamicsState3D* lds ) const
{
	return m_magnitude * lds->GetPosition().CalcFloatLength(); //MAGIC: Further from origin you move == stronger wind.
}


//--------------------------------------------------------------------------------------------------------------
Vector2f WormholeForce::CalcDirectionForState( const LinearDynamicsState2D* lds ) const
{
	return m_center.xy() - lds->GetPosition(); //MAGIC: Direction sends you back toward m_center.
}


//--------------------------------------------------------------------------------------------------------------
Vector3f WormholeForce::CalcDirectionForState( const LinearDynamicsState3D* lds ) const
{
	return m_center - lds->GetPosition(); //MAGIC: Direction sends you back toward m_center.
}


//--------------------------------------------------------------------------------------------------------------
Vector2f WormholeForce::CalcForceForStateAndMass( const LinearDynamicsState2D* lds, float /*mass*/ ) const
{
	Vector2f windVector = CalcDirectionForState( lds ) * CalcMagnitudeForState( lds );
	Vector2f undampedWindForce = lds->GetVelocity() - windVector;

	return undampedWindForce * -m_dampedness;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f WormholeForce::CalcForceForStateAndMass( const LinearDynamicsState3D* lds, float /*mass*/ ) const
{
	Vector3f windVector = CalcDirectionForState( lds ) * CalcMagnitudeForState( lds );
	Vector3f undampedWindForce = lds->GetVelocity() - windVector;

	return undampedWindForce * -m_dampedness;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f SpringForce::CalcForceForStateAndMass( const LinearDynamicsState3D* lds, float /*mass*/ ) const
{
	Vector3f dampedVelocity = lds->GetVelocity() * -m_dampedness;
	Vector3f stiffenedPosition = lds->GetPosition() * -m_stiffness;

	return dampedVelocity + stiffenedPosition;
}

//--------------------------------------------------------------------------------------------------------------
Vector2f SpringForce::CalcForceForStateAndMass( const LinearDynamicsState2D* lds, float /*mass*/ ) const
{
	Vector2f dampedVelocity = lds->GetVelocity() * -m_dampedness;
	Vector2f stiffenedPosition = lds->GetPosition() * -m_stiffness;

	return dampedVelocity + stiffenedPosition;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f DebrisForce::CalcForceForStateAndMass( const LinearDynamicsState3D* lds, float mass ) const
{
	return CalcDirectionForState( lds ) * CalcMagnitudeForState( lds ) * mass;
}


//--------------------------------------------------------------------------------------------------------------
float DebrisForce::CalcMagnitudeForState( const LinearDynamicsState3D* lds ) const
{
	float upComponentForPosition = DotProduct( lds->GetPosition(), WORLD3D_UP );
	float upComponentForVelocity = DotProduct( lds->GetVelocity(), WORLD3D_UP );

	if ( upComponentForPosition < m_groundHeight )
		upComponentForPosition *= -10.f;
	else if ( upComponentForPosition > m_groundHeight && upComponentForVelocity < 0 )
		upComponentForPosition *= .65f;

	return upComponentForPosition;
}


//--------------------------------------------------------------------------------------------------------------
Vector3f DebrisForce::CalcDirectionForState( const LinearDynamicsState3D* lds ) const //The real problem: can't directly drag down velocity to zero.
{
	float upComponent = DotProduct( lds->GetPosition(), WORLD3D_UP );

	//If upComponent is negative, we're below ground and need to invert direction with slightly less magnitude.
	if ( upComponent < m_groundHeight )
		return WORLD3D_UP;
	else
		return WORLD3D_DOWN;
}
