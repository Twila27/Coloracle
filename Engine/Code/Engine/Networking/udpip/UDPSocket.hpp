#pragma once


#include "Engine/EngineCommon.hpp"
#include "Engine/Networking/NetSystem.hpp"


class UDPSocket //Unlike TCP, UDP is "connectionless" -- everyone just squats on a addr+port=socket and yells.
		//i.e. No connect(), accept(), or listen() here like there was for TCP sockets.
{
public:
	static SOCKET CreateUDPSocket( const char* addrStr, const char* serviceStr, sockaddr_in* out_boundAddr );

	bool Bind( const char* addrStr, uint16_t port ); //Not in ctor--it may fail and need port scanning, see NetSession::Start().
	void Unbind();
	bool IsBound() const { return m_mySocket != INVALID_SOCKET; }

	size_t SendTo( sockaddr_in const& targetAddr, void const* data, const size_t dataSize );
	size_t RecvFrom( sockaddr_in* out_fromAddr, void* out_buffer, const size_t bufferSize );

	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const;
	void GetAddressObject( sockaddr_in* out_addr ) const;


private:
	SOCKET m_mySocket;
	sockaddr_in m_myAddr;
};
