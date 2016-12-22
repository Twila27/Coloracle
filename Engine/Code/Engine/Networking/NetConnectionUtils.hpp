#pragma once
#include <stdint.h>
#include <set>
#include "Engine/Networking/NetSystem.hpp"


//-----------------------------------------------------------------------------
class NetMessage;


//-----------------------------------------------------------------------------
extern bool UnsignedGreaterThan( uint16_t a, uint16_t b );
extern bool UnsignedGreaterThanOrEqual( uint16_t a, uint16_t b );
extern bool UnsignedLessThan( uint16_t a, uint16_t b );
extern bool UnsignedLessThanOrEqual( uint16_t a, uint16_t b );


//-----------------------------------------------------------------------------
struct UnsignedComparator
{
	bool operator() ( const uint16_t& a, const uint16_t& b ) const {
		return UnsignedLessThan( a, b );
	}
};


//-----------------------------------------------------------------------------
struct InOrderMessageComparator
{
	bool operator() ( const NetMessage* a, const NetMessage* b ) const;
};


//-----------------------------------------------------------------------------
struct InOrderChannelData
{
	InOrderChannelData()
		: nextSentSequenceID( 0 )
		, nextExpectedSequenceID( 0 )
	{
	}

	//Sending side:
	uint16_t GetNextSequenceIDToSend() { return nextSentSequenceID++; }
	uint16_t nextSentSequenceID;

	//Receiving side:
	uint16_t nextExpectedSequenceID/*ToBeReceived*/;

	NetMessage* FindAndRemoveMessageForSequenceID( uint16_t sid );
	std::set<NetMessage*, InOrderMessageComparator > outOfOrderReceivedSequencedMessages/*ToBeReceived*/;
		//C4 uses an in-place linked list for this, putting next and prev pointers on NetMessage* for this.
};


//-----------------------------------------------------------------------------
struct NetConnectionInfo
{
	sockaddr_in		address;
	char			guid[ MAX_GUID_LENGTH ];
	uint8_t			connectionIndex/*WithinSession*/; //Identifies locally to this session of the game which objects the connection corresponds to.
};


//-----------------------------------------------------------------------------
struct JoinRequest/*Handshake*/ //Data written to the message buffer sent to the host.
{
	//NOT the address, that'll come to the host from the socket. 
	//Likewise, the host determines your connection index, so it does not get sent.
	char msgGuid[ MAX_GUID_LENGTH ];

	uint64_t version;
	uint32_t nuonce; //Number used [only] once.

}; //SENT RELIABLE && CONNECTIONLESS


//-----------------------------------------------------------------------------
struct JoinDeny/*Info*/
{
	//It's empty?

}; //SENT UNRELIABLE && REQUIRES-CONNECTION


//-----------------------------------------------------------------------------
struct JoinAccept/*Info*/
{
	uint32_t nuonce; //See SendDeny below.
	//Join, cancel, and then join again--1st join's response may be tricked into being use for the 2nd request, but nuonce works as a 1-shot password to kill it. 
	//Comes into play esp. for longer join processes like NAT traversals taking ~10seconds--getting a deny for the wrong request.
	//Used to match a response and a request ==> this value needs to be kept unique. Can just start by ++ with every request. 
		//Using rand() requires you to know it'll actually not repeat within enough a time amount.
		//Increment it by a random uint8 every time Forseth/Cloudy came up with would be fairly unique and predictable each time without being that clear to malicious users.

	NetConnectionInfo hostInfo;
	NetConnectionInfo joineeInfo;

}; //SENT RELIABLE
