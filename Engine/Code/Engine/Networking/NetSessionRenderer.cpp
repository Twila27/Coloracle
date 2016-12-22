#include "Engine/Networking/NetSession.hpp"
#include "Engine/Networking/PacketChannel.hpp"
#include "Engine/Renderer/TheRenderer.hpp"
#include "Engine/Core/TheConsole.hpp"


//--------------------------------------------------------------------------------------------------------------
void NetSession::RenderDebug()
{
	if ( m_myPacketChannel == nullptr )
		return;

	float uniformStrHeight = g_theRenderer->CalcTextPxHeight( "@" ); //Just so we don't recalc each string below.

	const int MAX_ADDRSTR_LENGTH = 256;
	char address[ MAX_ADDRSTR_LENGTH ];
	m_myPacketChannel->GetConnectionAddress( address, MAX_ADDRSTR_LENGTH );

	double maxLagMs = 1000.0 * m_myPacketChannel->GetSimulatedMaxAdditionalLag();
	double minLagMs = 1000.0 * m_myPacketChannel->GetSimulatedMinAdditionalLag();
	double maxLoss = 100.0 * m_myPacketChannel->GetSimulatedMaxAdditionalLoss();
	double minLoss = 100.0 * m_myPacketChannel->GetSimulatedMinAdditionalLoss();

	int connCount = GetNumConnected();

	float currentTop = uniformStrHeight * ( 3 + connCount ) + 10; //(3 below fixed strs + x connection strs).
	if ( g_theConsole->IsVisible() )
		currentTop += g_theConsole->GetCurrentLogBoxTopLeft().y; //height is 0 at bottom of screen.
	float currentLeft = 50;

	StateNameStr stateStr = m_sessionStateMachine.GetCurrentState()->GetName();
	g_theRenderer->DrawTextProportional2D( Vector2f( currentLeft, currentTop ), Stringf("Session (State=%s) Bounds to %s", stateStr, address ) );
	currentTop -= uniformStrHeight;

	const std::string& simStr = Stringf( "Sim Lag: [%.0f,%.0f) ms - SimLoss: [%.2f%%,%.2f%%]", minLagMs, maxLagMs, minLoss, maxLoss );
	g_theRenderer->DrawTextProportional2D( Vector2f( currentLeft, currentTop ),	simStr );
	currentTop -= uniformStrHeight;

	g_theRenderer->DrawTextProportional2D( Vector2f( currentLeft, currentTop ), Stringf("Connection Count: %d/%d", connCount, MAX_CONNECTIONS ) );
	currentTop -= uniformStrHeight;

	for each ( NetConnection* conn in m_connections )
	{
		if ( conn == nullptr )
			continue;
		g_theRenderer->DrawTextProportional2D( Vector2f( currentLeft, currentTop ), conn->GetDebugString(), .18f );
		currentTop -= uniformStrHeight;
	}
}