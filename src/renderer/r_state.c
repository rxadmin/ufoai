/**
 * @file r_image.c
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "r_shader.h"
#include "r_error.h"

static shader_t *lighting_shader, *lighting_mtex_shader, *warp_shader;
static shader_t *activeLightShader;

/* useful for particles, pics, etc.. */
const float default_texcoords[] = {
	0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0
};

void R_SelectTexture (gltexunit_t *texunit)
{
	if (texunit == r_state.active_texunit)
		return;

	r_state.active_texunit = texunit;

	qglActiveTexture(texunit->texture);
	qglClientActiveTexture(texunit->texture);
}

void R_BindTexture (int texnum)
{
	if (texnum == r_state.active_texunit->texnum)
		return;

	r_state.active_texunit->texnum = texnum;

	qglBindTexture(GL_TEXTURE_2D, texnum);
	R_CheckError();
}

void R_BindLightmapTexture (GLuint texnum)
{
	if (texnum == r_state.lightmap_texunit.texnum)
		return;  /* small optimization to save state changes */

	R_SelectTexture(&r_state.lightmap_texunit);

	R_BindTexture(texnum);

	R_SelectTexture(&r_state.texture_texunit);
}

void R_BindArray (GLenum target, GLenum type, void *array)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			qglVertexPointer(2, type, 0, array);
		else
			qglVertexPointer(3, type, 0, array);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		qglTexCoordPointer(2, type, 0, array);
		break;
	case GL_COLOR_ARRAY:
		qglColorPointer(4, type, 0, array);
		break;
	case GL_NORMAL_ARRAY:
		qglNormalPointer(type, 0, array);
		break;
	}
}

void R_BindDefaultArray (GLenum target)
{
	switch (target) {
	case GL_VERTEX_ARRAY:
		if (r_state.ortho)
			R_BindArray(target, GL_SHORT, r_state.vertex_array_2d);
		else
			R_BindArray(target, GL_FLOAT, r_state.vertex_array_3d);
		break;
	case GL_TEXTURE_COORD_ARRAY:
		if (r_state.active_texunit == &r_state.lightmap_texunit)
			R_BindArray(target, GL_FLOAT, r_state.lmtexcoord_array);
		else
			R_BindArray(target, GL_FLOAT, r_state.texcoord_array);
		break;
	case GL_COLOR_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.color_array);
		break;
	case GL_NORMAL_ARRAY:
		R_BindArray(target, GL_FLOAT, r_state.normal_array);
		break;
	}
}

void R_BindBuffer (GLenum target, GLenum type, GLuint id)
{
	if (!r_vertexbuffers->integer)
		return;

	qglBindBuffer(GL_ARRAY_BUFFER, id);

	if (type && id)  /* assign the array pointer as well */
		R_BindArray(target, type, NULL);
}

void R_BlendFunc (GLenum src, GLenum dest)
{
	if (r_state.blend_src == src && r_state.blend_dest == dest)
		return;

	r_state.blend_src = src;
	r_state.blend_dest = dest;

	qglBlendFunc(src, dest);
}

void R_EnableBlend (qboolean enable)
{
	if (r_state.blend_enabled == enable)
		return;

	r_state.blend_enabled = enable;

	if (enable) {
		qglEnable(GL_BLEND);
		qglDepthMask(GL_FALSE);
	} else {
		qglDisable(GL_BLEND);
		qglDepthMask(GL_TRUE);
	}
}

void R_EnableAlphaTest (qboolean enable)
{
	if (enable == r_state.alpha_test_enabled)
		return;

	r_state.alpha_test_enabled = enable;

	if (enable)
		qglEnable(GL_ALPHA_TEST);
	else
		qglDisable(GL_ALPHA_TEST);
}

