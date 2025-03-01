// ================================================================================
// ==      This file is a part of Turbo Badger. (C) 2011-2014, Emil Segerås      ==
// ==                     See tb_core.h for more information.                    ==
// ================================================================================

#include "tb_renderer_gl.h"

#ifdef TB_RENDERER_GL
#include "tb_bitmap_fragment.h"
#include "tb_system.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

namespace tb {

#ifdef TB_RUNTIME_DEBUG_INFO
uint32_t dbg_bitmap_validations = 0;
#endif // TB_RUNTIME_DEBUG_INFO

// == Utilities ===================================================================================

#if defined(TB_RUNTIME_DEBUG_INFO) && !(defined(__APPLE__) && TARGET_OS_IPHONE)
#define GLCALL(CALL) do {												\
		CALL;															\
		GLenum err;														\
		while ((err = glGetError()) != GL_NO_ERROR) {					\
			TBDebugPrint("%s:%d, GL error 0x%x\n", __FILE__, __LINE__, err); \
		} } while (0)
#else
#define GLCALL(xxx) do { xxx; } while (0)
#endif

#if !defined(TB_RENDERER_GLES_2) && !defined(TB_RENDERER_GL3)
static void Ortho2D(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top)
{
#ifdef TB_RENDERER_GLES_1
	glOrthof(left, right, bottom, top, -1.0, 1.0);
#else
	GLCALL(glOrtho(left, right, bottom, top, -1.0, 1.0));
#endif
}

#else

static void MakeOrtho(float * ortho, float l, float r, float b, float t, float n, float f)
{
	ortho[0] = 2 / (r - l);
	ortho[1] = 0;
	ortho[2]  = 0;
	ortho[3] = 0;

	ortho[4] = 0;
	ortho[5] = 2 / (t - b);
	ortho[6]  = 0;
	ortho[7] = 0;

	ortho[8] = 0;
	ortho[9] = 0;
	ortho[10] = -2 / (f - n);
	ortho[11] = 0;

	ortho[12] = -(r+l)/(r-l);
	ortho[13] = -(t+b)/(t-b);
	ortho[14] = -(f+n)/(f-n);
	ortho[15] = 1;
}
#endif

#if defined(TB_RENDERER_GLES_2)
static bool gl_supports_ext(const char * extname)
{
	const char * fullext = (const char *)glGetString(GL_EXTENSIONS);
	if (fullext) {
		return strstr(fullext, extname) != NULL;
	}
#if defined(GL_NUM_EXTENSIONS)
	else {
		int NumberOfExtensions;
		bool supported = false;
		glGetError(); // clear error from glGetString()
		// try individually
		GLCALL(glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions));
		for (int i = 0; i < NumberOfExtensions; i++) {
			const char *ccc = (const char *)glGetStringi(GL_EXTENSIONS, i);
			if (!ccc)
				continue;
			if (!strcmp(ccc, extname))
				supported = true;
		}
		return supported;
	}
#else
    return false;
#endif
}
#endif

// == Batching ====================================================================================

void TBRendererGL::BindBitmap(TBBitmap *bitmap)
{
	GLuint texture = bitmap ? static_cast<TBBitmapGL*>(bitmap)->m_texture : 0;
	if (texture != m_current_texture)
	{
		m_current_texture = texture;
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
		GLCALL(glActiveTexture(GL_TEXTURE0));
#endif
		GLCALL(glBindTexture(GL_TEXTURE_2D, m_current_texture));
	}
}

// == TBBitmapGL ==================================================================================

TBBitmapGL::TBBitmapGL(TBRendererGL *renderer)
	: m_renderer(renderer), m_w(0), m_h(0), m_texture(0)
{
}

TBBitmapGL::~TBBitmapGL()
{
	// Must flush and unbind before we delete the texture
	if (m_renderer) {
		m_renderer->FlushBitmap(this);
		if (m_texture == m_renderer->m_current_texture)
			m_renderer->BindBitmap(nullptr);
	}

	GLCALL(glDeleteTextures(1, &m_texture));
}

