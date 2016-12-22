#pragma once
#include <map>
#include "Engine/Tools/StateMachine/State.hpp"


/* Intended Usage
	1. Create a StateMachine.
	2. Get states to configure via CreateState(name).
	3a. Configure their transitions via state.AddTransition(?).
	3b. Configure their internal stages' command buffers with state.AddStateCommand(cmd). 
		- where cmd wraps an EngineEvent.
		- order of addition == order of execution.
	4. Call stateMachine.Update() each frame to monitor hotkey transitions.
*/


//-----------------------------------------------------------------------------
class State;
typedef std::map< StateNameStr, State* > StateNameMap;


//-----------------------------------------------------------------------------
class StateMachine
{
public:
	StateMachine( bool cannotSetToSameState = true )
		: m_currentState( nullptr )
		, m_hasStartState( false )
		, m_hasRunOnStartStateEnter( false )
		, m_cannotSetToSameState( cannotSetToSameState )
	{
	}
	State* CreateState( StateNameStr name, bool isStartState = false );
	State* GetCurrentState() const { return m_currentState; }
	bool SetCurrentState( StateNameStr newStateName ); //Helper-wrapper, affords manual setter.
	bool SetCurrentState( State* newState );
	void Update();

	bool HasState( StateNameStr str ) const { return m_stateMap.count( str ) > 0; }
	State* FindState( StateNameStr str );

	//Is it faster to do strcmp or std::map::find?
	bool IsInState( StateNameStr str ) const { return strcmp( m_currentState->GetName(), str ) == 0; }

private:
	StateNameMap m_stateMap;
	State* m_currentState;

	bool m_hasStartState;
	bool m_hasRunOnStartStateEnter; //Else we never fire it, as it was never transitioned into from something.
	bool m_cannotSetToSameState;
};