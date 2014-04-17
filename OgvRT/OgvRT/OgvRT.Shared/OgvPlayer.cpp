#include "OgvPlayer.h"

#include <algorithm>
#include <chrono>
#include <functional>

using namespace OgvPlayer;

class Player::impl {
public:
	// Property backing vars
	std::wstring src;
	std::wstring poster;
	double durationHint = 0;
	int byteLength = 0;

	// Playback state vars
	bool started = true;
	bool paused = false;
	bool ended = false;
	bool muted = false;

	// Metadata and internals...
	OgvCodec::Codec codec;
	OgvCodec::VideoInfo videoInfo;
	OgvCodec::AudioInfo audioInfo;

	// Networking state
	std::unique_ptr<StreamFile> stream;

	// Playback stats, in counts
	int framesProcessed = 0;
	int framesPlayed = 0;
	int droppedAudio = 0;
	// Playback stats, in ms
	double targetPerFrameTime = 1000 / 60;
	double demuxingTime = 0;
	double videoDecodingTime = 0;
	double audioDecodingTime = 0;
	double bufferTime = 0;
	double colorTime = 0;
	double drawingTime  = 0;
	double totalJitter = 0;
	double lastFrameDecodeTime = 0;

	// Video internals
	double fps = 0;
	double lastFrameTimestamp = 0;
	double frameEndTimestamp = 0;
	double targetFrameTime = 0;

	// GPU drawing internals
	std::unique_ptr<YCbCrFrameSink> frameSink;

	// Audio internals
	std::unique_ptr<AudioFeeder> audioFeeder;
	int targetRate;
	int audioBufferSize;

	impl(YCbCrFrameSink *_frameSink, AudioFeeder *_audioFeeder)
		: frameSink(_frameSink), audioFeeder(_audioFeeder)
	{
		//
	}

	double getTimestamp() {
		return 0; // fixme
	}

	// Custom playback stats calls...
	PlaybackStats getPlaybackStats() {
		PlaybackStats stats;
		stats.targetPerFrameTime = targetPerFrameTime;
		stats.framesProcessed = framesProcessed;
		stats.demuxingTime = demuxingTime;
		stats.videoDecodingTime = videoDecodingTime;
		stats.audioDecodingTime = audioDecodingTime;
		stats.bufferTime = bufferTime;
		stats.colorTime = colorTime;
		stats.drawingTime = drawingTime;
		stats.droppedAudio = droppedAudio;
		stats.jitter = totalJitter / framesProcessed;
		return stats;
	}

	void resetPlaybackStats() {
		framesProcessed = 0;
		demuxingTime = 0;
		videoDecodingTime = 0;
		audioDecodingTime = 0;
		bufferTime = 0;
		colorTime = 0;
		drawingTime = 0;
		totalJitter = 0;
	}

	// Some properties
	void setSrc(std::wstring _src) {
		src = _src;
	}

	std::wstring getSrc() {
		return src;
	}

	// Playback state
	double getBufferedTime() {
		/*
		if (stream && byteLength && durationHint) {
			return ((bytesRead + stream.bytesAvailable) / byteLength) * durationHint;
		}
		else {
			return 0;
		}
		*/
		return 0;
	}

	double getCurrentTime() {
		if (codec.hasAudio()) {
			return audioFeeder->getPlaybackState().playbackPosition;
		}
		else if (codec.hasVideo()) {
			return framesPlayed / fps;
		}
		else {
			return 0;
		}
	}
	bool getPaused() {
		return paused;
	}
	bool getEnded() {
		return ended;
	}
	void setMuted(bool _muted) {
		muted = _muted;
	}
	bool getMuted() {
		return muted;
	}

	// Various metadata!
	void setDurationHint(double _durationHint) {
		durationHint = _durationHint;
	}
	double getDuration() {
		return durationHint;
	}
	int getVideoWidth() {
		if (codec.hasVideo()) {
			return videoInfo.picWidth;
		} else {
			return 0;
		}
	}
	int getVideoHeight() {
		if (codec.hasVideo()) {
			return videoInfo.picHeight;
		} else {
			return 0;
		}
	}
	double getFrameRate() {
		if (codec.hasVideo()) {
			return videoInfo.fps;
		}
		else {
			return 0;
		}
	}
	int getAudioChannels() {
		if (codec.hasAudio()) {
			return audioInfo.channels;
		}
		else {
			return 0;
		}
	}
	int getAudioSampleRate() {
		if (codec.hasAudio()){
			return audioInfo.sampleRate;
		}
		else {
			return 0;
		}
	};

