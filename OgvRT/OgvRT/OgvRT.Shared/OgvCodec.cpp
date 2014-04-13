#include "pch.h"

#include "OgvCodec.h"

extern "C" {
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <theora/theoradec.h>
}

using namespace OgvCodec;

enum AppState {
	STATE_BEGIN,
	STATE_HEADERS,
	STATE_DECODING
} appState;

class Codec::impl {
	/* Ogg and codec state for demux/decode */
	ogg_sync_state    oggSyncState;
	ogg_page          oggPage;

	/* Video decode state */
	ogg_stream_state  theoraStreamState;
	th_info           theoraInfo;
	th_comment        theoraComment;
	th_setup_info    *theoraSetupInfo;
	th_dec_ctx       *theoraDecoderContext;

	int              theora_p = 0;
	int              theora_processing_headers;

	/* single frame video buffering */
	int          videobuf_ready = 0;
	ogg_int64_t  videobuf_granulepos = -1;
	double       videobuf_time = 0;

	int          audiobuf_ready = 0;
	ogg_int64_t  audiobuf_granulepos = 0; /* time position of last sample */

	int          raw = 0;

	/* Audio decode state */
	int              vorbis_p = 0;
	int              vorbis_processing_headers;
	ogg_stream_state vo;
	vorbis_info      vi;
	vorbis_dsp_state vd;
	vorbis_block     vb;
	vorbis_comment   vc;
	int          crop = 0;


	ogg_packet oggPacket;
	ogg_packet audioPacket;
	ogg_packet videoPacket;

	int frames = 0;
	int process_audio, process_video;
	int needData = 1;

public:
	impl();
	~impl();

	void init(int process_audio_flag, int process_video_flag);

	void receiveInput(std::vector<byte> buffer);
	boolean process();

	boolean hasVideo();
	VideoInfo videoInfo();
	boolean frameReady();
	boolean decodeFrame(std::function<void(Frame)>);

	boolean hasAudio();
	AudioInfo audioInfo();
	boolean audioReady();
	boolean decodeAudio(std::function<void(AudioBuffer)>);

private:
	int queue_page(ogg_page *page);
	void video_write(std::function<void(Frame)>);
	void processBegin();
	void processHeaders();
	void processDecoding();

};



Codec::Codec(): pimpl(new impl){
	pimpl->init(1, 1);
}

Codec::~Codec() {
	// do I have to do anything to dispose of the state? Smart pointers and all...
}

void Codec::receiveInput(std::vector<byte> buffer) {
	pimpl->receiveInput(buffer);
}

boolean Codec::process() {
	return pimpl->process();
}

boolean Codec::decodeFrame(std::function<void(Frame)> callback) {
	return pimpl->decodeFrame(callback);
}

boolean Codec::decodeAudio(std::function<void(AudioBuffer)> callback) {
	return pimpl->decodeAudio(callback);
}

VideoInfo Codec::videoInfo() {
	return pimpl->videoInfo();
}
AudioInfo Codec::audioInfo(){
	return pimpl->audioInfo();
}

boolean Codec::hasVideo() {
	return pimpl->hasVideo();
}
boolean Codec::hasAudio() {
	return pimpl->hasAudio();
}

boolean Codec::frameReady() {
	return pimpl->frameReady();
}

boolean Codec::audioReady() {
	return pimpl->audioReady();
}

Codec::impl::impl() {
	//
}

/*Write out the planar YUV frame, uncropped.*/
void Codec::impl::video_write(std::function<void (Frame)>callback){
	th_ycbcr_buffer ycbcr;
	th_decode_ycbcr_out(theoraDecoderContext, ycbcr);

	int hdec = !(theoraInfo.pixel_fmt & 1);
	int vdec = !(theoraInfo.pixel_fmt & 2);

	Frame frame;
	frame.bytesY = ycbcr[0].data;
	frame.strideY = ycbcr[0].stride;
	frame.bytesCb = ycbcr[1].data;
	frame.strideCb = ycbcr[1].stride;
	frame.bytesCr = ycbcr[2].data;
	frame.strideCr = ycbcr[2].stride;
	frame.width = theoraInfo.frame_width;
	frame.height = theoraInfo.frame_height;
	frame.hdec = hdec;
	frame.vdec = vdec;
	frame.timestamp = videobuf_time;
	callback(frame);
}

/* helper: push a page into the appropriate steam */
/* this can be done blindly; a stream won't accept a page
that doesn't belong to it */
int Codec::impl::queue_page(ogg_page *page){
	if (theora_p) ogg_stream_pagein(&theoraStreamState, page);
	if (vorbis_p) ogg_stream_pagein(&vo, page);
	return 0;
}