void R_EnableMultitexture (qboolean enable)
{
	if (enable == r_state.multitexture_enabled)
		return;

	r_state.multitexture_enabled = enable;

	R_SelectTexture(&r_state.lightmap_texunit);

	if (enable) {
		/* activate lightmap texture unit */
		qglEnable(GL_TEXTURE_2D);
		if (r_lightmap->modified) {
			r_lightmap->modified = qfalse;
			if (r_lightmap->integer)
				R_TexEnv(GL_REPLACE);
			else
				R_TexEnv(GL_MODULATE);
		}
		qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	} else {
		/* disable on the second texture unit */
		qglDisable(GL_TEXTURE_2D);
		qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	R_SelectTexture(&r_state.texture_texunit);
}

void R_EnableLighting (qboolean enable)
{
	if (!r_light->integer || r_state.lighting_enabled == enable)
		return;

	if (!lighting_shader || !lighting_mtex_shader) {
		lighting_shader = R_GetShader("lighting");
		lighting_mtex_shader = R_GetShader("lighting_mtex");
		if (!lighting_shader || !lighting_mtex_shader)
			Sys_Error("Could not load lighting shaders");
	}

	r_state.lighting_enabled = enable;

	if (enable) {  /* toggle state */
		qglEnable(GL_LIGHTING);

		if (r_state.multitexture_enabled)
			activeLightShader = lighting_mtex_shader;
		else
			activeLightShader = lighting_shader;

		qglEnableClientState(GL_NORMAL_ARRAY);
		SH_UseShader(activeLightShader, qtrue);
	} else {
		int i;

		assert(activeLightShader);
		qglDisable(GL_LIGHTING);

		qglDisableClientState(GL_NORMAL_ARRAY);
		SH_UseShader(activeLightShader, qfalse);
		activeLightShader = NULL;
	}
}

void R_EnableWarp (qboolean enable)
{
	static vec4_t offset;

	if (!warp_shader)
		warp_shader = R_GetShader("warp");

	if (!warp_shader || r_state.warp_enabled == enable)
		return;

	r_state.warp_enabled = enable;

	R_SelectTexture(&r_state.lightmap_texunit);

	if (enable) {
		R_BindTexture(r_warptexture->texnum);
		qglEnable(GL_TEXTURE_2D);

		SH_UseShader(warp_shader, qtrue);

		offset[0] = offset[1] = refdef.time / 8.0;
		R_ShaderFragmentParameter(0, offset);
	} else {
		qglDisable(GL_TEXTURE_2D);

		SH_UseShader(warp_shader, qfalse);
	}

	R_CheckError();

	R_SelectTexture(&r_state.texture_texunit);
}

void R_DisableEffects (void)
{
	R_CheckError();

	if (r_state.alpha_test_enabled)
		R_EnableAlphaTest(qfalse);

	if (r_state.lighting_enabled)
		R_EnableLighting(qfalse);

	if (r_state.warp_enabled)
		R_EnableWarp(qfalse);

	R_CheckError();
}

/**
 * @sa R_SetupGL3D
 */
static void MYgluPerspective (GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax, yaspect = (double) refdef.height / refdef.width;

	if (r_isometric->integer) {
		qglOrtho(-10 * refdef.fov_x, 10 * refdef.fov_x, -10 * refdef.fov_x * yaspect, 10 * refdef.fov_x * yaspect, -zFar, zFar);
	} else {
		xmax = zNear * tan(refdef.fov_x * M_PI / 360.0);
		xmin = -xmax;

		ymin = xmin * yaspect;
		ymax = xmax * yaspect;

		qglFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

/**
 * @sa R_SetupGL2D
 */
void R_SetupGL3D (void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(refdef.x * viddef.width / viddef.width);
	x2 = ceil((refdef.x + refdef.width) * viddef.width / viddef.width);
	y = floor(viddef.height - refdef.y * viddef.height / viddef.height);
	y2 = ceil(viddef.height - (refdef.y + refdef.height) * viddef.height / viddef.height);

	w = x2 - x;
	h = y - y2;

	qglViewport(x, y2, w, h);
	R_CheckError();

	/* set up projection matrix */
	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();
	MYgluPerspective(4.0, 4096.0);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	qglRotatef(-90, 1, 0, 0);	/* put Z going up */
	qglRotatef(90, 0, 0, 1);	/* put Z going up */
	qglRotatef(-refdef.viewangles[2], 1, 0, 0);
	qglRotatef(-refdef.viewangles[0], 0, 1, 0);
	qglRotatef(-refdef.viewangles[1], 0, 0, 1);
	qglTranslatef(-refdef.vieworg[0], -refdef.vieworg[1], -refdef.vieworg[2]);

	qglGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

	r_state.ortho = qfalse;

	/* set vertex array pointer */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	qglDisable(GL_BLEND);

	qglEnable(GL_DEPTH_TEST);

	R_CheckError();
}

/**
 * @sa R_SetupGL3D
 */
void R_SetupGL2D (void)
{
	/* set 2D virtual screen size */
	qglViewport(0, 0, viddef.width, viddef.height);

	qglMatrixMode(GL_PROJECTION);
	qglLoadIdentity();

	/* switch to orthographic (2 dimensional) projection */
	qglOrtho(0, viddef.width, viddef.height, 0, 9999, -9999);

	qglMatrixMode(GL_MODELVIEW);
	qglLoadIdentity();

	r_state.ortho = qtrue;

	/* bind default vertex array */
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	R_EnableAlphaTest(qtrue);

	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	R_Color(NULL);

	qglDisable(GL_DEPTH_TEST);

	r_state.ortho = qtrue;

	R_CheckError();
}

/* global ambient lighting */
static const vec4_t ambient = {
	0.0, 0.0, 0.0, 1.0
};

/* material reflection */
static const vec4_t material = {
	1.0, 1.0, 1.0, 1.0
};

void R_SetDefaultState (void)
{
	int i;

	/* setup texture units */
	r_state.active_texunit = NULL;

	r_state.texture_texunit.texture = GL_TEXTURE0_ARB;
	r_state.lightmap_texunit.texture = GL_TEXTURE1_ARB;

	R_SelectTexture(&r_state.texture_texunit);

	qglEnable(GL_TEXTURE_2D);
	R_Color(NULL);
	qglClearColor(0, 0, 0, 0);

	qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	/* setup vertex array pointers */
	qglEnableClientState(GL_VERTEX_ARRAY);
	R_BindDefaultArray(GL_VERTEX_ARRAY);

	qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
	R_BindDefaultArray(GL_TEXTURE_COORD_ARRAY);

	qglEnableClientState(GL_COLOR_ARRAY);
	R_BindDefaultArray(GL_COLOR_ARRAY);
	qglDisableClientState(GL_COLOR_ARRAY);

	qglEnableClientState(GL_NORMAL_ARRAY);
	R_BindDefaultArray(GL_NORMAL_ARRAY);
	qglDisableClientState(GL_NORMAL_ARRAY);

	/* alpha test parameters */
	qglAlphaFunc(GL_GREATER, 0.1f);

	/* lighting parameters */
	qglLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
	qglMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, material);

	for (i = 0; i < MAX_GL_LIGHTS; i++) {
		qglLightf(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, 0.005);
		qglLightf(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, 0.0001);
	}

	/* alpha blend parameters */
	R_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void R_TexEnv (GLenum mode)
{
	if (mode == r_state.active_texunit->texenv)
		return;

	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
	r_state.active_texunit->texenv = mode;
}

typedef struct {
	const char *name;
	int mode;
} gltmode_t;

static const gltmode_t gl_alpha_modes[] = {
	{"default", 4},
	{"GL_RGBA", GL_RGBA},
	{"GL_RGBA8", GL_RGBA8},
	{"GL_RGB5_A1", GL_RGB5_A1},
	{"GL_RGBA4", GL_RGBA4},
	{"GL_RGBA2", GL_RGBA2},
};

#define NUM_R_ALPHA_MODES (sizeof(gl_alpha_modes) / sizeof (gltmode_t))

void R_TextureAlphaMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_ALPHA_MODES; i++) {
		if (!Q_stricmp(gl_alpha_modes[i].name, string))
			break;
	}

	if (i == NUM_R_ALPHA_MODES) {
		Com_Printf("bad alpha texture mode name\n");
		return;
	}

	gl_alpha_format = gl_alpha_modes[i].mode;
}

static const gltmode_t gl_solid_modes[] = {
	{"default", 3},
	{"GL_RGB", GL_RGB},
	{"GL_RGB8", GL_RGB8},
	{"GL_RGB5", GL_RGB5},
	{"GL_RGB4", GL_RGB4},
	{"GL_R3_G3_B2", GL_R3_G3_B2},
#ifdef GL_RGB2_EXT
	{"GL_RGB2", GL_RGB2_EXT},
#endif
};

#define NUM_R_SOLID_MODES (sizeof(gl_solid_modes) / sizeof (gltmode_t))

void R_TextureSolidMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_SOLID_MODES; i++) {
		if (!Q_stricmp(gl_solid_modes[i].name, string))
			break;
	}

	if (i == NUM_R_SOLID_MODES) {
		Com_Printf("bad solid texture mode name\n");
		return;
	}

	gl_solid_format = gl_solid_modes[i].mode;
}

