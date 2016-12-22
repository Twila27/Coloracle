#include "Engine/Tools/StateMachine/State.hpp"
#include "Engine/Tools/StateMachine/StateMachine.hpp"
#include "Engine/Core/EngineEvent.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Input/TheInput.hpp"


//-----------------------------------------------------------------------------
const char* StateCommand::GetName() const
{
	if ( eventContext == nullptr )
		return nullptr;

	return eventContext->GetID();
}


//-----------------------------------------------------------------------------
void StateStage::RunCommands()
{
	for each ( const StateCommand& cmd in m_commands ) 
		TheEventSystem::Instance()->TriggerEvent( cmd.GetName(), cmd.eventContext.get() );
}


//--------------------------------------------------------------------------------------------------------------
State::State( StateMachine* myStateMachine, StateNameStr name, bool isStartState )
	: m_name( name )
	, m_isStartState( isStartState )
	, m_myStateMachine( myStateMachine )
	, m_isSkippingEnterCommands( false )
	, m_isSkippingExitCommands( false )
{
	for each ( StateStage*& stage in m_stages )
		stage = nullptr;
}


//--------------------------------------------------------------------------------------------------------------
State::~State()
{
	for each ( StateStage* stage in m_stages )
		delete stage;
}


//--------------------------------------------------------------------------------------------------------------
void State::AddStateCommand( StateStageType forStage, StateCommand cmd )
{
	ASSERT_OR_DIE( forStage < NUM_STATE_STAGES, nullptr );

	StateStage* stage = m_stages[ forStage ];
	if ( stage == nullptr )
	{
		stage = new StateStage();
		m_stages[ forStage ] = stage;
	}

	stage->m_commands.push_back( cmd );
}


//--------------------------------------------------------------------------------------------------------------
void State::OnStateEntered_Handler()
{
	if ( IsSkippingEnterCommands() || ( m_stages[ STATE_STAGE_ENTERING ] == nullptr ) )
		return;

	m_stages[ STATE_STAGE_ENTERING ]->RunCommands();
}


//--------------------------------------------------------------------------------------------------------------
void State::OnStateWaiting_Handler()
{
	if ( m_stages[ STATE_STAGE_WAITING ] != nullptr )
		m_stages[ STATE_STAGE_WAITING ]->RunCommands();

	for each ( const InputTransitionPair& pair in m_inputMap )
	{
		if ( g_theInput->WasKeyPressedOnce( pair.first ) )
		{
			m_myStateMachine->SetCurrentState( pair.second );
			return; //So right now, only supports 1 hotkey -> 1 exit input mappings. Maybe expand to an array for pair.first?
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
void State::OnStateExited_Handler()
{
	if ( IsSkippingExitCommands() || ( m_stages[ STATE_STAGE_EXITING ] == nullptr ) )
		return;

	m_stages[ STATE_STAGE_EXITING ]->RunCommands();
}