	void log(std::string _str) {
		// ...
	}

	/**
	* Start loading the source file over the network.
	* Don't trigger any processing just yet.
	*/
	void load() {
		// ...
	}

	void play() {
		if (!started) {
			load();
		}
		if (paused) {
			paused = false;
			pingProcessing(0);
		}
		//jsCallback('onplay');
	}

	void pause() {
		if (!paused) {
			paused = true;
			/*
			if (nextProcessingTimer) {
				clearTimeout(nextProcessingTimer);
				nextProcessingTimer = 0;
			}
			*/
			//jsCallback('onpause');
		}
	}

	void pingProcessing(double delay) {
		/*
		if (nextProcessingTimer) {
			// already scheduled
		}
		else {
			if (delay < 0) {
				delay = 0;
			}
			nextProcessingTimer = setTimeout(doProcessing, delay);
		}
		*/
	}

	void doProcessing() {
		//nextProcessingTimer = 0;

		double audioBufferedDuration = 0;
		AudioFeeder::PlaybackState audioState;
		if (codec.hasAudio()) {
			audioState = audioFeeder->getPlaybackState();
			audioBufferedDuration = (audioState.samplesQueued / targetRate) * 1000;
		}

		int n = 0;
		while (true) {
			n++;
			if (n > 100) {
				log("Got stuck in the loop!");
				pingProcessing(10);
				return;
			}
			// Process until we run out of data or
			// completely decode a video frame...
			double currentTime = getTimestamp(),
				start,
				delta;
			bool ok;
			start = currentTime;

			bool hasAudio = codec.hasAudio(),
				hasVideo = codec.hasVideo(),
				more = codec.process();
			if (hasAudio != codec.hasAudio() || hasVideo != codec.hasVideo()) {
				// we just fell over from headers into content; reinit
				lastFrameTimestamp = getTimestamp();
				targetFrameTime = lastFrameTimestamp + 1000.0 / fps;
				pingProcessing(0);
				return;
			}

			delta = (getTimestamp() - start);
			lastFrameDecodeTime += delta;
			demuxingTime += delta;

			if (!more) {
				if (stream) {
					// Ran out of buffered input
					stream->readBytes();
				}
				else {
					// Ran out of stream!
					double finalDelay = 0;
					if (hasAudio) {
						if (durationHint) {
							finalDelay = durationHint * 1000 - audioState.playbackPosition;
						}
						else {
							finalDelay = audioBufferedDuration;
						}
					}
					/*
					log('End of stream reached in ' + finalDelay + ' ms.');
					setTimeout(function() :void {
						ended = true;
						//jsCallback('onended');
					}, finalDelay);
					*/
				}
				return;
			}

			if ((hasAudio || hasVideo) && !(codec.audioReady() || codec.frameReady())) {
				// Have to process some more pages to find data. Continue the loop.
				continue;
			}

			if (hasAudio) {
				// Drive on the audio clock!
				double fudgeDelta = 0.1;
				bool readyForAudio = audioState.samplesQueued <= (audioBufferSize * 2);
				double frameDelay = (frameEndTimestamp - audioState.playbackPosition) * 1000;
				bool readyForFrame = (frameDelay <= fudgeDelta);
				double startTimeSpent = getTimestamp();
				if (codec.audioReady() && readyForAudio) {
					start = getTimestamp();
					ok = codec.decodeAudio([this, start, &audioBufferedDuration](OgvCodec::AudioBuffer buffer) {
						double delta = (getTimestamp() - start);
						lastFrameDecodeTime += delta;
						audioDecodingTime += delta;
						audioBufferedDuration += audioFeeder->bufferData(buffer);
					});
				}
				if (codec.frameReady() && readyForFrame) {
					start = getTimestamp();
					ok = codec.decodeFrame([this, start](OgvCodec::Frame yCbCrBuffer) {
						double delta = (getTimestamp() - start);
						lastFrameDecodeTime += delta;
						videoDecodingTime += delta;
						drawFrame(yCbCrBuffer);
					});
					targetFrameTime = currentTime + 1000.0 / fps;
				}

				// Check in when all audio runs out
				double bufferDuration = (audioBufferSize / targetRate) * 1000;
				std::vector<double> nextDelays;
				if (audioBufferedDuration <= bufferDuration * 2) {
					// NEED MOAR BUFFERS
				}
				else {
					// Check in when the audio buffer runs low again...
					nextDelays.push_back(bufferDuration / 2);

					if (hasVideo) {
						// Check in when the next frame is due
						// Subtract time we already spent decoding
						double deltaTimeSpent = getTimestamp() - startTimeSpent;
						nextDelays.push_back(frameDelay - deltaTimeSpent);
					}
				}

				//log([n, audioState.playbackPosition, frameEndTimestamp, audioBufferedDuration, bufferDuration, frameDelay, '[' + nextDelays.join("/") + ']'].join(", "));
				double nextDelay = *std::min_element(nextDelays.begin(), nextDelays.end());
				if (nextDelays.size() > 0) {
					pingProcessing(nextDelay - delta);
					return;
				}
			}
			else if (hasVideo) {
				// Video-only: drive on the video clock
				if (codec.frameReady() && getTimestamp() >= targetFrameTime) {
					// it's time to draw
					start = getTimestamp();
					ok = codec.decodeFrame([this, start, &delta](OgvCodec::Frame frame) {
						delta = (getTimestamp() - start);
						lastFrameDecodeTime += delta;
						videoDecodingTime += delta;
						drawFrame(frame);
						targetFrameTime += 1000.0 / fps;
					});
					if (ok) {
						// yay
						pingProcessing(0);
					}
					else {
						log("Bad video packet or something");
						pingProcessing(targetFrameTime - getTimestamp());
					}
				}
				else {
					// check in again soon!
					pingProcessing(targetFrameTime - getTimestamp());
				}
				return;
			}
			else {
				// Ok we're just waiting for more input.
				log("Still waiting for headers...");
			}
		}
	}


