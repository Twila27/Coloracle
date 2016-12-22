#pragma once
#include <vector>
#include <map>
#include "Engine/Core/EngineEvent.hpp"
#include "Engine/EngineCommon.hpp"


//-----------------------------------------------------------------------------
class State;
class StateMachine;
typedef std::pair< unsigned char, State* > InputTransitionPair;
typedef std::map< unsigned char, State* > InputTransitionMap;


//-----------------------------------------------------------------------------
enum StateStageType //Not actually switched between by State class, but event callbacks.
{
	STATE_STAGE_ENTERING,
	STATE_STAGE_WAITING,
	STATE_STAGE_EXITING, 
	NUM_STATE_STAGES
};


//-----------------------------------------------------------------------------
struct StateCommand //Assumes registry of the event is handled by the caller.
{
	StateCommand( EngineEventID eventNameForCommandToTrigger, void* intendedRecipientObj = nullptr ) : eventContext( new EngineEvent( eventNameForCommandToTrigger, intendedRecipientObj ) ) {}
	StateCommand( EngineEvent* eventToTrigger ) : eventContext( eventToTrigger ) {} //In case someone has unchanging data they want to supply via EngineEvent subclass.
	std::shared_ptr<EngineEvent> eventContext; //May be a polymorphic game-side subclass.
		//May want to reduce values in this to a key-value map to avoid dynamic cast in future.
	const char* GetName() const;
};


//-----------------------------------------------------------------------------
struct StateStage
{
	std::vector<StateCommand> m_commands;
	void RunCommands();
};


//-----------------------------------------------------------------------------
class State
{
public:
	State( StateMachine* myStateMachine, StateNameStr name, bool isStartState );
	~State();
	void SetIsSkippingEnterCommands( bool val ) { m_isSkippingEnterCommands = val; }
	void SetIsSkippingExitCommands( bool val ) { m_isSkippingExitCommands = val; }

	bool IsStartState() const { return m_isStartState; }
	bool IsSkippingEnterCommands() const { return m_isSkippingEnterCommands; }
	bool IsSkippingExitCommands() const { return m_isSkippingExitCommands; }

	StateNameStr GetName() const { return m_name; }

	void AddStateCommand( StateStageType forStage, StateCommand cmd );
	void AddInputTransition( unsigned char key, State* toState ) { m_inputMap[ key ] = toState; }

	void OnStateExited_Handler();
	void OnStateWaiting_Handler();
	void OnStateEntered_Handler();

private:
	StateMachine* m_myStateMachine; //For transitions.
	StateNameStr m_name;
	StateStage* m_stages[ NUM_STATE_STAGES ];
	InputTransitionMap m_inputMap;

	bool m_isStartState;
	bool m_isSkippingEnterCommands;
	bool m_isSkippingExitCommands;

	//May want void SetIsOneShot() and m_isOneShot that if true 
	//sets the isSkipping pair false inside Enter/Leave_Handler, 
	//so those commands only run one time. Or a count if >1?
};