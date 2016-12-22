#include "Engine/Renderer/TextRenderer.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/FixedBitmapFont.hpp"
#include "Engine/Renderer/MeshRenderer.hpp"
#include "Engine/Renderer/Vertexes.hpp"
#include "Engine/Renderer/Material.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Shader.hpp"


//--------------------------------------------------------------------------------------------------------------
STATIC const float TextRenderer::DROP_SHADOW_OFFSET = 2.f;
STATIC MeshRenderer* TextRenderer::s_textMeshRenderer = nullptr;
STATIC Material* TextRenderer::s_defaultTextMaterial = nullptr;
STATIC const RenderState TextRenderer::s_defaultTextRenderState = RenderState( CULL_MODE_BACK, BLEND_MODE_SOURCE_ALPHA, BLEND_MODE_ONE_MINUS_SOURCE_ALPHA, DEPTH_COMPARE_MODE_LESS, false );
STATIC FixedBitmapFont* TextRenderer::s_defaultMonospacedFont = nullptr;
STATIC BitmapFont* TextRenderer::s_defaultProportionalFont = nullptr;
STATIC TextRendererRegistry TextRenderer::s_allScreenText;

//--------------------------------------------------------------------------------------------------------------
static const char* defaultVertexShaderSource = "\
#version 410 core\n\
\n\
//Recall, uniforms are 'uniform' across all vertices, where in-variables are unique per-vertex. \n\
uniform mat4 uProj; //Identity for screen space. \n\
uniform mat4 uView; //NO uModel because the text will be in their positions already when we make the meshes. \n\
					//Since we're making these meshes each frame in screen space.\n\
\n\
in vec2 inPosition;\n\
in vec4 inColor;\n\
in vec2 inUV0;\n\
\n\
out vec2 passthroughUV0;\n\
out vec4 passthroughTintColor;\n\
\n\
void main()\n\
{\n\
	passthroughUV0 = inUV0;\n\
	passthroughTintColor = inColor;\n\
\n\
	vec4 pos = vec4( inPosition, 0, 1 ); //1 to preserve translation, 0 to ignore depth.\n\
										 //Could sort by using a value other than 0 for z, rather than sorting by layers.\n\
										 //This would use the depth buffer, but encounters issues of z-fighting, i.e. layers flickering back and forth, whereas painters always just splats one onto the other.\n\
	gl_Position = pos * uView * uProj;\n\
}";


//--------------------------------------------------------------------------------------------------------------
static const char* defaultFragmentShaderSource = "\n\
#version 410 core\n\
\n\
//A layer tint to tint the entire layer could be sent as a uniform, e.g. fade out the UI and game layers on pause.\n\
//Noted that layer effects would all be shaders.\n\
uniform sampler2D uTexDiffuse;\n\
\n\
in vec2 passthroughUV0;\n\
in vec4 passthroughTintColor;\n\
\n\
out vec4 outColor;\n\
\n\
void main()\n\
{\n\
	vec4 diffuseTexColor = texture( uTexDiffuse, passthroughUV0 );\n\
	outColor = diffuseTexColor * passthroughTintColor;\n\
}";


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::Startup()
{
	Shader* defaultVertexShader = Shader::CreateShaderFromSource( defaultVertexShaderSource, strlen( defaultVertexShaderSource ), ShaderType::VERTEX_SHADER );
	Shader* defaultFragmentShader = Shader::CreateShaderFromSource( defaultFragmentShaderSource, strlen( defaultFragmentShaderSource ), ShaderType::FRAGMENT_SHADER );

	const unsigned int THE_RENDERER_DEFAULT_SAMPLER_ID = 1;
	s_defaultTextMaterial = Material::CreateOrGetMaterial( "DefaultTextMaterial", &s_defaultTextRenderState, &Vertex2D_PCT::DEFINITION, "DefaultTextMaterial", defaultVertexShader, defaultFragmentShader );
	s_defaultTextMaterial->SetSampler( "uTexDiffuse", THE_RENDERER_DEFAULT_SAMPLER_ID );
	//MVP settings now under SetupView2D() below, so Renderer can pass arguments to it.

	s_textMeshRenderer = new MeshRenderer( nullptr, nullptr ); //Need mesh to be there before material can be bound on it (to guard against mismatched vertex defns).

	s_defaultProportionalFont = BitmapFont::CreateOrGetFont( "Data/Fonts/Input.fnt" );
	s_defaultMonospacedFont = FixedBitmapFont::CreateOrGetFont( "Data/Fonts/SquirrelFixedFont.png" );
}