bool TBBitmapGL::Init(int width, int height, uint32_t *data)
{
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
#else
	assert(width == TBGetNearestPowerOfTwo(width));
	assert(height == TBGetNearestPowerOfTwo(height));
#endif

	m_w = width;
	m_h = height;

	GLCALL(glGenTextures(1, &m_texture));
	m_renderer->BindBitmap(this);
	GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GLCALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

	SetData(data);

	return true;
}

void TBBitmapGL::SetData(uint32_t *data)
{
	m_renderer->FlushBitmap(this);
	m_renderer->BindBitmap(this);
	GLCALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_w, m_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data));
	//GLCALL(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_w, m_h, GL_RGBA, GL_UNSIGNED_BYTE, data));
	TB_IF_DEBUG_SETTING(RENDER_BATCHES, dbg_bitmap_validations++);
}

// == TBRendererGL ================================================================================

TBRendererGL::TBRendererGL()
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	: m_white(this),
	  m_current_texture(-1),
	  m_current_batch(nullptr)
#endif
{
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	GLchar vertexShaderString[] =  
#if defined(TB_RENDERER_GL3)
		"#version 150                          \n"
		"#define attribute in                  \n"
		"#define varying out                   \n"
#endif
		"attribute vec2 xy;                    \n"
		"attribute vec2 uv;                    \n"
		"attribute vec4 col;                   \n"
		"uniform mat4 ortho;                   \n"
		"uniform sampler2D tex;                \n"
		"varying vec2 uvo;                     \n"
		"varying lowp vec4 color;              \n"
		"void main()                           \n"
		"{                                     \n"
		"  gl_Position = ortho * vec4(xy,0,1); \n"
		"  uvo = uv;                           \n"
		"  color = col;                        \n"
		"}                                     \n";
	GLchar fragmentShaderString[] =
#if defined(TB_RENDERER_GL3)
		"#version 150                                  \n"
		"#define varying in                            \n"
		"out vec4 fragData[1];                         \n"
		"#define gl_FragColor fragData[0]              \n"
		"#define texture2D texture                     \n"
#endif
		"precision mediump float;                      \n"
		"uniform sampler2D tex;                        \n"
		"varying vec2 uvo;                             \n"
		"varying lowp vec4 color;                      \n"
		"void main()                                   \n"
		"{                                             \n"
		"  gl_FragColor = color * texture2D(tex, uvo); \n"
		"}                                             \n";

#if defined(TB_SYSTEM_WINDOWS)
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		TBDebugOut("glewInit failed.\n");
		return;
	}
#endif

	GLuint vertexShader;
	GLuint fragmentShader;
	GLint linked;

	vertexShader = LoadShader(GL_VERTEX_SHADER, vertexShaderString);
	fragmentShader = LoadShader(GL_FRAGMENT_SHADER, fragmentShaderString);

	m_program = glCreateProgram();
	if (m_program == 0)
	{
		TBDebugOut("glCreateProgram failed.\n");
		return;
	}

	glAttachShader(m_program, vertexShader);
	glAttachShader(m_program, fragmentShader);
	GLCALL(glBindAttribLocation(m_program, 0, "xy"));
	GLCALL(glBindAttribLocation(m_program, 1, "uv"));
	GLCALL(glBindAttribLocation(m_program, 2, "color"));
	glLinkProgram(m_program);
	glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		GLint infoLen = 0;
		glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char * infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetProgramInfoLog(m_program, infoLen, nullptr, infoLog);
			TBDebugPrint("Error linking program:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteProgram(m_program);
		TBDebugOut("glLinkProgram failed.\n");
		return;
	}

	m_orthoLoc = glGetUniformLocation(m_program, "ortho");
	m_texLoc = glGetUniformLocation(m_program, "tex");

