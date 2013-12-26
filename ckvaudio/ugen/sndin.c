
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#include "../../ckvm.h"
#include "ugen.h"

typedef struct _SndIn {
	AVFormatContext *pFormatCtx;
	AVCodecContext *pCodecCtx;
	AVPacket decodingPacket;
	AVFrame *frame;
	SwrContext *resampler;
	int audioStreamIndex; /* which stream is audio */

	float *buffer;
	int samplesLeft;
	float nextSampleIndex; /* can advance at fractional rates */

	int eof, closed;
} SndIn;

static int sndin_handle_packet(SndIn *sndin); /* returns number of samples in frame */
static void sndin_handle_frame(SndIn *sndin);

/* returns 0 on failure */
static
int
sndin_open(SndIn *sndin, const char *filename, int out_sample_rate)
{
	AVCodec *pCodec;

	sndin->pFormatCtx = NULL;
	sndin->pCodecCtx = NULL;
	sndin->frame = NULL;
	sndin->decodingPacket.size = 0;
	sndin->buffer = NULL;

	sndin->samplesLeft = sndin->nextSampleIndex = 0;
	sndin->eof = 0;
	sndin->closed = 0;

	/* Register all formats and codecs */
	av_register_all();

	/* open audio file */
	if(avformat_open_input(&sndin->pFormatCtx, filename, NULL, NULL) != 0)
		return 0;

	/* get stream information */
	if (avformat_find_stream_info(sndin->pFormatCtx, NULL) < 0) {
		avformat_close_input(&sndin->pFormatCtx);
		return 0; /* Couldn't find stream information */
	}

	/* identify best audio stream */
	sndin->audioStreamIndex = av_find_best_stream(sndin->pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &pCodec, 0);
	if (sndin->audioStreamIndex < 0) {
		avformat_close_input(&sndin->pFormatCtx);
		return 0;
	}

	/* allocate audio buffer */
	sndin->frame = avcodec_alloc_frame();
	if (!sndin->frame) {
		avformat_close_input(&sndin->pFormatCtx);
		return 0;
	}

	/* Get the codec context for the audio stream */
	AVStream *audioStream = sndin->pFormatCtx->streams[sndin->audioStreamIndex];
	sndin->pCodecCtx = audioStream->codec;
	sndin->pCodecCtx->codec = pCodec;

	if (avcodec_open2(sndin->pCodecCtx, sndin->pCodecCtx->codec, NULL) != 0) {
		av_free(sndin->frame);
		avformat_close_input(&sndin->pFormatCtx);
		return 0;
	}

	/* initialize a converter from the input sample format to float samples */
	sndin->resampler = swr_alloc();
	av_opt_set_int(sndin->resampler, "in_channel_layout", sndin->pCodecCtx->channel_layout ? sndin->pCodecCtx->channel_layout : av_get_default_channel_layout(sndin->pCodecCtx->channels), 0);
	av_opt_set_int(sndin->resampler, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
	av_opt_set_int(sndin->resampler, "in_sample_rate", sndin->pCodecCtx->sample_rate, 0);
	av_opt_set_int(sndin->resampler, "out_sample_rate", out_sample_rate, 0);
	av_opt_set_sample_fmt(sndin->resampler, "in_sample_fmt", sndin->pCodecCtx->sample_fmt, 0);
	av_opt_set_sample_fmt(sndin->resampler, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
	if(swr_init(sndin->resampler) != 0) {
		avcodec_close(sndin->pCodecCtx);
		av_free(sndin->frame);
		avformat_close_input(&sndin->pFormatCtx);
		return 0;
	}

	return 1;
}

static
void
sndin_get_samples(SndIn *sndin)
{
	AVPacket readingPacket;
	
	if(sndin->closed || sndin->eof)
		return;

	av_init_packet(&readingPacket);

	/* check if there are samples left over from the last decodingPacket */
	if (sndin_handle_packet(sndin) > 0) {
		sndin_handle_frame(sndin);
		return;
	}

	/* read new frame then read from decodingPacket again */
	while(av_read_frame(sndin->pFormatCtx, &readingPacket) == 0) {
		/* is this a packet from the audio stream? */
		if(readingPacket.stream_index == sndin->audioStreamIndex) {
			sndin->decodingPacket = readingPacket;

			if (sndin_handle_packet(sndin) > 0) {
				sndin_handle_frame(sndin);
				av_free_packet(&readingPacket);
				return;
			}
		}

		av_free_packet(&readingPacket);
	}

	/*
	Some codecs will cause frames to be buffered up in the decoding process.
	If the CODEC_CAP_DELAY flag is set, there can be buffered up frames that
	need to be flushed, so we'll do that.
	*/
	if (sndin->pCodecCtx->codec->capabilities & CODEC_CAP_DELAY) {
		av_init_packet(&readingPacket);

		/* Decode all the remaining frames in the buffer, until the end is reached */
		int gotFrame = 0;
		if(avcodec_decode_audio4(sndin->pCodecCtx, sndin->frame, &gotFrame, &readingPacket) >= 0 && gotFrame) {
			sndin_handle_frame(sndin);
			return;
		}
	}

	/* we ran out of packets */
	sndin->samplesLeft = sndin->nextSampleIndex = 0;
	sndin->eof = 1;
}

static
int
sndin_handle_packet(SndIn *sndin)
{
	/* Audio packets can have multiple audio frames in a single packet */
	if(sndin->decodingPacket.size > 0) {
		/*
		Try to decode the packet into a frame.
		Some frames rely on multiple packets, so we have to make sure
		the frame is finished before we can use it.
		*/
		int gotFrame = 0;
		int len = avcodec_decode_audio4(sndin->pCodecCtx, sndin->frame, &gotFrame, &sndin->decodingPacket);

		if(len >= 0 && gotFrame) {
			sndin->decodingPacket.size -= len;
			sndin->decodingPacket.data += len;
			return len;
		} else {
			sndin->decodingPacket.size = 0;
			sndin->decodingPacket.data = NULL;
		}
	}

	return 0;
}

static
void
sndin_handle_frame(SndIn *sndin)
{
	if(sndin->buffer) {
		av_freep(&sndin->buffer);
	}

	int out_samples = av_rescale_rnd(swr_get_delay(sndin->resampler, sndin->pCodecCtx->sample_rate) + sndin->frame->nb_samples, sndin->pCodecCtx->sample_rate, sndin->pCodecCtx->sample_rate, AV_ROUND_UP);
	av_samples_alloc((uint8_t **)&sndin->buffer, NULL, 1 /* channels */, out_samples, AV_SAMPLE_FMT_FLT, 0);
	sndin->nextSampleIndex -= sndin->samplesLeft;
	sndin->samplesLeft = swr_convert(sndin->resampler, (uint8_t **)&sndin->buffer, out_samples, (const uint8_t **)sndin->frame->extended_data, sndin->frame->nb_samples);
}

static
void
sndin_close(SndIn *sndin)
{
	av_free(sndin->frame);
	avcodec_close(sndin->pCodecCtx);
	avformat_close_input(&sndin->pFormatCtx);

	sndin->eof = 1;
	sndin->closed = 1;
}

/* args: self */
static
int
ckv_sndin_tick(lua_State *L)
{
	SndIn *sndin;
	lua_Number last_value;
	float rate;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	
	lua_getfield(L, -1, "obj");
	sndin = (SndIn *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if(sndin->closed || sndin->eof)
		return 0;
	
	while(!sndin->eof && sndin->nextSampleIndex >= sndin->samplesLeft)
		sndin_get_samples(sndin);
	if(sndin->eof) {
		/* ran out of data */
		sndin_close(sndin);
		last_value = 0;
	} else {
		lua_getfield(L, -1, "rate");
		rate = lua_tonumber(L, -1);
		lua_pop(L, 1);
		if(rate < 0) {
			fprintf(stderr, "[ckv] SndIn rate must be positive\n");
			return 0;
		}
		
		last_value = sndin->buffer[(int) sndin->nextSampleIndex];
		sndin->nextSampleIndex += rate;
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
	sndin = (SndIn *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	
	if(!sndin->closed)
		sndin_close(sndin);
	
	return 0;
}

static
int
ckv_sndin_release(lua_State *L)
{
	SndIn *sndin = (SndIn *)lua_touserdata(L, 1);
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
	luaL_checktype(L, 1, LUA_TSTRING);
	
	SndIn *sndin;
	const char *filename;

	/* get the current sample rate */
	ckvm_pushstdglobal(L, "sample_rate");
	lua_Number sample_rate = lua_tonumber(L, -1);
	lua_pop(L, 1);
	
	sndin = (SndIn *)malloc(sizeof(struct _SndIn));
	if(sndin == NULL) {
		fprintf(stderr, "[ckv] memory error allocating SndIn\n");
		return 0;
	}
	
	filename = lua_tostring(L, 1);
	if(!sndin_open(sndin, filename, sample_rate)) {
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
	lua_pushnumber(L, sndin->pFormatCtx->duration / 1000000.0 * sample_rate);
	lua_setfield(L, -2, "duration");
	
	/* self.rate = 1 */
	lua_pushnumber(L, 1);
	lua_setfield(L, -2, "rate");
	
	/* add sndin methods */
	luaL_register(L, NULL, ckvugen_sndin);
	
	return 1; /* return self */
}

/* LIBRARY REGISTRATION */

int
open_ugen_sndin(lua_State *L)
{
	lua_pushcfunction(L, ckv_sndin_new);
	lua_setglobal(L, "SndIn");
	
	(void) luaL_dostring(L,
	"function play(filename, dest)"
	"  fork(function()"
	"    sndin = SndIn(filename);"
	"    dest = dest or speaker;"
	"    connect(sndin, dest);"
	"    yield(sndin.duration);"
	"    disconnect(sndin, dest);"
	"  end)"
	"end"
	);
	
	return 0;
}
