#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Engine/Memory/Memory.hpp"
#include "Engine/Core/Logger.hpp"
#include "Engine/Tools/Profiling/Profiler.hpp"
#include "Engine/Tools/Profiling/ProfileLogSection.hpp"
#include "Engine/Concurrency/JobUtils.hpp"

#include "Game/TheApp.hpp"
#include "Engine/TheEngine.hpp"



//--------------------------------------------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE applicationInstanceHandle, HINSTANCE, LPSTR commandLineString, int )
{
	UNREFERENCED_PARAMETER( commandLineString );

	const int NUM_WORKER_THREADS = -4; //The negative implies "save 4, give me all the rest."

	Logger::Startup(); //Might be able to move into engine.
	MemoryAnalytics::Startup(); //Here rather than in app/engine to track allocation of the app/engine themselves.
	Profiler::Instance()->Startup();
	JobSystem::Instance()->Startup( NUM_WORKER_THREADS );

	g_theApp = new TheApp();

	g_theApp->Startup( applicationInstanceHandle );

	const bool SHOULD_RENDER = false;

	while ( !g_theApp->IsQuitting() )
	{
		g_theApp->HandleInput();
		Profiler::Instance()->StartFrame();
		g_theEngine->RunFrame( SHOULD_RENDER ); //Handled by clients in this game.
		g_theApp->FlipAndPresent();
	}

	g_theApp->Shutdown();

	delete g_theApp;
	g_theApp = nullptr;

	JobSystem::Instance()->Shutdown();
	Profiler::Instance()->Shutdown();
	MemoryAnalytics::Shutdown();
	Logger::Shutdown();

	return 0;
}