//--------------------------------------------------------------------------------------------------------------
TextRenderer::TextRenderer( TextRenderType drawType, bool enableImmediately, float screenX, float screenY, const std::string& contents, float sizeScale /*= .25f*/, const BitmapFont* font /*= nullptr*/, const Rgba& color /*= Rgba()*/, bool drawDropShadow /*= true*/, const Rgba& shadowColor /*= Rgba::BLACK */ )
	: m_drawType( drawType )
	, m_screenPosition( screenX, screenY )
	, m_contents( contents )
	, m_proportionalFont( font )
	, m_color( color )
	, m_sizeScale( sizeScale )
	, m_shadowColor( shadowColor )
	, m_shouldRebuildMesh( true )
	, m_isEnabled( false )
	, m_isVisible( true )
	, m_isShadowDrawn( drawDropShadow ) //new Mesh( BufferUsage::STATIC_DRAW, Vertex2D_PCT::DEFINITION, _countof( quad ), quad, _countof( quadIndices ), quadIndices, 1, quadDrawInstructions );
	, m_textMesh( std::shared_ptr<Mesh>( new Mesh( Vertex2D_PCT::DEFINITION ) ) )
	, m_textShadowMesh( drawDropShadow ? std::shared_ptr<Mesh>( new Mesh( Vertex2D_PCT::DEFINITION ) ) : nullptr ) //No way to assign this later on, so for now no toggling shadows.
{
	if ( font == nullptr )
	{
		m_monospacedFont = s_defaultMonospacedFont;
		m_proportionalFont = s_defaultProportionalFont;
	}

	if ( enableImmediately ) //Not enforced the same for all text objects because TheRenderer uses a never-enabled fire-and-forget version its DrawTextProportional2D.
		this->Enable(); //Else assume user will invoke Enable() when they please.
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::Enable()
{
	if ( !m_isEnabled )
	{
		m_isEnabled = true;
		TextRenderer::Register( this );
	}
	else ERROR_RECOVERABLE( "Enabling an already enabled TextRenderer!" );
}

//--------------------------------------------------------------------------------------------------------------
void TextRenderer::Disable()
{
	if ( m_isEnabled )
	{
		m_isEnabled = false;
		TextRenderer::Unregister( this );
	}
	else ERROR_RECOVERABLE( "Disabling an already disabled TextRenderer!" );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetText( const std::string& newText )
{
	m_contents = newText;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetPosition( const Vector2f& newPos )
{
	m_screenPosition = newPos;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetPosition( float newScreenX, float newScreenY )
{
	m_screenPosition.x = newScreenX;
	m_screenPosition.y = newScreenY;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetDrawType( TextRenderType val )
{
	m_drawType = val;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetFont( const BitmapFont* font )
{
	m_proportionalFont = font;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetSizeScale( float scale )
{
	m_sizeScale = scale;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetColor( const Rgba& newColor )
{
	m_color = newColor;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
void TextRenderer::SetShadowColor( const Rgba& newColor )
{
	m_shadowColor = newColor;
	SetDirty( true );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::SetupView2D( const Vector2f& bottomLeft, const Vector2f& topRight )
{
	s_defaultTextMaterial->SetMatrix4x4( "uView", false, &Matrix4x4f::IDENTITY );

	Matrix4x4f ortho( COLUMN_MAJOR );
	ortho.ClearToOrthogonalProjection( bottomLeft.x, topRight.x, bottomLeft.y, topRight.y, -1.f, 1.f, ortho.GetOrdering() );
	s_defaultTextMaterial->SetMatrix4x4( "uProj", false, &ortho );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::UpdateAllScreenText()
{
	for each ( TextRenderer* textRenderer in s_allScreenText )
		TextRenderer::UpdateText( textRenderer );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::RenderAllScreenText()
{
	for each ( TextRenderer* textRenderer in s_allScreenText )
		TextRenderer::RenderText( textRenderer );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::CleanupAllScreenText()
{
	//Material cleaned up by its registry.

	delete s_textMeshRenderer;
	s_textMeshRenderer = nullptr;

	for each ( TextRenderer* textRenderer in s_allScreenText )
		delete textRenderer;
	s_allScreenText.clear();

	//Font objects will be cleaned up in TheRenderer::DeleteFonts.
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::Register( TextRenderer* newText )
{
	TODO( "Graduate this system to use render layers and layer IDs like SpriteRenderer." );
	s_allScreenText.push_back( newText );
}


//--------------------------------------------------------------------------------------------------------------
STATIC bool TextRenderer::Unregister( TextRenderer* newText )
{
	for ( TextRendererRegistry::iterator textIter = s_allScreenText.begin(); textIter != s_allScreenText.end(); ++textIter )
	{
		if ( *textIter == newText )
		{
			s_allScreenText.erase( textIter );
			return true;
		}
	}
	return false;
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::RenderText( TextRenderer* textRenderer )
{
	if ( !textRenderer->IsVisible() )
		return;

	if ( textRenderer->m_shouldRebuildMesh )
	{
		UpdateText( textRenderer ); //Do to the scattered nature of DrawText calls, not sure a strict Update->Render order can be globally enforced here.
//		ERROR_RECOVERABLE( "Found text renderer in need of rebuilding! Please call TextRenderer::UpdateAllScreenText by calling TheRenderer::Update!" );
//		return;
	}

	//Assuming that textMat is using identity matrix for its projection and view matrices == drawing in screen space.

	//Update text material to use the current TextRenderer object's font's diffuse texture.
	Texture* fontTexture = ( textRenderer->m_drawType == TEXT_MONOSPACED_2D )
		? textRenderer->m_monospacedFont->GetFontTexture()
		: textRenderer->m_proportionalFont->GetFontTexture(); //Assuming just one font texture page for now.
	s_defaultTextMaterial->SetTexture( "uTexDiffuse", fontTexture->GetTextureID() ); 

	if ( textRenderer->m_isShadowDrawn )
	{
		TextRenderer::s_textMeshRenderer->SetMesh( textRenderer->m_textShadowMesh );
		s_textMeshRenderer->SetMaterial( s_defaultTextMaterial, true );
		TextRenderer::s_textMeshRenderer->Render(); //Uses same material as above, the color differences are in the vertex attribute data.
	}

	TextRenderer::s_textMeshRenderer->SetMesh( textRenderer->m_textMesh );
	s_textMeshRenderer->SetMaterial( s_defaultTextMaterial, true ); //Needs to be rebound on the GPU side, for the VBO of the new mesh!
	TextRenderer::s_textMeshRenderer->Render();

//	TextRenderer::s_textMeshRenderer->SetMesh( nullptr ); //Enforces dropping our shared_ptr ref.
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::UpdateText( TextRenderer* textRenderer )
{
	if ( !textRenderer->m_shouldRebuildMesh )
		return;

	if ( textRenderer->m_contents.size() < 1 )
		return; //No need to go through the trouble for a zero-vertex mesh.

	switch ( textRenderer->m_drawType )
	{
		case TEXT_PROPORTIONAL_2D: RebuildTextAsProportional2D( textRenderer ); break;
		case TEXT_MONOSPACED_2D: RebuildTextAsMonospaced2D( textRenderer ); break;
		case TEXT_PROPORTIONAL_3D: RebuildTextAsProportional3D( textRenderer ); break;
		default: break;
	}

	textRenderer->SetDirty( false );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::RebuildTextAsProportional2D( TextRenderer* textRenderer )
{
	const BitmapFont* font = textRenderer->m_proportionalFont;
	const std::string& inputText = textRenderer->m_contents;
	float scale = textRenderer->m_sizeScale;
	Rgba textColor = textRenderer->m_color;
	bool isUpdatingShadow = textRenderer->m_isShadowDrawn;
	Vector2f shadowOffset = Vector2f( DROP_SHADOW_OFFSET, -DROP_SHADOW_OFFSET );
	Rgba shadowColor = textRenderer->m_shadowColor;	
	std::vector< unsigned int > indices;
	std::vector< Vertex2D_PCT > vertices, shadowVertices; //Note same IBO used, since only position/color change.

	std::shared_ptr<Mesh> textMesh = textRenderer->m_textMesh;
	std::shared_ptr<Mesh> textShadowMesh = textRenderer->m_textShadowMesh;
	textMesh->ClearDrawInstructions();
	if ( isUpdatingShadow )
		textShadowMesh->ClearDrawInstructions();

	if ( font == nullptr )
		font = s_defaultProportionalFont;	

	Vector2f cursor = textRenderer->m_screenPosition; //Assuming its the LOWER-left.

	const Glyph* previousGlyph = nullptr; //For kerning check.

	for ( unsigned int charIndex = 0; charIndex < inputText.size(); charIndex++ )
	{
		char c = inputText[ charIndex ];
		const Glyph* currentGlyph = font->GetGlyphForChar( c ); //Use m_fontGlyphs.

		if ( currentGlyph == nullptr )
			continue; //Skips unsupported chars.

		float kerningAmount; //BMFont only does kerning horizontally.
		if ( previousGlyph != nullptr )
		{
			kerningAmount = previousGlyph->GetKerning( currentGlyph->m_id );
			cursor.x += ( kerningAmount * scale ); //	+ ( kerning.y * scale );
		}

		const Vector2f offset = Vector2f( static_cast<float>( currentGlyph->m_xoffset ), static_cast<float>( currentGlyph->m_yoffset ) ); //"caching" off is good!
		const Vector2f size = Vector2f( static_cast<float>( currentGlyph->m_width ), static_cast<float>( currentGlyph->m_height ) ); //in px, not uv == normalized texel coordinates

		//Remember: in 2D, y+ is up, x+ is right. But positive yoffset means DOWN, though positive xoffset still means right.
		const Vector2f topLeftCorner		= Vector2f( cursor.x + offset.x * scale, cursor.y - offset.y * scale );
		const Vector2f bottomLeftCorner		= Vector2f( topLeftCorner.x,			topLeftCorner.y - ( size.y * scale ) );
		const Vector2f bottomRightCorner	= Vector2f( bottomLeftCorner.x + ( size.x * scale ), bottomLeftCorner.y );
		const Vector2f topRightCorner		= Vector2f( topLeftCorner.x + ( size.x * scale ),	topLeftCorner.y );

		AABB2f texCoords = font->GetTexCoordsForGlyph( currentGlyph->m_id );

		vertices.push_back( Vertex2D_PCT( topRightCorner,	 Vector2f( texCoords.maxs.x, texCoords.mins.y ), textColor ) );
		vertices.push_back( Vertex2D_PCT( bottomLeftCorner,	 Vector2f( texCoords.mins.x, texCoords.maxs.y ), textColor ) );
		vertices.push_back( Vertex2D_PCT( bottomRightCorner, Vector2f( texCoords.maxs.x, texCoords.maxs.y ), textColor ) );
		vertices.push_back( Vertex2D_PCT( topLeftCorner,	 Vector2f( texCoords.mins.x, texCoords.mins.y ), textColor ) );
		unsigned int currentNumVerts = vertices.size() - 4; //We want to advance every iteration through the loop, but ignore what was just pushed.
			//Else we'll start at 4+0, instead of 0+0 when pushing back indices!

		//We need to send the IBO for a quad out, per each letter in the string. Might try to optimize out duplicated indices for adjacent letters?
		indices.push_back( currentNumVerts + 0 );
		indices.push_back( currentNumVerts + 1 );
		indices.push_back( currentNumVerts + 2 );

		indices.push_back( currentNumVerts + 3 );
		indices.push_back( currentNumVerts + 1 );
		indices.push_back( currentNumVerts + 0 ); //Pushed in accordance with DrawAABB's quadIndicesCounterClockwise.

		if ( isUpdatingShadow )
		{
			shadowVertices.push_back( Vertex2D_PCT( topRightCorner + shadowOffset,		Vector2f( texCoords.maxs.x, texCoords.mins.y ), shadowColor ) );
			shadowVertices.push_back( Vertex2D_PCT( bottomLeftCorner + shadowOffset,	Vector2f( texCoords.mins.x, texCoords.maxs.y ), shadowColor ) );
			shadowVertices.push_back( Vertex2D_PCT( bottomRightCorner + shadowOffset,	Vector2f( texCoords.maxs.x, texCoords.maxs.y ), shadowColor ) );
			shadowVertices.push_back( Vertex2D_PCT( topLeftCorner + shadowOffset,		Vector2f( texCoords.mins.x, texCoords.mins.y ), shadowColor ) );
		}

		cursor.x += ( currentGlyph->m_xadvance * scale ); //Move to next glyph.

		previousGlyph = currentGlyph; //For next kerning check.
	}

	textMesh->AddDrawInstruction( VertexGroupingRule::AS_TRIANGLES, 0, indices.size(), true ); //THE REAL SAVE--DRAWING IT ALL IN ONE glDrawElements COMMAND!
	textMesh->SetThenUpdateMeshBuffers( vertices.size(), vertices.data(), indices.size(), indices.data() );

	if ( isUpdatingShadow )
	{
		textShadowMesh->AddDrawInstruction( VertexGroupingRule::AS_TRIANGLES, 0, indices.size(), true ); //THE REAL SAVE--DRAWING IT ALL IN ONE glDrawElements COMMAND!
		textShadowMesh->SetThenUpdateMeshBuffers( shadowVertices.size(), shadowVertices.data(), indices.size(), indices.data() );
	}
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::RebuildTextAsProportional3D( TextRenderer* )
{
	TODO( "RebuildTextAsProportional3D" );
	ERROR_AND_DIE( "Unimplemented! Move from TheRenderer!" );
}


//--------------------------------------------------------------------------------------------------------------
STATIC void TextRenderer::RebuildTextAsMonospaced2D( TextRenderer* )
{
	TODO( "RebuildTextAsMonospaced2D" );
	ERROR_AND_DIE( "Unimplemented! Move from TheRenderer!" );
}
