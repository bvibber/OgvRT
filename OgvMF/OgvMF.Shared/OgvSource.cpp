#include "pch.h"

#include "OgvSource.h"
#include "OgvStream.h"

ComPtr<OgvSource> OgvSource::CreateInstance()
{
	ComPtr<OgvSource> spSource = Make<OgvSource>();
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

	// Create the media event queue.
	ThrowIfError(MFCreateEventQueue(m_spEventQueue.ReleaseAndGetAddressOf()));

	// Cache the byte-stream pointer.
	m_spByteStream = pStream;

	// Validate the capabilities of the byte stream.
	// The byte stream must be readable.
	ThrowIfError(pStream->GetCapabilities(&dwCaps));

	if ((dwCaps & MFBYTESTREAM_IS_READABLE) == 0)
	{
		ThrowException(MF_E_UNSUPPORTED_BYTESTREAM_TYPE);
	}

	m_state = STATE_OPENING;

	return concurrency::create_task(_openedEvent);
}

OgvSource::OgvSource() :
	m_state(STATE_INVALID),
	m_flRate(1.0f)
{
	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->IncrementObjectCount();
	}
}

OgvSource::~OgvSource()
{
	if (m_state != STATE_SHUTDOWN)
	{
		Shutdown();
	}

	auto module = ::Microsoft::WRL::GetModuleBase();
	if (module != nullptr)
	{
		module->DecrementObjectCount();
	}
}

// IMFMediaEventGenerator
HRESULT OgvSource::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_mutex);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->BeginGetEvent(pCallback, punkState);
	}

	return hr;
}

HRESULT OgvSource::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_mutex);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->EndGetEvent(pResult, ppEvent);
	}

	return hr;
}

HRESULT OgvSource::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent)
{
	// NOTE:
	// GetEvent can block indefinitely, so we don't hold the lock.
	// This requires some juggling with the event queue pointer.

	HRESULT hr = S_OK;

	ComPtr<IMFMediaEventQueue> spQueue;

	{
		AutoLock lock(m_mutex);

		// Check shutdown
		hr = CheckShutdown();

		// Get the pointer to the event queue.
		if (SUCCEEDED(hr))
		{
			spQueue = m_spEventQueue;
		}
	}

	// Now get the event.
	if (SUCCEEDED(hr))
	{
		hr = spQueue->GetEvent(dwFlags, ppEvent);
	}

	return hr;
}

HRESULT OgvSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
	HRESULT hr = S_OK;

	AutoLock lock(m_mutex);

	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = m_spEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
	}

	return hr;
}


// IMFMediaSource
HRESULT OgvSource::CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor)
{
	if (ppPresentationDescriptor == nullptr)
	{
		return E_POINTER;
	}

	HRESULT hr = S_OK;

	AutoLock lock(m_mutex);

	// Fail if the source is shut down.
	hr = CheckShutdown();

	// Fail if the source was not initialized yet.
	if (SUCCEEDED(hr))
	{
		hr = IsInitialized();
	}

	// Do we have a valid presentation descriptor?
	if (SUCCEEDED(hr))
	{
		if (m_spPresentationDescriptor == nullptr)
		{
			hr = MF_E_NOT_INITIALIZED;
		}
	}

	// Clone our presentation descriptor.
	if (SUCCEEDED(hr))
	{
		hr = m_spPresentationDescriptor->Clone(ppPresentationDescriptor);
	}

	return hr;
}

HRESULT OgvSource::GetCharacteristics(DWORD *pdwCharacteristics)
{
	return E_NOTIMPL;
}

HRESULT OgvSource::Pause()
{
	AutoLock lock(m_mutex);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = QueueAsyncOp([this] {
			try {
				if (m_state != STATE_STARTED) {
					ThrowException(MF_E_INVALID_STATE_TRANSITION);
				}
				else
				{
					m_state = STATE_PAUSED;
					ForEachStream([](ComPtr<OgvStream> stream) {
						stream->Pause();
					});
				}
				m_spEventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, S_OK, nullptr);
			}
			catch (Exception ^ex) {
				m_spEventQueue->QueueEventParamVar(MESourcePaused, GUID_NULL, ex->HResult, nullptr);
			}
		});
	}

	return hr;
}

HRESULT OgvSource::Shutdown()
{

	AutoLock lock(m_mutex);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		ForEachStream([](ComPtr<OgvStream> stream) {
			stream->Shutdown();
		});

		m_state = STATE_SHUTDOWN;

		if (m_spEventQueue)
		{
			(void)m_spEventQueue->Shutdown();
		}

		m_streams.empty();
		m_asyncOpQueue.empty();

		m_spEventQueue.Reset();
		m_spPresentationDescriptor.Reset();
		m_spByteStream.Reset();
	}

	return hr;
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
	AutoLock lock(m_mutex);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		hr = QueueAsyncOp([this] {
			try {
				if (m_state != STATE_STARTED) {
					ThrowException(MF_E_INVALID_STATE_TRANSITION);
				}
				else
				{
					m_state = STATE_STOPPED;
					ForEachStream([](ComPtr<OgvStream> stream) {
						stream->Stop();
					});
				}
				m_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, nullptr);
			}
			catch (Exception ^ex) {
				m_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, ex->HResult, nullptr);
			}
		});
	}

	return hr;
}

// IMFGetService
HRESULT OgvSource::GetService(_In_ REFGUID guidService, _In_ REFIID riid, _Out_opt_ LPVOID *ppvObject)
{
	return E_NOTIMPL;
}

// IMFRateControl
HRESULT OgvSource::SetRate(BOOL fThin, float flRate)
{
	if (fThin)
	{
		return MF_E_THINNING_UNSUPPORTED;
	}
	if (flRate < 0.0f)
	{
		return MF_E_UNSUPPORTED_RATE;
	}

	AutoLock lock(m_mutex);

	HRESULT hr = S_OK;

	// Fail if the source is shut down.
	hr = CheckShutdown();

	if (SUCCEEDED(hr))
	{
		if (flRate != m_flRate) {
			hr = QueueAsyncOp([this, flRate] {
				try {
					if (m_state != STATE_STARTED) {
						ThrowException(MF_E_INVALID_STATE_TRANSITION);
					}
					else
					{
						m_state = STATE_PAUSED;
						ForEachStream([flRate](ComPtr<OgvStream> stream) {
							stream->SetRate(flRate);
						});
					}
					m_spEventQueue->QueueEventParamVar(MESourceRateChanged, GUID_NULL, S_OK, nullptr);
				}
				catch (Exception ^ex) {
					m_spEventQueue->QueueEventParamVar(MESourceRateChanged, GUID_NULL, ex->HResult, nullptr);
				}
			});
		}
	}

	return hr;
}

HRESULT OgvSource::GetRate(_Inout_opt_ BOOL *pfThin, _Inout_opt_ float *pflRate)
{
	if (pfThin == nullptr || pflRate == nullptr)
	{
		return E_INVALIDARG;
	}

	AutoLock lock(m_mutex);
	*pfThin = FALSE;
	*pflRate = m_flRate;

	return S_OK;
}
