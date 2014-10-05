#include "OgvStream.h"

enum SourceState
{
	STATE_INVALID,      // Initial state. Have not started opening the stream.
	STATE_OPENING,      // BeginOpen is in progress.
	STATE_STOPPED,
	STATE_PAUSED,
	STATE_STARTED,
	STATE_SHUTDOWN
};


const UINT32 MAX_STREAMS = 32;

class StreamList sealed
{
	ComPtr<OgvStream>  m_streams[MAX_STREAMS];
	BYTE m_id[MAX_STREAMS];
	UINT32 m_count;

public:
	StreamList() : m_count(0)
	{
	}

	~StreamList()
	{
		Clear();
	}

	UINT32 GetCount() const { return m_count; }

	void Clear()
	{
		for (UINT32 i = 0; i < MAX_STREAMS; i++)
		{
			m_streams[i].Reset();
		}
		m_count = 0;
	}

	HRESULT AddStream(BYTE id, OgvStream *pStream)
	{
		if (GetCount() >= MAX_STREAMS)
		{
			return E_FAIL;
		}

		m_streams[m_count] = pStream;
		m_id[m_count] = id;
		m_count++;

		return S_OK;
	}

	OgvStream *Find(BYTE id)
	{

		// This method can return nullptr if the source did not create a
		// stream for this ID. In particular, this can happen if:
		//
		// 1) The stream type is not supported. See IsStreamTypeSupported().
		// 2) The source is still opening.
		//
		// Note: This method does not AddRef the stream object. The source
		// uses this method to access the streams. If the source hands out
		// a stream pointer (e.g. in the MENewStream event), the source
		// must AddRef the stream object.

		OgvStream *pStream = nullptr;
		for (UINT32 i = 0; i < m_count; i++)
		{
			if (m_id[i] == id)
			{
				pStream = m_streams[i].Get();
				break;
			}
		}
		return pStream;
	}

	// Accessor.
	OgvStream *operator[](DWORD index)
	{
		assert(index < m_count);
		return m_streams[index].Get();
	}

	// Const accessor.
	OgvStream *const operator[](DWORD index) const
	{
		assert(index < m_count);
		return m_streams[index].Get();
	}
};


// OgvSource: The media source object.
class OgvSource WrlSealed :
	//public OpQueue<CMPEG1Source, SourceOp>,
	public IMFMediaSource,
	public IMFGetService,
	public IMFRateControl
{
public:
	static ComPtr<OgvSource> CreateInstance();

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

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

	// Called by the byte stream handler.
	concurrency::task<void> OpenAsync(IMFByteStream *pStream);

	// Queues an asynchronous operation, specify by op-type.
	// (This method is public because the streams call it.)
	//HRESULT QueueAsyncOperation(SourceOp::Operation OpType);

	// Lock/Unlock:
	// Holds and releases the source's critical section. Called by the streams.
	/*
	_Acquires_lock_(m_critSec)
		void    Lock() { m_critSec.Lock(); }

	_Releases_lock_(m_critSec)
		void    Unlock() { m_critSec.Unlock(); }
		*/
	// Callbacks
	HRESULT OnByteStreamRead(IMFAsyncResult *pResult);  // Async callback for RequestData

private:
	OgvSource();
	~OgvSource();

	long m_cRef;
	CritSec                     m_critSec;                  // critical section for thread safety
	SourceState                 m_state;                    // Current state (running, stopped, paused)

	ComPtr<IMFMediaEventQueue>  m_spEventQueue;             // Event generator helper
	ComPtr<IMFPresentationDescriptor> m_spPresentationDescriptor; // Presentation descriptor.
	concurrency::task_completion_event<void> _openedEvent;  // Event used to signalize end of open operation.
	ComPtr<IMFByteStream>       m_spByteStream;

	StreamList                  m_streams;                  // Array of streams.

	DWORD                       m_cPendingEOS;              // Pending EOS notifications.
	ULONG                       m_cRestartCounter;          // Counter for sample requests.

	//ComPtr<SourceOp>            m_spCurrentOp;
	//ComPtr<SourceOp>            m_spSampleRequest;

	// Async callback helper.
	//AsyncCallback<OgvSource>  m_OnByteStreamRead;

	float                       m_flRate;
};
