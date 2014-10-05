#include "pch.h"
#include "OgvSource.h"
#include <wrl\module.h>

ComPtr<OgvSource> OgvSource::CreateInstance()
{
	ComPtr<OgvSource> spSource;
	spSource.Attach(new (std::nothrow) OgvSource());
	if (spSource == nullptr) {
		throw ref new OutOfMemoryException();
	}
	return spSource;
}

concurrency::task<void> OgvSource::OpenAsync(IMFByteStream *pStream)
{
	if (pStream == nullptr)
	{
		throw ref new InvalidArgumentException();
	}

	if (m_state != STATE_INVALID)
	{
		ThrowException(MF_E_INVALIDREQUEST);
	}

	DWORD dwCaps = 0;

	AutoLock lock(m_critSec);

	// Create the media event queue.
	ThrowIfError(MFCreateEventQueue(m_spEventQueue.ReleaseAndGetAddressOf()));

	// Cache the byte-stream pointer.
	m_spByteStream = pStream;

	// Validate the capabilities of the byte stream.
	// The byte stream must be readable and seekable.
	ThrowIfError(pStream->GetCapabilities(&dwCaps));

	if ((dwCaps & MFBYTESTREAM_IS_SEEKABLE) == 0)
	{
		ThrowException(MF_E_BYTESTREAM_NOT_SEEKABLE);
	}
	else if ((dwCaps & MFBYTESTREAM_IS_READABLE) == 0)
	{
		ThrowException(MF_E_UNSUPPORTED_BYTESTREAM_TYPE);
	}

	// @todo create our codec objects and stuff

	//RequestData(READ_SIZE);

	m_state = STATE_OPENING;

	return concurrency::create_task(_openedEvent);
}

OgvSource::OgvSource()
{
}

OgvSource::~OgvSource()
{
}

// IUnknown
HRESULT OgvSource::QueryInterface(REFIID riid, void **ppv)
{
	if (ppv == nullptr) {
		return E_POINTER;
	}

	HRESULT hr = E_NOINTERFACE;
	if (riid == IID_IUnknown ||
		riid == IID_IMFMediaEventGenerator ||
		riid == IID_IMFMediaSource)
	{
		(*ppv) = static_cast<IMFMediaSource *>(this);
		AddRef();
		hr = S_OK;
	}
	else if (riid == IID_IMFGetService)
	{
		(*ppv) = static_cast<IMFGetService *>(this);
		AddRef();
		hr = S_OK;
	}
	else if (riid == IID_IMFRateControl)
	{
		(*ppv) = static_cast<IMFRateControl *>(this);
		AddRef();
		hr = S_OK;
	}

	return hr;
}

ULONG OgvSource::AddRef()
{
	return _InterlockedIncrement(&m_cRef);
}

ULONG OgvSource::Release()
{
	LONG cRef = _InterlockedDecrement(&m_cRef);
	if (cRef == 0)
	{
		delete this;
	}
	return cRef;
}

// IMFMediaEventGenerator
HRESULT OgvSource::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
	return E_NOTIMPL;
}


// IMFMediaSource
HRESULT OgvSource::CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::GetCharacteristics(DWORD *pdwCharacteristics)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::Pause()
{
	return E_NOTIMPL;
}

HRESULT OgvSource::Shutdown()
{
	return E_NOTIMPL;
}

HRESULT OgvSource::Start(
	IMFPresentationDescriptor *pPresentationDescriptor,
	const GUID *pguidTimeFormat,
	const PROPVARIANT *pvarStartPosition
	)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::Stop()
{
	return E_NOTIMPL;
}

// IMFGetService
HRESULT OgvSource::GetService(_In_ REFGUID guidService, _In_ REFIID riid, _Out_opt_ LPVOID *ppvObject)
{
	return E_NOTIMPL;
}

// IMFRateControl
HRESULT OgvSource::SetRate(BOOL fThin, float flRate)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::GetRate(_Inout_opt_ BOOL *pfThin, _Inout_opt_ float *pflRate)
{
	return E_NOTIMPL;
}
