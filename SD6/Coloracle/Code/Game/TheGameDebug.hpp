#pragma once


//-----------------------------------------------------------------------------
class TextRenderer;


//-----------------------------------------------------------------------------
class TheGameDebug
{
public:
	static TheGameDebug* /*CreateOrGet*/Instance();
	static void Shutdown() { delete s_theGameDebug; }
	void HandleDebug( float deltaSeconds );


private:
	TheGameDebug();
	~TheGameDebug();

	void RenderFramerate( float deltaSeconds );
	void RenderReticle();
	void RenderLeftDebugText2D();
	void RenderRightDebugText2D();

	bool m_wasInDebugModeLastFrame;

	TextRenderer* m_camPosText;
	TextRenderer* m_camOriText;
	TextRenderer* m_camDirText;
	TextRenderer* m_spritesCulledText;
	TextRenderer* m_liveParticlesText;

	static TheGameDebug* s_theGameDebug;
};