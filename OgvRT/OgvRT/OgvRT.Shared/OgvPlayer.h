#pragma once;

/**
* High-level player interface wrapped around ogg/vorbis/theora libraries
*
* Uses low-level interface in OgvCodec.cpp
*
* @author Brion Vibber <brion@pobox.com>
* @copyright 2014
* @license MIT-style
*/
#include <memory>
#include <string>

#include "OgvCodec.h"

namespace OgvPlayer {

	///
	/// StreamFile abstract interface
	///
	/// Client should implement this with platform-specific backend.
	///
	class StreamFile {
	public:
		std::wstring url;
		std::function<void()> onstart;
		std::function<void()> onbuffer;
		std::function<void(std::vector<unsigned char>)> onread;
		std::function<void()> ondone;
		std::function<void()> onerror;

		virtual void load() = 0;
		virtual void readBytes() = 0;
		virtual long getBytesBuffered() = 0;
		virtual long getBytesRead() = 0;
	};

	///
	/// AudioFeeder abstract interface
	///
	/// Client should implement this with platform-specific backend
	///
	class AudioFeeder {
	public:
		struct PlaybackState {
			double playbackPosition;
			int samplesQueued;
			int dropped;
		};

		AudioFeeder(OgvCodec::AudioInfo audioInfo);

		virtual double bufferData(OgvCodec::AudioBuffer buffer) = 0;
		virtual PlaybackState getPlaybackState() = 0;
		virtual void close() = 0;
	};

	///
	/// YCbCrFrameSink abstract interface
	///
	/// Client should implement this with platform-specific backend
	///
	class YCbCrFrameSink {
	public:
		virtual void init(OgvCodec::VideoInfo info) = 0;
		virtual void drawFrame(OgvCodec::Frame frame) = 0;
	};

	struct PlaybackStats {
		double targetPerFrameTime;
		int framesProcessed;
		double demuxingTime;
		double videoDecodingTime;
		double audioDecodingTime;
		double bufferTime;
		double colorTime;
		double drawingTime;
		double droppedAudio;
		double jitter;
	};

	///
	/// The actual player class
	///
	class Player {
	public:
		Player();

		void load();
		void play();
		void pause();

		PlaybackStats getPlaybackStats();
		void resetPlaybackStats();

		void setSrc(std::wstring _src);
		std::wstring getSrc();

		double getBufferedTime();
		double getCurrentTime();
		bool getPaused();
		bool getEnded();
		void setMuted(bool _muted);
		bool getMuted();
		int getVideoWidth();
		int getVideoHeight();
		
		double getFrameRate();
		int getAudioChannels();
		int getAudioSampleRate();

	private:
		class impl;
		std::unique_ptr<impl> pimpl;
	};
};
