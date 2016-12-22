#pragma once
#include <vector>
#include "Engine/EngineCommon.hpp"

//-----------------------------------------------------------------------------
class Force;
class EphanovParticle;


//-----------------------------------------------------------------------------
class LinearDynamicsState2D //Future variant or subclass of an entity's rigid-body PhysicsComponent. Distinct from AngularDynamics (see MP3 notes).
{
public:

	LinearDynamicsState2D( const Vector2f& position = Vector2f::ZERO, const Vector2f& velocity = Vector2f::ZERO )
		: m_position( position )
		, m_velocity( velocity )
	{
	}
	~LinearDynamicsState2D();

	void StepWithForwardEuler( float mass, float deltaSeconds );
	void StepWithVerlet( float mass, float deltaSeconds );
	Vector2f GetPosition() const { return m_position; }
	Vector2f GetVelocity() const { return m_velocity; }
	void SetPosition( const Vector2f& newPos ) { m_position = newPos; }
	void SetVelocity( const Vector2f& newVel ) { m_velocity = newVel; }
	void AddForce( Force* newForce ) { m_forces.push_back( newForce ); }
	void GetForces( std::vector< Force* >& out_forces ) { out_forces = m_forces; }
	void ClearForces( bool keepGravity = true );
	bool HasForces() const { return m_forces.size() > 0; }

private:

	Vector2f m_position;
	Vector2f m_velocity;
	std::vector<Force*> m_forces; //i.e. All forces acting on whatever this LDS is attached to.

	LinearDynamicsState2D dStateForMass( float mass ) const; //Solves accel, for use in Step() integrators.
	Vector2f CalcNetForceForMass( float mass ) const; //Used by dState(), gets Sigma[F] by looping m_forces.
};