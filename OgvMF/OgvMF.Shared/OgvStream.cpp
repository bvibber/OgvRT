#include "pch.h"

#include "OgvSource.h"
#include "OgvStream.h"

OgvStream::OgvStream(OgvSource *pSource, IMFStreamDescriptor *pSD, OgvStreamType streamType) :
	m_cRef(1),
	m_spSource(pSource),
	m_spStreamDescriptor(pSD),
	m_cStreamType(streamType)
{
	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->IncrementObjectCount();
	}

	assert(pSource != nullptr);
	assert(pSD != nullptr);
}

OgvStream::~OgvStream()
{
	//assert(m_state == STATE_SHUTDOWN);

	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->DecrementObjectCount();
	}
}

// IUnknown
HRESULT OgvStream::QueryInterface(REFIID iid, void **ppv)
{
	if (ppv == nullptr)
	{
		return E_POINTER;
	}

	HRESULT hr = E_NOINTERFACE;
	if (iid == IID_IUnknown ||
		iid == IID_IMFMediaEventGenerator ||
		iid == IID_IMFMediaStream)
	{
		(*ppv) = static_cast<IMFMediaStream *>(this);
		AddRef();
		hr = S_OK;
	}

	return hr;
}

ULONG OgvStream::AddRef()
{
	return _InterlockedIncrement(&m_cRef);
}

ULONG OgvStream::Release()
{
	LONG cRef = _InterlockedDecrement(&m_cRef);
	if (cRef == 0)
	{
		delete this;
	}
	return cRef;
}

// IMFMediaEventGenerator
HRESULT OgvStream::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	return E_NOTIMPL;
}

HRESULT OgvStream::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
	return E_NOTIMPL;
}

HRESULT OgvStream::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent){
	return E_NOTIMPL;
}

HRESULT OgvStream::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
	return E_NOTIMPL;
}


// IMFMediaStream
HRESULT OgvStream::GetMediaSource(IMFMediaSource **ppMediaSource)
{
	return E_NOTIMPL;
}

HRESULT OgvStream::GetStreamDescriptor(IMFStreamDescriptor **ppStreamDescriptor)
{
	return E_NOTIMPL;
}

HRESULT OgvStream::RequestSample(IUnknown *pToken)
{
	return E_NOTIMPL;
}


// For the source

HRESULT OgvStream::Start()
{
	return E_NOTIMPL;
}

HRESULT OgvStream::Pause()
{
	return E_NOTIMPL;
}

HRESULT OgvStream::Stop()
{
	return E_NOTIMPL;
}

HRESULT OgvStream::Shutdown()
{
	return E_NOTIMPL;
}

BOOL OgvStream::IsActive()
{
	return true; // ???
}

HRESULT OgvStream::SetRate(float rate)
{
	m_flRate = rate;
	return S_OK;
}