	// Video output functions...
	void drawFrame(OgvCodec::Frame yCbCrBuffer) {
		frameEndTimestamp = yCbCrBuffer.timestamp;

		double start, delta;

		start = getTimestamp();
		frameSink->drawFrame(yCbCrBuffer);
		delta = getTimestamp() - start;

		lastFrameDecodeTime += delta;
		drawingTime += delta;
		framesProcessed++;
		framesPlayed++;

		doFrameComplete();
	}

	void doFrameComplete() {
		double newFrameTimestamp = getTimestamp(),
			wallClockTime = newFrameTimestamp - lastFrameTimestamp,
			jitter = abs(wallClockTime - 1000 / videoInfo.fps);
		totalJitter += jitter;

		/*
		jsCallback('onframecallback', {
		cpuTime: lastFrameDecodeTime,
			 clockTime : wallClockTime
		});
		*/
		lastFrameDecodeTime = 0;
		lastFrameTimestamp = newFrameTimestamp;
	}

};

Player::Player() {
	//
}

void Player::load() {
	pimpl->load();
}
void Player::play() {
	pimpl->play();
}
void Player::pause(){
	pimpl->pause();
}

PlaybackStats Player::getPlaybackStats() {
	return pimpl->getPlaybackStats();
}
void Player::resetPlaybackStats() {
	pimpl->resetPlaybackStats();
}

void Player::setSrc(std::wstring _src) {
	pimpl->setSrc(_src);
}
std::wstring Player::getSrc() {
	return pimpl->getSrc();
}

double Player::getBufferedTime() {
	return pimpl->getBufferedTime();
}
double Player::getCurrentTime() {
	return pimpl->getCurrentTime();
}
bool Player::getPaused() {
	return pimpl->getPaused();
}
bool Player::getEnded() {
	return pimpl->getEnded();
}
void Player::setMuted(bool _muted) {
	pimpl->setMuted(_muted);
}
bool Player::getMuted() {
	return pimpl->getMuted();
}
int Player::getVideoWidth() {
	return pimpl->getVideoWidth();
}
int Player::getVideoHeight() {
	return pimpl->getVideoHeight();
}

double Player::getFrameRate() {
	return pimpl->getFrameRate();
}
int Player::getAudioChannels() {
	return pimpl->getAudioChannels();
}
int Player::getAudioSampleRate() {
	return pimpl->getAudioSampleRate();
}
