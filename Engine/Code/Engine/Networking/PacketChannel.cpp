#include "Engine/Networking/PacketChannel.hpp"
#include "Engine/Networking/udpip/UDPSocket.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Time/Time.hpp"
#include "Engine/Core/TheConsole.hpp"
#include "Engine/Core/Command.hpp"


//--------------------------------------------------------------------------------------------------------------
PacketChannel::PacketChannel()
	: m_additionalLossPercentile( MIN_PACKET_DROP_RATE, MAX_PACKET_DROP_RATE )
	, m_additionalLagSeconds( MIN_ADDITIONAL_LAG_SECONDS, MAX_ADDITIONAL_LAG_SECONDS )
	, m_wrappedSocket( new UDPSocket() )
	, m_lastAllocatedPacket( nullptr )
{
	m_packetMemoryPool.Init( MAX_CHANNEL_PACKETS );
}


//--------------------------------------------------------------------------------------------------------------
bool PacketChannel::Bind( const char* addrStr, uint16_t port )
{
	return m_wrappedSocket->Bind( addrStr, port );
}


//--------------------------------------------------------------------------------------------------------------
void PacketChannel::Unbind()
{
	return m_wrappedSocket->Unbind();
}


//--------------------------------------------------------------------------------------------------------------
bool PacketChannel::IsBound() const
{
	return m_wrappedSocket->IsBound();
}


//--------------------------------------------------------------------------------------------------------------
size_t PacketChannel::SendTo( sockaddr_in const& targetAddr, void const* data, const size_t dataSize )
{
	return m_wrappedSocket->SendTo( targetAddr, data, dataSize );
}


//--------------------------------------------------------------------------------------------------------------
size_t PacketChannel::RecvFrom( sockaddr_in* out_fromAddr, void* out_buffer, const size_t bufferSize )
{
	if ( ( m_additionalLossPercentile == Interval<float>::ZERO ) && ( m_additionalLagSeconds == Interval<double>::ZERO ) )
	{
		return m_wrappedSocket->RecvFrom( out_fromAddr, out_buffer, bufferSize );
	}
	//Above skips the extra stuff, so we can still use PacketChannel in the default case (affording NetSimLoss/Lag commands at all times).

	ActuallyReceivePackets( out_fromAddr );

	return ActuallyProcessPacket( out_fromAddr, out_buffer );
}


//--------------------------------------------------------------------------------------------------------------
void PacketChannel::ActuallyReceivePackets( sockaddr_in* out_fromAddr )
{
	
	//Actually receive packets (emphasis on plural) -- note here we simulate packet drop based on %net-loss variable and introduce processing delays (net-lag interval var).
	while ( true )
	{
		if ( m_lastAllocatedPacket == nullptr ) //Prevents per-frame allocation by reusing last allocated packet until pool.Delete() nulls it out.
			m_lastAllocatedPacket = m_packetMemoryPool.Allocate();

		/*WRAPPED CALL*/size_t bytesRead = m_wrappedSocket->RecvFrom( out_fromAddr, m_lastAllocatedPacket->m_myPacket.GetPayloadBuffer(), MAX_PACKET_SIZE );
			//Going to have 0 num messages at this point, since we haven't read out what NetSession reads out when we return from PacketChannel::RecvFrom.
			//Although we could read it and rewind, not sure it's worth incurring that cost per-packet.
		if ( bytesRead > 0 )
		{
			if ( GetRandomFloatZeroTo( 1.f ) < m_additionalLossPercentile.GetRandomElement() )
			{
				m_packetMemoryPool.Delete( m_lastAllocatedPacket ); //Just throw it out.
				//Keep looping to grab next packet, to simulate one coming in immediately (is this accurate?) where the previous packet had been lost.
			}
			else
			{
				double delaySeconds = m_additionalLagSeconds.GetRandomElement(); //Making this a range gives us the out-of-order trait for free!
				m_lastAllocatedPacket->m_myPacket.SetTotalReadableBytes( bytesRead );
				m_lastAllocatedPacket->m_whenToProcessTimeStampSeconds = GetCurrentTimeSeconds() + delaySeconds;
				m_lastAllocatedPacket->m_fromAddr = *out_fromAddr; //If we delay in ActuallyProcess, then above recvfrom's out_fromAddr memory will be out of date.
				m_inboundPackets.insert( TimeOrderedMapPair( m_lastAllocatedPacket->m_whenToProcessTimeStampSeconds, m_lastAllocatedPacket ) ); //Inserts in-order by process time in inboundPackets.
				//Keep looping, grab as much as possible before proceeding to ActuallyProcessPacket.
			}
			m_lastAllocatedPacket = nullptr; //Re-enables pool.Allocate() next time, and keeps us from overwriting the same allocated packet payload repeatedly.
		}
		else
		{
			break;
		}
	}
}


//--------------------------------------------------------------------------------------------------------------
size_t PacketChannel::ActuallyProcessPacket( sockaddr_in* out_fromAddr, void* out_buffer )
{
	//Actually process packet, after the delay of any additional lag from above has expired. Note we refuse packets that aren't ready yet via their timestamp.
	if ( m_inboundPackets.size() > 0 )
	{
		double currentTimeSeconds = GetCurrentTimeSeconds();
		TimeOrderedMap::iterator packetIter = m_inboundPackets.begin();
		TimeStampedPacket* earliestInboundPacket = packetIter->second; //Note this is unique from m_lastAllocatedPacket in the calling function!
		if ( currentTimeSeconds >= earliestInboundPacket->m_whenToProcessTimeStampSeconds )
		{
			*out_fromAddr = earliestInboundPacket->m_fromAddr;
			size_t sentMessageSize = earliestInboundPacket->m_myPacket.GetTotalReadableBytes(); //Different from calling function's size_t bytesRead.
			memcpy( out_buffer, earliestInboundPacket->m_myPacket.GetPayloadBuffer(), sentMessageSize );
			m_packetMemoryPool.Delete( earliestInboundPacket );
			m_inboundPackets.erase( packetIter ); //Remove from the front of the "channel" simulation.
			return sentMessageSize;
		}
	}
	//Noted that this above code only processes one packet at a time, but is repeatedly called by NetSession's ReceivePackets anyway.

	return 0;
}


//--------------------------------------------------------------------------------------------------------------
void PacketChannel::GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const
{
	m_wrappedSocket->GetConnectionAddress( out_addrStrBuffer, bufferSize );
}


//--------------------------------------------------------------------------------------------------------------
void PacketChannel::GetAddressObject( sockaddr_in* out_addr ) const
{
	m_wrappedSocket->GetAddressObject( out_addr );
}
