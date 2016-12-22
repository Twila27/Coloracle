#pragma once


//--------------------------------------------------------------------------------------------------------------
class TheEngine
{
public:
	void RunFrame( bool shouldRender = true );
	void Startup( double screenWidth, double screenHeight );
	void Shutdown();
	
private:
	void Render();
	void Update( float deltaSeconds );

	void RenderDebug3D();
	void RenderDebug2D();

	void RenderLeftDebugText2D();
	void RenderRightDebugText2D();
	void RenderDebugMemoryWindow();
};


//--------------------------------------------------------------------------------------------------------------
extern TheEngine* g_theEngine;
