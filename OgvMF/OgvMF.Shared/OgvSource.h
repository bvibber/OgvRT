#pragma once

class OgvStream;

enum SourceState
{
	STATE_INVALID,      // Initial state. Have not started opening the stream.
	STATE_OPENING,      // BeginOpen is in progress.
	STATE_STOPPED,
	STATE_PAUSED,
	STATE_STARTED,
	STATE_SHUTDOWN
};


// OgvSource: The media source object.
class OgvSource WrlSealed :
	public RuntimeClass<
		RuntimeClassFlags<ClassicCom>,
		IMFMediaSource,
		IMFGetService,
		IMFRateControl
	>
{
public:
	// IMFMediaEventGenerator
	STDMETHODIMP BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState);
	STDMETHODIMP EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
	STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent);
	STDMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);

	// IMFMediaSource
	STDMETHODIMP CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor);
	STDMETHODIMP GetCharacteristics(DWORD *pdwCharacteristics);
	STDMETHODIMP Pause();
	STDMETHODIMP Shutdown();
	STDMETHODIMP Start(
		IMFPresentationDescriptor *pPresentationDescriptor,
		const GUID *pguidTimeFormat,
		const PROPVARIANT *pvarStartPosition
		);
	STDMETHODIMP Stop();

	// IMFGetService
	IFACEMETHOD(GetService) (_In_ REFGUID guidService, _In_ REFIID riid, _Out_opt_ LPVOID *ppvObject);

	// IMFRateControl
	IFACEMETHOD(SetRate) (BOOL fThin, float flRate);
	IFACEMETHOD(GetRate) (_Inout_opt_ BOOL *pfThin, _Inout_opt_ float *pflRate);

	// Helpers for the byte stream
	static ComPtr<OgvSource> CreateInstance();
	concurrency::task<void> OpenAsync(IMFByteStream *pStream);

	OgvSource();
	~OgvSource();

protected:
	ComPtr<IMFMediaEventQueue>  m_spEventQueue;             // Event generator helper
	ComPtr<IMFPresentationDescriptor> m_spPresentationDescriptor; // Presentation descriptor.
	ComPtr<IMFByteStream>       m_spByteStream;

	// Thread safety
	std::mutex m_mutex;
	typedef std::unique_lock<std::mutex> AutoLock;

	// Async operation queue
	typedef std::function<void(void)> OpFunc;
	std::queue<OpFunc> m_asyncOpQueue;
	std::thread m_asyncThread;

	HRESULT QueueAsyncOp(OpFunc op) {
		m_asyncOpQueue.push(op);
		return S_OK;
	}

	// Streams
	std::vector<ComPtr<OgvStream>> m_streams;
	void ForEachStream(std::function<void(ComPtr<OgvStream>)> func) {
		std::for_each(std::begin(m_streams), std::end(m_streams), func);
	}

	// State
	SourceState                 m_state;                    // Current state (running, stopped, paused)

	HRESULT CheckShutdown() const
	{
		return (m_state == STATE_SHUTDOWN ? MF_E_SHUTDOWN : S_OK);
	}

	HRESULT IsInitialized() const
	{
		if (m_state == STATE_OPENING || m_state == STATE_INVALID)
		{
			return MF_E_NOT_INITIALIZED;
		}
		else
		{
			return S_OK;
		}
	}

	// Rate!
	float                       m_flRate;

	concurrency::task_completion_event<void> _openedEvent;  // Event used to signalize end of open operation.
};