void Codec::impl::init(int process_audio_flag, int process_video_flag) {
	// Allow the caller to specify whether we want audio, video, or both.
	// Or neither, but that won't be very useful.
	process_audio = process_audio_flag;
	process_video = process_video_flag;

	appState = STATE_BEGIN;

	/* start up Ogg stream synchronization layer */
	ogg_sync_init(&oggSyncState);

	/* init supporting Vorbis structures needed in header parsing */
	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	/* init supporting Theora structures needed in header parsing */
	th_comment_init(&theoraComment);
	th_info_init(&theoraInfo);
}

boolean Codec::impl::hasVideo() {
	return (theora_p > 0);
}
boolean Codec::impl::hasAudio() {
	return (vorbis_p > 0);
}

boolean Codec::impl::frameReady() {
	return videobuf_ready;
}

boolean Codec::impl::audioReady() {
	return audiobuf_ready;
}

VideoInfo Codec::impl::videoInfo() {
	VideoInfo videoInfo;
	videoInfo.frameWidth = theoraInfo.frame_width;
	videoInfo.frameHeight = theoraInfo.frame_height;
	videoInfo.hdec = !(theoraInfo.pixel_fmt & 1);
	videoInfo.vdec = !(theoraInfo.pixel_fmt & 2);
	videoInfo.fps = (float) theoraInfo.fps_numerator / theoraInfo.fps_denominator;
	videoInfo.picWidth = theoraInfo.pic_width;
	videoInfo.picHeight = theoraInfo.pic_height;
	videoInfo.picX = theoraInfo.pic_x;
	videoInfo.picY = theoraInfo.pic_y;
	return videoInfo;
}

AudioInfo Codec::impl::audioInfo() {
	AudioInfo audioInfo;
	audioInfo.channels = vi.channels;
	audioInfo.sampleRate = vi.rate;
	return audioInfo;
}

void Codec::impl::processBegin() {
	if (ogg_page_bos(&oggPage)) {
		printf("Packet is at start of a bitstream\n");
		int got_packet;

		// Initialize a stream state object...
		ogg_stream_state test;
		ogg_stream_init(&test, ogg_page_serialno(&oggPage));
		ogg_stream_pagein(&test, &oggPage);

		// Peek at the next packet, since th_decode_headerin() will otherwise
		// eat the first Theora video packet...
		got_packet = ogg_stream_packetpeek(&test, &oggPacket);
		if (!got_packet) {
			return;
		}

		/* identify the codec: try theora */
		if (process_video && !theora_p && (theora_processing_headers = th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetupInfo, &oggPacket)) >= 0){

			/* it is theora -- save this stream state */
			printf("found theora stream!\n");
			memcpy(&theoraStreamState, &test, sizeof(test));
			theora_p = 1;

			if (theora_processing_headers == 0) {
				printf("Saving first video packet for later!\n");
			}
			else {
				ogg_stream_packetout(&theoraStreamState, NULL);
			}
		}
		else if (process_audio && !vorbis_p && (vorbis_processing_headers = vorbis_synthesis_headerin(&vi, &vc, &oggPacket)) == 0) {
			// it's vorbis!
			// save this as our audio stream...
			printf("found vorbis stream! %d\n", vorbis_processing_headers);
			memcpy(&vo, &test, sizeof(test));
			vorbis_p = 1;

			// ditch the processed packet...
			ogg_stream_packetout(&vo, NULL);
		}
		else {
			printf("already have stream, or not theora or vorbis packet\n");
			/* whatever it is, we don't care about it */
			ogg_stream_clear(&test);
		}
	}
	else {
		printf("Moving on to header decoding...\n");
		// Not a bitstream start -- move on to header decoding...
		appState = STATE_HEADERS;
		//processHeaders();
	}
}

void Codec::impl::processHeaders() {
	if ((theora_p && theora_processing_headers) || (vorbis_p && vorbis_p < 3)) {
		printf("processHeaders pass... %d %d %d\n", theora_p, theora_processing_headers, vorbis_p);
		int ret;

		/* look for further theora headers */
		if (theora_p && theora_processing_headers) {
			printf("checking theora headers...\n");
			ret = ogg_stream_packetpeek(&theoraStreamState, &oggPacket);
			if (ret < 0) {
				printf("Error reading theora headers: %d.\n", ret);
				exit(1);
			}
			if (ret > 0) {
				printf("Checking another theora header packet...\n");
				theora_processing_headers = th_decode_headerin(&theoraInfo, &theoraComment, &theoraSetupInfo, &oggPacket);
				if (theora_processing_headers == 0) {
					// We've completed the theora header
					printf("Completed theora header.\n");
					theora_p = 3;
				}
				else {
					printf("Still parsing theora headers...\n");
					ogg_stream_packetout(&theoraStreamState, NULL);
				}
			}
			if (ret == 0) {
				printf("No theora header packet...\n");
			}
		}

		if (vorbis_p && (vorbis_p < 3)) {
			printf("checking vorbis headers...\n");
			ret = ogg_stream_packetpeek(&vo, &oggPacket);
			if (ret < 0) {
				printf("Error reading vorbis headers: %d.\n", ret);
				exit(1);
			}
			if (ret > 0) {
				printf("Checking another vorbis header packet...\n");
				vorbis_processing_headers = vorbis_synthesis_headerin(&vi, &vc, &oggPacket);
				if (vorbis_processing_headers == 0) {
					printf("Completed another vorbis header (of 3 total)...\n");
					vorbis_p++;
				}
				else {
					printf("Invalid vorbis header?\n");
					exit(1);
				}
				ogg_stream_packetout(&vo, NULL);
			}
			if (ret == 0) {
				printf("No vorbis header packet...\n");
			}
		}
	}
	else {
		/* and now we have it all.  initialize decoders */
		if (theora_p){
			theoraDecoderContext = th_decode_alloc(&theoraInfo, theoraSetupInfo);
		}

		if (vorbis_p) {
			vorbis_synthesis_init(&vd, &vi);
			vorbis_block_init(&vd, &vb);
		}

		appState = STATE_DECODING;
	}
}



