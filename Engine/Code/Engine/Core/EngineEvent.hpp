#pragma once


#include "Engine/EngineCommon.hpp"


//Create as many subclasses as needed on the game side to pass around freely.
class EngineEvent
{

public:
	EngineEvent( EngineEventID id, void* intendedRecipientObj = nullptr ) : m_id( id ), m_intendedRecipient( intendedRecipientObj ) {}
	void SetID( EngineEventID id ) { m_id = id; }
	EngineEventID GetID() const { return m_id; }
	bool IsIntendedForThis( void* yourThisPtr ) const { return yourThisPtr == m_intendedRecipient; }


private:
	EngineEventID m_id;
	void* m_intendedRecipient; //An event trigger goes out to all subscribers. They can check if their this ptr == this var, quit if not, when set.
	virtual void ValidateDynamicCast() {}
};


//-----------------------------------------------------------------------------
struct EngineEventUpdate : public EngineEvent
{
	EngineEventUpdate( float deltaSeconds ) : EngineEvent( "EngineEventUpdate" ), deltaSeconds( deltaSeconds ) {}
	float deltaSeconds;
};


//-----------------------------------------------------------------------------
struct MemoryAllocatedEvent : public EngineEvent
{
	MemoryAllocatedEvent() : EngineEvent( "OnMemoryAllocated" ) {}
	int numBytes;
};


//-----------------------------------------------------------------------------
struct MemoryFreedEvent : public EngineEvent
{
	MemoryFreedEvent() : EngineEvent( "OnMemoryFreed" ) {}
	int numBytes;
};