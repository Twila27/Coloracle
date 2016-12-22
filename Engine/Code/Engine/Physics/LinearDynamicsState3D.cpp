#include "Engine/Physics/LinearDynamicsState3D.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/EngineCommon.hpp"
#include "Engine/Physics/Forces.hpp"
#include "Engine/Physics/EphanovParticle.hpp"


//--------------------------------------------------------------------------------------------------------------
LinearDynamicsState3D::~LinearDynamicsState3D()
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
void LinearDynamicsState3D::StepWithForwardEuler( float mass, float deltaSeconds )
{
	//Forward Euler: state_next := state_prev + dt * d(state_prev)/dt.

	LinearDynamicsState3D dState = dStateForMass( mass );

	m_position += dState.m_position * deltaSeconds; //x := x + (veloc * dt)
	m_velocity += dState.m_velocity * deltaSeconds; //v := v + (accel * dt)
}

void LinearDynamicsState3D::StepWithVerlet( float mass, float deltaSeconds )
{
	//https://en.wikipedia.org/wiki/Verlet_integration#Velocity_Verlet - to do away with the x_(t-1) at t=0 problem.
	static Vector3f prevAccel = 0.f;

	LinearDynamicsState3D dState = dStateForMass( mass );

	m_position += ( m_velocity*deltaSeconds ) + ( dState.m_velocity*.5f*deltaSeconds*deltaSeconds ); //x := x + v*dt + .5*a*dt*dt.
	m_velocity += ( prevAccel + dState.m_velocity )*.5f*deltaSeconds; //v := v + .5*(a + a_next)*dt.
	prevAccel = dState.m_velocity;
}


//--------------------------------------------------------------------------------------------------------------
LinearDynamicsState3D LinearDynamicsState3D::dStateForMass( float mass ) const
{
	Vector3f acceleration = CalcNetForceForMass( mass ) * ( 1.f / mass ); //Newton's 2nd law, rearranged.

	return LinearDynamicsState3D( m_velocity, acceleration );
}


//--------------------------------------------------------------------------------------------------------------
Vector3f LinearDynamicsState3D::CalcNetForceForMass( float mass ) const
{
	Vector3f netForce( 0.f );

	auto forceIterEnd = m_forces.cend();
	for ( auto forceIter = m_forces.cbegin(); forceIter != forceIterEnd; ++forceIter )
	{
		Force* currentForce = *forceIter;
		netForce += currentForce->CalcForceForStateAndMass( this, mass );
	}

	return netForce;
}


//--------------------------------------------------------------------------------------------------------------
void LinearDynamicsState3D::ClearForces( bool keepGravity /*= true*/ )
{
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
