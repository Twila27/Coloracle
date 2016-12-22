#include "Engine/Networking/udpip/UDPSocket.hpp"
#include "Engine/Networking/NetSystem.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC SOCKET UDPSocket::CreateUDPSocket( const char* addrStr, const char* serviceStr, sockaddr_in* out_boundAddr )
{
	//First, grab net addrs. Note serviceStr is only a port # for TCP/IP or UDP/IP.
	addrinfo* addrList = NetSystem::AllocAddressesGivenHostAndPort( addrStr, serviceStr, AF_INET/*IPv4*/, SOCK_DGRAM/*UDP*/, AI_PASSIVE );

	if ( addrList == nullptr )
		return false; //No addrs found for arguments!

	//Try to create a SOCKET from above addr info.
	SOCKET mySocket = INVALID_SOCKET;
	addrinfo* addrIter = addrList;
	//1st success we find will break 2nd condition to end loop, or we'll hit list end:
	while ( ( addrIter != nullptr ) && ( mySocket == INVALID_SOCKET ) )
	{
		//First make the unbound socket via addrinfo details. Hardcoding would state AF_INET, SOCK_DGRAM, IPPROTO_UDP for UDP/IPv4.
		mySocket = GLOBAL::socket( addrIter->ai_family, addrIter->ai_socktype, IPPROTO_UDP/*addrIter->ai_protocol*/ );

		if ( mySocket != INVALID_SOCKET )
		{
			//Getting here means we managed to create it, now try to bind it (i.e. associate addr argument to it).
			int result = GLOBAL::bind( mySocket, addrIter->ai_addr, (int)(addrIter->ai_addrlen) );

			if ( SOCKET_ERROR != result )
			{
				//Set it to non-block: we'll work with it on our main thread, we don't want to stall other code.
				u_long nonblocking = 1;
				ioctlsocket( mySocket, FIONBIO, &nonblocking );

				//Save off address when available.
				ASSERT_OR_DIE( addrIter->ai_addrlen == sizeof( sockaddr_in ), nullptr ); //Ensure it's IPv4.
				if ( nullptr != out_boundAddr )
					memcpy( out_boundAddr, addrIter->ai_addr, addrIter->ai_addrlen );
			}
			else //Cleanup on fail:
			{
				closesocket( mySocket );
				mySocket = INVALID_SOCKET; //Won't break out of while loop via the 2nd condition now.
			}
		}

		addrIter = addrIter->ai_next;
	}

	NetSystem::FreeAddresses( addrList ); //We allocate it, so it free it too.

	return mySocket; //Return created socket.
}





//--------------------------------------------------------------------------------------------------------------
size_t UDPSocket::SendTo( sockaddr_in const& targetAddr, void const* data, const size_t dataSize )
{
	if ( m_mySocket != INVALID_SOCKET )
	{
		//sendto()'s retval should == dataSize, or something went wrong.
		int size = GLOBAL::sendto( m_mySocket,
								   (const char*)data,	//payload
								   (int)dataSize,		//payload size
								   0,					//no flags -- see MSDN if curious
								   (sockaddr const*)&targetAddr, //where we send it
								   sizeof( sockaddr_in ) ); //size of what we're sending
		if ( size > 0 )
			return size;
	}
	
	//Can add error check here if something breaks.
	return 0;
}


//--------------------------------------------------------------------------------------------------------------
size_t UDPSocket::RecvFrom( sockaddr_in* out_fromAddr, void* out_buffer, const size_t bufferSize )
{
	if ( m_mySocket != INVALID_SOCKET )
	{
		//::recvfrom should always be <= bufferSize.
		//If sendto(3KB), may get several recvs of say 1KB, 512B, 1.5KB.
		//This is because 3KB > MTU (see NetPacket.hpp) and got split!

		sockaddr_storage addr;
		int addrlen = sizeof( addr );

		int size = GLOBAL::recvfrom( m_mySocket,
									 (char*)out_buffer, //what we're reading into
									 bufferSize,	//max data we can read
									 0, //no flags -- see MSDN if curious
									 (sockaddr*)&addr, //who sent the msg
									 &addrlen ); //length of their addr

		if ( size > 0 )
		{
			//Only doing IPv4, else assumes garbage:
			ASSERT_OR_DIE( addrlen == sizeof( sockaddr_in ), nullptr );
			memcpy( out_fromAddr, &addr, addrlen );
			return size;
		}
	}

	//Can add error check here if something breaks.
	return 0U;
}


//--------------------------------------------------------------------------------------------------------------
bool UDPSocket::Bind( const char* addr, uint16_t port )
{
	const int MAX_PORT_STRLEN = 10;
	char portBuffer[ MAX_PORT_STRLEN ];
	snprintf( portBuffer, strlen( portBuffer ), "%u", port ); //Alt to itoa which was said to not be thread-safe.

	m_mySocket = UDPSocket::CreateUDPSocket( addr, portBuffer, &m_myAddr );
	return IsBound();
}


//--------------------------------------------------------------------------------------------------------------
void UDPSocket::Unbind()
{
	if ( IsBound() )
	{
		NetSystem::CloseSocket( m_mySocket );
		m_mySocket = INVALID_SOCKET;
	}
}


//--------------------------------------------------------------------------------------------------------------
void UDPSocket::GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const
{
	NetSystem::SockAddrToString( &m_myAddr, out_addrStrBuffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
void UDPSocket::GetAddressObject( sockaddr_in* out_addr ) const
{
	*out_addr = m_myAddr;
}
