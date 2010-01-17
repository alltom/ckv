
#include "../ckv.h"
#include "ugen.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct SndIn {
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	int audioStream; /* which stream is audio */
	int16_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE) / sizeof(int16_t)];
	int curr_buf_count; /* number of samples currently in buffer */
	int audio_buf_ptr; /* next sample to read from buffer */
	int eof, closed;
} SndIn;

/* returns 0 on failure */
static
int
sndin_open(SndIn *sndin, const char *filename)
{
	int i;
	AVCodec *pCodec;

	/* Register all formats and codecs */
	av_register_all();

	if(av_open_input_file(&sndin->pFormatCtx, filename, NULL, 0, NULL) != 0)
		return 0;

	if(av_find_stream_info(sndin->pFormatCtx) < 0)
		return 0; /* Couldn't find stream information */

	/* Dump information about file onto standard error */
	/* dump_format(pFormatCtx, 0, filename, false); */

	/* Find the first audio stream */
	sndin->audioStream = -1;
	for(i = 0; i < sndin->pFormatCtx->nb_streams; i++)
		if(sndin->pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
			sndin->audioStream = i;
			break;
		}
	if(sndin->audioStream == -1)
		return 0;

	/* Get a pointer to the codec context for the audio stream */
	sndin->pCodecCtx = sndin->pFormatCtx->streams[sndin->audioStream]->codec;

	/* Find the decoder for the audio stream */
	pCodec = avcodec_find_decoder(sndin->pCodecCtx->codec_id);
	if(pCodec == NULL)
		return 0;

	if(avcodec_open(sndin->pCodecCtx, pCodec) < 0)
		return 0;
	
	sndin->curr_buf_count = sndin->audio_buf_ptr = 0;
	sndin->eof = 0;
	sndin->closed = 0;
	
	return 1;
}

static
void
sndin_get_samples(SndIn *sndin)
{
	AVPacket packet;
	int i, chans, frames;
	
	if(sndin->closed || sndin->eof)
		return;
	
	while(av_read_frame(sndin->pFormatCtx, &packet) >= 0) {
		/* is this a packet from the audio stream? */
		if(packet.stream_index == sndin->audioStream) {
			int buf_size = sizeof(sndin->audio_buf) * sizeof(sndin->audio_buf[0]);
			
			/* decode audio buf */
			int len = avcodec_decode_audio3(sndin->pCodecCtx,
			                                sndin->audio_buf,
			                                &buf_size,
			                                &packet);
			
			/* len is the number of encoded bytes used, or negative if error */
			if(len < 0) {
				/* error, skip packet */
				continue;
			}
			
			/* got buf_size bytes! */
			/* format is int16_t (signed short) */
			sndin->curr_buf_count = buf_size/sizeof(sndin->audio_buf[0]);
			
			if(sndin->pCodecCtx->channels > 1) {
				/* take the first channel */
				chans = sndin->pCodecCtx->channels;
				frames = sndin->curr_buf_count / chans;
				for(i = 0; i < frames; i++)
					sndin->audio_buf[i] = sndin->audio_buf[i * chans];
				
				sndin->curr_buf_count = frames;
			}
			
			sndin->audio_buf_ptr = 0;
			return;
		}
	}
	
	/* we ran out of packets */
	sndin->curr_buf_count = sndin->audio_buf_ptr = 0;
	sndin->eof = 1;
}

static
void
sndin_close(SndIn *sndin)
{
	avcodec_close(sndin->pCodecCtx);
	av_close_input_file(sndin->pFormatCtx);
	sndin->eof = 1;
	sndin->closed = 1;
}

/* args: self */
static
int
ckv_sndin_tick(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	
	SndIn *sndin;
	lua_Number last_value;
	
	lua_getfield(L, -1, "obj");
	sndin = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if(sndin->closed || sndin->eof) {
		lua_pushnumber(L, 0);
		return 1;
	}
	
	if(sndin->audio_buf_ptr == sndin->curr_buf_count)
		sndin_get_samples(sndin);
	if(sndin->audio_buf_ptr == sndin->curr_buf_count) {
		/* ran out of data */
		sndin_close(sndin);
		last_value = 0;
	} else {
		last_value = sndin->audio_buf[sndin->audio_buf_ptr++] / 32767.0;
	}
	
	lua_pushnumber(L, last_value);
	lua_setfield(L, 1, "last");
	
	return 0;
}

