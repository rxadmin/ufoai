/**
 * @file snd_openal.c
 * @brief Sound functions for openAL
 * @note To activate openAL in UFO:AI you have to set the snd_openal cvar to 1
 */

/*
Copyright (C) 1997-2001 UFO:AI team

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

#include "snd_openal.h"

static unsigned int alSource;
static unsigned int alSampleSet;

/* extern in system implementation */
oalState_t	oalState;

/* currently playing music? */
static qboolean openal_playing = qfalse;
static qboolean openal_alut = qfalse;

cvar_t *snd_openal_volume;
cvar_t *snd_openal_device;

/**
 * @brief Init function that should be called after system implementation QAL_Init was called
 * @sa QAL_Init
 */
qboolean SND_OAL_Init (char* device)
{
	if (!openal_active) {
		Com_Printf("Error: OpenAL bindings not initialized yet\n");
		return qfalse;
	}

	if ((oalState.device = qalcOpenDevice((ALubyte*)device) == NULL)) {
		Com_Printf("Error: OpenAL device (%s) could not be opened\n", device);
		return qfalse;
	}

	Com_Printf("OpenAL: initialized:\n");
	Com_Printf("..version: \"%s\"\n", qalGetString(AL_VERSION));
	Com_Printf("..vendor: \"%s\"\n", qalGetString(AL_VENDOR));
	Com_Printf("..renderer: \"%s\"\n", qalGetString(AL_RENDERER));
	Com_Printf("..device: \"%s\"\n", qalcGetString (oalState.device, ALC_DEFAULT_DEVICE_SPECIFIER));

	if ((oalState.context = qalcCreateContext(oalState.device, NULL)) == NULL) {
		Com_Printf("Error: OpenAL context creation error\n");
		/* TODO: close device here, too */
		return qfalse;
	}

	qalcMakeContextCurrent(oalState.context);
	/* clear error code */
	qalGetError();

	snd_openal_device = Cvar_Get ("snd_openal_device", "", CVAR_ARCHIVE, "Device for openAL");
	snd_openal_volume = Cvar_Get ("snd_openal_volume", "", CVAR_ARCHIVE, "Volume for openAL");

	if (qalutInit(0, NULL))
		openal_alut = qtrue;
	else
		Com_Printf("OpenAL utils init failed\n");


	return qtrue;
}

/**
 * @brief
 * @sa SND_OAL_LoadSound
 */
static qboolean SND_OAL_LoadFile (char* filename, ALboolean loop)
{
	ALuint buffer;
	ALenum error;

	/* Create an AL buffer from the given sound file. */
	buffer = qalutCreateBufferFromFile(filename);
	if (buffer == AL_NONE) {
		error = qalutGetError();
		Com_Printf("Error loading file: '%s'\n", qalutGetErrorString(error));
		return qfalse;
	}

	/* Generate a single source, attach the buffer to it and start playing. */
	qalGenSources(1, &alSource);
	qalSourcei(alSource, AL_BUFFER, buffer);

	/* Normally nothing should go wrong above, but one never knows... */
	error = qalGetError();
	if (error != ALUT_ERROR_NO_ERROR) {
		Com_Printf("%s\n", qalGetString (error));
		return qfalse;
	}
	return qtrue;
}

/**
 * @brief Loads a sound via openAL
 * @sa SND_OAL_LoadFile
 */
qboolean SND_OAL_LoadSound (char* filename, qboolean looping)
{
	/* load our sound */
	if (!SND_OAL_LoadFile(filename, looping)) {
		return qfalse;
	}

	/* set the pitch */
	qalSourcef(alSource, AL_PITCH, 1.0f);
	/* set the gain */
	qalSourcef(alSource, AL_GAIN, 1.0f);
	if (looping) {
		/* set looping to true */
		qalSourcei(alSource, AL_LOOPING, AL_TRUE);
	}
	return qtrue;
}

/**
 * @brief
 */
qboolean SND_OAL_Stream (char* filename)
{
	if (!openal_active || !openal_alut)
		return qfalse;

	if (!openal_playing)
		SND_OAL_LoadSound(filename, qtrue);

	if (!openal_playing)
		return qfalse;

	qalSourcePlay(alSource);
	return qtrue;
}

/**
 * @brief
 */
void SND_OAL_PlaySound (void)
{
	if (!openal_active)
		return;

	qalSourcePlay(alSource);
}

/**
 * @brief
 */
void SND_OAL_StopSound (void)
{
	if (!openal_active)
		return;

	if (openal_playing)
		qalSourceStop(alSource);
}

/**
 * @brief
 */
void SND_OAL_DestroySound (void)
{
	if (!openal_active)
		return;

	qalDeleteSources(1,&alSource);
	qalDeleteBuffers(1,&alSampleSet);
}
