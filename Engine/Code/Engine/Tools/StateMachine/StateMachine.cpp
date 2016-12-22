#include "Engine/Tools/StateMachine/StateMachine.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Error/ErrorWarningAssert.hpp"


//--------------------------------------------------------------------------------------------------------------
bool StateMachine::SetCurrentState( StateNameStr newStateName )
{
	State* newState = FindState( newStateName );
	if ( newState == nullptr )
	{
		LogAndShowPrintfWithTag( "StateMachine", "No state found in SetCurrentState!" );
		return false;
	}

	return SetCurrentState( newState );
}


//--------------------------------------------------------------------------------------------------------------
bool StateMachine::SetCurrentState( State* newState )
{
	if ( newState == nullptr )
		return false;

	if ( GetCurrentState()->GetName() == newState->GetName() )
	{
		if ( m_cannotSetToSameState )
			ERROR_RECOVERABLE( "Tried to StateMachine::SetCurrentState to current state!" );
		return false;
	}

	LogAndShowPrintfWithTag( "StateMachine", "Updating State from %s to %s.", m_currentState->GetName(), newState->GetName() );

	//Can expand these calls to OnStateExited_%s with state name if we need to support multiple listeners:
	m_currentState->OnStateExited_Handler();
	m_currentState = newState;
	m_currentState->OnStateEntered_Handler();

	return true;
}


//--------------------------------------------------------------------------------------------------------------
State* StateMachine::FindState( StateNameStr str )
{
	StateNameMap::iterator found = m_stateMap.find( str );

	if ( found == m_stateMap.end() )
		return nullptr;

	return found->second;
}


//--------------------------------------------------------------------------------------------------------------
State* StateMachine::CreateState( StateNameStr nameStr, bool isStartState /*= false*/ )
{
	if ( HasState( nameStr ) )
	{
		Logger::PrintfWithTag( "StateMachine", "Duplicate in StateMachineState::CreateState!" );
		return nullptr;
	}

	State* newState = new State( this, nameStr, isStartState );
	m_stateMap[ nameStr ] = newState;

	if ( isStartState )
	{
		if ( m_hasStartState )
		{
			Logger::PrintfWithTag( "StateMachine", "Already has start state in StateMachineState::CreateState!" );
		}
		else
		{
			m_hasStartState = true;
			m_currentState = newState;
		}
	}

	return newState; //For configuration.
}


//--------------------------------------------------------------------------------------------------------------
void StateMachine::Update()
{
	if ( !m_hasStartState )
	{
		ERROR_RECOVERABLE( "No start state found in StateMachine::Update." );
		return;
	}
	else if ( !m_hasRunOnStartStateEnter )
	{
		m_currentState->OnStateEntered_Handler();
		m_hasRunOnStartStateEnter = true;
	}

	if ( m_currentState == nullptr )
	{
		ERROR_RECOVERABLE( "No current state found in StateMachine::Update." );
		return;
	}

	m_currentState->OnStateWaiting_Handler();
}
