#include "Engine/Networking/NetSystem.hpp"

#include "Engine/EngineCommon.hpp"
#include "Engine/Error/ErrorWarningAssert.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Engine/Core/Command.hpp"
#include "Engine/Networking/RemoteCommandService.hpp" //For console command registration and autostart.
#include "Engine/Networking/NetSession.hpp" //For registering console commands.
#include "Engine/Networking/PacketChannel.hpp" //For registering console commands.

//--------------------------------------------------------------------------------------------------------------
static const size_t BUFFER_SIZE = 256;


//--------------------------------------------------------------------------------------------------------------
static void NetListTCPAddresses( Command& args )
{
	std::string port = args.GetArgsString();
	if ( port != "" )
	{
		char nameBuffer[ BUFFER_SIZE ];
		NetSystem::GetLocalHostName( nameBuffer, BUFFER_SIZE );

		addrinfo* addrList = NetSystem::AllocAddressesGivenHostAndPort( nameBuffer, port.c_str(), AF_INET/*IPv4 Assumed!*/, SOCK_STREAM/*TCP*/, 0 );
		addrinfo* addrIter = addrList;
		char addrStr[ BUFFER_SIZE ];
		while ( addrIter != nullptr )
		{
			NetSystem::SockAddrToString( addrIter->ai_addr, addrStr, BUFFER_SIZE );
			g_theConsole->Printf( addrStr );
			
			addrIter = addrIter->ai_next;
		}
	}
	else
	{
		g_theConsole->Printf( "Incorrect arguments." );
		g_theConsole->Printf( "Usage: NetListTCPAddresses <PortNumber>" );
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC bool NetSystem::Startup()
{
	//WSA = WinSock API. Note the var is uninitialized when we pass it (because we don't care, but it requires the argument).
	WSADATA wsa_data;

	//MAKEWORD, provided by Windows, takes uint8 and puts them into a uint16 (since DWORD is 32 bits). The 2,2 specifies WinSock v2.2.
	int error = WSAStartup( MAKEWORD( 2, 2 ), &wsa_data );

	ASSERT_OR_DIE( error == 0, "NetSystem::Startup error! Perhaps tried to start NetSystem twice?" );
	
	TODO( "Support the disabling of all game networking commands if the above condition falls apart." );

	//If this function does not run prior to the use of this system, WSAGetLastError() returns a WSA not initialized error upon WSA use.

	g_theConsole->RegisterCommand( "NetListTCPAddresses", NetListTCPAddresses );
	RemoteCommandService::RegisterConsoleCommands();
	RemoteCommandService::Autoconnect();

	return ( error != 0 );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetSystem::Shutdown()
{
	WSACleanup();
}


//--------------------------------------------------------------------------------------------------------------
STATIC addrinfo* NetSystem::AllocAddressesGivenHostAndPort( const char* hostNameStr, //e.g. google.com
									   const char* serviceStr, //usually port # as string
									   int family, //connection family, AF_INET (IPv4) in SD6 A1
									   int socktype, //socket type, SOCK_STREAM (TCP) or SOCK_DGRAM (UDP) in SD6
									   int flags /*= 0 */ ) //search flag hints, we use for AI_PASSIVE (bindable addresses)
{
	addrinfo hints; //Descriptor for what we're listening for as a host, think like filters.
	memset( &hints, 0, sizeof( hints ) ); //Clean memory in advance, since it's dirty upon declaration.

	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_flags = flags;

	//Below allocates all addresses into a singly linked list with the head put into the variable "result":
	addrinfo* result;
	int status = getaddrinfo( hostNameStr, serviceStr, &hints, &result ); //Sets info to a list of addrinfo's using the hints buffer.
	if ( status != 0 )
		return nullptr; //Failed. Can grab error if wanted, but just means no names could be resolved.

	return result;
}


//--------------------------------------------------------------------------------------------------------------
void NetSystem::FreeAddresses( addrinfo* addresses )
{
	if ( addresses != nullptr )
		freeaddrinfo( addresses );
}


//--------------------------------------------------------------------------------------------------------------
bool NetSystem::GetLocalHostName( char* out_buffer, size_t bufferSize )
{
	//If successful, returns the router IP, supporting LAN or port-forwarded Internet connection.
	if ( GLOBAL::gethostname( out_buffer, bufferSize ) == 0 )
	{
		return true;
	}
	else //So on failure, this only allows same-machine connections.
	{
		out_buffer = "localhost"; 
		return false;
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC SOCKET NetSystem::CreateListenSocket( const char* addrStr, const char* serviceStr, sockaddr_in* out_sockAddr, int queueCount /*= 1 */ )
{
	//Note serviceStr is usually the port # passed as a string.

	//All-lowercase functions and types come from WinSock.
	addrinfo* addrList = AllocAddressesGivenHostAndPort( addrStr, serviceStr, AF_INET, SOCK_STREAM, AI_PASSIVE );
		//addrinfo is a single linked list of pointers to the next one, when you hit NULL, no more infos.
		//AI_PASSIVE means we need sockets that can be bound locally <-> aren't currently in use. Fails if you don't send an address that's "you".
		//(Because it needs to be something you can bind on, and you only have yourself for that.)

	SOCKET mySocket = INVALID_SOCKET;
	addrinfo* addrIter = addrList;
	while ( ( addrIter != nullptr ) && ( mySocket == INVALID_SOCKET ) )
	{
		//STEP #1: MAKE SOCKET.
		mySocket = GLOBAL::socket( addrIter->ai_family, addrIter->ai_socktype, addrIter->ai_protocol );
			//Family is either AF_INET (for IPv4) or AF_INET6 (for IPv6). This is the IP part.
			//Type for this class is either SOCK_STREAM (UDP) or SOCK_DGRAM (UDP).
			//Protocol will be AF_INET. Technically AP_NET for application protocol, but they work interchangeably.

		if ( mySocket != INVALID_SOCKET )
		{
			//STEP #2: BIND SOCKET.
			int result = GLOBAL::bind( mySocket, addrIter->ai_addr, (int)( addrIter->ai_addrlen ) );
				//2nd arg has value sockaddr_in6 for IPv6, sockaddr_in for IPv4.

			if ( result != SOCKET_ERROR )
			{
				//STEP 3: ASYNC LISTENING FOR ATTEMPTED CONNECTIONS TO THE SOCKET.
				u_long nonblocking = 1; //We set it nonblocking to work with it on our main thread.
				ioctlsocket( mySocket, FIONBIO, &nonblocking );
				//Can check error on this here as well, in lecture assumed it won't go awry here.

				result = listen( mySocket, queueCount );
				ASSERT_OR_DIE( result != SOCKET_ERROR, nullptr );

				//Save out the created listen socket's address, if requested.
				ASSERT_OR_DIE( addrIter->ai_addrlen == sizeof( sockaddr_in ), nullptr );
				if ( out_sockAddr != nullptr )
					memcpy( out_sockAddr, addrIter->ai_addr, addrIter->ai_addrlen );
			}
			else
			{
				CloseSocket( mySocket );
				mySocket = INVALID_SOCKET; //Breaks while loop.
			}
		}

		addrIter = addrIter->ai_next;
	}

	FreeAddresses( addrList );

	return mySocket;
}


//--------------------------------------------------------------------------------------------------------------
STATIC SOCKET NetSystem::AcceptConnection( SOCKET hostSocket, sockaddr_in* out_theirAddr )
{
	//Every time accept() succeeds, a new client joins the host's session. YOU CALL THIS FUNCTION PER FRAME!

	sockaddr_storage theirAddr; //A struct big enough to handle any sock_addr kind (v4, v6) we could want.
	int theirAddrSize = sizeof( theirAddr );

	//STEP #4: accept the joining connection to the listening host.
	SOCKET theirSocket = GLOBAL::accept( hostSocket, (sockaddr*)&theirAddr, &theirAddrSize );

	if ( theirSocket != INVALID_SOCKET )
	{
		TODO( "No longer a valid check with IPv6!" );
		if ( out_theirAddr != nullptr )
		{
			ASSERT_OR_DIE( theirAddrSize == sizeof( sockaddr_in ), "Check is invalid for IPv6!" ); //Be wholly sure it's sized to v4 instead of v6.
			memcpy( out_theirAddr, &theirAddr, theirAddrSize );
		}

		return theirSocket;
	}
	else
	{
		// From Forseth in SD6 A1 spec:
		// if we fail to accept(), it might be we lost
		// connection - you can check the same way we'll do it
		// for send and recv below, and potentially return 
		// that error code somehow (if you move this code into a method
		// you could disconnect directly)

		int err = WSAGetLastError();
		if ( SocketErrorShouldDisconnect( err ) )
			CloseSocket( hostSocket ); //disconnect()?
	}

	return INVALID_SOCKET;
}


//--------------------------------------------------------------------------------------------------------------
static void SockAddrToStringHelper( sockaddr const* addr, sockaddr_in const* addr_in, char* out_buffer, size_t bufferSize )
{
	// inet_ntop converts an address type to a human readable string,
	// e.g. 0x7f000001 => "127.0.0.1"
	// GetInAddr (defined below) gets the pointer to the address part of the sockaddr
	char hostname[ BUFFER_SIZE ];
	inet_ntop( addr_in->sin_family, NetSystem::GetInAddr( addr ), hostname, BUFFER_SIZE );

	//Combine result with port, latter is stored in net order, so convert it to host order.
	sprintf_s( out_buffer, bufferSize, "%s:%u", hostname, ntohs( addr_in->sin_port ) ); //Note ntohs (Network-to-Host [for] [u]Short)
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetSystem::SockAddrToString( sockaddr const* addr, char* out_buffer, size_t bufferSize )
{
	//Below line hard-codes IPv4 for brevity, but can extend later to work for IPv6:
	sockaddr_in* addr_in = (sockaddr_in*)addr;

	SockAddrToStringHelper( addr, addr_in, out_buffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void NetSystem::SockAddrToString( sockaddr_in const* addr_in, char* out_buffer, size_t bufferSize )
{
	//Below line hard-codes IPv4 for brevity, but can extend later to work for IPv6:
	sockaddr* addr = (sockaddr*)addr_in;

	SockAddrToStringHelper( addr, addr_in, out_buffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void* NetSystem::GetInAddr( sockaddr const* sa )
{
	if ( sa->sa_family == AF_INET )
		return &(((sockaddr_in*)sa)->sin_addr); //IPv4.
	else 
		return &( ( (sockaddr_in6*)sa )->sin6_addr ); //IPv6.
}


//--------------------------------------------------------------------------------------------------------------
bool NetSystem::SockAddrFromStringIPv4( sockaddr_in* out_addr, const std::string& ipStr, uint16_t port )
{
	if ( out_addr == nullptr )
		return false;

	memset( out_addr, 0, sizeof( sockaddr_in ) );

	int err = inet_pton( AF_INET, ipStr.c_str(), &( out_addr->sin_addr ) );
	out_addr->sin_port = htons( port ); //Host to net order.
	out_addr->sin_family = AF_INET; //IPv4 assumed.

	return ( err == 1 ); //1 on success, -1 on failure, 0 if input wasn't a valid IP.
}


//--------------------------------------------------------------------------------------------------------------
STATIC bool NetSystem::DoSockAddrMatch( sockaddr_in a, sockaddr_in b )
{
	return ( ( a.sin_addr.S_un.S_addr == b.sin_addr.S_un.S_addr ) && ( a.sin_port == b.sin_port ) );
}
