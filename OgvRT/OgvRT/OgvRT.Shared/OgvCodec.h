#pragma once

#include <functional>
#include <memory>
#include <vector>


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
		const unsigned char *bytesY;
		int strideY;
		const unsigned char *bytesCb;
		int strideCb;
		const unsigned char *bytesCr;
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

		void receiveInput(std::vector<unsigned char> buffer);
		bool process();

		bool hasVideo();
		VideoInfo videoInfo();
		bool frameReady();
		bool decodeFrame(std::function<void(Frame)>);

		bool hasAudio();
		AudioInfo audioInfo();
		bool audioReady();
		bool decodeAudio(std::function<void(AudioBuffer)>);

	private:
		class impl;
		std::unique_ptr<impl> pimpl;
	};

}