void Codec::impl::processDecoding() {
	needData = 0;
	if (theora_p && !videobuf_ready) {
		/* theora is one in, one out... */
		if (ogg_stream_packetpeek(&theoraStreamState, &videoPacket) > 0){
			videobuf_ready = 1;
		}
		else {
			needData = 1;
		}
	}

	if (vorbis_p && !audiobuf_ready) {
		if (ogg_stream_packetpeek(&vo, &audioPacket) > 0) {
			audiobuf_ready = 1;
		}
		else {
			needData = 1;
		}
	}
}

boolean Codec::impl::decodeFrame(std::function<void (Frame)> callback) {
	if (ogg_stream_packetout(&theoraStreamState, &videoPacket) <= 0) {
		printf("Theora packet didn't come out of stream\n");
		return 0;
	}
	videobuf_ready = 0;
	int ret = th_decode_packetin(theoraDecoderContext, &videoPacket, &videobuf_granulepos);
	if (ret == 0){
		double t = th_granule_time(theoraDecoderContext, videobuf_granulepos);
		if (t > 0) {
			videobuf_time = t;
		}
		else {
			// For some reason sometimes we get a bunch of 0s out of th_granule_time
			videobuf_time += 1.0 / ((double) theoraInfo.fps_numerator / theoraInfo.fps_denominator);
		}
		frames++;
		video_write(callback);
		return 1;
	}
	else if (ret == TH_DUPFRAME) {
		// Duplicated frame, advance time
		videobuf_time += 1.0 / ((double) theoraInfo.fps_numerator / theoraInfo.fps_denominator);
		frames++;
		video_write(callback);
		return 1;
	}
	else {
		printf("Theora decoder failed mysteriously? %d\n", ret);
		return 0;
	}
}

boolean Codec::impl::decodeAudio(std::function<void (AudioBuffer)> callback) {
	int packetRet = 0;
	audiobuf_ready = 0;
	int foundSome = 0;
	if (ogg_stream_packetout(&vo, &audioPacket) > 0) {
		int ret = vorbis_synthesis(&vb, &audioPacket);
		if (ret == 0) {
			foundSome = 1;
			vorbis_synthesis_blockin(&vd, &vb);

			float **pcm;
			int sampleCount = vorbis_synthesis_pcmout(&vd, &pcm);
			AudioBuffer audioBuffer;
			audioBuffer.data = (const float **)pcm;
			audioBuffer.channels = vi.channels;
			audioBuffer.sampleCount = sampleCount;
			callback(audioBuffer);

			vorbis_synthesis_read(&vd, sampleCount);
		}
		else {
			throw std::exception("Vorbis decoder failed mysteriously?");
		}
	}

	return foundSome;
}

void Codec::impl::receiveInput(std::vector<byte> buffer) {
	if (buffer.size() > 0) {
		char *dest = ogg_sync_buffer(&oggSyncState, buffer.size());
		memcpy(dest, buffer.data(), buffer.size());
		ogg_sync_wrote(&oggSyncState, buffer.size());
	}
}

boolean Codec::impl::process() {
	if (needData) {
		if (ogg_sync_pageout(&oggSyncState, &oggPage) > 0) {
			queue_page(&oggPage);
		}
		else {
			// out of data!
			return 0;
		}
	}
	if (appState == STATE_BEGIN) {
		processBegin();
	}
	else if (appState == STATE_HEADERS) {
		processHeaders();
	}
	else if (appState == STATE_DECODING) {
		processDecoding();
	}
	return 1;
}


Codec::impl::~impl() {
	if (theora_p){
		ogg_stream_clear(&theoraStreamState);
		th_decode_free(theoraDecoderContext);
		th_comment_clear(&theoraComment);
		th_info_clear(&theoraInfo);
	}
	ogg_sync_clear(&oggSyncState);
}
