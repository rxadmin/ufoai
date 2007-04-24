/**
 * @file snd_loc.h
 * @brief Private sound functions.
 */

/*
All original materal Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/client/snd_loc.h
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

#ifndef CLIENT_SND_LOC_H
#define CLIENT_SND_LOC_H

#if defined (__linux__) || defined (__FreeBSD__) || defined(__NetBSD__)
#include <vorbis/vorbisfile.h>
#else
#include "../ports/win32/vorbisfile.h"
#endif

/* !!! if this is changed, the asm code must change !!! */
typedef struct {
	int left;
	int right;
} portable_samplepair_t;

typedef struct {
	int length;
	int loopstart;
	int speed;					/* not needed, because converted on load? */
	int width;
	int stereo;
	byte data[1];				/* variable sized */
} sfxcache_t;

typedef struct sfx_s {
	char name[MAX_QPATH];
	int registration_sequence;
	sfxcache_t *cache;
	char *truename;

	qboolean			loaded;
	int					samples;
	int					rate;
	unsigned			format;
	unsigned			bufferNum;
} sfx_t;

/* a playsound_t will be generated by each call to S_StartSound, */
/* when the mixer reaches playsound->begin, the playsound will */
/* be assigned to a channel */
typedef struct playsound_s {
	struct playsound_s *prev, *next;
	sfx_t *sfx;
	float volume;
	float attenuation;
	int entnum;
	int entchannel;
	qboolean fixed_origin;		/* use origin field instead of entnum's origin */
	vec3_t origin;
	unsigned begin;				/* begin on this sample */
} playsound_t;

typedef struct {
	int channels;
	int samples;				/* mono samples in buffer */
	int submission_chunk;		/* don't mix less than this # */
	int samplepos;				/* in mono samples */
	int samplebits;
	int speed;
	byte *buffer;
	int dmapos;
	int dmasize;
} dma_t;

/* !!! if this is changed, the asm code must change !!! */
typedef struct {
	sfx_t *sfx;					/* sfx number */
	int leftvol;				/* 0-255 volume */
	int rightvol;				/* 0-255 volume */
	int end;					/* end time in global paintsamples */
	int pos;					/* sample position in sfx */
	int looping;				/* where to loop, -1 = no looping OBSOLETE? */
	int entnum;					/* to allow overriding a specific sound */
	int entchannel;				/* */
	vec3_t origin;				/* only use if fixed_origin is set */
	vec_t dist_mult;			/* distance multiplier (attenuation/clipK) */
	int master_vol;				/* 0-255 master volume */
	qboolean fixed_origin;		/* use origin instead of fetching entnum's origin */
	qboolean autosound;			/* from an entity->sound, cleared each frame */
} channel_t;

typedef struct music_s {
	OggVorbis_File ovFile; /**< currently playing ogg vorbis file */
	char newFilename[MAX_QPATH]; /**< after fading out ovFile play newFilename */
	char ovBuf[4096]; /**< ogg vorbis buffer */
	int ovSection; /**< number of the current logical bitstream */
	float fading; /**< current volumn - if le zero play newFilename */
	char ovPlaying[MAX_QPATH]; /**< currently playing ogg tracks basename */
	int rate;
	unsigned format;
} music_t;

extern music_t music;

typedef struct {
	int rate;
	int width;
	int channels;
	int loopstart;
	int samples;
	int dataofs;				/* chunk starts this many bytes from file start */
} wavinfo_t;

/* struct for passing info to the sound driver dlls */
struct sndinfo {
	dma_t *dma;
	cvar_t *bits;
	cvar_t *channels;
	cvar_t *device;
	cvar_t *khz;

	void (*Com_Printf) (const char *fmt, ...);
	void (*Com_DPrintf) (const char *fmt, ...);
	void (*S_PaintChannels) (int);
	cvar_t* (*Cvar_Get)(const char *var_name, const char *value, int flags, const char *desc);
	cvar_t* (*Cvar_Set)(const char *var_name, const char *value);
	int *paintedtime;
#ifdef _WIN32
	HWND cl_hwnd;
#endif
};

/*
====================================================================
SYSTEM SPECIFIC FUNCTIONS
====================================================================
*/

qboolean OGG_Open(const char *filename);
void OGG_Stop(void);
int OGG_Read(void);

/*==================================================================== */

#define	MAX_CHANNELS			32
extern channel_t channels[MAX_CHANNELS];

extern int paintedtime;
extern int s_rawend;
extern dma_t dma;
extern playsound_t s_pendingplays;

#define	MAX_RAW_SAMPLES	8192
extern portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];

extern cvar_t *snd_volume;
extern cvar_t *snd_nosound;
extern cvar_t *snd_loadas8bit;
extern cvar_t *snd_khz;
extern cvar_t *snd_show;
extern cvar_t *snd_mixahead;
extern cvar_t *snd_testsound;
extern cvar_t *snd_music_volume;
extern cvar_t *snd_music_loop;

void S_InitScaletable(void);

sfxcache_t *S_LoadSound(sfx_t * s);

void S_IssuePlaysound(playsound_t * ps);

void S_PaintChannels(int endtime);

void S_ClearBuffer(void);

#endif /* CLIENT_SND_LOC_H */