#if defined(TB_RENDERER_GL3)
	m_hasvao = true;
#elif defined(TB_RENDERER_GLES_2)
	if (gl_supports_ext("OES_vertex_array_object") || gl_supports_ext("GL_ARB_vertex_array_object"))
		m_hasvao = false;
#else
    m_hasvao = false;
#endif

#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
    if (m_hasvao) {
		GLCALL(glGenVertexArrays(_NUM_VBOS, m_vao));
	}

	// Generate & allocate GL_ARRAY_BUFFER buffers
	_vboidx = 0;
	GLCALL(glGenBuffers(_NUM_VBOS, m_vbo));
	for (unsigned int ii = 0; ii < _NUM_VBOS; ii++) {
		if (m_hasvao)
			GLCALL(glBindVertexArray(m_vao[ii]));
		GLCALL(glBindBuffer(GL_ARRAY_BUFFER, m_vbo[ii]));
		GLCALL(glBufferData(GL_ARRAY_BUFFER, sizeof(batch.vertex), nullptr, GL_STREAM_DRAW)); // or DYNAMIC?
		GLCALL(glEnableVertexAttribArray(0));
		GLCALL(glEnableVertexAttribArray(1));
		GLCALL(glEnableVertexAttribArray(2));
		GLCALL(glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, sizeof(Vertex), &((Vertex *)nullptr)->x));
		GLCALL(glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(Vertex), &((Vertex *)nullptr)->u));
		GLCALL(glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(Vertex), &((Vertex *)nullptr)->col));
	}
#endif

	// Setup white 1-pixel "texture" as default
	{
		uint32_t whitepix = 0xffffffff;
		m_white.Init(1, 1, &whitepix);
	}
#endif // defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
}

TBRendererGL::~TBRendererGL()
{
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	GLCALL(glDeleteBuffers(_NUM_VBOS, m_vbo));
#endif
}

#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
GLuint TBRendererGL::LoadShader(GLenum type, const GLchar *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	shader = glCreateShader(type);
	if (shader == 0)
		return 0;

	glShaderSource(shader, 1, &shaderSrc, nullptr);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if (infoLen > 1)
		{
			char * infoLog = (char *)malloc(sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
			TBDebugPrint("Error compiling shader:\n%s\n", infoLog);
			free(infoLog);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}
#endif

void TBRendererGL::BeginPaint(int render_target_w, int render_target_h)
{
#ifdef TB_RUNTIME_DEBUG_INFO
	dbg_bitmap_validations = 0;
#endif
	//TBDebugPrint("Frame start idx %d.\n", _vboidx);

	TBRendererBatcher::BeginPaint(render_target_w, render_target_h);

	m_current_texture = (GLuint)-1;
	m_current_batch = nullptr;

#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	MakeOrtho(m_ortho, 0, (GLfloat)render_target_w, (GLfloat)render_target_h, 0, -1.0, 1.0);
#else
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	Ortho2D(0, (GLfloat)render_target_w, (GLfloat)render_target_h, 0);
	glMatrixMode(GL_MODELVIEW);
#endif

	glViewport(0, 0, render_target_w, render_target_h);
	glScissor(0, 0, render_target_w, render_target_h);

	GLCALL(glEnable(GL_BLEND));
#if !defined(TB_RENDERER_GLES_2) && !defined(TB_RENDERER_GL3)
	GLCALL(glEnable(GL_TEXTURE_2D));
#endif
	GLCALL(glDisable(GL_DEPTH_TEST));
	GLCALL(glEnable(GL_SCISSOR_TEST));
	GLCALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	//GLCALL(glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA));
	//GLCALL(glAlphaFunc(GL_GREATER, 0.1));
	//GLCALL(glEnable(GL_ALPHA_TEST));

#if !defined(TB_RENDERER_GLES_2) && !defined(TB_RENDERER_GL3)
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
#else
	//GLCALL(glEnableVertexAttribArray(0));
	//GLCALL(glEnableVertexAttribArray(1));
	//GLCALL(glEnableVertexAttribArray(2));
#endif
}

void TBRendererGL::EndPaint()
{
	TBRendererBatcher::EndPaint();
	GLCALL(glDisable(GL_BLEND));
	GLCALL(glDisable(GL_SCISSOR_TEST));
	GLCALL(glFlush());
#ifdef TB_RUNTIME_DEBUG_INFO
	if (TB_DEBUG_SETTING(RENDER_BATCHES))
		TBDebugPrint("Frame caused %d bitmap validations.\n", dbg_bitmap_validations);
#endif // TB_RUNTIME_DEBUG_INFO
	//TBDebugPrint("Frame end idx %d.\n", _vboidx);
}

TBBitmap *TBRendererGL::CreateBitmap(int width, int height, uint32_t *data)
{
	TBBitmapGL *bitmap = new TBBitmapGL(this);
	if (!bitmap || !bitmap->Init(width, height, data))
	{
		delete bitmap;
		return nullptr;
	}
	return bitmap;
}

void TBRendererGL::RenderBatch(Batch *batch)
{
	// Bind texture and array pointers
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	GLCALL(glUseProgram(m_program));
	GLCALL(glUniformMatrix4fv(m_orthoLoc, 1, GL_FALSE, m_ortho));
	BindBitmap(batch->bitmap ? batch->bitmap : &m_white);
#else
	BindBitmap(batch->bitmap);
#endif

	if (m_current_batch != batch)
	{
#if !defined(TB_RENDERER_GLES_2) && !defined(TB_RENDERER_GL3)
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void *) &batch->vertex[0].r);
		glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void *) &batch->vertex[0].u);
		glVertexPointer(2, GL_FLOAT, sizeof(Vertex), (void *) &batch->vertex[0].x);
