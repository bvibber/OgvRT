#pragma once

#include <memory>

namespace OgvCodec {

	struct VideoInfo {
		int picWidth;
		int picHeight;
		int picX;
		int picY;
		int frameWidth;
		int frameHeight;
		int hdec;
		int vdec;
		float fps;
	};

	struct AudioInfo {
		int sampleRate;
		int channels;
	};

	/// Warning: the pointers in here are owned by the codec!
	/// You must not assume that data in them can be accessed
	/// after another call into further processing.
	struct Frame {
		const byte *bytesY;
		int strideY;
		const byte *bytesCb;
		int strideCb;
		const byte *bytesCr;
		int strideCr;

		int width;
		int height;
		int hdec;
		int vdec;
		double timestamp;
	};

	/// Warning: the pointers in here are owned by the codec!
	/// You must not assume that data in them can be accessed
	/// after another call into further processing.
	struct AudioBuffer {
		const float **data;
		int channels;
		int sampleCount;
	};

	class Codec {
	public:
		Codec();
		~Codec();

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
		class impl;
		std::unique_ptr<impl> pimpl;
	};

}
