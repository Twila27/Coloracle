#pragma once


#include "Engine/Networking/NetSystem.hpp"
#include "Engine/Networking/NetPacket.hpp"
#include "Engine/Math/Interval.hpp"
#include "Engine/Memory/ObjectPool.hpp"
#include <map>


//-----------------------------------------------------------------------------
class UDPSocket;
#define MIN_PACKET_DROP_RATE (.01f)  //0-1 percentile!
#define MAX_PACKET_DROP_RATE (.05f)  //0-1 percentile!
#define MIN_ADDITIONAL_LAG_SECONDS (.1) //100ms.
#define MAX_ADDITIONAL_LAG_SECONDS (.15) //150ms.
#define MAX_CHANNEL_PACKETS	(1000) //See ReadMe for comments on this being exceeded.


//-----------------------------------------------------------------------------
struct TimeStampedPacket
{
	NetPacket m_myPacket;
	double m_whenToProcessTimeStampSeconds; //Helps introduce lag.
	sockaddr_in m_fromAddr;
};

//-----------------------------------------------------------------------------
typedef std::multimap< double, TimeStampedPacket* > TimeOrderedMap;
typedef std::pair< double, TimeStampedPacket* > TimeOrderedMapPair;


//USAGE: rename the UDPSocket object in NetSession to PacketChannel, and that should be it.
class PacketChannel //Simulates a network channel to afford introducing additional lag/loss conditions for testing.
{
public:
	PacketChannel();
	bool Bind( const char* addrStr, uint16_t port ); //Not in ctor--it may fail and need port scanning, see NetSession::Start().
	void Unbind();
	bool IsBound() const;

	size_t SendTo( sockaddr_in const& targetAddr, void const* data, const size_t dataSize );
	size_t RecvFrom( sockaddr_in* out_fromAddr, void* out_buffer, const size_t bufferSize ); //DIFFERENT FROM UDPSOCKET!


	void GetConnectionAddress( char* out_addrStrBuffer, size_t bufferSize ) const;
	void GetAddressObject( sockaddr_in* out_addr ) const;
	double GetSimulatedMaxAdditionalLag() const { return m_additionalLagSeconds.maxInclusive; }
	double GetSimulatedMinAdditionalLag() const { return m_additionalLagSeconds.minInclusive; }
	float GetSimulatedMaxAdditionalLoss() const { return m_additionalLossPercentile.maxInclusive; }
	float GetSimulatedMinAdditionalLoss() const { return m_additionalLossPercentile.minInclusive; }

	//Note these rely on the calling commands to validate that min <= max.
	void SetSimulatedMaxAdditionalLag( int ms ) { m_additionalLagSeconds.maxInclusive = ms / 1000.0; }
	void SetSimulatedMinAdditionalLag( int ms ) { m_additionalLagSeconds.minInclusive = ms / 1000.0; }
	void SetSimulatedMaxAdditionalLoss( float lossPercentile01 ) { m_additionalLossPercentile.maxInclusive = lossPercentile01; }
	void SetSimulatedMinAdditionalLoss( float lossPercentile01 ) { m_additionalLossPercentile.minInclusive = lossPercentile01; }
	void SetSimulatedAdditionalLoss( float lossPercentile01 ) { SetSimulatedMinAdditionalLoss( lossPercentile01 ); SetSimulatedMaxAdditionalLoss( lossPercentile01 ); }


private:
	UDPSocket* m_wrappedSocket;

	//Simulated network channel aspect:
	void ActuallyReceivePackets( sockaddr_in* out_fromAddr );
	size_t ActuallyProcessPacket( sockaddr_in* out_fromAddr, void* out_buffer );
	TimeOrderedMap m_inboundPackets; TODO( "Replace with a faster sorted container for frequent insert/remove than a map." );
	
	//Knobs to control lag and loss, respectively.
	Interval<double> m_additionalLagSeconds;
	Interval<float> m_additionalLossPercentile; //Should be a 0-1 percentile!


	ObjectPool< TimeStampedPacket > m_packetMemoryPool;
	TimeStampedPacket* m_lastAllocatedPacket; //To prevent per-frame allocation from above pool.
};