#endif
		m_current_batch = batch;
	}

	// Flush
#if defined(TB_RENDERER_GLES_2) || defined(TB_RENDERER_GL3)
	_vboidx = (_vboidx + 1) % _NUM_VBOS;
	if (m_hasvao) {
		GLCALL(glBindVertexArray(m_vao[_vboidx]));
		GLCALL(glBindBuffer(GL_ARRAY_BUFFER, m_vbo[_vboidx]));
		GLCALL(glBufferSubData(GL_ARRAY_BUFFER, 0, batch->vertex_count * sizeof(Vertex), (void *)&batch->vertex[0]));
	}
	else {
		GLCALL(glBindBuffer(GL_ARRAY_BUFFER, m_vbo[_vboidx]));
		GLCALL(glBufferSubData(GL_ARRAY_BUFFER, 0, batch->vertex_count * sizeof(Vertex), (void *)&batch->vertex[0]));
		GLCALL(glEnableVertexAttribArray(0));
		GLCALL(glEnableVertexAttribArray(1));
		GLCALL(glEnableVertexAttribArray(2));
		GLCALL(glVertexAttribPointer(0, 2, GL_FLOAT,         GL_FALSE, sizeof(Vertex), &((Vertex *)nullptr)->x));
		GLCALL(glVertexAttribPointer(1, 2, GL_FLOAT,         GL_FALSE, sizeof(Vertex), &((Vertex *)nullptr)->u));
		GLCALL(glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(Vertex), &((Vertex *)nullptr)->col));
	}
#endif
	GLCALL(glDrawArrays(GL_TRIANGLES, 0, batch->vertex_count));
	//TBDebugPrint("Batch: %d\n", batch->vertex_count);
}

void TBRendererGL::SetClipRect(const TBRect & /*rect*/)
{
	GLCALL(glScissor(m_clip_rect.x, m_screen_rect.h - (m_clip_rect.y + m_clip_rect.h), m_clip_rect.w, m_clip_rect.h));
}

} // namespace tb

#endif // TB_RENDERER_GL