const vec4_t color_white = {1, 1, 1, 1};

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 * @sa R_Color
 * @note Enables openGL blend if alpha value is lower than 1.0
 */
void R_ColorBlend (const float *rgba)
{
	if (rgba && rgba[3] < 1.0f) {
		R_EnableBlend(qtrue);
	} else if (!rgba) {
		R_EnableBlend(qfalse);
	}
	R_Color(rgba);
}

/**
 * @brief Change the color to given value
 * @param[in] rgba A pointer to a vec4_t with rgba color value
 * @note To reset the color let rgba be NULL
 * @sa R_ColorBlend
 */
void R_Color (const float *rgba)
{
	const float *color;
	if (rgba)
		color = rgba;
	else
		color = color_white;

	R_CheckError();
	if (r_state.color[0] != color[0] || r_state.color[1] != color[1]
	 || r_state.color[2] != color[2] || r_state.color[3] != color[3]) {
		Vector4Copy(color, r_state.color);
		qglColor4fv(r_state.color);
		R_CheckError();
	}
}

typedef struct {
	const char *name;
	int minimize, maximize;
} glTextureMode_t;

static const glTextureMode_t gl_texture_modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};
#define NUM_R_MODES (sizeof(gl_texture_modes) / sizeof(glTextureMode_t))

void R_TextureMode (const char *string)
{
	int i;

	for (i = 0; i < NUM_R_MODES; i++) {
		if (!Q_stricmp(gl_texture_modes[i].name, string))
			break;
	}

	if (i == NUM_R_MODES) {
		Com_Printf("bad filter name\n");
		return;
	}
	R_UpdateTextures(gl_texture_modes[i].minimize, gl_texture_modes[i].maximize);
}
