#include "precompiled.h"

#include "lib.h"
#include "sdl.h"
#include "ogl.h"
#include "detect.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
	// cannot get rid of glu32 - seems to be loaded by opengl32,
	// even though dependency walker marks it as demand-loaded.
#endif


// define extension function pointers
extern "C"
{
#define FUNC(ret, name, params) ret (CALL_CONV *name) params;
#include "glext_funcs.h"
#undef FUNC
}


static const char* exts;


// check if the extension <ext> is supported by the OpenGL implementation
bool oglExtAvail(const char* ext)
{
	assert(exts && "call oglInit before using this function");

	const char *p = exts, *end;

	// make sure ext is valid & doesn't contain spaces
	if(!ext || ext == '\0' || strchr(ext, ' '))
		return false;

	for(;;)
	{
		p = strstr(p, ext);
		if(!p)
			return false; // <ext> string not found - extension not supported
		end = p + strlen(ext); // end of current substring

		// make sure the substring found is an entire extension string,
		// i.e. it starts and ends with ' '
		if(p == exts || *(p-1) == ' ')		// valid start?
			if(*end == ' ' || *end == '\0') // valid end?
				return true;
		p = end;
	}
}


void oglPrintErrors()
{
#define E(e) case e: printf("%s\n", #e); break;

	for(;;)
		switch(glGetError())
		{
		case GL_NO_ERROR:
			return;

E(GL_INVALID_ENUM)
E(GL_INVALID_VALUE)
E(GL_INVALID_OPERATION)
E(GL_STACK_OVERFLOW)
E(GL_STACK_UNDERFLOW)
E(GL_OUT_OF_MEMORY)
		}
}


int max_tex_size;				// [pixels]
int tex_units;
int max_VAR_elements = -1;		// GF2: 64K; GF3: 1M
bool tex_compression_avail;		// S3TC / DXT{1,3,5}
int video_mem;					// [MB]; approximate


// gfx_card and gfx_drv_ver are unchanged on failure.
int ogl_get_gfx_info()
{
	const char* vendor   = (const char*)glGetString(GL_VENDOR);
	const char* renderer = (const char*)glGetString(GL_RENDERER);
	const char* version  = (const char*)glGetString(GL_VERSION);

	// can fail if OpenGL not yet initialized,
	// or if called between glBegin and glEnd.
	if(!vendor || !renderer || !version)
		return -1;

	strncpy(gfx_card, vendor, sizeof(gfx_card)-1);

	// reduce string to "ATI" or "NVIDIA"
	if(!strcmp(gfx_card, "ATI Technologies Inc."))
		gfx_card[3] = 0;
	if(!strcmp(gfx_card, "NVIDIA Corporation"))
		gfx_card[6] = 0;

	strcat(gfx_card, renderer);
		// don't bother cutting off the crap at the end.
		// too risky, and too many different strings.

	snprintf(gfx_drv_ver, sizeof(gfx_drv_ver), "OpenGL %s", version);
		// add "OpenGL" to differentiate this from the real driver version
		// (returned by platform-specific detect routines).

	return 0;
}


// call after each video mode change
void oglInit()
{
	exts = (const char*)glGetString(GL_EXTENSIONS);
	if(!exts)
	{
		debug_warn("oglInit called before OpenGL is ready for use");
	}

	// import functions
#define FUNC(ret, name, params) *(void**)&name = SDL_GL_GetProcAddress(#name);
#include "glext_funcs.h"
#undef FUNC

	// detect OpenGL / graphics card caps

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);
	glGetIntegerv(GL_MAX_TEXTURE_UNITS, &tex_units);
	// make sure value is -1 if not supported
	if(oglExtAvail("GL_NV_vertex_array_range"))
		glGetIntegerv(GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV, &max_VAR_elements);

	tex_compression_avail = oglExtAvail("GL_ARB_texture_compression") &&
						   (oglExtAvail("GL_EXT_texture_compression_s3tc") || oglExtAvail("GL_S3_s3tc"));

	video_mem = (SDL_GetVideoInfo()->video_mem) / 1048576;	// [MB]
	// TODO: add sizeof(FB)?
}
