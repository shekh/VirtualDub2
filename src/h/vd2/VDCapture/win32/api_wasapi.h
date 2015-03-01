#ifndef f_VD2_VDCAPTURE_WIN32_API_WASAPI_H
#define f_VD2_VDCAPTURE_WIN32_API_WASAPI_H

#include <windows.h>

#ifdef NTDDI_VISTA

#include <Audioclient.h>
#include <mmdeviceapi.h>

#else

struct IMMNotificationClient;
struct IMMDeviceCollection;
struct IPropertyStore;

typedef LONGLONG REFERENCE_TIME;

typedef enum {
	eRender,
	eCapture,
	eAll,
	EDataFlow_enum_count
} EDataFlow;

typedef enum {
	eConsole,
	eMultimedia,
	eCommunications,
	ERole_enum_count
} ERole;

typedef enum _AUDCLNT_SHAREMODE {
	AUDCLNT_SHAREMODE_SHARED,
	AUDCLNT_SHAREMODE_EXCLUSIVE
} AUDCLNT_SHAREMODE;

#define AUDCLNT_STREAMFLAGS_LOOPBACK 0x00020000
#define AUDCLNT_STREAMFLAGS_EVENTCALLBACK 0x00040000

#define AUDCLNT_BUFFERFLAGS_SILENT 0x2

#define DEVICE_STATE_ACTIVE			0x00000001
#define DEVICE_STATE_DISABLED		0x00000002
#define DEVICE_STATE_NOTPRESENT		0x00000004
#define DEVICE_STATE_UNPLUGGED		0x00000008
#define DEVICE_STATEMASK_ALL		0x0000000F

class __declspec(uuid("BCDE0395-E52F-467C-8E3D-C4579291692E")) MMDeviceEnumerator;

struct __declspec(uuid("D666063F-1587-4E43-81F1-B948E807363F")) IMMDevice : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Activate(REFIID iid, DWORD dwClsCtx, PROPVARIANT *pActivationParams, void **ppInterface) = 0;
	virtual HRESULT STDMETHODCALLTYPE OpenPropertyStore(DWORD stgmAccess, IPropertyStore *ppProperties) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetId(LPWSTR *ppstrId) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetState(DWORD *pdwState) = 0;
};

struct __declspec(uuid("A95664D2-9614-4F35-A746-DE8DB63617E6")) IMMDeviceEnumerator : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow dataFlow, DWORD dwStateMask, IMMDeviceCollection **ppDevices) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow dataFlow, ERole role, IMMDevice **ppDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR pwstrId, IMMDevice **ppDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient *pNotify) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient *pNotify) = 0;
};

struct __declspec(uuid("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2")) IAudioClient : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Initialize(AUDCLNT_SHAREMODE ShareMode, DWORD StreamFlags, REFERENCE_TIME hnsBufferDuration, REFERENCE_TIME hnsPeriodicity, const WAVEFORMATEX *pFormat, LPCGUID AudioSessionGuid) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetBufferSize(UINT32 *pNumBufferFrames) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME *phnsLatency) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentPadding(UINT32 *pNumPaddingFrames) = 0;
	virtual HRESULT STDMETHODCALLTYPE IsFormatSupported(AUDCLNT_SHAREMODE ShareMode, const WAVEFORMATEX *pFormat, WAVEFORMATEX **ppClosestMatch) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetMixFormat(WAVEFORMATEX **ppDeviceFormat) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDevicePeriod(REFERENCE_TIME *phnsDefaultDevicePeriod, REFERENCE_TIME *phnsMinimumDevicePeriod) = 0;
	virtual HRESULT STDMETHODCALLTYPE Start() = 0;
	virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE SetEventHandle(HANDLE eventHandle) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetService(REFIID riid, void **ppv) = 0;
};

struct __declspec(uuid("F294ACFC-3146-4483-A7BF-ADDCA7C260E2")) IAudioRenderClient : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT32 NumFramesRequested, BYTE **ppData) = 0;
	virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT32 NumFramesWritten, DWORD dwFlags) = 0;
};

struct __declspec(uuid("C8ADBD64-E71E-48a0-A4DE-185C395CD317")) IAudioCaptureClient : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE **ppData, UINT32 *pNumFramesToRead, DWORD *pdwFlags, UINT64 *pu64DevicePosition, UINT64 *pu64QPCPosition) = 0;
	virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT32 NumFramesRead) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetNextPacketSize(UINT32 *pNumFramesInNextPacket) = 0;
};

#endif

#endif
