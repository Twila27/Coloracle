#include "Engine/Physics/LinearDynamicsState2D.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/EngineCommon.hpp"
#include "Engine/Physics/Forces.hpp"
#include "Engine/Physics/EphanovParticle.hpp"


//--------------------------------------------------------------------------------------------------------------
LinearDynamicsState2D::~LinearDynamicsState2D()
{
	auto forceIterEnd = m_forces.cend();
	for ( auto forceIter = m_forces.cbegin(); forceIter != forceIterEnd; ++forceIter )
	{
		Force* currentForce = *forceIter;
		if ( currentForce != nullptr )
		{
			delete currentForce;
			currentForce = nullptr;
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void LinearDynamicsState2D::StepWithForwardEuler( float mass, float deltaSeconds )
{
	//Forward Euler: state_next := state_prev + dt * d(state_prev)/dt.

	LinearDynamicsState2D dState = dStateForMass( mass );

	m_position += dState.m_position * deltaSeconds; //x := x + (veloc * dt)
	m_velocity += dState.m_velocity * deltaSeconds; //v := v + (accel * dt)
}

void LinearDynamicsState2D::StepWithVerlet( float mass, float deltaSeconds )
{
	//https://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet - to do away with the x_(t-1) at t=0 problem.
	static Vector2f prevAccel = 0.f;

	LinearDynamicsState2D dState = dStateForMass( mass );

	m_position += ( m_velocity*deltaSeconds ) + ( dState.m_velocity*.5f*deltaSeconds*deltaSeconds ); //x := x + v*dt + .5*a*dt*dt.
	m_velocity += ( prevAccel + dState.m_velocity )*.5f*deltaSeconds; //v := v + .5*(a + a_next)*dt.
	prevAccel = dState.m_velocity;
}


//--------------------------------------------------------------------------------------------------------------
LinearDynamicsState2D LinearDynamicsState2D::dStateForMass( float mass ) const
{
	Vector2f acceleration = CalcNetForceForMass( mass ) * ( 1.f / mass ); //Newton's 2nd law, rearranged.

	return LinearDynamicsState2D( m_velocity, acceleration );
}


//--------------------------------------------------------------------------------------------------------------
Vector2f LinearDynamicsState2D::CalcNetForceForMass( float mass ) const
{
	Vector2f netForce( 0.f );

	auto forceIterEnd = m_forces.cend();
	for ( auto forceIter = m_forces.cbegin(); forceIter != forceIterEnd; ++forceIter )
	{
		Force* currentForce = *forceIter;
		netForce += currentForce->CalcForceForStateAndMass( this, mass );
	}

	return netForce;
}


//--------------------------------------------------------------------------------------------------------------
void LinearDynamicsState2D::ClearForces( bool keepGravity /*= true*/ )
{
	if ( m_forces.size() == 0 )
		return;

	for ( auto forceIter = m_forces.begin(); forceIter != m_forces.end(); )
	{
		Force* currentForce = *forceIter;
		if ( keepGravity && ( dynamic_cast<GravityForce*>( currentForce ) != nullptr ) )
		{
			++forceIter;
			continue;
		}

		if ( currentForce != nullptr )
		{
			delete currentForce;
			currentForce = nullptr;
			forceIter = m_forces.erase( forceIter );
		}
		else ++forceIter;
	}
	m_forces.clear();
}