/* args: self */
static
int
ckv_sndin_close(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	
	SndIn *sndin;
	
	lua_getfield(L, -1, "obj");
	sndin = lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if(!sndin->closed)
		sndin_close(sndin);
	
	return 0;
}

static
int
ckv_sndin_release(lua_State *L)
{
	SndIn *sndin = lua_touserdata(L, 1);
	sndin_close(sndin);
	free(sndin);
	
	return 0;
}

static
const
luaL_Reg
ckvugen_sndin[] = {
	{ "tick", ckv_sndin_tick },
	{ "close", ckv_sndin_close },
	{ NULL, NULL }
};

/* args: SndIn (class), filename */
static
int
ckv_sndin_new(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TSTRING);
	
	SndIn *sndin;
	const char *filename;
	
	sndin = malloc(sizeof(SndIn));
	if(sndin == NULL) {
		fprintf(stderr, "[ckv] memory error allocating SndIn\n");
		return 0;
	}
	
	filename = lua_tostring(L, 2);
	if(!sndin_open(sndin, filename)) {
		fprintf(stderr, "[ckv] could not open file \"%s\"\n", filename);
		return 0;
	}
	
	/* self */
	lua_createtable(L, 0 /* array */, 2 /* non-array */);
	
	/* set GC routine, then */
	/* self.obj = sndin* */
	lua_pushlightuserdata(L, sndin);
	lua_createtable(L, 0 /* array */, 1 /* non-array */); /* new metatable */
	lua_pushcfunction(L, ckv_sndin_release);
	lua_setfield(L, -2, "__gc");
	lua_setmetatable(L, -2);
	lua_setfield(L, -2, "obj");
	
	/* self.filename = filename */
	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "filename");
	
	/* self.duration = ... */
	pushstdglobal(L, "sample_rate");
	lua_Number sample_rate = lua_tonumber(L, -1);
	lua_pop(L, 1);
	lua_pushnumber(L, sndin->pFormatCtx->duration / 1000000.0 * sample_rate);
	lua_setfield(L, -2, "duration");
	
	/* add sndin methods */
	luaL_register(L, NULL, ckvugen_sndin);
	
	/* UGen.initialize_io(self) */
	lua_getfield(L, LUA_GLOBALSINDEX, "UGen");
	lua_getfield(L, -1, "initialize_io");
	lua_pushvalue(L, -3); /* self */
	lua_call(L, 1, 0);
	lua_pop(L, 1); /* pop UGen */
	
	return 1; /* return self */
}

/* args: SndIn (class), filename */
static
int
ckv_sndin_play(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TSTRING);
	
	if(lua_gettop(L) == 2)
		pushstdglobal(L, "speaker");
	
	/* create SndIn obj */
	lua_pushcfunction(L, ckv_sndin_new);
	lua_pushvalue(L, 1); /* SndIn */
	lua_pushvalue(L, 2); /* filename */
	lua_call(L, 2 /* args */, 1 /* return values */);
	
	if(lua_isnil(L, -1))
		return 0;
	
	pushstdglobal(L, "fork");
	lua_getfield(L, 1, "__play_thread");
	lua_pushvalue(L, -3); /* SndIn created above */
	lua_pushvalue(L, 3); /* dest ugen */
	lua_call(L, 3 /* args */, 1 /* return values */);
	
	return 1; /* return forked thread */
}

/* LIBRARY REGISTRATION */

int
open_ugen_sndin(lua_State *L)
{
	lua_createtable(L, 0, 2 /* estimated number of functions */);
	lua_pushcfunction(L, ckv_sndin_new); lua_setfield(L, -2, "new");
	lua_pushcfunction(L, ckv_sndin_play); lua_setfield(L, -2, "play");
	lua_setglobal(L, "SndIn"); /* pops */
	
	/* this function is in Lua so that it can resume after yielding */
	(void) luaL_dostring(L,
	"SndIn.__play_thread = function(sndin, dest)"
	"  connect(sndin, dest);"
	"  yield(sndin.duration);"
	"  disconnect(sndin, dest);"
	"end"
	);
	
	return 0;
}
