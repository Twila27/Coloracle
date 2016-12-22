#pragma once


#include "Engine/Math/Vector2.hpp"
#include "Engine/Renderer/Rgba.hpp"
#include "Engine/Renderer/Mesh.hpp"
#include "Engine/Renderer/RenderState.hpp"
#include "Engine/Memory/UntrackedAllocator.hpp"
#include <memory>


//-----------------------------------------------------------------------------
class BitmapFont;
class FixedBitmapFont;
class Material;
class MeshRenderer;
class TextRenderer;
typedef std::vector<TextRenderer*, UntrackedAllocator<TextRenderer*>> TextRendererRegistry;


//-----------------------------------------------------------------------------
enum TextRenderType
{
	TEXT_PROPORTIONAL_2D,
	TEXT_MONOSPACED_2D,
	TEXT_PROPORTIONAL_3D
};


//-----------------------------------------------------------------------------
class TextRenderer
{
public:
	TextRenderer( TextRenderType drawType, bool enableImmediately, float screenX, float screenY, const std::string& contents, float sizeScale = .25f, const BitmapFont* font = nullptr, const Rgba& color = Rgba(), bool drawDropShadow = true, const Rgba& shadowColor = Rgba::BLACK );
	TextRenderer( TextRenderType drawType, bool enableImmediately, const Vector2f& screenPos, const std::string& contents, float sizeScale = .25f, const BitmapFont* font = nullptr, const Rgba& color = Rgba(), bool drawDropShadow = true, const Rgba& shadowColor = Rgba::BLACK )
		: TextRenderer( drawType, enableImmediately, screenPos.x, screenPos.y, contents, sizeScale, font, color, drawDropShadow, shadowColor ) {}

	bool IsEnabled() const { return m_isEnabled; }
	void Enable(); //Borrowed from Sprite::Enable's paradigm.
	void Disable();

	bool IsVisible() const { return m_isVisible; }
	void Show() { m_isVisible = true; }
	void Hide() { m_isVisible = false; }

	void SetText( const std::string& newText );
	void SetPosition( const Vector2f& newPos );
	void SetPosition( float newScreenX, float newScreenY );
	void SetDrawType( TextRenderType val );
	void SetFont( const BitmapFont* font );
	void SetSizeScale( float scale );
	void SetColor( const Rgba& newColor );
	void SetShadowColor( const Rgba& newShadowColor );

	static void UpdateAllScreenText(); //Rebuilds happen here. Recommended to call TheRenderer::Update instead of this, else it'll likely get called twice.
	static void UpdateText( TextRenderer* );
	static void RenderAllScreenText();
	static void RenderText( TextRenderer* );
	static void CleanupAllScreenText();
	static void Startup(); //Called in TheRenderer's PreGameStartup to initialize static default rendering components.
	static void SetupView2D( const Vector2f& newBottomLeft, const Vector2f& newTopRight );


private:
	static void Register( TextRenderer* );
	static bool Unregister( TextRenderer* );
	
	static void RebuildTextAsProportional2D( TextRenderer* );
	static void RebuildTextAsProportional3D( TextRenderer* );
	static void RebuildTextAsMonospaced2D( TextRenderer* );

	void SetDirty( bool newVal ) { m_shouldRebuildMesh = newVal; }
	
	TextRenderType m_drawType;
	Vector2f m_screenPosition;
	Rgba m_color;
	float m_sizeScale;
	const BitmapFont* m_proportionalFont;
	const FixedBitmapFont* m_monospacedFont;
	std::string m_contents;
	Rgba m_shadowColor;
	
	bool m_shouldRebuildMesh; //Dirty flag.
	bool m_isEnabled;
	bool m_isVisible;
	bool m_isShadowDrawn;

	std::shared_ptr<Mesh> m_textMesh;
	std::shared_ptr<Mesh> m_textShadowMesh;
//	Material* m_textMaterial; //Having all text use the default for now.

	static FixedBitmapFont* s_defaultMonospacedFont;
	static BitmapFont* s_defaultProportionalFont;
	static const float DROP_SHADOW_OFFSET;

	static TextRendererRegistry s_allScreenText;
	static MeshRenderer* s_textMeshRenderer;
	static Material* s_defaultTextMaterial;
	const static RenderState s_defaultTextRenderState;
};
