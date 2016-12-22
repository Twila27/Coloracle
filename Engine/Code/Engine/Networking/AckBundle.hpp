#pragma once
#include <vector>
#include "Engine/EngineCommon.hpp"


//--------------------------------------------------------------------------------------------------------------
#define INVALID_PACKET_ACK (0xFFFF) //"-1" for uint16 => 65535.
#define MAX_RELIABLES_PER_PACKET (32)


//-----------------------------------------------------------------------------
struct AckBundle //Whatever information we need to remember about a reliably sent packet.
{
	//WARNING: These are only for locally tracking packet IDs, i.e. unlike msg reliableIDs they aren't sent.
		//(How acks ARE sent is through the PacketHeader class.)

	AckBundle() : ackID( INVALID_PACKET_ACK ) {}
	uint16_t ackID; //Which sent packet this is associated with. 
		//This goes up to 65535, then back to 0, so you may skip one every so often (~per 5min) but that's fine.

	uint16_t sentReliableIDs[ MAX_RELIABLES_PER_PACKET ]; //Describes which reliables were sent with this ack.
	uint32_t numReliableIDsSent;

	void AddReliable( uint16_t newID )
	{
		sentReliableIDs[ numReliableIDsSent ] = newID;
		++numReliableIDsSent; //NOTE OVERFLOW.
	}
};