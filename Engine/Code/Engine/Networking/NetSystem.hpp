#pragma once


#pragma comment( lib, "ws2_32.lib" )


#include <WinSock2.h>
#include <WS2tcpip.h> //Just the debug commands for SD6 A1.
#include "Engine/EngineCommon.hpp"


//-----------------------------------------------------------------------------
class NetSystem //Basically a WinSock wrapper.
{
public:

	static bool Startup();
	static void Shutdown();
	
	static bool GetLocalHostName( char* out_buffer, size_t bufferSize );
	static addrinfo* AllocAddressesGivenHostAndPort( const char* hostNameStr, const char* serviceStr, int family, int socktype, int flags = 0 ); //Returns list of addresses given host name (IP) and port (serviceStr).
	static bool DoSockAddrMatch( sockaddr_in a, sockaddr_in b );
	static bool SockAddrFromStringIPv4( sockaddr_in* out_addr, const std::string& ipStr, uint16_t port ); //string -> sockaddr. Returns whether conversion failed.
	static void SockAddrToString( sockaddr const* addr, char* out_buffer, size_t bufferSize ); //sockaddr -> string.
	static void SockAddrToString( sockaddr_in const* addr, char* out_buffer, size_t bufferSize ); //sockaddr_in -> string.
	static void* GetInAddr( sockaddr const* sa ); //Get pointer to address part of sockaddr variable, IPv4 or IPv6. (For SockAddrToString.)
	static void FreeAddresses( addrinfo* info );

	//Only used by TCP, not UDP which is "connectionless" in concept.
	static SOCKET CreateListenSocket( const char* addrStr, const char* serviceStr, sockaddr_in* out_sockAddr, int queueCount = 1 );
	static SOCKET AcceptConnection( SOCKET hostSocket, sockaddr_in* out_theirAddr );

	static void CloseSocket( SOCKET sock ) { closesocket( sock ); }

	static bool SocketErrorShouldDisconnect( const int32_t error ) //Ignores non-fatal errors.
	{
		switch ( error )
		{
		case WSAEWOULDBLOCK: // nothing to do - would've blocked if set to blocking
		case WSAEMSGSIZE:	 // UDP message too large - ignore that packet.
		case WSAECONNRESET:	 // Other side reset their connection.
			return false;
		default:
			return true;
		}
	}
};