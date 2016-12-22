#include "Engine/Time/Stopwatch.hpp"
#include "Engine/Core/TheEventSystem.hpp"
#include "Engine/Core/EngineEvent.hpp"


void Stopwatch::Update( float deltaSeconds )
{
	if ( m_isPaused )
		return;

	m_currentTimeSeconds += deltaSeconds;

	if ( m_currentTimeSeconds > m_endTimeSeconds )
	{
		Stop(); //Pause yourself BEFORE triggering the event, so the event can restart this stopwatch if wanted.
			//Otherwise it would reset this stopwatch and we'd immediately Stop() afterward.

		if ( m_endingEventID != nullptr )
			TheEventSystem::Instance()->TriggerEvent( m_endingEventID );
	}
}
