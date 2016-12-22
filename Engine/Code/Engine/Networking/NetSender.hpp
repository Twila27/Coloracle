#pragma once

#include "Engine/Networking/NetSystem.hpp"


//-----------------------------------------------------------------------------
class NetSession;
class NetConnection;


//-----------------------------------------------------------------------------
struct NetSender
{
	NetSession* ourSession; //Shared by connected players; what's received the message.
	sockaddr_in sourceAddr;
	NetConnection* sourceConnection;
};
