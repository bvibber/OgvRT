#pragma once

// forward
class OgvSource;
//#include "OgvSource.h"

enum OgvStreamType {
	OGV_STREAM_UNKNOWN,
	OGV_STREAM_VORBIS,
	OGV_STREAM_THEORA
};

class OgvStream WrlSealed : public IMFMediaStream {

public:
	OgvStream(OgvSource *pSource, IMFStreamDescriptor *pSD, OgvStreamType pStreamType);
	~OgvStream();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFMediaEventGenerator
	STDMETHODIMP BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState);
	STDMETHODIMP EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
	STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent);
	STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);

	// IMFMediaStream
	STDMETHODIMP GetMediaSource(IMFMediaSource **ppMediaSource);
	STDMETHODIMP GetStreamDescriptor(IMFStreamDescriptor **ppStreamDescriptor);
	STDMETHODIMP RequestSample(IUnknown *pToken);

	// For the source to call
	STDMETHODIMP Start();
	STDMETHODIMP Pause();
	STDMETHODIMP Stop();
	STDMETHODIMP Shutdown();
	STDMETHODIMP_(BOOL) IsActive();
	STDMETHODIMP SetRate(float rate);

private:
	long m_cRef;
	ComPtr<OgvSource> m_spSource;
	ComPtr<IMFStreamDescriptor> m_spStreamDescriptor;
	ComPtr<IMFMediaEventQueue> m_spEventQueue;

	OgvStreamType m_cStreamType;

	float m_flRate;
};
