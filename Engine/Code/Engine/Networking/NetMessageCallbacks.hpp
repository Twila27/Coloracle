#pragma once


//-----------------------------------------------------------------------------
struct NetSender;
class NetMessage;


//-----------------------------------------------------------------------------
//i.e. For core engine-side messages, not game-side messages.
extern void OnPingReceived( const NetSender& sender, NetMessage& msg );
extern void OnPongReceived( const NetSender& sender, NetMessage& msg );

extern void OnJoinRequestReceived( const NetSender& from, NetMessage& msg );
extern void OnJoinAcceptReceived( const NetSender& from, NetMessage& msg );
extern void OnJoinDenyReceived( const NetSender& from, NetMessage& msg );

extern void OnLeaveReceived( const NetSender& from, NetMessage& msg );