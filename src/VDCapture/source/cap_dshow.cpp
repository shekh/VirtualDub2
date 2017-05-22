//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#define NO_DSHOW_STRSAFE
#include <vd2/VDCapture/capdriver.h>
#include <vd2/VDCapture/cap_dshow.h>
#include <vd2/system/profile.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/time.h>
#include <vd2/system/fraction.h>
#include <vd2/system/error.h>
#include <vd2/system/math.h>
#include <vd2/system/log.h>
#include <vd2/system/refcount.h>
#include <vd2/system/registry.h>
#include <vd2/system/strutil.h>
#include <vd2/system/thread.h>
#include <objbase.h>
#include <dshow.h>
#include <windows.h>
#include <guiddef.h>
#include <dvdmedia.h>		// VIDEOINFOHEADER2
#include <ks.h>
#include <ksmedia.h>
#include <streams.h>
#include <initguid.h>
#include <vector>

using namespace nsVDCapture;

#pragma comment(lib, "amstrmid.lib")

extern HINSTANCE g_hInst;

#ifdef _MSC_VER
	#pragma warning(disable: 4355)		// warning C4355: 'this' : used in base member initializer list
#endif

#ifdef _DEBUG
	#define DS_VERIFY(exp, msg) if (FAILED(hr = (exp))) { VDDEBUG("Failed: " msg " [%08lx : %s]\n", hr, GetDXErrorName(hr)); VDDumpFilterGraphDShow(mpGraph); TearDownGraph(); return false; } else
#else
	#define DS_VERIFY(exp, msg) if (FAILED(hr = (exp))) { VDLogF(kVDLogWarning, L"CapDShow: Failed to build filter graph: " L##msg L"(error code: %08x)\n", hr); TearDownGraph(); return false; } else
#endif

//#define VD_DSHOW_VERBOSE_LOGGING 1

#if VD_DSHOW_VERBOSE_LOGGING
	#define DS_VERBOSE_LOG(msg) (VDLog(kVDLogInfo, VDStringW(L##msg)))
	#define DS_VERBOSE_LOGF(...) (VDLog(kVDLogInfo, VDswprintf(__VA_ARGS__)))
#else
	#define DS_VERBOSE_LOG(msg) ((void)0)
	#define DS_VERBOSE_LOGF(...) ((void)0)
#endif
///////////////////////////////////////////////////////////////////////////
// qedit.h replacement
//
// Microsoft dropped <qedit.h> when DirectShow was moved from the DirectX
// SDK to the Platform/Windows SDK, and they never put it back. Doing a
// straight #import throws a buttload of duplicate definition errors, so
// we copy the specifically needed interfaces here.
//
// #import "libid:78530B68-61F9-11D2-8CAD-00A024580902" raw_interfaces_only no_namespace named_guids

extern "C" const GUID __declspec(selectany) CLSID_SampleGrabber =
    {0xc1f400a0,0x3f08,0x11d3,{0x9f,0x0b,0x00,0x60,0x08,0x03,0x9e,0x37}};
extern "C" const GUID __declspec(selectany) CLSID_NullRenderer =
    {0xc1f400a4,0x3f08,0x11d3,{0x9f,0x0b,0x00,0x60,0x08,0x03,0x9e,0x37}};

extern "C" const GUID __declspec(selectany) IID_ISampleGrabberCB =
    {0x0579154a,0x2b53,0x4994,{0xb0,0xd0,0xe7,0x73,0x14,0x8e,0xff,0x85}};
extern "C" const GUID __declspec(selectany) IID_ISampleGrabber =
    {0x6b652fff,0x11fe,0x4fce,{0x92,0xad,0x02,0x66,0xb5,0xd7,0xc7,0x8f}};

struct __declspec(uuid("0579154a-2b53-4994-b0d0-e773148eff85"))
ISampleGrabberCB : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SampleCB (
        double SampleTime,
        struct IMediaSample * pSample ) = 0;
      virtual HRESULT __stdcall BufferCB (
        double SampleTime,
        unsigned char * pBuffer,
        long BufferLen ) = 0;
};

struct __declspec(uuid("6b652fff-11fe-4fce-92ad-0266b5d7c78f"))
ISampleGrabber : IUnknown
{
    //
    // Raw methods provided by interface
    //

      virtual HRESULT __stdcall SetOneShot (
        long OneShot ) = 0;
      virtual HRESULT __stdcall SetMediaType (
        struct _AMMediaType * pType ) = 0;
      virtual HRESULT __stdcall GetConnectedMediaType (
        struct _AMMediaType * pType ) = 0;
      virtual HRESULT __stdcall SetBufferSamples (
        long BufferThem ) = 0;
      virtual HRESULT __stdcall GetCurrentBuffer (
        /*[in,out]*/ long * pBufferSize,
        /*[out]*/ long * pBuffer ) = 0;
      virtual HRESULT __stdcall GetCurrentSample (
        /*[out,retval]*/ struct IMediaSample * * ppSample ) = 0;
      virtual HRESULT __stdcall SetCallback (
        struct ISampleGrabberCB * pCallback,
        long WhichMethodToCallback ) = 0;
};

///////////////////////////////////////////////////////////////////////////
//
//	smart pointers
//
///////////////////////////////////////////////////////////////////////////

namespace {
	// New auto ptr for COM because the MS one has some really unsafe
	// overloads -- for instance, operator&() makes it a landmine if you
	// try putting it in an STL container. Also, the DDK doesn't come
	// with comsupp.lib.

	template<class T, const IID *T_IID>
	class VD_MSCOMAutoPtr {
	public:
		VD_MSCOMAutoPtr() : mp(NULL) {}

		VD_MSCOMAutoPtr(const VD_MSCOMAutoPtr& src)
			: mp(src.mp)
		{
			if (mp)
				mp->AddRef();
		}

		VD_MSCOMAutoPtr(T *p) : mp(p) {
			if (mp)
				mp->AddRef();
		}

		~VD_MSCOMAutoPtr() {
			if (mp)
				mp->Release();
		}

		VD_MSCOMAutoPtr& operator=(const VD_MSCOMAutoPtr& src) {
			T *const p = src.mp;

			if (p != mp) {
				if (p)
					p->AddRef();
				T *pOld = mp;
				mp = p;
				if (pOld)
					pOld->Release();
			}
			return *this;
		}

		VD_MSCOMAutoPtr& operator=(T *p) {
			if (mp != p) {
				if(p)
					p->AddRef();
				T *pOld = mp;
				mp = p;
				if (pOld)
					pOld->Release();
			}
			return *this;
		}

		operator T*() const {
			return mp;
		}

		T& operator*() const {
			return *mp;
		}

		T *operator->() const {
			return mp;
		}

		T **operator~() {
			if (mp) {
				mp->Release();
				mp = NULL;
			}
			return &mp;
		}

		bool operator!() const { return !mp; }
		bool operator==(T* p) const { return mp == p; }
		bool operator!=(T* p) const { return mp != p; }

		void Swap(VD_MSCOMAutoPtr& x) {
			std::swap(mp, x.mp);
		}

		HRESULT CreateInstance(const CLSID& clsid, IUnknown *pOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) {
			if (mp) {
				mp->Release();
				mp = NULL;
			}

			return CoCreateInstance(clsid, pOuter, dwClsContext, *T_IID, (void **)&mp);
		}

		T *mp;
	};

	template<class T>
	bool operator==(T *p, const VD_MSCOMAutoPtr<T, &__uuidof(T)>& ap) {
		return p == ap.mp;
	}

	template<class T>
	bool operator!=(T *p, const VD_MSCOMAutoPtr<T, &__uuidof(T)>& ap) {
		return p != ap.mp;
	}

	#define I_HATE(x) typedef VD_MSCOMAutoPtr<x, &__uuidof(x)> x##Ptr

	I_HATE(IAMAnalogVideoDecoder);
	I_HATE(IAMAudioInputMixer);
	I_HATE(IAMCrossbar);
	I_HATE(IAMStreamConfig);
	I_HATE(IAMStreamControl);
	I_HATE(IAMTuner);
	I_HATE(IAMTVTuner);
	I_HATE(IAMVfwCaptureDialogs);
	I_HATE(IBaseFilter);
	I_HATE(ICaptureGraphBuilder2);
	I_HATE(IEnumFilters);
	I_HATE(IEnumMediaTypes);
	I_HATE(IEnumPins);
	I_HATE(IFilterGraph);
	I_HATE(IGraphBuilder);
	I_HATE(IMediaControl);
	I_HATE(IMediaEventEx);
	I_HATE(IMediaFilter);
	I_HATE(IMediaSample);
	I_HATE(IMoniker);
	I_HATE(IPin);
	I_HATE(IReferenceClock);
	I_HATE(ISampleGrabber);
	I_HATE(ISpecifyPropertyPages);
	I_HATE(IVideoWindow);
	I_HATE(IKsPropertySet);
	I_HATE(IClassFactory);
	I_HATE(IAMVideoProcAmp);

	#undef I_HATE
}

namespace {
	#ifdef _DEBUG
		const char *GetDXErrorName(const HRESULT hr) {
#define X(err) case err: return #err
			switch(hr) {
				X(VFW_E_INVALIDMEDIATYPE);
				X(VFW_E_INVALIDSUBTYPE);
				X(VFW_E_NEED_OWNER);
				X(VFW_E_ENUM_OUT_OF_SYNC);
				X(VFW_E_ALREADY_CONNECTED);
				X(VFW_E_FILTER_ACTIVE);
				X(VFW_E_NO_TYPES);
				X(VFW_E_NO_ACCEPTABLE_TYPES);
				X(VFW_E_INVALID_DIRECTION);
				X(VFW_E_NOT_CONNECTED);
				X(VFW_E_NO_ALLOCATOR);
				X(VFW_E_RUNTIME_ERROR);
				X(VFW_E_BUFFER_NOTSET);
				X(VFW_E_BUFFER_OVERFLOW);
				X(VFW_E_BADALIGN);
				X(VFW_E_ALREADY_COMMITTED);
				X(VFW_E_BUFFERS_OUTSTANDING);
				X(VFW_E_NOT_COMMITTED);
				X(VFW_E_SIZENOTSET);
				X(VFW_E_NO_CLOCK);
				X(VFW_E_NO_SINK);
				X(VFW_E_NO_INTERFACE);
				X(VFW_E_NOT_FOUND);
				X(VFW_E_CANNOT_CONNECT);
				X(VFW_E_CANNOT_RENDER);
				X(VFW_E_CHANGING_FORMAT);
				X(VFW_E_NO_COLOR_KEY_SET);
				X(VFW_E_NOT_OVERLAY_CONNECTION);
				X(VFW_E_NOT_SAMPLE_CONNECTION);
				X(VFW_E_PALETTE_SET);
				X(VFW_E_COLOR_KEY_SET);
				X(VFW_E_NO_COLOR_KEY_FOUND);
				X(VFW_E_NO_PALETTE_AVAILABLE);
				X(VFW_E_NO_DISPLAY_PALETTE);
				X(VFW_E_TOO_MANY_COLORS);
				X(VFW_E_STATE_CHANGED);
				X(VFW_E_NOT_STOPPED);
				X(VFW_E_NOT_PAUSED);
				X(VFW_E_NOT_RUNNING);
				X(VFW_E_WRONG_STATE);
				X(VFW_E_START_TIME_AFTER_END);
				X(VFW_E_INVALID_RECT);
				X(VFW_E_TYPE_NOT_ACCEPTED);
				X(VFW_E_SAMPLE_REJECTED);
				X(VFW_E_SAMPLE_REJECTED_EOS);
				X(VFW_E_DUPLICATE_NAME);
				X(VFW_S_DUPLICATE_NAME);
				X(VFW_E_TIMEOUT);
				X(VFW_E_INVALID_FILE_FORMAT);
				X(VFW_E_ENUM_OUT_OF_RANGE);
				X(VFW_E_CIRCULAR_GRAPH);
				X(VFW_E_NOT_ALLOWED_TO_SAVE);
				X(VFW_E_TIME_ALREADY_PASSED);
				X(VFW_E_ALREADY_CANCELLED);
				X(VFW_E_CORRUPT_GRAPH_FILE);
				X(VFW_E_ADVISE_ALREADY_SET);
				X(VFW_S_STATE_INTERMEDIATE);
				X(VFW_E_NO_MODEX_AVAILABLE);
				X(VFW_E_NO_ADVISE_SET);
				X(VFW_E_NO_FULLSCREEN);
				X(VFW_E_IN_FULLSCREEN_MODE);
				X(VFW_E_UNKNOWN_FILE_TYPE);
				X(VFW_E_CANNOT_LOAD_SOURCE_FILTER);
				X(VFW_S_PARTIAL_RENDER);
				X(VFW_E_FILE_TOO_SHORT);
				X(VFW_E_INVALID_FILE_VERSION);
				X(VFW_S_SOME_DATA_IGNORED);
				X(VFW_S_CONNECTIONS_DEFERRED);
				X(VFW_E_INVALID_CLSID);
				X(VFW_E_INVALID_MEDIA_TYPE);
				X(VFW_E_SAMPLE_TIME_NOT_SET);
				X(VFW_S_RESOURCE_NOT_NEEDED);
				X(VFW_E_MEDIA_TIME_NOT_SET);
				X(VFW_E_NO_TIME_FORMAT_SET);
				X(VFW_E_MONO_AUDIO_HW);
				X(VFW_S_MEDIA_TYPE_IGNORED);
				X(VFW_E_NO_DECOMPRESSOR);
				X(VFW_E_NO_AUDIO_HARDWARE);
				X(VFW_S_VIDEO_NOT_RENDERED);
				X(VFW_S_AUDIO_NOT_RENDERED);
				X(VFW_E_RPZA);
				X(VFW_S_RPZA);
				X(VFW_E_PROCESSOR_NOT_SUITABLE);
				X(VFW_E_UNSUPPORTED_AUDIO);
				X(VFW_E_UNSUPPORTED_VIDEO);
				X(VFW_E_MPEG_NOT_CONSTRAINED);
				X(VFW_E_NOT_IN_GRAPH);
				X(VFW_S_ESTIMATED);
				X(VFW_E_NO_TIME_FORMAT);
				X(VFW_E_READ_ONLY);
				X(VFW_S_RESERVED);
				X(VFW_E_BUFFER_UNDERFLOW);
				X(VFW_E_UNSUPPORTED_STREAM);
				X(VFW_E_NO_TRANSPORT);
				X(VFW_S_STREAM_OFF);
				X(VFW_S_CANT_CUE);
				X(VFW_E_BAD_VIDEOCD);
				X(VFW_S_NO_STOP_TIME);
				X(VFW_E_OUT_OF_VIDEO_MEMORY);
				X(VFW_E_VP_NEGOTIATION_FAILED);
				X(VFW_E_DDRAW_CAPS_NOT_SUITABLE);
				X(VFW_E_NO_VP_HARDWARE);
				X(VFW_E_NO_CAPTURE_HARDWARE);
				X(VFW_E_DVD_OPERATION_INHIBITED);
				X(VFW_E_DVD_INVALIDDOMAIN);
				X(VFW_E_DVD_NO_BUTTON);
				X(VFW_E_DVD_GRAPHNOTREADY);
				X(VFW_E_DVD_RENDERFAIL);
				X(VFW_E_DVD_DECNOTENOUGH);
				X(VFW_E_DVD_NOT_IN_KARAOKE_MODE);
				X(VFW_E_FRAME_STEP_UNSUPPORTED);
				X(VFW_E_PIN_ALREADY_BLOCKED_ON_THIS_THREAD);
				X(VFW_E_PIN_ALREADY_BLOCKED);
				X(VFW_E_CERTIFICATION_FAILURE);
				X(VFW_E_VMR_NOT_IN_MIXER_MODE);
				X(VFW_E_VMR_NO_AP_SUPPLIED);
				X(VFW_E_VMR_NO_DEINTERLACE_HW);
				X(VFW_E_VMR_NO_PROCAMP_HW);
				X(VFW_E_DVD_VMR9_INCOMPATIBLEDEC);
				X(VFW_E_BAD_KEY);
			default:
				return "";
			}
#undef X
		}
	#else
		const char *GetDXErrorName(const HRESULT hr) {
			return "";
		}
	#endif

	HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
	{
		IMoniker * pMoniker;
		IRunningObjectTable *pROT;
		if (FAILED(GetRunningObjectTable(0, &pROT))) {
			return E_FAIL;
		}
		WCHAR wsz[256];
		swprintf(wsz, 256, L"FilterGraph %08p pid %08x", (void*)pUnkGraph, GetCurrentProcessId());
		HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
		if (SUCCEEDED(hr)) {
			hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
			pMoniker->Release();
		}
		pROT->Release();
		return hr;
	}

	void RemoveFromRot(DWORD pdwRegister)
	{
		IRunningObjectTable *pROT;
		if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
			pROT->Revoke(pdwRegister);
			pROT->Release();
		}
	}

	void DestroySubgraph(IFilterGraph *pGraph, IBaseFilter *pFilt, IBaseFilter *pKeepFilter, IBaseFilter *pKeepFilter2) {
		IEnumPins *pEnum;

		if (!pFilt)
			return;

		if (SUCCEEDED(pFilt->EnumPins(&pEnum))) {
			IPin *pPin;

			pEnum->Reset();

			for(;;) {
				HRESULT hr = pEnum->Next(1, &pPin, 0);
				if (hr == VFW_E_ENUM_OUT_OF_SYNC) {
					hr = pEnum->Reset();
					if (SUCCEEDED(hr))
						continue;
				}
				if (hr != S_OK)
					break;

				PIN_DIRECTION dir;

				VDVERIFY(SUCCEEDED(pPin->QueryDirection(&dir)));

				if (dir == PINDIR_OUTPUT) {
					IPin *pPin2;

					if (SUCCEEDED(pPin->ConnectedTo(&pPin2))) {
						PIN_INFO pi;

						if (SUCCEEDED(pPin2->QueryPinInfo(&pi))) {
							DestroySubgraph(pGraph, pi.pFilter, pKeepFilter, pKeepFilter2);

							if ((!pKeepFilter || pi.pFilter != pKeepFilter) && (!pKeepFilter2 || pi.pFilter != pKeepFilter2))
								VDVERIFY(SUCCEEDED(pGraph->RemoveFilter(pi.pFilter)));

							pi.pFilter->Release();
						}

						pPin2->Release();
					}
				}

				pPin->Release();
			}

			pEnum->Release();
		}
	}

	AM_MEDIA_TYPE *RizaCopyMediaType(const AM_MEDIA_TYPE *pSrc) {
		AM_MEDIA_TYPE *pamt;

		if (pamt = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE))) {

			*pamt = *pSrc;

			if (pamt->pbFormat = (BYTE *)CoTaskMemAlloc(pSrc->cbFormat)) {
				memcpy(pamt->pbFormat, pSrc->pbFormat, pSrc->cbFormat);

				if (pamt->pUnk)
					pamt->pUnk->AddRef();

				return pamt;
			}

			CoTaskMemFree(pamt);
		}

		return NULL;
	}

	bool RizaCopyMediaType(AM_MEDIA_TYPE *pDst, const AM_MEDIA_TYPE *pSrc) {
		*pDst = *pSrc;

		if (pDst->pbFormat = (BYTE *)CoTaskMemAlloc(pSrc->cbFormat)) {
			memcpy(pDst->pbFormat, pSrc->pbFormat, pSrc->cbFormat);

			if (pDst->pUnk)
				pDst->pUnk->AddRef();

			return true;
		}

		return false;
	}

	void RizaDeleteMediaType(AM_MEDIA_TYPE *pamt) {
		if (!pamt)
			return;

		if (pamt->pUnk)
			pamt->pUnk->Release();

		if (pamt->pbFormat)
			CoTaskMemFree(pamt->pbFormat);

		CoTaskMemFree(pamt);
	}

	bool SetClockFromDownstream(IBaseFilter *pFilt, IMediaFilter *pGraphMF) {
		IReferenceClock *pRC;
		if (SUCCEEDED(pFilt->QueryInterface(IID_IReferenceClock, (void **)&pRC))) {
			pGraphMF->SetSyncSource(pRC);
			pRC->Release();

			return true;
		}

		bool success = false;

		IEnumPins *pEnum;
		if (SUCCEEDED(pFilt->EnumPins(&pEnum))) {
			IPin *pPin;

			pEnum->Reset();

			for(;;) {
				HRESULT hr = pEnum->Next(1, &pPin, 0);
				if (hr == VFW_E_ENUM_OUT_OF_SYNC) {
					hr = pEnum->Reset();
					if (SUCCEEDED(hr))
						continue;
				}
				if (hr != S_OK)
					break;

				PIN_DIRECTION dir;

				VDVERIFY(SUCCEEDED(pPin->QueryDirection(&dir)));

				if (dir == PINDIR_OUTPUT) {
					IPin *pPin2;

					if (SUCCEEDED(pPin->ConnectedTo(&pPin2))) {
						PIN_INFO pi;

						if (SUCCEEDED(pPin2->QueryPinInfo(&pi))) {
							if (SetClockFromDownstream(pi.pFilter, pGraphMF))
								success = true;

							pi.pFilter->Release();
						}

						pPin2->Release();
					}
				}

				pPin->Release();

				if (success)
					break;
			}

			pEnum->Release();
		}

		return success;
	}

	typedef std::vector<std::pair<IMonikerPtr, VDStringW> > tDeviceVector;

	void Enumerate(tDeviceVector& devlist, REFCLSID devclsid) {
		ICreateDevEnum *pCreateDevEnum;
		IEnumMoniker *pEm = NULL;

		if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pCreateDevEnum))) {
			pCreateDevEnum->CreateClassEnumerator(devclsid, &pEm, 0);
			pCreateDevEnum->Release();
		}

		if (pEm) {
			IMoniker *pM;
			ULONG cFetched;
			vdvector<VDStringW> namesToCheck;

			while(S_OK == pEm->Next(1, &pM, &cFetched) && cFetched==1) {
				IPropertyBag *pPropBag;

				if (SUCCEEDED(pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag))) {
					VARIANT varName;

					varName.vt = VT_BSTR;
					varName.bstrVal = NULL;

					if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, 0))) {
						VDStringW name(varName.bstrVal);

						// Check if we have a name conflict.
						VDStringW nameTemplate;
						int collisionCounter = 1;

						for(;;) {
							bool collisionDetected = false;

							for(vdvector<VDStringW>::const_iterator it(namesToCheck.begin()), itEnd(namesToCheck.end()); it != itEnd; ++it) {
								const VDStringW& ent = *it;

								if (!vdwcsicmp(ent.c_str(), name.c_str())) {
									if (collisionCounter == 1)
										nameTemplate = name;

									++collisionCounter;

									name.sprintf(L"%ls #%d", nameTemplate.c_str(), collisionCounter);
									collisionDetected = true;
									break;
								}
							}

							if (!collisionDetected)
								break;
						}

						namesToCheck.push_back(name);

						if (devclsid == CLSID_VideoInputDeviceCategory) {
							bool isVFWDriver = false;
							LPOLESTR displayName;
							if (SUCCEEDED(pM->GetDisplayName(NULL, NULL, &displayName))) {
								// Detect a VFW driver by the compression manager tag.
								if (!wcsncmp(displayName, L"@device:cm:", 11))
									isVFWDriver = true;
								CoTaskMemFree(displayName);
							}
							name += (isVFWDriver ? L" (VFW>DirectShow)" : L" (DirectShow)");
						}

						devlist.push_back(tDeviceVector::value_type(pM, name));

						SysFreeString(varName.bstrVal);
					}

					pPropBag->Release();
				}

				pM->Release();
			}

			pEm->Release();
		}
	}

	class VDWaveFormatAsDShowMediaType : public AM_MEDIA_TYPE {
	public:
		VDWaveFormatAsDShowMediaType(const WAVEFORMATEX *pwfex, UINT size) {
			majortype		= MEDIATYPE_Audio;
			subtype.Data1	= pwfex->wFormatTag;
			subtype.Data2	= 0;
			subtype.Data3	= 0x0010;
			subtype.Data4[0] = 0x80;
			subtype.Data4[1] = 0x00;
			subtype.Data4[2] = 0x00;
			subtype.Data4[3] = 0xAA;
			subtype.Data4[4] = 0x00;
			subtype.Data4[5] = 0x38;
			subtype.Data4[6] = 0x9B;
			subtype.Data4[7] = 0x71;
			bFixedSizeSamples	= TRUE;
			bTemporalCompression	= FALSE;
			lSampleSize		= pwfex->nBlockAlign;
			formattype		= FORMAT_WaveFormatEx;
			pUnk			= NULL;
			cbFormat		= size;
			pbFormat		= (BYTE *)pwfex;
		}
	};

	class VDAMMediaType : public AM_MEDIA_TYPE {
	public:
		VDAMMediaType() {
			cbFormat = 0;
			pbFormat = NULL;
		}

		VDAMMediaType(const VDAMMediaType& src) {
			*static_cast<AM_MEDIA_TYPE *>(this) = src;

			if (pbFormat) {
				pbFormat = (BYTE *)CoTaskMemAlloc(cbFormat);
				if (pbFormat)
					memcpy(pbFormat, src.pbFormat, cbFormat);
				else
					cbFormat = 0;
			} else
				cbFormat = 0;
		}

		VDAMMediaType(const AM_MEDIA_TYPE& src) {
			*static_cast<AM_MEDIA_TYPE *>(this) = src;

			if (pbFormat) {
				pbFormat = (BYTE *)CoTaskMemAlloc(cbFormat);
				if (pbFormat)
					memcpy(pbFormat, src.pbFormat, cbFormat);
				else
					cbFormat = 0;
			} else
				cbFormat = 0;
		}

		~VDAMMediaType() {
			if (pbFormat)
				CoTaskMemFree(pbFormat);
		}

		VDAMMediaType& operator=(const VDAMMediaType& src) {
			if (pbFormat)
				CoTaskMemFree(pbFormat);

			*static_cast<AM_MEDIA_TYPE *>(this) = src;

			if (pbFormat) {
				pbFormat = (BYTE *)CoTaskMemAlloc(cbFormat);
				if (pbFormat)
					memcpy(pbFormat, src.pbFormat, cbFormat);
				else
					cbFormat = 0;
			} else
				cbFormat = 0;

			return *this;
		}

		VDAMMediaType& operator=(const AM_MEDIA_TYPE& src) {
			if (pbFormat)
				CoTaskMemFree(pbFormat);

			*static_cast<AM_MEDIA_TYPE *>(this) = src;

			if (pbFormat) {
				pbFormat = (BYTE *)CoTaskMemAlloc(cbFormat);
				if (pbFormat)
					memcpy(pbFormat, src.pbFormat, cbFormat);
				else
					cbFormat = 0;
			} else
				cbFormat = 0;

			return *this;
		}

		void clear() {
			if (pbFormat) {
				CoTaskMemFree(pbFormat);
				pbFormat = NULL;
				cbFormat = 0;
			}

			majortype = GUID_NULL;
			subtype = GUID_NULL;
			formattype = GUID_NULL;
		}

		void *realloc(size_t n) {
			if (cbFormat != n) {
				if (pbFormat) {
					CoTaskMemFree(pbFormat);
					pbFormat = NULL;
					cbFormat = 0;
				}

				pbFormat = (BYTE *)CoTaskMemAlloc(n);
				if (pbFormat)
					cbFormat = n;
				else
					cbFormat = 0;
			}

			return pbFormat;
		}
	};

	const wchar_t *VDGetNameForPhysicalConnectorTypeDShow(PhysicalConnectorType type) {
		switch(type) {
		case PhysConn_Video_Tuner:				return L"Video Tuner";
		case PhysConn_Video_Composite:			return L"Video Composite";
		case PhysConn_Video_SVideo:				return L"Video SVideo";
		case PhysConn_Video_RGB:				return L"Video RGB";
		case PhysConn_Video_YRYBY:				return L"Video YRYBY";
		case PhysConn_Video_SerialDigital:		return L"Video Serial Digital";
		case PhysConn_Video_ParallelDigital:	return L"Video Parallel Digital";
		case PhysConn_Video_SCSI:				return L"Video SCSI";
		case PhysConn_Video_AUX:				return L"Video AUX";
		case PhysConn_Video_1394:				return L"Video 1394";
		case PhysConn_Video_USB:				return L"Video USB";
		case PhysConn_Video_VideoDecoder:		return L"Video Decoder";
		case PhysConn_Video_VideoEncoder:		return L"Video Encoder";
		case PhysConn_Video_SCART:				return L"Video SCART";

		case PhysConn_Audio_Tuner:				return L"Audio Tuner";
		case PhysConn_Audio_Line:				return L"Audio Line";
		case PhysConn_Audio_Mic:				return L"Audio Mic";
		case PhysConn_Audio_AESDigital:			return L"Audio AES Digital";
		case PhysConn_Audio_SPDIFDigital:		return L"Audio SPDIF Digital";
		case PhysConn_Audio_SCSI:				return L"Audio SCSI";
		case PhysConn_Audio_AUX:				return L"Audio AUX";
		case PhysConn_Audio_1394:				return L"Audio 1394";
		case PhysConn_Audio_USB:				return L"Audio USB";
		case PhysConn_Audio_AudioDecoder:		return L"Audio Decoder";

		default:								return L"(Unknown type)";
		}
	}

	bool VDIsPinConnectedDShow(IPin *pPin) {
		IPinPtr pConn;
		return SUCCEEDED(pPin->ConnectedTo(~pConn));
	}

	bool VDGetFilterConnectedToPinDShow(IPin *pPin, IBaseFilter **ppFilter) {
		IPinPtr pConn;
		if (FAILED(pPin->ConnectedTo(~pConn)))
			return false;

		PIN_INFO pi;
		if (FAILED(pConn->QueryPinInfo(&pi)))
			return false;

		*ppFilter = pi.pFilter;
		return true;
	}

	bool VDIsFilterInGraphDShow(IBaseFilter *pFilter) {
		FILTER_INFO fi;

		if (FAILED(pFilter->QueryFilterInfo(&fi)))
			return false;

		bool inGraph = fi.pGraph != NULL;

		if (fi.pGraph)
			fi.pGraph->Release();

		return inGraph;
	}

	void VDDumpFilterGraphDShow(IFilterGraph *pGraph) {
		std::list<IBaseFilterPtr> filters;

		IEnumFiltersPtr pEnumFilters;
		if (SUCCEEDED(pGraph->EnumFilters(~pEnumFilters))) {
			for(;;) {
				IBaseFilterPtr pFilter;
				HRESULT hr = pEnumFilters->Next(1, ~pFilter, NULL);
				if (hr == VFW_E_ENUM_OUT_OF_SYNC) {
					filters.clear();
					if (FAILED(pEnumFilters->Reset()))
						break;
					continue;
				}

				if (hr != S_OK)
					break;

				filters.push_back(pFilter);
			}
			pEnumFilters = NULL;
		}

		VDDEBUG("Filter graph %p:\n", &*pGraph);

		IMediaFilterPtr pGraphMF;
		if (SUCCEEDED(pGraph->QueryInterface(IID_IMediaFilter, (void **)~pGraphMF))) {
			IReferenceClockPtr pRefClock;

			if (SUCCEEDED(pGraphMF->GetSyncSource(~pRefClock))) {
				IBaseFilterPtr pRefFilter;
				if (!pRefClock)
					VDDEBUG("  Reference clock: none\n");
				else if (SUCCEEDED(pRefClock->QueryInterface(IID_IFilterGraph, (void **)~IFilterGraphPtr())))
					VDDEBUG("  Reference clock: filter graph\n");
				else if (SUCCEEDED(pRefClock->QueryInterface(IID_IBaseFilter, (void **)~pRefFilter))) {
					FILTER_INFO fi;

					if (SUCCEEDED(pRefFilter->QueryFilterInfo(&fi))) {
						if (fi.pGraph)
							fi.pGraph->Release();

						VDDEBUG("  Reference clock: filter \"%ls\"\n", fi.achName);
					}
				} else if (pRefClock)
					VDDEBUG("  Reference clock: unknown\n");
			}
		}

		while(!filters.empty()) {
			IBaseFilterPtr pFilter;

			pFilter.Swap(filters.front());
			filters.pop_front();

			FILTER_INFO fi;
			fi.achName[0] = 0;
			if (SUCCEEDED(pFilter->QueryFilterInfo(&fi))) {
				fi.pGraph->Release();
			}
			VDDEBUG("  Filter %p [%ls]:\n", &*pFilter, fi.achName);

			IEnumPinsPtr pEnumPins;
			if (SUCCEEDED(pFilter->EnumPins(~pEnumPins))) {
				for(;;) {
					IPinPtr pPin;
					HRESULT hr = pEnumPins->Next(1, ~pPin, NULL);
					if (hr != S_OK)
						break;

					PIN_INFO pi;
					VDVERIFY(SUCCEEDED(pPin->QueryPinInfo(&pi)));
					pi.pFilter->Release();
					VDDEBUG("    Pin \"%ls\" (%s): ", pi.achName, pi.dir == PINDIR_INPUT ? "input " : "output");

					IPinPtr pPinConn;
					if (SUCCEEDED(pPin->ConnectedTo(~pPinConn))) {
						PIN_INFO pi2;
						FILTER_INFO fi2;
						VDVERIFY(SUCCEEDED(pPinConn->QueryPinInfo(&pi2)));
						VDVERIFY(SUCCEEDED(pi2.pFilter->QueryFilterInfo(&fi2)));
						VDDEBUG(" connected to pin \"%ls\" of filter \"%ls\"\n", pi2.achName, fi2.achName);
						fi2.pGraph->Release();
						pi2.pFilter->Release();
					} else
						VDDEBUG(" unconnected\n");
				}
			}
		}
		VDDEBUG("\n");
	}

	bool VDSaveFilterGraphDShow(const wchar_t *filename, IFilterGraph *pGraph) {
		bool success = false;

		IStorage *pStorage = NULL;
		HRESULT hr = StgCreateDocfile(filename, STGM_CREATE | STGM_TRANSACTED | STGM_WRITE | STGM_SHARE_DENY_READ, 0, &pStorage);
		if (SUCCEEDED(hr)) {
			IPersistStream *pPersistStream = NULL;
			hr = pGraph->QueryInterface(IID_IPersistStream, (void **)&pPersistStream);
			if (SUCCEEDED(hr)) {
				IStream *pStream = NULL;
				hr = pStorage->CreateStream(L"ActiveMovieGraph", STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &pStream);
				if(SUCCEEDED(hr)) {
					hr = pPersistStream->Save(pStream, TRUE);
					success = SUCCEEDED(hr);
					pStream->Release();
				}
				pPersistStream->Release();
			}
			pStorage->Commit(STGC_DEFAULT);
			pStorage->Release();
		}
		return success;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	DirectShow sample callbacks.
//
//	We place these in separate classes because we need two of them, and
//	that means we can't place it in the device. *sigh*
//
///////////////////////////////////////////////////////////////////////////

class IVDCaptureDSCallback {
public:
	virtual bool CapTryEnterCriticalSection() = 0;
	virtual void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key) = 0;
	virtual void CapLeaveCriticalSection() = 0;
};

class VDCapDevDSCallback : public ISampleGrabberCB {
protected:

	VDAtomicInt mRefCount;
	IVDCaptureDSCallback *mpCallback;
	VDAtomicInt mBlockSamples;
	bool mIgnoreTimestamps;
	uint32 mTimeBase;

public:

	VDCapDevDSCallback(IVDCaptureDSCallback *pCB)
		: mRefCount(1)
		, mpCallback(pCB)
		, mBlockSamples(true)
		, mIgnoreTimestamps(false)
		, mTimeBase(0)
	{
	}

	void SetBlockSamples(bool block) {
		mBlockSamples = block;
	}

	void SetIgnoreTimestamps(bool enabled, uint32 timeBase) {
		mIgnoreTimestamps = enabled;
		mTimeBase = timeBase;
	}

	// IUnknown

	HRESULT __stdcall QueryInterface(REFIID iid, void **ppvObject) {
		if (iid == IID_IUnknown) {
			*ppvObject = static_cast<IUnknown *>(this);
			AddRef();
			return S_OK;
		} else if (iid == IID_ISampleGrabberCB) {
			*ppvObject = static_cast<ISampleGrabberCB *>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = NULL;
		return E_NOINTERFACE;
	}

	ULONG __stdcall AddRef() {
		return ++mRefCount;
	}

	ULONG __stdcall Release() {
		int rv = --mRefCount;

		if (!rv)
			delete this;

		return (ULONG)rv;
	}

	// ISampleGrabberCB

	HRESULT __stdcall BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) {
		return E_FAIL;
	}
};

class VDCapDevDSVideoCallback : public VDCapDevDSCallback {
public:
	VDCapDevDSVideoCallback(IVDCaptureDSCallback *pCB)
		: VDCapDevDSCallback(pCB)
		, mChannel(0)
		, mVCallback("DSVideo")
	{
	}

	void SetChannel(int chan) { mChannel = chan; }
	void SetFrameCount(int count) { mFrameCount = count; }
	int GetFrameCount() const { return mFrameCount; }

	HRESULT __stdcall SampleCB(double SampleTime, IMediaSample *pSample) {
		BYTE *pData;
		HRESULT hr;

		if (mBlockSamples)
			return S_OK;

		mVCallback.Begin(0xe0e0e0, "VC");
		if (mpCallback->CapTryEnterCriticalSection()) {
			// retrieve sample pointer
			hr = pSample->GetPointer(&pData);
			if (FAILED(hr)) {
				mpCallback->CapLeaveCriticalSection();
				mVCallback.End();
				return hr;
			}

			// retrieve times
			__int64 t1, t2;
			if (mIgnoreTimestamps) {
				t1 = (VDGetAccurateTick() - mTimeBase) * 1000;
			} else {
				hr = pSample->GetTime(&t1, &t2);
				if (FAILED(hr))
					t1 = t2 = -1;
				else
					t1 = (t1+5)/10;
			}

			mpCallback->CapProcessData(mChannel, pData, pSample->GetActualDataLength(), t1, S_OK == pSample->IsSyncPoint());
			++mFrameCount;

			mpCallback->CapLeaveCriticalSection();
		}
		mVCallback.End();

		return S_OK;
	}

protected:
	int mChannel;
	VDAtomicInt	mFrameCount;
	VDRTProfileChannel mVCallback;
};

class VDCapDevDSAudioCallback : public VDCapDevDSCallback {
public:
	VDCapDevDSAudioCallback(IVDCaptureDSCallback *pCB)
		: VDCapDevDSCallback(pCB)
		, mChannel(1)
		, mACallback("DSAudio")
	{
	}

	void SetChannel(int chan) { mChannel = chan; }

	HRESULT __stdcall SampleCB(double SampleTime, IMediaSample *pSample) {
		BYTE *pData;
		HRESULT hr;

		if (mBlockSamples)
			return S_OK;

		mACallback.Begin(0xe0e0e0, "AC");
		if (mpCallback->CapTryEnterCriticalSection()) {
			// retrieve sample pointer
			hr = pSample->GetPointer(&pData);
			if (FAILED(hr)) {
				mpCallback->CapLeaveCriticalSection();
				mACallback.End();
				return hr;
			}

			// Retrieve times. Note that if no clock is set, this will fail.
			REFERENCE_TIME t1, t2;
			hr = pSample->GetTime(&t1, &t2);
			if (FAILED(hr))
				t1 = t2 = -1;
			else
				t1 = (t1+5)/10;

			mpCallback->CapProcessData(mChannel, pData, pSample->GetActualDataLength(), t1, S_OK == pSample->IsSyncPoint());
			mpCallback->CapLeaveCriticalSection();
		}
		mACallback.End();

		return S_OK;
	}

protected:
	int mChannel;
	VDRTProfileChannel mACallback;
};

///////////////////////////////////////////////////////////////////////////
//
//	DirectShow audio mask
//
///////////////////////////////////////////////////////////////////////////

class VDAudioMaskFilter: public CTransformFilter{
public:
	int mask0;
	int mask1;

	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	VDAudioMaskFilter();
	void SetParam(VDAudioMaskParam& param);

	// CTransformFilter
	HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);
	HRESULT CheckInputType(const CMediaType *mtIn);
	HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);
	HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

private:
	CMediaType m_mt;

	BOOL CanPerformTransform(const CMediaType *pMediaType) const;
	HRESULT Copy(IMediaSample *pSource, IMediaSample *pDest) const;
};

// {49143D5E-8D36-4255-954C-C235B169810A}
DEFINE_GUID(CLSID_AudioMaskFilter, 
0x49143d5e, 0x8d36, 0x4255, 0x95, 0x4c, 0xc2, 0x35, 0xb1, 0x69, 0x81, 0xa);

VDAudioMaskFilter::VDAudioMaskFilter()
	: CTransformFilter("mask", 0, CLSID_AudioMaskFilter)
{
	mask0 = 1;
	mask1 = 2;
}

void VDAudioMaskFilter::SetParam(VDAudioMaskParam& param) {
	mask0 = 0;
	mask1 = 0;
	for(int i=0; i<16; i++) {
		int m = param.mix[i];
		int f = 1<<i;
		if (m & 1) mask0 |= f;
		if (m & 2) mask1 |= f;
	}
}

STDMETHODIMP VDAudioMaskFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	CheckPointer(ppv,E_POINTER);
	return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

HRESULT VDAudioMaskFilter::Transform(IMediaSample *pIn, IMediaSample *pOut)
{
	HRESULT hr = S_OK;
	// input
	AM_MEDIA_TYPE* pTypeIn = &m_pInput->CurrentMediaType();
	WAVEFORMATEX *pihIn = (WAVEFORMATEX *)pTypeIn->pbFormat;
	unsigned char *pSrc = 0;
	pIn->GetPointer((unsigned char **)&pSrc);

	// output
	AM_MEDIA_TYPE *pTypeOut = &m_pOutput->CurrentMediaType();
	WAVEFORMATEX *pihOut = (WAVEFORMATEX *)pTypeOut->pbFormat;
	short *pDst = 0;
	pOut->GetPointer((unsigned char **)&pDst);

	hr = Copy(pIn, pOut);
	if (hr != S_OK) return hr;
	return NOERROR;
}

HRESULT VDAudioMaskFilter::CheckInputType(const CMediaType *mtIn)
{
	// check this is a VIDEOINFOHEADER type
	if (*mtIn->FormatType() != FORMAT_WaveFormatEx) {
		return E_INVALIDARG;
	}

	// Can we transform this type
	if (CanPerformTransform(mtIn)) {
		CopyMediaType(&m_mt, mtIn);
		return NOERROR;
	}
	return E_FAIL;
}

HRESULT VDAudioMaskFilter::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	if (CanPerformTransform(mtIn)) {
		return S_OK;
	}
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT VDAudioMaskFilter::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
	if (m_pInput->IsConnected() == FALSE) {
		return E_UNEXPECTED;
	}

	ASSERT(pAlloc);
	ASSERT(pProperties);
	HRESULT hr = NOERROR;

	// get input dimensions
	CMediaType inMediaType = m_pInput->CurrentMediaType();
	WAVEFORMATEX *pwfx = (WAVEFORMATEX *)m_mt.Format();
	pProperties->cBuffers = 1;
	int size = pwfx->nAvgBytesPerSec / 2;
	pProperties->cbBuffer = size; // same as input pin
	ASSERT(pProperties->cbBuffer);

	// Ask the allocator to reserve us some sample memory, NOTE the function
	// can succeed (that is return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted

	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(pProperties,&Actual);
	if (FAILED(hr)) {
		return hr;
	}

	ASSERT( Actual.cBuffers == 1 );

	if (pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer) {
		return E_FAIL;
	}
	return NOERROR;
}

HRESULT VDAudioMaskFilter::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	// Is the input pin connected
	if (m_pInput->IsConnected() == FALSE)
		return E_UNEXPECTED;

	// This should never happen
	if (iPosition < 0)
		return E_INVALIDARG;

	// Do we have more items to offer
	if (iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;

	WAVEFORMATEX *pwfxin = (WAVEFORMATEX *)m_mt.pbFormat;

	if (0) {
		WAVEFORMATEXTENSIBLE *pwfx = (WAVEFORMATEXTENSIBLE *)pMediaType->AllocFormatBuffer(sizeof(WAVEFORMATEXTENSIBLE));
		pwfx->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		pwfx->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
		//pwfx->Format.nChannels = pwfxin->nChannels;
		pwfx->Format.nChannels = 2;
		pwfx->Format.nSamplesPerSec = pwfxin->nSamplesPerSec;
		pwfx->Format.wBitsPerSample = pwfxin->wBitsPerSample;
		pwfx->Format.nAvgBytesPerSec = pwfx->Format.nSamplesPerSec * pwfx->Format.wBitsPerSample * pwfx->Format.nChannels / 8;
		pwfx->Format.nBlockAlign = pwfx->Format.wBitsPerSample * pwfx->Format.nChannels / 8;
		pwfx->dwChannelMask = (1 << pwfx->Format.nChannels) - 1;
		pwfx->Samples.wValidBitsPerSample = pwfx->Format.wBitsPerSample;
		pwfx->SubFormat = MEDIASUBTYPE_PCM;
		pMediaType->SetFormat((BYTE*)pwfx, sizeof(WAVEFORMATEXTENSIBLE));
		pMediaType->SetSampleSize(pwfx->Format.nBlockAlign);
	} else {
		WAVEFORMATEX *pwfx = (WAVEFORMATEX *)pMediaType->AllocFormatBuffer(sizeof(WAVEFORMATEX));
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
		pwfx->cbSize = 0;
		pwfx->nChannels = 2;
		pwfx->nSamplesPerSec = pwfxin->nSamplesPerSec;
		pwfx->wBitsPerSample = pwfxin->wBitsPerSample;
		pwfx->nAvgBytesPerSec = pwfx->nSamplesPerSec * pwfx->wBitsPerSample * pwfx->nChannels / 8;
		pwfx->nBlockAlign = pwfx->wBitsPerSample * pwfx->nChannels / 8;
		pMediaType->SetFormat((BYTE*)pwfx, sizeof(WAVEFORMATEX));
		pMediaType->SetSampleSize(pwfx->nBlockAlign);
	}

	pMediaType->SetType(&MEDIATYPE_Audio);
	pMediaType->SetFormatType(&FORMAT_WaveFormatEx);
	pMediaType->SetTemporalCompression(FALSE);

	// Work out the GUID for the subtype from the header info.
	GUID SubTypeGUID = MEDIASUBTYPE_PCM;
	pMediaType->SetSubtype(&SubTypeGUID);

	return NOERROR;
}

BOOL VDAudioMaskFilter::CanPerformTransform(const CMediaType *pMediaType) const
{
	if (IsEqualGUID(*pMediaType->Type(), MEDIATYPE_Audio)) {
		GUID SubTypeGUID = MEDIASUBTYPE_PCM;
		if (IsEqualGUID(*pMediaType->Subtype(), SubTypeGUID)) {
			WAVEFORMATEX *pwfx = (WAVEFORMATEX *) pMediaType->Format();
			if (pwfx->wBitsPerSample!=16)
				return FALSE;
			return TRUE;
		}
	}
	return FALSE;
} 

HRESULT VDAudioMaskFilter::Copy(IMediaSample *pSource, IMediaSample *pDest) const
{
	WAVEFORMATEX *pwfxin = (WAVEFORMATEX *)m_mt.pbFormat;

	long size = pSource->GetActualDataLength();
	long dst_size	= pDest->GetSize();
	int sample_size = 2;
	int cn = pwfxin->nChannels;
	int n = size/(sample_size*cn);
	long out_size = n*sample_size*2;

	ASSERT(dst_size>=out_size);

	int16* src;
	int16* dst;
	pSource->GetPointer((BYTE**)&src);
	pDest->GetPointer((BYTE**)&dst);

	int mask0 = this->mask0;
	int mask1 = this->mask1;

	{for(int i=0; i<n; i++){
		int v0 = 0;
		int v1 = 0;
		{for(int c=0; c<cn; c++){
			if((1<<c) & mask0) v0 += *src;
			if((1<<c) & mask1) v1 += *src;
			src++;
		}}
		if(v0<-32768) v0=-32768;
		if(v0>32767) v0=32767;
		if(v1<-32768) v1=-32768;
		if(v1>32767) v1=32767;
		dst[0] = v0;
		dst[1] = v1;
		dst+=2;
	}}

	pDest->SetActualDataLength(out_size);

	// Copy the sample times

	REFERENCE_TIME TimeStart, TimeEnd;
	if (NOERROR == pSource->GetTime(&TimeStart, &TimeEnd)) {
		pDest->SetTime(&TimeStart, &TimeEnd);
	}

	LONGLONG MediaStart, MediaEnd;
	if (pSource->GetMediaTime(&MediaStart,&MediaEnd) == NOERROR) {
		pDest->SetMediaTime(&MediaStart,&MediaEnd);
	}

	// Copy the Sync point property

	HRESULT hr = pSource->IsSyncPoint();
	if (hr == S_OK) {
		pDest->SetSyncPoint(TRUE);
	} else if (hr == S_FALSE) {
		pDest->SetSyncPoint(FALSE);
	} else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	// Copy the preroll property

	hr = pSource->IsPreroll();
	if (hr == S_OK) {
		pDest->SetPreroll(TRUE);
	} else if (hr == S_FALSE) {
		pDest->SetPreroll(FALSE);
	} else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	// Copy the discontinuity property

	hr = pSource->IsDiscontinuity();
	if (hr == S_OK) {
		pDest->SetDiscontinuity(TRUE);
	} else if (hr == S_FALSE) {
		pDest->SetDiscontinuity(FALSE);
	} else {  // an unexpected error has occured...
		return E_UNEXPECTED;
	}

	return NOERROR;
}

///////////////////////////////////////////////////////////////////////////
//
//	capture driver: DirectShow
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureDriverDS : public IVDCaptureDriver, public IVDCaptureDriverDShow, public IVDCaptureDSCallback {
	VDCaptureDriverDS(const VDCaptureDriverDS&);
	VDCaptureDriverDS& operator=(const VDCaptureDriverDS&);
public:
	VDCaptureDriverDS(IMoniker *pVideoDevice);
	~VDCaptureDriverDS();

	void	*AsInterface(uint32 id);

	bool	Init(VDGUIHandle hParent);
	void	Shutdown();
	void	LoadVideoConfig(VDRegistryAppKey& key);
	void	SaveVideoConfig(VDRegistryAppKey& key);
	void	LoadAudioConfig(VDRegistryAppKey& key);
	void	SaveAudioConfig(VDRegistryAppKey& key);

	void	SetCallback(IVDCaptureDriverCallback *pCB);

	void	LockUpdates();
	void	UnlockUpdates();

	bool	IsHardwareDisplayAvailable();

	void	SetDisplayMode(nsVDCapture::DisplayMode m);
	nsVDCapture::DisplayMode		GetDisplayMode();

	void	SetDisplayRect(const vdrect32& r);
	vdrect32	GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFramePeriod(sint32 framePeriod100nsUnits);
	sint32	GetFramePeriod();

	uint32	GetPreviewFrameCount();

	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat);
	bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size);

	bool	SetTunerChannel(int channel);
	int		GetTunerChannel();
	bool	GetTunerChannelRange(int& minChannel, int& maxChannel);
	uint32	GetTunerFrequencyPrecision();
	uint32	GetTunerExactFrequency();
	bool	SetTunerExactFrequency(uint32 freq);
	nsVDCapture::TunerInputMode	GetTunerInputMode();
	void	SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode);

	int		GetAudioDeviceCount();
	const wchar_t *GetAudioDeviceName(int idx);
	bool	SetAudioDevice(int idx);
	int		GetAudioDeviceIndex();
	bool	IsAudioDeviceIntegrated(int idx);

	int		GetVideoSourceCount();
	const wchar_t *GetVideoSourceName(int idx);
	bool	SetVideoSource(int idx);
	int		GetVideoSourceIndex();

	int		GetAudioSourceCount();
	const wchar_t *GetAudioSourceName(int idx);
	bool	SetAudioSource(int idx);
	int		GetAudioSourceIndex();

	int		GetAudioSourceForVideoSource(int idx);

	int		GetAudioInputCount();
	const wchar_t *GetAudioInputName(int idx);
	bool	SetAudioInput(int idx);
	int		GetAudioInputIndex();

	bool	IsAudioCapturePossible();
	bool	IsAudioCaptureEnabled();
	bool	IsAudioPlaybackPossible();
	bool	IsAudioPlaybackEnabled();
	void	SetAudioCaptureEnabled(bool b);
	void	SetAudioAnalysisEnabled(bool b);
	void	SetAudioPlaybackEnabled(bool b);
	void	SetAudioMask(VDAudioMaskParam& param){ audioMask=param; }

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	IsPropertySupported(uint32 id);
	sint32	GetPropertyInt(uint32 id, bool *pAutomatic);
	void	SetPropertyInt(uint32 id, sint32 value, bool automatic);
	void	GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual);

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

public:
	bool	GetDisableClockForPreview();
	void	SetDisableClockForPreview(bool enabled);

	bool	GetForceAudioRendererClock();
	void	SetForceAudioRendererClock(bool enabled);

	bool	GetIgnoreVideoTimestamps();
	void	SetIgnoreVideoTimestamps(bool enabled);

protected:
	struct InputSource;
	typedef std::vector<InputSource>	InputSources;

	void	UpdateDisplay();
	bool	StartGraph();
	bool	StopGraph();
	bool	BuildPreviewGraph();
	bool	BuildCaptureGraph();
	bool	BuildGraph(bool bNeedCapture, bool bEnableAudio);
	void	TearDownGraph();
	void	CheckForWindow();
	void	CheckForChanges();
	bool	DisplayPropertyPages(IUnknown *ptr, HWND hwndParent, const GUID *pDisablePages, int nDisablePages);
	int		EnumerateCrossbarSources(InputSources& sources, IAMCrossbar *pCrossbar, int output);
	int		UpdateCrossbarSource(InputSources& sources, IAMCrossbar *pCrossbar, int output);
	void	DoEvents();

	static LRESULT CALLBACK StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT MessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	bool CapTryEnterCriticalSection();
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key);
	void CapLeaveCriticalSection();

	IVDCaptureDriverCallback	*mpCB;
	DisplayMode			mDisplayMode;

	typedef std::vector<std::pair<IMonikerPtr, VDStringW> > tDeviceVector;

	IMonikerPtr			mVideoDeviceMoniker;
	tDeviceVector		mAudioDevices;
	int					mAudioDeviceIndex;

	// Some essentials for the filter graph.
	IFilterGraphPtr				mpGraph;
	IGraphBuilderPtr			mpGraphBuilder;
	ICaptureGraphBuilder2Ptr	mpCapGraphBuilder2;
	IMediaControlPtr			mpGraphControl;
	IMediaEventExPtr			mpMediaEventEx;

	// Pointers to filters and pins in the graph.
	IBaseFilterPtr		mpCapFilt;
	IBaseFilterPtr		mpCapSplitFilt;			// DV Splitter, if INTERLEAVED is detected
	IBaseFilterPtr		mpCapTransformFilt;		//
	IPinPtr				mpShadowedRealCapturePin;		// the one on the cap filt
	IPinPtr				mpRealCapturePin;		// the one on the cap filt, or a transform filter if present
	IPinPtr				mpRealPreviewPin;		// the one on the cap filt
	IPinPtr				mpRealAudioPin;			// the one on the cap filt
	IPinPtr				mpCapFiltVideoPortPin;	// on cap filt
	IPinPtr				mpAudioPin;
	VDAudioMaskFilter*  mpAudioMask;
	VDAudioMaskParam	audioMask;
	IAMAnalogVideoDecoderPtr mpAnalogVideoDecoder;
	IAMCrossbarPtr		mpCrossbar;
	IAMCrossbarPtr		mpCrossbar2;
	IAMTunerPtr			mpTuner;
	IAMTVTunerPtr		mpTVTuner;
	IAMVideoProcAmpPtr	mpVideoProcAmp;
	IVideoWindowPtr		mpVideoWindow;
	typedef std::list<IVideoWindowPtr> VideoWindows;
	VideoWindows		mVideoWindows;
	IQualProp			*mpVideoQualProp;

	typedef std::list<IBaseFilterPtr> ExtraFilters;
	ExtraFilters		mExtraFilters;

	// Video formats.
	typedef std::list<VDAMMediaType> VideoFormats;
	VideoFormats mPreferredVideoFormats;

	// Audio filter.  We may not have one of these, actually.
	IBaseFilterPtr		mpAudioCapFilt;

	// Audio inputs
	struct InputSource {
		int						mCrossbarPin;
		int						mPhysicalType;
		VDStringW				mName;

		InputSource(int crossbarInputPin, int physType, const VDStringW& name) : mCrossbarPin(crossbarInputPin), mPhysicalType(physType), mName(name) {}
	};

	struct AudioInput {
		IAMAudioInputMixerPtr	mMixerEnable;
		VDStringW				mName;

		AudioInput(IAMAudioInputMixer *pMixerEnable, const VDStringW& name) : mMixerEnable(pMixerEnable), mName(name) {}
	};

	typedef std::vector<AudioInput>	AudioInputs;

	InputSources	mVideoSources;
	int				mCurrentVideoSource;
	InputSources	mAudioSources;
	int				mCurrentAudioSource;
	AudioInputs	mAudioInputs;
	int			mCurrentAudioInput;
	IAMCrossbar	*mpAudioCrossbar;		// This aliases either mpCrossbar or mpCrossbar2.
	IAMCrossbar	*mpVideoCrossbar;		// This aliases either mpCrossbar or mpCrossbar2.
	int			mAudioCrossbarOutput;
	int			mVideoCrossbarOutput;

	// These have to be nullified when we destroy parts of the graph.
	ISampleGrabberPtr	mpVideoGrabber;
	ISampleGrabberPtr	mpAudioGrabber;

	IAMVfwCaptureDialogsPtr		mpVFWDialogs;
	IAMStreamConfigPtr			mpVideoConfigCap;
	IAMStreamConfigPtr			mpVideoConfigPrv;
	IAMStreamConfigPtr			mpAudioConfig;

	// Callbacks
	VDCapDevDSVideoCallback		mVideoCallback;
	VDCapDevDSAudioCallback		mAudioCallback;
	VDSemaphore					mGraphStateLock;

	// Misc flags & state
	DWORD mdwRegisterGraph;				// Used to register filter graph in ROT for graphedit
	HWND mhwndParent;					// Parent window
	HWND mhwndEventSink;				// Our dummy window used as DS event sink
	vdrect32 mDisplayRect;

	uint32	mUpdateLocks;
	bool	mbUpdatePending;
	bool	mbStartPending;

	bool mbAudioCaptureEnabled;
	bool mbAudioAnalysisEnabled;
	bool mbAudioPlaybackEnabled;
	bool mbGraphActive;					// true if the graph is currently running
	bool mbGraphHasPreview;				// true if the graph has separate capture and preview pins
	bool mbDisplayVisible;
	bool mbForceAudioRendererClock;		// force the audio renderer to be the clock when present
	bool mbDisableClockForPreview;		// disable the clock by default
	bool mbIgnoreVideoTimestamps;

	// state tracking for reporting changes
	sint32		mTrackedFramePeriod;
	vdstructex<BITMAPINFOHEADER>	mTrackedVideoFormat;

	vdstructex<WAVEFORMATEX>	mAudioFormat;

	uint32	mCaptureStart;

	VDAtomicInt	mCaptureStopQueued;

	HANDLE	mCaptureThread;
	VDAtomicPtr<MyError>	mpCaptureError;

	static ATOM sMsgSinkClass;
};

ATOM VDCaptureDriverDS::sMsgSinkClass;

VDCaptureDriverDS::VDCaptureDriverDS(IMoniker *pVideoDevice)
	: mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mVideoDeviceMoniker(pVideoDevice)
	, mAudioDeviceIndex(0)
	, mpVideoQualProp(NULL)
	, mdwRegisterGraph(0)
	, mCurrentVideoSource(-1)
	, mCurrentAudioSource(-1)
	, mCurrentAudioInput(-1)
	, mpAudioCrossbar(NULL)
	, mpVideoCrossbar(NULL)
	, mpAudioMask(NULL)
	, mVideoCallback(this)
	, mAudioCallback(this)
	, mGraphStateLock(2)
	, mhwndParent(NULL)
	, mhwndEventSink(NULL)
	, mDisplayRect(0,0,0,0)
	, mUpdateLocks(0)
	, mbUpdatePending(false)
	, mbStartPending(false)
	, mbAudioCaptureEnabled(true)
	, mbAudioAnalysisEnabled(false)
	, mbAudioPlaybackEnabled(false)
	, mbGraphActive(false)
	, mbGraphHasPreview(false)
	, mbDisplayVisible(false)
	, mbDisableClockForPreview(false)
	, mbForceAudioRendererClock(true)
	, mTrackedFramePeriod(0)
	, mCaptureStart(0)
	, mCaptureStopQueued(0)
	, mCaptureThread(NULL)
	, mpCaptureError(NULL)
{
}

VDCaptureDriverDS::~VDCaptureDriverDS() {
	Shutdown();
}

void *VDCaptureDriverDS::AsInterface(uint32 id) {
	if (id == IVDCaptureDriverDShow::kTypeID)
		return static_cast<IVDCaptureDriverDShow *>(this);
	return NULL;
}

void VDCaptureDriverDS::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverDS::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	HRESULT hr;

	if (!sMsgSinkClass) {
		WNDCLASS wc = { 0, StaticMessageSinkWndProc, 0, sizeof(void *), g_hInst, NULL, NULL, NULL, NULL, "Riza DirectShow event sink" };

		sMsgSinkClass = RegisterClass(&wc);

		if (!sMsgSinkClass)
			return false;
	}

	// Create message sink.

	if (!(mhwndEventSink = CreateWindow((LPCTSTR)sMsgSinkClass, "", WS_POPUP, 0, 0, 0, 0, mhwndParent, NULL, g_hInst, this)))
		return false;

	// Create a filter graph manager.
	DS_VERIFY(mpGraph.CreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER), "create filter graph manager");
	DS_VERIFY(mpGraph->QueryInterface(IID_IGraphBuilder, (void **)~mpGraphBuilder), "find graph builder if");

	// Create a capture filter graph builder (we're lazy).

	DS_VERIFY(mpCapGraphBuilder2.CreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER), "create filter graph builder");

	mpCapGraphBuilder2->SetFiltergraph(mpGraphBuilder);

	AddToRot(mpGraphBuilder, &mdwRegisterGraph);

	VDDEBUG("ROT entry: %x   PID: %x\n", mdwRegisterGraph, GetCurrentProcessId());

	// Try to find the event sink interface.
	if (SUCCEEDED(mpGraphBuilder->QueryInterface(IID_IMediaEventEx,(void **)~mpMediaEventEx))) {
		mpMediaEventEx->SetNotifyWindow((OAHWND)mhwndEventSink, WM_APP, 0);
		mpMediaEventEx->CancelDefaultHandling(EC_VIDEO_SIZE_CHANGED);
	}

	// Attempt to instantiate the capture filter.

	DS_VERIFY(mVideoDeviceMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)~mpCapFilt), "create capture filter");
	DS_VERIFY(mpGraphBuilder->AddFilter(mpCapFilt, L"Capture device"), "add capture filter");

	// Find the capture pin first.  If we don't have one of these, we
	// might as well give up.
	bool interleaved = true;

	hr = mpCapGraphBuilder2->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, TRUE, 0, ~mpRealCapturePin);
	if (SUCCEEDED(hr)) {
		// We got one, so attach a DV splitter now.
		DS_VERIFY(mpCapSplitFilt.CreateInstance(CLSID_DVSplitter, NULL, CLSCTX_INPROC_SERVER), "create DV splitter");
		DS_VERIFY(mpGraphBuilder->AddFilter(mpCapSplitFilt, L"DV splitter"), "add DV splitter");

		IPinPtr pCapSplitIn;
		IPinPtr pCapSplitOutVideo;
		IPinPtr pCapSplitOutAudio;
		DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapSplitFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pCapSplitIn), "find DV splitter input");
		DS_VERIFY(mpGraphBuilder->Connect(mpRealCapturePin, pCapSplitIn), "connect capture -> dv splitter");

		DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapSplitFilt, PINDIR_OUTPUT, NULL, &MEDIATYPE_Video, TRUE, 0, ~pCapSplitOutVideo), "find DV splitter video output");
		DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapSplitFilt, PINDIR_OUTPUT, NULL, &MEDIATYPE_Audio, TRUE, 0, ~pCapSplitOutAudio), "find DV splitter audio output");

		// Treat the outputs of the DV splitter as the "real" audio and video capture pins.
		mpRealCapturePin = pCapSplitOutVideo;
		mpRealAudioPin = pCapSplitOutAudio;
	} else {
		hr = mpCapGraphBuilder2->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, ~mpRealCapturePin);

		DS_VERIFY(hr, "find capture pin");

		interleaved = false;
	}

	// If we have the transform filter, insert it inline now and make its output pin
	// the "capture" pin.
	VDRegistryAppKey key("Hidden features");
	VDStringW name;

	if (key.getString("CapDShow: Transform filter name", name)) {
		IClassFactoryPtr pCF;
		hr = CoGetObject(name.c_str(), NULL, IID_IBaseFilter, (void **)~mpCapTransformFilt);
		if (SUCCEEDED(hr)) {
			IPinPtr pPinTIn, pPinTOut;
			DS_VERIFY(mpGraphBuilder->AddFilter(mpCapTransformFilt, L"Video transform"), "add transform filter");
			DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapTransformFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinTIn), "find transform filter input");
			DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapTransformFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinTOut), "find transform filter output");
			DS_VERIFY(mpGraphBuilder->Connect(mpRealCapturePin, pPinTIn), "connect capture -> transform");
			mpShadowedRealCapturePin = mpRealCapturePin;
			mpRealCapturePin = pPinTOut;
		}
	}

	// Look for a preview pin.  It's actually likely that we won't get
	// one if someone has a USB webcam, so we have to be prepared for
	// it.
	hr = mpCapGraphBuilder2->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, TRUE, 0, ~mpRealPreviewPin);
	mbGraphHasPreview = SUCCEEDED(hr);

	// Enumerate video formats from the capture pin.
	IEnumMediaTypesPtr pEnum;
	if (SUCCEEDED(mpRealCapturePin->EnumMediaTypes(~pEnum))) {
		AM_MEDIA_TYPE *pMediaType;
		for(;;) {
			HRESULT hr = pEnum->Next(1, &pMediaType, NULL);

			if (hr == VFW_E_ENUM_OUT_OF_SYNC) {
				mPreferredVideoFormats.clear();
				if (FAILED(pEnum->Reset()))
					break;
				continue;
			}

			if (hr != S_OK)
				break;

			if (pMediaType->majortype == MEDIATYPE_Video && pMediaType->formattype == FORMAT_VideoInfo
				&& pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) {

				mPreferredVideoFormats.push_back(*pMediaType);
			}

			RizaDeleteMediaType(pMediaType);
		}
	}

	// Look for an audio pin. We may need to attach a null renderer to
	// it for smooth previews.
	if (!interleaved)
		hr = mpCapGraphBuilder2->FindPin(mpCapFilt, PINDIR_OUTPUT, NULL, &MEDIATYPE_Audio, TRUE, 0, ~mpRealAudioPin);

	// Get video format configurator

	hr = mpCapGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigCap);

	if (FAILED(hr))
		hr = mpCapGraphBuilder2->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigCap);

	DS_VERIFY(hr, "find video format config if");

	hr = mpCapGraphBuilder2->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Interleaved, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigPrv);

	if (FAILED(hr))
		hr = mpCapGraphBuilder2->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigPrv);

	// Look for a video port pin. We _HAVE_ to render this if it exists; otherwise,
	// the ATI All-in-Wonder driver can lock on a wait in kernel mode and zombie
	// our process. And no, a Null Renderer doesn't work. It's OK for this to fail.

	hr = mpCapGraphBuilder2->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_VIDEOPORT, NULL, FALSE, 0, ~mpCapFiltVideoPortPin);

	// Check for VFW capture dialogs, TV tuner, and crossbar

	mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMVfwCaptureDialogs, (void **)~mpVFWDialogs);
	mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMCrossbar, (void **)~mpCrossbar);
	if (mpCrossbar) {
		IBaseFilterPtr pXFilt;

		if (SUCCEEDED(mpCrossbar->QueryInterface(IID_IBaseFilter, (void **)~pXFilt)))
			mpCapGraphBuilder2->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pXFilt, IID_IAMCrossbar, (void **)~mpCrossbar2);
	}

	// Search for tuner interfaces. Note that IAMTVTuner inherits from IAMTuner.
	if (SUCCEEDED(mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMTVTuner, (void **)~mpTVTuner)))
		mpTuner = mpTVTuner;
	else
		mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMTuner, (void **)~mpTuner);

	mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMAnalogVideoDecoder, (void **)~mpAnalogVideoDecoder);
	mpCapGraphBuilder2->FindInterface(NULL, NULL, mpCapFilt, IID_IAMVideoProcAmp, (void **)~mpVideoProcAmp);

	// If there is at least one crossbar, examine the crossbars to see if we can spot
	// an audio input switch.
	mpAudioCrossbar = NULL;
	mpVideoCrossbar = NULL;
	for(int i=0; i<2; ++i) {
		IAMCrossbar *pCrossbar;
		if (i == 0)
			pCrossbar = mpCrossbar;
		else
			pCrossbar = mpCrossbar2;

		if (!pCrossbar)
			continue;

		long outputs, inputs;
		if (FAILED(pCrossbar->get_PinCounts(&outputs, &inputs)))
			continue;

		for(int pin=0; pin<outputs; ++pin) {
			long related;
			long phystype;

			if (FAILED(pCrossbar->get_CrossbarPinInfo(FALSE, pin, &related, &phystype)))
				continue;

			if (!mpAudioCrossbar) {
				if (phystype == PhysConn_Audio_AudioDecoder) {
					mpAudioCrossbar = pCrossbar;
					mAudioCrossbarOutput = pin;
				}
			}
			
			if (!mpVideoCrossbar) {
				if (phystype == PhysConn_Video_VideoDecoder) {
					mpVideoCrossbar = pCrossbar;
					mVideoCrossbarOutput = pin;
				}
			}
		}
	}

	mCurrentAudioSource = -1;
	mCurrentVideoSource = -1;
	mVideoSources.clear();
	mAudioSources.clear();

	// Scan crossbars for sources
	if (mpAudioCrossbar)
		mCurrentAudioSource = EnumerateCrossbarSources(mAudioSources, mpAudioCrossbar, mAudioCrossbarOutput);

	if (mpVideoCrossbar)
		mCurrentVideoSource = EnumerateCrossbarSources(mVideoSources, mpVideoCrossbar, mVideoCrossbarOutput);

	DS_VERIFY(mpGraphBuilder->QueryInterface(IID_IMediaControl, (void **)~mpGraphControl), "find graph control interface");

	// Enumerate audio drivers. If the capture filter supports audio, make it audio
	// device zero.
	if (mpRealAudioPin) {
		FILTER_INFO fi;

		if (SUCCEEDED(mpCapFilt->QueryFilterInfo(&fi))) {
			mAudioDevices.push_back(tDeviceVector::value_type(0, VDStringW(fi.achName)));

			fi.pGraph->Release();
		}
	}

	Enumerate(mAudioDevices, CLSID_AudioInputDeviceCategory);

#if 0	// Disabled Dazzle hack for now.
	Enumerate(mAudioDevices, KSCATEGORY_CAPTURE);
#endif

	// Select the first audio device if there is one.
	mAudioDeviceIndex = -1;
	if (!mAudioDevices.empty())
		SetAudioDevice(0);

	UpdateDisplay();

	return true;
}

void VDCaptureDriverDS::Shutdown() {
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	if (mpVideoWindow) {
		mpVideoWindow->put_Visible(OAFALSE);
		mpVideoWindow->put_Owner(NULL);
		mpVideoWindow = NULL;
	}

	mVideoWindows.clear();

	if (mpGraphBuilder)
		TearDownGraph();

	// FIXME:	We need to tear down the filter graph manager, but to do so we
	//			need to manually kill the smart pointers.  This is not the
	//			cleanest way to wipe a graph and there's probably something
	//			wrong with doing it this way.

	mpAudioCapFilt		= NULL;
	mpAudioGrabber		= NULL;
	mpAudioConfig		= NULL;

	mpCapFilt			= NULL;
	mpCapSplitFilt		= NULL;
	mpCapTransformFilt	= NULL;
	mpShadowedRealCapturePin	= NULL;
	mpRealCapturePin	= NULL;
	mpRealPreviewPin	= NULL;
	mpCapFiltVideoPortPin	= NULL;
	mpRealAudioPin		= NULL;
	mpAudioPin			= NULL;
	mpAnalogVideoDecoder = NULL;
	mpCrossbar			= NULL;
	mpCrossbar2			= NULL;
	mpTuner				= NULL;
	mpTVTuner			= NULL;
	mpVideoProcAmp		= NULL;
	mpVideoGrabber		= NULL;
	mpVFWDialogs		= NULL;
	mpVideoConfigCap	= NULL;
	mpVideoConfigPrv	= NULL;
	mpMediaEventEx		= NULL;
	mpGraphBuilder				= NULL;
	mpCapGraphBuilder2		= NULL;
	mpGraphControl		= NULL;

	mpAudioCrossbar		= NULL;
	mpVideoCrossbar		= NULL;

	if (mhwndEventSink) {
		DestroyWindow(mhwndEventSink);
		mhwndEventSink = NULL;
	}

	mAudioDevices.clear();
	mAudioInputs.clear();

	if (mdwRegisterGraph) {
		RemoveFromRot(mdwRegisterGraph);
		mdwRegisterGraph = 0;
	}

	if (mCaptureThread) {
		CloseHandle(mCaptureThread);
		mCaptureThread = NULL;
	}

	if (MyError *e = mpCaptureError.xchg(NULL))
		delete e;
}

void VDCaptureDriverDS::LockUpdates() {
	++mUpdateLocks;
}

void VDCaptureDriverDS::UnlockUpdates() {
	--mUpdateLocks;
	VDASSERT((int)mUpdateLocks >= 0);

	if (!mUpdateLocks) {
		if (mbUpdatePending)
			UpdateDisplay();
		else if (mbStartPending)
			StartGraph();

		mbUpdatePending = mbStartPending = false;
	}
}

bool VDCaptureDriverDS::IsHardwareDisplayAvailable() {
	return true;
}

void VDCaptureDriverDS::SetDisplayMode(nsVDCapture::DisplayMode mode) {
	if (mDisplayMode == mode)
		return;

	mDisplayMode = mode;

	if (mode == kDisplayNone) {
		TearDownGraph();
		return;
	}

	UpdateDisplay();
}

nsVDCapture::DisplayMode VDCaptureDriverDS::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverDS::SetDisplayRect(const vdrect32& r) {
	mDisplayRect = r;

	if (mpVideoWindow) {
		mpVideoWindow->put_Left(r.left);
		mpVideoWindow->put_Top(r.top);
		mpVideoWindow->put_Width(r.width());
		mpVideoWindow->put_Height(r.height());
	}
}

vdrect32 VDCaptureDriverDS::GetDisplayRectAbsolute() {
	return mDisplayRect;
}

void VDCaptureDriverDS::SetDisplayVisibility(bool vis) {
	if (mbDisplayVisible == vis)
		return;

	mbDisplayVisible = vis;

	if (mpVideoWindow)
		mpVideoWindow->put_Visible(vis ? OATRUE : OAFALSE);
}

void VDCaptureDriverDS::SetFramePeriod(sint32 framePeriod100nsUnits) {
	AM_MEDIA_TYPE *past;
	VDFraction pf;
	bool bRet = false;

	StopGraph();

	if (mpVideoConfigCap && SUCCEEDED(mpVideoConfigCap->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo) {
			VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER *)past->pbFormat;

			pvih->AvgTimePerFrame = framePeriod100nsUnits;

			bRet = SUCCEEDED(mpVideoConfigCap->SetFormat(past));
			VDASSERT(bRet);
		}

		RizaDeleteMediaType(past);
	}

	if (mpVideoConfigPrv && SUCCEEDED(mpVideoConfigPrv->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo) {
			VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER *)past->pbFormat;

			pvih->AvgTimePerFrame = framePeriod100nsUnits;

			bRet = SUCCEEDED(mpVideoConfigPrv->SetFormat(past));
			VDASSERT(bRet);
		}

		RizaDeleteMediaType(past);
	}

	CheckForChanges();

	VDDEBUG("Desired frame period = %u*100ns, actual = %u*100ns\n", framePeriod100nsUnits, GetFramePeriod());

	StartGraph();

	VDASSERT(bRet);
}

sint32 VDCaptureDriverDS::GetFramePeriod() {
	AM_MEDIA_TYPE *past;
	sint32 rate;
	bool bRet = false;

	if (SUCCEEDED(mpVideoConfigCap->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo || past->formattype == FORMAT_MPEGVideo) {
			const VIDEOINFOHEADER *pvih = (const VIDEOINFOHEADER *)past->pbFormat;

			rate = VDClampToSint32(pvih->AvgTimePerFrame);

			bRet = true;
		} else if (past->formattype == FORMAT_VideoInfo2 || past->formattype == FORMAT_MPEG2Video) {
			const VIDEOINFOHEADER2 *pvih = (const VIDEOINFOHEADER2 *)past->pbFormat;

			rate = VDClampToSint32(pvih->AvgTimePerFrame);

			bRet = true;
		} else if (past->formattype == FORMAT_DvInfo) {
			const DVINFO& dvi = *(const DVINFO *)past->pbFormat;

			if (dvi.dwDVVAuxSrc & 0x200000)
				rate = 400000;	// PAL
			else
				rate = 333667;	// NTSC

			bRet = true;
		}

		RizaDeleteMediaType(past);
	}

	if (!bRet) {
		VDASSERT(false);
		return 10000000/15;
	}

	return rate;
}

uint32 VDCaptureDriverDS::GetPreviewFrameCount() {
	int framesDrawn;

	if (mDisplayMode == kDisplayAnalyze)
		return mVideoCallback.GetFrameCount();
	else if (mpVideoQualProp && SUCCEEDED(mpVideoQualProp->get_FramesDrawn(&framesDrawn)))
		return (uint32)framesDrawn;

	return 0;
}

bool VDCaptureDriverDS::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	AM_MEDIA_TYPE *pmt = NULL;
	// If the DV splitter is in use, we need to query what its video pin outputs and
	// not the capture filter itself.
	if (mpCapSplitFilt || mpCapTransformFilt) {
		IEnumMediaTypesPtr pEnum;

		if (SUCCEEDED(mpRealCapturePin->EnumMediaTypes(~pEnum))) {
			ULONG cFetched;

			if (S_OK == pEnum->Next(1, &pmt, &cFetched)) {
				if (pmt->majortype == MEDIATYPE_Video
					&& pmt->formattype == FORMAT_VideoInfo) {
					vformat.assign(&((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader, pmt->cbFormat - offsetof(VIDEOINFOHEADER, bmiHeader));
					RizaDeleteMediaType(pmt);
					return true;
				}

				RizaDeleteMediaType(pmt);
			}
		}
	} else if (SUCCEEDED(mpVideoConfigCap->GetFormat(&pmt))) {
		if (pmt->majortype == MEDIATYPE_Video
			&& pmt->formattype == FORMAT_VideoInfo) {
			vformat.assign(&((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader, pmt->cbFormat - offsetof(VIDEOINFOHEADER, bmiHeader));
			RizaDeleteMediaType(pmt);
			return true;
		}
		RizaDeleteMediaType(pmt);
	}

	vformat.clear();
	return false;
}

bool VDCaptureDriverDS::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	vdstructex<VIDEOINFOHEADER> vhdr;

	vhdr.resize(offsetof(VIDEOINFOHEADER, bmiHeader) + size);
	memcpy(&vhdr->bmiHeader, pbih, size);

	vhdr->rcSource.left		= 0;
	vhdr->rcSource.top		= 0;
	vhdr->rcSource.right	= 0;
	vhdr->rcSource.bottom	= 0;
	vhdr->rcTarget			= vhdr->rcSource;
	vhdr->AvgTimePerFrame	= GetFramePeriod();
	vhdr->dwBitRate			= VDRoundToInt(pbih->biSizeImage * 80000000.0 / vhdr->AvgTimePerFrame);
	vhdr->dwBitErrorRate	= 0;

	AM_MEDIA_TYPE vformat;

	vformat.majortype			= MEDIATYPE_Video;
	vformat.subtype.Data1		= pbih->biCompression;
	vformat.subtype.Data2		= 0;
	vformat.subtype.Data3		= 0x0010;
	vformat.subtype.Data4[0]	= 0x80;
	vformat.subtype.Data4[1]	= 0x00;
	vformat.subtype.Data4[2]	= 0x00;
	vformat.subtype.Data4[3]	= 0xAA;
	vformat.subtype.Data4[4]	= 0x00;
	vformat.subtype.Data4[5]	= 0x38;
	vformat.subtype.Data4[6]	= 0x9B;
	vformat.subtype.Data4[7]	= 0x71;
	vformat.bFixedSizeSamples	= FALSE;
	vformat.bTemporalCompression = TRUE;
	vformat.lSampleSize			= 0;
	vformat.formattype			= FORMAT_VideoInfo;
	vformat.pUnk				= NULL;
	vformat.cbFormat			= vhdr.size();
	vformat.pbFormat			= (BYTE *)vhdr.data();

	// Check for one of the special subtypes....

	bool fixed = false;

	switch(pbih->biCompression) {
	case BI_RGB:
		if (pbih->biPlanes == 1) {
			if (pbih->biBitCount == 1) {
				vformat.subtype = MEDIASUBTYPE_RGB1;
				fixed = true;
			} else if (pbih->biBitCount == 4) {
				vformat.subtype = MEDIASUBTYPE_RGB4;
				fixed = true;
			} else if (pbih->biBitCount == 8) {
				vformat.subtype = MEDIASUBTYPE_RGB8;
				fixed = true;
			} else if (pbih->biBitCount == 16) {
				vformat.subtype = MEDIASUBTYPE_RGB555;
				fixed = true;
			} else if (pbih->biBitCount == 24) {
				vformat.subtype = MEDIASUBTYPE_RGB24;
				fixed = true;
			} else if (pbih->biBitCount == 32) {
				vformat.subtype = MEDIASUBTYPE_RGB32;
				fixed = true;
			}
		}
		break;
	case BI_BITFIELDS:
		{
			const BITMAPV4HEADER& v4hdr = *(const BITMAPV4HEADER *)pbih;
			DWORD a = 0;

			if (v4hdr.bV4Size >= sizeof(BITMAPV4HEADER))
				a = v4hdr.bV4AlphaMask;

			const DWORD r = v4hdr.bV4RedMask;
			const DWORD g = v4hdr.bV4GreenMask;
			const DWORD b = v4hdr.bV4BlueMask;
			const int bits = v4hdr.bV4BitCount;

			switch(bits) {
			case 16:
				if (r == 0x7c00 && g == 0x03e0 && b == 0x001f) {
					if (!a) {
						vformat.subtype = MEDIASUBTYPE_RGB555;
						fixed = true;
					} else if (a == 0x8000) {
						vformat.subtype = MEDIASUBTYPE_ARGB1555;
						fixed = true;
					}
				} else if (r == 0xf800 && g == 0x07e0 && b == 0x001f) {
					if (!a)	{
						vformat.subtype = MEDIASUBTYPE_RGB565;
						fixed = true;
					}
				}
				break;
			case 24:
				if (!a && r == 0xff0000 && g == 0xff00 && b == 0xff) {
					vformat.subtype = MEDIASUBTYPE_RGB24;
					fixed = true;
				}
				break;
			case 32:
				if (r == 0xff0000 && g == 0xff00 && b == 0xff) {
					if (!a) {
						vformat.subtype = MEDIASUBTYPE_RGB32;
						fixed = true;
					} else if (a == 0xff000000) {
						vformat.subtype = MEDIASUBTYPE_ARGB32;
						fixed = true;
					}
				}
				break;
			}
		}
		break;
	case MAKEFOURCC('A', 'Y', 'U', 'V'):
	case MAKEFOURCC('U', 'Y', 'V', 'Y'):
	case MAKEFOURCC('Y', '4', '1', '1'):
	case MAKEFOURCC('Y', '4', '1', 'P'):
	case MAKEFOURCC('Y', '2', '1', '1'):
	case MAKEFOURCC('Y', 'U', 'Y', '2'):
	case MAKEFOURCC('Y', 'V', 'Y', 'U'):
	case MAKEFOURCC('Y', 'U', 'Y', 'V'):
	case MAKEFOURCC('I', 'F', '0', '9'):
	case MAKEFOURCC('I', 'Y', 'U', 'V'):
	case MAKEFOURCC('Y', 'V', '1', '6'):
	case MAKEFOURCC('Y', 'V', '1', '2'):
	case MAKEFOURCC('Y', 'V', 'U', '9'):
	case MAKEFOURCC('v', '2', '1', '0'):
	case MAKEFOURCC('P', '2', '1', '0'):
	case MAKEFOURCC('P', '2', '1', '6'):
	case MAKEFOURCC('P', '0', '1', '0'):
	case MAKEFOURCC('P', '0', '1', '6'):
	case MAKEFOURCC('v', '4', '1', '0'):
	case MAKEFOURCC('Y', '4', '1', '0'):
	case MAKEFOURCC('Y', '4', '1', '6'):
	case MAKEFOURCC('r', '2', '1', '0'):
	case MAKEFOURCC('R', '1', '0', 'k'):
		// The FOURCC encoding is already correct for these formats, but
		// we also happen to know that these are uncompressed formats.
		fixed = true;
		break;
	}

	if (fixed) {
		vformat.bFixedSizeSamples		= TRUE;
		vformat.bTemporalCompression	= FALSE;
		vformat.lSampleSize				= pbih->biSizeImage;
	} else {
		// We don't recognize the format, so iterate through the list of
		// preferred formats and see if one of the existing formats is
		// similar to what we have here. If so, copy the fields from
		// that format.

		VideoFormats::const_iterator it(mPreferredVideoFormats.begin()), itEnd(mPreferredVideoFormats.end());

		for(; it!=itEnd; ++it) {
			const AM_MEDIA_TYPE& amtype = *it;
			const VIDEOINFOHEADER& vhdr2 = *(const VIDEOINFOHEADER *)amtype.pbFormat;

			if (vhdr2.bmiHeader.biCompression == pbih->biCompression) {
				vformat.bFixedSizeSamples		= amtype.bFixedSizeSamples;
				vformat.bTemporalCompression	= amtype.bTemporalCompression;
				vformat.lSampleSize				= amtype.lSampleSize;

				// tweak the frame rate to be the same if they're within the margin
				// of error (since we use 1us and not 100ns units)
				__int64 deltaTime = vhdr->AvgTimePerFrame - vhdr2.AvgTimePerFrame;
				if (-20 < deltaTime && deltaTime < 20)
					vhdr->AvgTimePerFrame = vhdr2.AvgTimePerFrame;

				break;
			}
		}
	}

	// tear down graph
	TearDownGraph();

	// check if format is compatible
	bool success = false;
	HRESULT hr = mpVideoConfigCap->SetFormat(&vformat);
	if (SUCCEEDED(hr)) {
		success = true;
	} else {
		VDDEBUG("Riza/CapDShow: Unable to set video format: [%s]\n", GetDXErrorName(hr));
	}

	// rebuild display
	UpdateDisplay();

	return success;
}

bool VDCaptureDriverDS::SetTunerChannel(int channel) {
	return mpTuner && SUCCEEDED(mpTuner->put_Channel(channel, AMTUNER_SUBCHAN_DEFAULT, AMTUNER_SUBCHAN_DEFAULT));
}

int VDCaptureDriverDS::GetTunerChannel() {
	long ch, videoSub, audioSub;
	if (mpTuner && SUCCEEDED(mpTuner->get_Channel(&ch, &videoSub, &audioSub))) {
		return ch;
	}

	return -1;
}

bool VDCaptureDriverDS::GetTunerChannelRange(int& minChannel, int& maxChannel) {
	long mn, mx;

	if (mpTuner && SUCCEEDED(mpTuner->ChannelMinMax(&mn, &mx))) {
		minChannel = mn;
		maxChannel = mx;
		return true;
	}

	return false;
}

namespace
{
	struct TunerInfo {
		bool Init(IUnknown *pTuner) {
			if (!pTuner)
				return false;

			if (FAILED(pTuner->QueryInterface(IID_IKsPropertySet, (void **)~mpPropSet)))
				return false;

			DWORD dwSupport;
			if (FAILED(mpPropSet->QuerySupported(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS, &dwSupport)))
				return false;

			if (!(dwSupport & KSPROPERTY_SUPPORT_GET))
				return false;

			DWORD actual;

			// uselessness of IKsPropertySet::Get() docs is astounding... InstanceData is supposed
			// to be the portion of the struct after sizeof(KSPROPERTY).
			memset(&mTunerModeCaps, 0, sizeof mTunerModeCaps);
			mTunerModeCaps.Mode = KSPROPERTY_TUNER_MODE_TV;
			if (FAILED(mpPropSet->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_MODE_CAPS, (KSPROPERTY *)&mTunerModeCaps + 1, sizeof mTunerModeCaps - sizeof(KSPROPERTY), &mTunerModeCaps, sizeof mTunerModeCaps, &actual)))
				return false;

			return true;
		}

		IKsPropertySetPtr mpPropSet;
		KSPROPERTY_TUNER_MODE_CAPS_S mTunerModeCaps;
	};
}

uint32 VDCaptureDriverDS::GetTunerFrequencyPrecision() {
	TunerInfo info;

	if (!info.Init(mpTuner))
		return 0;

	return info.mTunerModeCaps.TuningGranularity;
}

uint32 VDCaptureDriverDS::GetTunerExactFrequency() {
	TunerInfo info;

	if (!info.Init(mpTuner))
		return 0;

	KSPROPERTY_TUNER_FREQUENCY_S freq = {0};
	DWORD actual;

	if (FAILED(info.mpPropSet->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_FREQUENCY, (KSPROPERTY *)&freq + 1, sizeof freq - sizeof(KSPROPERTY), &freq, sizeof freq, &actual)))
		return 0;

	return freq.Frequency;
}

bool VDCaptureDriverDS::SetTunerExactFrequency(uint32 freq) {
	TunerInfo info;

	if (!info.Init(mpTuner))
		return 0;

	if (freq < info.mTunerModeCaps.MinFrequency || freq > info.mTunerModeCaps.MaxFrequency)
		return 0;

	KSPROPERTY_TUNER_FREQUENCY_S freqData = {0};
	DWORD actual;

	if (FAILED(info.mpPropSet->Get(PROPSETID_TUNER, KSPROPERTY_TUNER_FREQUENCY, (KSPROPERTY *)&freq + 1, sizeof freqData - sizeof(KSPROPERTY), &freqData, sizeof freqData, &actual)))
		return 0;

	freqData.Frequency = freq;
	freqData.TuningFlags = KS_TUNER_TUNING_EXACT;

	HRESULT hr = info.mpPropSet->Set(PROPSETID_TUNER, KSPROPERTY_TUNER_FREQUENCY, (KSPROPERTY *)&freqData + 1, sizeof freqData - sizeof(KSPROPERTY), &freqData, sizeof freqData);

	return SUCCEEDED(hr);
}

nsVDCapture::TunerInputMode	VDCaptureDriverDS::GetTunerInputMode() {
	if (!mpTVTuner)
		return kTunerInputUnknown;

	long index;
	HRESULT hr = mpTVTuner->get_ConnectInput(&index);
	if (FAILED(hr))
		return kTunerInputUnknown;

	TunerInputType type;
	hr = mpTVTuner->get_InputType(index, &type);
	if (FAILED(hr))
		return kTunerInputUnknown;

	switch(type) {
		case TunerInputCable:
			return kTunerInputCable;
		case TunerInputAntenna:
			return kTunerInputAntenna;
		default:
			return kTunerInputUnknown;
	}
}

void VDCaptureDriverDS::SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode) {
	if (!mpTVTuner)
		return;

	long index;
	HRESULT hr = mpTVTuner->get_ConnectInput(&index);
	if (FAILED(hr))
		return;

	TunerInputType type;

	switch(tunerMode) {
		case kTunerInputAntenna:
		default:
			type = TunerInputAntenna;
			break;

		case kTunerInputCable:
			type = TunerInputCable;
			break;
	}

	mpTVTuner->put_InputType(index, type);
}

int VDCaptureDriverDS::GetAudioDeviceCount() {
	return mAudioDevices.size();
}

const wchar_t *VDCaptureDriverDS::GetAudioDeviceName(int idx) {
	if ((unsigned)idx >= mAudioDevices.size())
		return NULL;

	return mAudioDevices[idx].second.c_str();
}

bool VDCaptureDriverDS::SetAudioDevice(int idx) {
	if (idx == mAudioDeviceIndex)
		return true;

	if ((unsigned)idx >= mAudioDevices.size())
		idx = -1;

	StopGraph();

	// Kill an existing device.
	if (mpAudioCapFilt) {
		DestroySubgraph(mpGraphBuilder, mpAudioCapFilt, NULL, NULL);
		VDVERIFY(SUCCEEDED(mpGraphBuilder->RemoveFilter(mpAudioCapFilt)));
		mpAudioCapFilt = NULL;
	}

	mAudioDeviceIndex = -1;
	mCurrentAudioInput = -1;
	mAudioInputs.clear();

	// Attempt to connect to existing device

	bool success = false;

	if (idx < 0) {
		success = true;
	} else {
		const IMonikerPtr& pAudioDeviceMoniker = mAudioDevices[idx].first;
		HRESULT hr = S_OK;

		if (!pAudioDeviceMoniker) {
			VDASSERT(mpRealAudioPin);
			mpAudioPin = mpRealAudioPin;
		} else {
			hr = pAudioDeviceMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)~mpAudioCapFilt);
			if (FAILED(hr)) {
				VDDEBUG("Failed to create audio filter: %s\n", GetDXErrorName(hr));
				goto fail;
			}

			// Do NOT add the audio capture filter into the graph at this
			// point. See BuildGraph() for an explanation of why.

			if (SUCCEEDED(hr)) {
				hr = mpCapGraphBuilder2->FindPin(mpAudioCapFilt, PINDIR_OUTPUT, NULL, &MEDIATYPE_Audio, TRUE, 0, ~mpAudioPin);

				if (FAILED(hr))
					VDDEBUG("Couldn't find audio output pin: %s\n", GetDXErrorName(hr));
			}

			// scan the inputs of the audio capture filter and look for IAMAudioInputMixer
			// interfaces

			IEnumPinsPtr pEnumPins;
			if (SUCCEEDED(mpAudioCapFilt->EnumPins(~pEnumPins))) {
				IPinPtr pPin;

				mCurrentAudioInput = -1;

				for(;;) {
					ULONG actual;
					HRESULT hr = pEnumPins->Next(1, ~pPin, &actual);

					if (hr == VFW_E_ENUM_OUT_OF_SYNC) {
						mAudioInputs.clear();
						mCurrentAudioInput = -1;
						if (FAILED(pEnumPins->Reset()))
							break;
						continue;
					}

					if (hr != S_OK)
						break;

					PIN_DIRECTION dir;
					if (SUCCEEDED(pPin->QueryDirection(&dir)) && dir == PINDIR_INPUT) {
						IAMAudioInputMixerPtr pInputMixer;

						if (SUCCEEDED(pPin->QueryInterface(IID_IAMAudioInputMixer, (void **)~pInputMixer))) {
							// attempt to get pin name
							PIN_INFO pinInfo;
							if (SUCCEEDED(pPin->QueryPinInfo(&pinInfo))) {
								mAudioInputs.push_back(AudioInput(pInputMixer, VDStringW(pinInfo.achName)));

								// if we haven't already found an enabled pin, check if this one
								// is enabled
								if (mCurrentAudioInput < 0) {
									BOOL enabled;
									if (SUCCEEDED(pInputMixer->get_Enable(&enabled)) && enabled)
										mCurrentAudioInput = mAudioInputs.size() - 1;
								}

								pinInfo.pFilter->Release();
							}
						}
					}
				}
			}
		}

		mAudioDeviceIndex = idx;

		// Reset audio format to first available format.
		if (mpAudioPin) {
			std::list<vdstructex<WAVEFORMATEX> > aformats;

			GetAvailableAudioFormats(aformats);

			mAudioFormat.clear();
			if (!aformats.empty())
				mAudioFormat = aformats.front();
		}
	}

fail:
	UpdateDisplay();

	return success;
}

int VDCaptureDriverDS::GetAudioDeviceIndex() {
	return mAudioDeviceIndex;
}

bool VDCaptureDriverDS::IsAudioDeviceIntegrated(int idx) {
	return (unsigned)idx < mAudioDevices.size() && !mAudioDevices[idx].first;
}

int VDCaptureDriverDS::GetVideoSourceCount() {
	return mVideoSources.size();
}

const wchar_t *VDCaptureDriverDS::GetVideoSourceName(int idx) {
	return (unsigned)idx < mVideoSources.size() ? mVideoSources[idx].mName.c_str() : NULL;
}

bool VDCaptureDriverDS::SetVideoSource(int idx) {
	if (!mpVideoCrossbar)
		return false;

	const unsigned nSources = mVideoSources.size();
	if ((unsigned)idx >= nSources && idx != -1)
		return false;

	if (idx == mCurrentVideoSource)
		return true;

	const int inputPin = idx<0 ? -1 : mVideoSources[idx].mCrossbarPin;

	VDASSERT(inputPin >= -1);
	if (FAILED(mpVideoCrossbar->Route(mVideoCrossbarOutput, inputPin)))
		return false;

	mCurrentVideoSource = idx;

	return true;
}

int VDCaptureDriverDS::GetVideoSourceIndex() {
	return mCurrentVideoSource;
}

int VDCaptureDriverDS::GetAudioSourceCount() {
	return mAudioSources.size();
}

const wchar_t *VDCaptureDriverDS::GetAudioSourceName(int idx) {
	return (unsigned)idx < mAudioSources.size() ? mAudioSources[idx].mName.c_str() : NULL;
}

bool VDCaptureDriverDS::SetAudioSource(int idx) {
	if (!mpAudioCrossbar)
		return false;

	const unsigned nSources = mAudioSources.size();
	if ((unsigned)idx >= nSources && idx != -1)
		return false;

	if (idx == mCurrentAudioSource)
		return true;

	const int inputPin = idx<0 ? -1 : mAudioSources[idx].mCrossbarPin;

	VDASSERT(inputPin >= -1);
	if (FAILED(mpAudioCrossbar->Route(mAudioCrossbarOutput, inputPin)))
		return false;

	mCurrentAudioSource = idx;

	return true;
}

int VDCaptureDriverDS::GetAudioSourceIndex() {
	return mCurrentAudioSource;
}

int VDCaptureDriverDS::GetAudioSourceForVideoSource(int idx) {
	if (idx == -1)
		return -1;

	if ((unsigned)idx >= mVideoSources.size())
		return -2;

	int audioPhysType = -1;

	switch(mVideoSources[idx].mPhysicalType) {
	case PhysConn_Video_Tuner:		audioPhysType = PhysConn_Audio_Tuner; break;
	case PhysConn_Video_Composite:	audioPhysType = PhysConn_Audio_Line; break;
	case PhysConn_Video_SVideo:		audioPhysType = PhysConn_Audio_Line; break;
	case PhysConn_Video_SCSI:		audioPhysType = PhysConn_Audio_SCSI; break;
	case PhysConn_Video_AUX:		audioPhysType = PhysConn_Audio_AUX; break;
	case PhysConn_Video_1394:		audioPhysType = PhysConn_Audio_1394; break;
	case PhysConn_Video_USB:		audioPhysType = PhysConn_Audio_USB; break;
	default:						return -2;
	}

	const int audioSources = mAudioSources.size();

	for(int i=0; i<audioSources; ++i) {
		if (mAudioSources[i].mPhysicalType == audioPhysType)
			return i;
	}

	return -2;
}

int VDCaptureDriverDS::GetAudioInputCount() {
	return mpAudioPin != 0 ? mAudioInputs.size() : 0;
}

const wchar_t *VDCaptureDriverDS::GetAudioInputName(int idx) {
	return (unsigned)idx < mAudioInputs.size() ? mAudioInputs[idx].mName.c_str() : NULL;
}

bool VDCaptureDriverDS::SetAudioInput(int idx) {
	if (!mpAudioPin)
		return false;

	const size_t nInputs = mAudioInputs.size();
	if ((unsigned)idx >= nInputs && idx != -1)
		return false;

	if (idx == mCurrentAudioInput)
		return true;

	// run through all of the pins and set the enables accordingly
	for(int i=0; (unsigned)i<nInputs; ++i) {
		const AudioInput& ai = mAudioInputs[i];

		if (ai.mMixerEnable) {
			// We deliberately ignore the return value here. The reason is
			// that the put_Enable method can return E_NOTIMPL when
			// attempting to set an input to FALSE on a device that can only
			// support one enabled input at a time (Creative SBLive!). This
			// strikes me as BS since E_NOTIMPL is supposed to mean a function
			// that is not implemented, but we must handle it nevertheless.
			ai.mMixerEnable->put_Enable(i == idx);
		}
	}

	mCurrentAudioInput = idx;

	return true;
}

int VDCaptureDriverDS::GetAudioInputIndex() {
	return mpAudioPin != 0 ? mCurrentAudioInput : -1;
}

bool VDCaptureDriverDS::IsAudioCapturePossible() {
	return !!mpAudioPin;
}

bool VDCaptureDriverDS::IsAudioCaptureEnabled() {
	return mbAudioCaptureEnabled && !!mpAudioPin;
}

bool VDCaptureDriverDS::IsAudioPlaybackPossible() {
	return !!mpAudioPin;
}

bool VDCaptureDriverDS::IsAudioPlaybackEnabled() {
	return mpAudioPin && mbAudioPlaybackEnabled;
}

void VDCaptureDriverDS::SetAudioCaptureEnabled(bool b) {
	if (mbAudioCaptureEnabled == b)
		return;

	mbAudioCaptureEnabled = b;

	if (mbAudioAnalysisEnabled)
		UpdateDisplay();
}

void VDCaptureDriverDS::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioCaptureEnabled)
		UpdateDisplay();
}

void VDCaptureDriverDS::SetAudioPlaybackEnabled(bool b) {
	if (mbAudioPlaybackEnabled == b)
		return;

	mbAudioPlaybackEnabled = b;

	if (mpAudioPin)
		UpdateDisplay();
}

namespace {
	struct AudioFormatSorter {
		bool operator()(const vdstructex<WAVEFORMATEX>& af1, const vdstructex<WAVEFORMATEX>& af2) {
			const WAVEFORMATEX& f1 = *af1;
			const WAVEFORMATEX& f2 = *af2;

			if (f1.wFormatTag < f2.wFormatTag)
				return true;
			if (f1.wFormatTag > f2.wFormatTag)
				return false;

			if (f1.nSamplesPerSec > f2.nSamplesPerSec)
				return true;
			if (f1.nSamplesPerSec < f2.nSamplesPerSec)
				return false;

			if (f1.wBitsPerSample > f2.wBitsPerSample)
				return true;
			if (f1.wBitsPerSample < f2.wBitsPerSample)
				return false;

			if (f1.nChannels > f2.nChannels)
				return true;

			return false;
		}
	};
}

void VDCaptureDriverDS::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	aformats.clear();

	if (!mpAudioPin)
		return;

	IEnumMediaTypesPtr pEnum;

	if (FAILED(mpAudioPin->EnumMediaTypes(~pEnum)))
		return;

	AM_MEDIA_TYPE *pMediaType;
	while(S_OK == pEnum->Next(1, &pMediaType, NULL)) {

		if (pMediaType->majortype == MEDIATYPE_Audio && pMediaType->formattype == FORMAT_WaveFormatEx
			&& pMediaType->cbFormat >= sizeof(WAVEFORMATEX)) {
			const WAVEFORMATEX *pwfex = (const WAVEFORMATEX *)pMediaType->pbFormat;

			if (pwfex->wFormatTag == WAVE_FORMAT_PCM)
				aformats.push_back(vdstructex<WAVEFORMATEX>(pwfex, sizeof(WAVEFORMATEX)));
			else if (pwfex->cbSize + sizeof(WAVEFORMATEX) <= pMediaType->cbFormat)
				aformats.push_back(vdstructex<WAVEFORMATEX>(pwfex, pwfex->cbSize + sizeof(WAVEFORMATEX)));
		}

		RizaDeleteMediaType(pMediaType);
	}

	aformats.sort(AudioFormatSorter());

	std::list<vdstructex<WAVEFORMATEX> >::iterator it(aformats.begin()), itEnd(aformats.end());
	if (it != itEnd) {
		for(;;) {
			std::list<vdstructex<WAVEFORMATEX> >::iterator itNext(it);
			++itNext;

			if (itNext == itEnd)
				break;

			const vdstructex<WAVEFORMATEX>& f1 = *it;
			const vdstructex<WAVEFORMATEX>& f2 = *itNext;

			if (f1 == f2)
				aformats.erase(it);

			it = itNext;
		}
	}
}

bool VDCaptureDriverDS::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	if (!mpAudioPin)
		return false;

	if (!mAudioFormat.empty()) {
		aformat = mAudioFormat;
		return true;
	}

	return false;
}

bool VDCaptureDriverDS::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	if (!mpAudioPin)
		return false;

	VDWaveFormatAsDShowMediaType	amt(pwfex, size);

	if (SUCCEEDED(mpAudioPin->QueryAccept(&amt))) {
		mAudioFormat.assign(pwfex, size);

		if (mbAudioCaptureEnabled && mbAudioAnalysisEnabled)
			UpdateDisplay();

		return true;
	}

	return false;
}

bool VDCaptureDriverDS::IsDriverDialogSupported(nsVDCapture::DriverDialog dlg) {
	switch(dlg) {
	case kDialogVideoFormat:		return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Format));
	case kDialogVideoSource:		return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Source));
	case kDialogVideoDisplay:
		if (mpCapTransformFilt)
			return DisplayPropertyPages(mpCapTransformFilt, NULL, NULL, 0);
		else
			return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Display));
	case kDialogVideoCaptureFilter:	return DisplayPropertyPages(mpCapFilt, NULL, NULL, 0);
	case kDialogAudioCaptureFilter:	return DisplayPropertyPages(mpAudioCapFilt, NULL, NULL, 0);
	case kDialogVideoCapturePin:	return DisplayPropertyPages(mpRealCapturePin, NULL, NULL, 0);
	case kDialogVideoPreviewPin:	return DisplayPropertyPages(mpRealPreviewPin, NULL, NULL, 0);
	case kDialogVideoCrossbar:		return DisplayPropertyPages(mpCrossbar, NULL, NULL, 0);
	case kDialogVideoCrossbar2:		return DisplayPropertyPages(mpCrossbar2, NULL, NULL, 0);
	case kDialogTVTuner:			return DisplayPropertyPages(mpTVTuner, NULL, NULL, 0);
	}

	return false;
}

void VDCaptureDriverDS::DisplayDriverDialog(nsVDCapture::DriverDialog dlg) {
	bool bRestart = mbGraphActive;

	switch(dlg) {
	case kDialogVideoDisplay:
		if (mpCapTransformFilt) {
			DisplayPropertyPages(mpCapTransformFilt, mhwndParent, NULL, 0);
			break;
		}
		// fall through
	case kDialogVideoFormat:
	case kDialogVideoSource:

		if (mpVFWDialogs) {						
			// Disable the parent window manually to prevent reentrancy. The
			// dialogs are supposed to do this, but for some reason the VFW dialogs
			// sometimes don't.

			bool wasEnabled = false;
			
			if (mhwndParent) {
				wasEnabled = !!IsWindowEnabled(mhwndParent);

				if (wasEnabled)
					EnableWindow(mhwndParent, FALSE);
			}

			// The filter graph must be stopped for VFW dialogs.
			StopGraph();

			HRESULT hr;
			switch(dlg) {
			case kDialogVideoFormat:
				hr = mpVFWDialogs->ShowDialog(VfwCaptureDialog_Format, mhwndParent);
				break;
			case kDialogVideoSource:
				hr = mpVFWDialogs->ShowDialog(VfwCaptureDialog_Source, mhwndParent);
				break;
			case kDialogVideoDisplay:
				hr = mpVFWDialogs->ShowDialog(VfwCaptureDialog_Display, mhwndParent);
				break;
			}

			// The VFW dialogs have a habit of coming up with a NULL parent handle
			// when we're not in Hardware display mode. I don't know why; it appears
			// to be something funky in the o100vc driver, not in DirectShow. Anyway,
			// we kludge that here.
			SetForegroundWindow(mhwndParent);

			if (hr == VFW_E_CANNOT_CONNECT)
				BuildPreviewGraph();

			// Restart the filter graph.

			if (bRestart)
				StartGraph();

			// reenable main window
			if (mhwndParent && wasEnabled)
				EnableWindow(mhwndParent, TRUE);
		}

		break;

	case kDialogVideoCapturePin:
	case kDialogVideoPreviewPin:
		TearDownGraph();

		switch(dlg) {
		case kDialogVideoCapturePin:
			{
				// For some strange reason, the Adaptec GameBridge 1.00 drivers specify two property
				// pages for the video capture pin, one of which is CLSID_AudioSamplingRate. Attempting
				// to display this page causes a crash (reproduced with AMCAP). We detect this errant
				// page and specifically exclude it from being displayed.
				//
				// Interestingly, GraphEdit does NOT have this problem... but it also doesn't seem
				// to use OleCreatePropertyFrame. Either the GraphEdit guys accidentally bypassed this
				// bug with custom code, or they know something and aren't telling us.
				//
				static const GUID kInvalidVideoCapturePinGuids[]={
					{ 0x05ea6f6b, 0x3b1e, 0x4958, 0xa6, 0x8d, 0xa3, 0x7f, 0x0b, 0x6a, 0x29, 0x95 }
				};

				DisplayPropertyPages(mpRealCapturePin, mhwndParent, kInvalidVideoCapturePinGuids, sizeof(kInvalidVideoCapturePinGuids)/sizeof(kInvalidVideoCapturePinGuids[0]));
			}
			break;
		case kDialogVideoPreviewPin:
			DisplayPropertyPages(mpRealPreviewPin, mhwndParent, NULL, 0);
			break;
		}

		// Restart the filter graph.

		BuildPreviewGraph();
		if (bRestart)
			StartGraph();
		break;

	case kDialogVideoCaptureFilter:
		DisplayPropertyPages(mpCapFilt, mhwndParent, NULL, 0);
		break;
	case kDialogAudioCaptureFilter:
		DisplayPropertyPages(mpAudioCapFilt, mhwndParent, NULL, 0);
		break;
	case kDialogVideoCrossbar:
		DisplayPropertyPages(mpCrossbar, mhwndParent, NULL, 0);
		break;
	case kDialogVideoCrossbar2:
		DisplayPropertyPages(mpCrossbar2, mhwndParent, NULL, 0);
		break;
	case kDialogTVTuner:
		DisplayPropertyPages(mpTVTuner, mhwndParent, NULL, 0);
		break;
	}

	CheckForChanges();
}

namespace {
	bool VDGetDShowProcAmpPropertyFromCaptureProperty(uint32 id, VideoProcAmpProperty& dshowPropId) {
		switch(id) {
			case kPropBacklightCompensation:
				dshowPropId = VideoProcAmp_BacklightCompensation;
				return true;

			case kPropBrightness:
				dshowPropId = VideoProcAmp_Brightness;
				return true;

			case kPropColorEnable:
				dshowPropId = VideoProcAmp_ColorEnable;
				return true;

			case kPropContrast:
				dshowPropId = VideoProcAmp_Contrast;
				return true;

			case kPropGain:
				dshowPropId = VideoProcAmp_Gain;
				return true;

			case kPropGamma:
				dshowPropId = VideoProcAmp_Gamma;
				return true;

			case kPropSaturation:
				dshowPropId = VideoProcAmp_Saturation;
				return true;

			case kPropHue:
				dshowPropId = VideoProcAmp_Hue;
				return true;

			case kPropSharpness:
				dshowPropId = VideoProcAmp_Sharpness;
				return true;

			case kPropWhiteBalance:
				dshowPropId = VideoProcAmp_WhiteBalance;
				return true;

			default:
				return false;
		}
	}
}

bool VDCaptureDriverDS::IsPropertySupported(uint32 id) {
	if (!mpVideoProcAmp)
		return false;

	VideoProcAmpProperty prop;
	if (!VDGetDShowProcAmpPropertyFromCaptureProperty(id, prop))
		return false;

	long minVal, maxVal, steppingDelta, defaultVal, capsFlags;
	HRESULT hr = mpVideoProcAmp->GetRange(prop, &minVal, &maxVal, &steppingDelta, &defaultVal, &capsFlags);
	return SUCCEEDED(hr);
}

sint32 VDCaptureDriverDS::GetPropertyInt(uint32 id, bool *pAutomatic) {
	VideoProcAmpProperty prop;
	if (!mpVideoProcAmp || !VDGetDShowProcAmpPropertyFromCaptureProperty(id, prop))
		return false;

	long lValue, lFlags;
	HRESULT hr = mpVideoProcAmp->Get(prop, &lValue, &lFlags);

	if (FAILED(hr))
		lValue = lFlags = 0;

	if (pAutomatic)
		*pAutomatic = (lFlags == VideoProcAmp_Flags_Auto);

	return lValue;
}

void VDCaptureDriverDS::SetPropertyInt(uint32 id, sint32 value, bool automatic) {
	VideoProcAmpProperty prop;
	if (!mpVideoProcAmp || !VDGetDShowProcAmpPropertyFromCaptureProperty(id, prop))
		return;

	HRESULT hr = mpVideoProcAmp->Set(prop, value, automatic ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual);
	VDASSERT(SUCCEEDED(hr));
}

void VDCaptureDriverDS::GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual) {
	minVal = maxVal = 0;
	step = 1;
	defaultVal = 0;
	automatic = false;
	manual = false;

	VideoProcAmpProperty prop;
	if (!mpVideoProcAmp || !VDGetDShowProcAmpPropertyFromCaptureProperty(id, prop))
		return;

	long lMinVal, lMaxVal, lSteppingDelta, lDefaultVal, lCapsFlags;
	HRESULT hr = mpVideoProcAmp->GetRange(prop, &lMinVal, &lMaxVal, &lSteppingDelta, &lDefaultVal, &lCapsFlags);
	if (SUCCEEDED(hr)) {
		minVal = lMinVal;
		maxVal = lMaxVal;
		step = lSteppingDelta;
		defaultVal = lDefaultVal;
		automatic = (lCapsFlags & VideoProcAmp_Flags_Auto) != 0;
		manual = (lCapsFlags & VideoProcAmp_Flags_Manual) != 0;
	}
}

bool VDCaptureDriverDS::CaptureStart() {
	DS_VERBOSE_LOG("DShow: Entering CaptureStart().");

	if (VDINLINEASSERTFALSE(mCaptureThread)) {
		CloseHandle(mCaptureThread);
		mCaptureThread = NULL;
	}

	HANDLE hProcess = GetCurrentProcess();
	if (!DuplicateHandle(hProcess, GetCurrentThread(), hProcess, &mCaptureThread, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
		VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to duplicate process handle.\n"));
		return false;
	}

	// there had better not be update locks when we're trying to start a capture!
	VDASSERT(!mUpdateLocks);

	// switch to a capture graph, but don't start it.
	DS_VERBOSE_LOG("DShow: Building capture graph.");

	int audioSource = mCurrentAudioSource;
	if (BuildCaptureGraph()) {
		if (audioSource != mCurrentAudioSource)
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Audio source change was detected during capture start.\n"));

		// cancel default handling for EC_REPAINT, otherwise
		// the Video Renderer will start sending back extra requests
		mpMediaEventEx->CancelDefaultHandling(EC_REPAINT);

		// kick the sample grabbers and go!!
		mpVideoGrabber->SetCallback(&mVideoCallback, 0);

		if (mpAudioGrabber)
			mpAudioGrabber->SetCallback(&mAudioCallback, 0);

		mCaptureStart = VDGetAccurateTick();
		mCaptureStopQueued = false;

		// Kick the graph into PAUSED state.

		DS_VERBOSE_LOG("DShow: Pausing filter graph.");		
		HRESULT hr = mpGraphControl->Pause();

		if (hr == S_FALSE) {
			OAFilterState state;
			for(int i=0; i<30; ++i) {
				hr = mpGraphControl->GetState(1000, &state);
				if (hr != VFW_S_STATE_INTERMEDIATE)
					break;
			}

			if (hr == VFW_S_STATE_INTERMEDIATE)
				VDLog(kVDLogWarning, VDStringW(L"CapDShow: Filter graph took more than 30 seconds to transition state.\n"));
		}

		if (FAILED(hr)) {
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to transition filter graph to paused state.\n"));
		} else {
			if (!mpCB || mpCB->CapEvent(kEventPreroll, 0)) {
				// reset capture start time in case there was a preroll dialog
				mCaptureStart = VDGetAccurateTick();
				mVideoCallback.SetIgnoreTimestamps(mbIgnoreVideoTimestamps, mCaptureStart);

				bool success = false;
				if (mpCB) {
					try {
						mpCB->CapBegin(0);
						success = true;
					} catch(MyError& e) {
						MyError *p = new MyError;
						p->TransferFrom(e);
						delete mpCaptureError.xchg(p);
					}
				}

				if (success) {
					DS_VERBOSE_LOG("DShow: Running filter graph.");

					if (StartGraph()) {
						DS_VERBOSE_LOG("DShow: Exiting CaptureStart() (success).");
						return true;
					}

					DS_VERBOSE_LOG("DShow: Failed to run filter graph.");
				}
			}
		}

		mGraphStateLock.Wait();		// 2x because of audio/video
		mGraphStateLock.Wait();

		hr = mpGraphControl->Stop();		// Have to do this because our graph state flag doesn't track PAUSED. Okay to fail.
		if (hr == S_FALSE) {
			OAFilterState state;
			for(int i=0; i<30; ++i) {
				hr = mpGraphControl->GetState(1000, &state);
				if (hr != VFW_S_STATE_INTERMEDIATE)
					break;
			}

			if (hr == VFW_S_STATE_INTERMEDIATE)
				VDLog(kVDLogWarning, VDStringW(L"CapDShow: Filter graph took more than 30 seconds to transition state.\n"));
		}

		if (FAILED(hr))
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to stop filter graph.\n"));

		mGraphStateLock.Post();		// 2x because of audio/video
		mGraphStateLock.Post();

		if (mpCB)
			mpCB->CapEnd(mpCaptureError ? mpCaptureError : NULL);

		delete mpCaptureError.xchg(NULL);

		if (mpAudioGrabber)
			mpAudioGrabber->SetCallback(NULL, 0);

		mpVideoGrabber->SetCallback(NULL, 0);
	}

	mpMediaEventEx->RestoreDefaultHandling(EC_REPAINT);

	UpdateDisplay();

	DS_VERBOSE_LOG("DShow: Exiting CaptureStart() (failed).");
	return false;
}

void VDCaptureDriverDS::CaptureStop() {
	DS_VERBOSE_LOG("DShow: Entering CaptureStop().");

	if (mCaptureThread) {
		mCaptureStopQueued = true;

		StopGraph();

		if (mpVideoGrabber)
			mpVideoGrabber->SetCallback(NULL, 0);

		mpMediaEventEx->RestoreDefaultHandling(EC_REPAINT);

		if (mpCB) {
			mpCB->CapEnd(mpCaptureError ? mpCaptureError : NULL);
		}
		delete mpCaptureError.xchg(NULL);

		if (mCaptureThread) {
			CloseHandle(mCaptureThread);
			mCaptureThread = NULL;
		}

		mCaptureStopQueued = false;

		// Switch to a preview graph.

		int audioSource = mCurrentAudioSource;
		UpdateDisplay();
		if (audioSource != mCurrentAudioSource)
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Audio source change was detected during capture stop.\n"));
	}

	DS_VERBOSE_LOG("DShow: Exiting CaptureStop().");
}

void VDCaptureDriverDS::CaptureAbort() {
	CaptureStop();
}

bool VDCaptureDriverDS::GetDisableClockForPreview() {
	return mbDisableClockForPreview;
}

void VDCaptureDriverDS::SetDisableClockForPreview(bool enabled) {
	mbDisableClockForPreview = enabled;
}

bool VDCaptureDriverDS::GetForceAudioRendererClock() {
	return mbForceAudioRendererClock;
}

void VDCaptureDriverDS::SetForceAudioRendererClock(bool enabled) {
	mbForceAudioRendererClock = enabled;
}

bool VDCaptureDriverDS::GetIgnoreVideoTimestamps() {
	return mbIgnoreVideoTimestamps;
}

void VDCaptureDriverDS::SetIgnoreVideoTimestamps(bool enabled) {
	mbIgnoreVideoTimestamps = enabled;
}

void VDCaptureDriverDS::UpdateDisplay() {
	if (mUpdateLocks)
		mbUpdatePending = true;
	else {
		mbUpdatePending = false;

		DS_VERBOSE_LOG("DShow: Entering UpdateDisplay().");

		VDVERIFY(BuildPreviewGraph() && StartGraph());

		DS_VERBOSE_LOG("DShow: Exiting UpdateDisplay().");
	}
}

bool VDCaptureDriverDS::StopGraph() {
	if (!mbGraphActive)
		return true;

	mbGraphActive = false;
	mbStartPending = false;

	mVideoCallback.SetBlockSamples(true);
	mAudioCallback.SetBlockSamples(true);

#ifdef _DEBUG
	uint32 startTime = VDGetAccurateTick();
	VDDEBUG("Riza/CapDShow: Filter graph stopping...\n");
#endif

	mGraphStateLock.Wait();		// 2x because of audio/video
	mGraphStateLock.Wait();

	HRESULT hr = mpGraphControl->Stop();

	if (hr == S_FALSE) {
		OAFilterState state;
		for(int i=0; i<30; ++i) {
			hr = mpGraphControl->GetState(1000, &state);
			if (hr != VFW_S_STATE_INTERMEDIATE)
				break;
		}

		if (hr == VFW_S_STATE_INTERMEDIATE)
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Filter graph took more than 30 seconds to transition state.\n"));
	}

	if (FAILED(hr))
		VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to stop filter graph.\n"));

	mGraphStateLock.Post();		// 2x because of audio/video
	mGraphStateLock.Post();

#ifdef _DEBUG
	VDDEBUG("Riza/CapDShow: Filter graph stopped in %d ms.\n", VDGetAccurateTick() - startTime);
#endif

	return SUCCEEDED(hr);
}

bool VDCaptureDriverDS::StartGraph() {
	if (mbGraphActive)
		return true;

	mVideoCallback.SetFrameCount(0);

	if (mUpdateLocks) {
		mbStartPending = true;
		return true;
	}

	mbStartPending = false;

	mVideoCallback.SetBlockSamples(false);
	mAudioCallback.SetBlockSamples(false);

	mbGraphActive = false;
	HRESULT hr = mpGraphControl->Run();

	if (hr == S_FALSE) {
		OAFilterState state;
		for(int i=0; i<30; ++i) {
			hr = mpGraphControl->GetState(1000, &state);
			if (hr != VFW_S_STATE_INTERMEDIATE)
				break;
		}

		if (hr == VFW_S_STATE_INTERMEDIATE)
			VDLog(kVDLogWarning, VDStringW(L"CapDShow: Filter graph took more than 30 seconds to transition state.\n"));
	}

	if (FAILED(hr)) {
		const char *err = GetDXErrorName(hr);
		VDLog(kVDLogWarning, VDswprintf(L"CapDShow: Unable to transition filter graph to run state: hr = %08x (%hs)\n", 2, &hr, &err));

		mVideoCallback.SetBlockSamples(true);
		mAudioCallback.SetBlockSamples(true);
	} else {
		mbGraphActive = true;
	}

	return mbGraphActive;
}

//	BuildPreviewGraph()
//
//	This routine builds the preview part of a graph.  It turns out that
//	if you try to leave the Capture pin connected, VFW drivers may not
//	send anything over their Preview pin (@#*&$*($).
//
//	Usually, this simply involves creating a video renderer and slapping
//	it on the Preview pin.
//
bool VDCaptureDriverDS::BuildPreviewGraph() {
	mVideoCallback.SetChannel(-1);
	mAudioCallback.SetChannel(-2);
	if (!BuildGraph(mDisplayMode == kDisplayAnalyze, mbAudioCaptureEnabled && mbAudioAnalysisEnabled))
		return false;

	if (mpAudioGrabber && mbAudioCaptureEnabled && mbAudioAnalysisEnabled)
		mpAudioGrabber->SetCallback(&mAudioCallback, 0);

	if (mpVideoGrabber && mDisplayMode == kDisplayAnalyze)
		mpVideoGrabber->SetCallback(&mVideoCallback, 0);

	return true;
}

//	BuildCaptureGraph()
//
//	This routine builds the capture part of the graph:
//
//	* Check if the capture filter only has a capture pin.  If so, insert
//	  a smart tee filter to make a fake preview pin.
//
//	* Connect a sample grabber and then a null renderer onto the capture
//	  pin.
//
//	* Render the preview pin.
//
bool VDCaptureDriverDS::BuildCaptureGraph() {
	if (BuildGraph(true, mbAudioCaptureEnabled)) {
		mVideoCallback.SetChannel(0);
		mAudioCallback.SetChannel(1);
		return true;
	}

	return false;
}

bool VDCaptureDriverDS::BuildGraph(bool bNeedCapture, bool bEnableAudio) {
	IPinPtr pCapturePin = mpRealCapturePin;
	IPinPtr pPreviewPin = mpRealPreviewPin;
	IPinPtr	pVideoPortPin = mpCapFiltVideoPortPin;
	HRESULT hr;

	VDASSERT(!mUpdateLocks);

	// Tear down existing graph.
	TearDownGraph();

	// Check if the audio filter is in the filter graph, and if so, rip
	// it out of the graph. The reason we have to do so is that the DivX
	// Decoder filter is broken and can connect to MEDIASUBTYPE_AnalogAudio
	// inputs on the audio capture filter, completely screwing up the
	// graph building process. This was observed with the DivX 5.2.1
	// DirectShow decoder when rendering the preview pin of the Plextor
	// PX-M402U capture filter with an SBLive! Capture filter already in
	// the graph.

	if (mpAudioCapFilt) {
		if (VDIsFilterInGraphDShow(mpAudioCapFilt))
			mpGraphBuilder->RemoveFilter(mpAudioCapFilt);
	}

	///////////////////////////////////////////////////////////////////////
	//
	// VIDEO PORTION
	//
	///////////////////////////////////////////////////////////////////////
	VDASSERT(!mpAudioPin || !VDIsPinConnectedDShow(mpAudioPin));

	// Check if we need to reattach a transform filter (it can unattach
	// on a format change).
	if (mpCapTransformFilt) {
		IPinPtr pPinTest;

		if (VFW_E_NOT_CONNECTED == mpShadowedRealCapturePin->ConnectedTo(~pPinTest)) {
			// Reconnect transform filter input to capture pin
			IPinPtr pPinTIn;
			DS_VERIFY(mpCapGraphBuilder2->FindPin(mpCapTransformFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinTIn), "find transform filter input");
			DS_VERIFY(mpGraphBuilder->Connect(mpShadowedRealCapturePin, pPinTIn), "reconnect capture -> transform");
		}
	}

	if (bNeedCapture || mDisplayMode == kDisplaySoftware) {
		// Create a sample grabber.
		IBaseFilterPtr pVideoPullFilt;

		DS_VERIFY(pVideoPullFilt.CreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER), "create sample grabber");
		DS_VERIFY(mpGraphBuilder->AddFilter(pVideoPullFilt, L"Video pulldown"), "add sample grabber");

		mExtraFilters.push_back(pVideoPullFilt);

		// Set the sample grabber to continuous mode.
		//
		// NOTE: In preview mode we use this sample grabber to force the
		// stream to a particular format. We don't actually install a
		// callback.

		DS_VERIFY(pVideoPullFilt->QueryInterface(IID_ISampleGrabber, (void **)~mpVideoGrabber), "find sample grabber if");
		DS_VERIFY(mpVideoGrabber->SetOneShot(FALSE), "switch sample grabber to continuous");

		AM_MEDIA_TYPE vamtDummy;

		vamtDummy.majortype	= MEDIATYPE_Video;
		vamtDummy.subtype	= GUID_NULL;
		vamtDummy.bFixedSizeSamples = FALSE;
		vamtDummy.bTemporalCompression = FALSE;
		vamtDummy.lSampleSize	= 0;
		vamtDummy.formattype	= FORMAT_VideoInfo;
		vamtDummy.pUnk		= NULL;
		vamtDummy.cbFormat	= 0;
		vamtDummy.pbFormat	= NULL;

		DS_VERIFY(mpVideoGrabber->SetMediaType(&vamtDummy), "set video sample grabber format");

		// Attach the sink to the grabber, and the grabber to the capture pin.

		IPinPtr pPinSGIn, pPinSGOut;

		DS_VERIFY(mpCapGraphBuilder2->FindPin(pVideoPullFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinSGIn), "find sample grabber input");
		DS_VERIFY(mpCapGraphBuilder2->FindPin(pVideoPullFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinSGOut), "find sample grabber output");

		hr = mpGraphBuilder->Connect(pCapturePin, pPinSGIn);

		if (FAILED(hr)) {
			VDLogF(kVDLogWarning, L"CapDShow: Failed to build filter graph: cannot connect sample grabber filter to capture filter (error code: %08x)\n", hr);

			// see if we can tell what formats are supported by the pin
			vdrefptr<IEnumMediaTypes> pEnumMediaTypes;
			if (SUCCEEDED(pCapturePin->EnumMediaTypes(~pEnumMediaTypes))) {
				for(;;) {
					AM_MEDIA_TYPE *pMediaType = NULL;
					if (S_OK != pEnumMediaTypes->Next(1, &pMediaType, NULL))
						break;

					VDLogF(kVDLogInfo, L"Supported media type: major {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} subtype {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} formattype {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n"
						, pMediaType->majortype.Data1
						, pMediaType->majortype.Data2
						, pMediaType->majortype.Data3
						, pMediaType->majortype.Data4[0]
						, pMediaType->majortype.Data4[1]
						, pMediaType->majortype.Data4[2]
						, pMediaType->majortype.Data4[3]
						, pMediaType->majortype.Data4[4]
						, pMediaType->majortype.Data4[5]
						, pMediaType->majortype.Data4[6]
						, pMediaType->majortype.Data4[7]
						, pMediaType->subtype.Data1
						, pMediaType->subtype.Data2
						, pMediaType->subtype.Data3
						, pMediaType->subtype.Data4[0]
						, pMediaType->subtype.Data4[1]
						, pMediaType->subtype.Data4[2]
						, pMediaType->subtype.Data4[3]
						, pMediaType->subtype.Data4[4]
						, pMediaType->subtype.Data4[5]
						, pMediaType->subtype.Data4[6]
						, pMediaType->subtype.Data4[7]
						, pMediaType->formattype.Data1
						, pMediaType->formattype.Data2
						, pMediaType->formattype.Data3
						, pMediaType->formattype.Data4[0]
						, pMediaType->formattype.Data4[1]
						, pMediaType->formattype.Data4[2]
						, pMediaType->formattype.Data4[3]
						, pMediaType->formattype.Data4[4]
						, pMediaType->formattype.Data4[5]
						, pMediaType->formattype.Data4[6]
						, pMediaType->formattype.Data4[7]
						);

					RizaDeleteMediaType(pMediaType);
				}
			}

			pEnumMediaTypes.clear();

			TearDownGraph();
			return false;
		}

		pCapturePin = pPinSGOut;
	}
	VDASSERT(!mpAudioPin || !VDIsPinConnectedDShow(mpAudioPin));

	// Render the preview part.
	//
	//
	// Software configuration:
	//
	// +----------------+~capture  +--------------+     +----------+
	// |                |--------->|sample grabber|---->| renderer |
	// | capture filter |          +--------------+     +----------+
	// |                |->
	// +----------------+preview
	//
	//
	// Hardware configuration:
	// +----------------+~capture  +--------------+
	// |                |--------->|sample grabber|---------------------->
	// | capture filter |          +--------------+     +----------+
	// |                |------------------------------>| renderer |
	// +----------------+preview                        +----------+
	//
	// Note that we use the video port pin preferentially to the preview
	// pin.

	VDASSERT(pPreviewPin != mpAudioPin);
	VDASSERT(pCapturePin != mpAudioPin);

	switch(mDisplayMode) {
	case kDisplayHardware:
		// Render the preview pin if it exists, otherwise fall through.
		if (pVideoPortPin) {
			// No need to handle this here, as we always render the VP pin below.
			break;
		} else if (pPreviewPin) {
			DS_VERIFY(mpGraphBuilder->Render(pPreviewPin), "render preview pin (hardware display)");
			pPreviewPin = NULL;
			break;
		}
	case kDisplaySoftware:
		// In software mode we force the rendering path to use the same format that
		// would be used for capture. This is already done for us above, so we
		// simply add the renderer.
		//
		// Note that while it might seem like a good idea to use a Smart Tee to rip off the
		// timestamps, this doesn't work since it causes the video to play at maximum speed,
		// causing horrible stuttering on the PX-M402U.
		//
		DS_VERIFY(mpGraphBuilder->Render(pCapturePin), "render capture pin (hardware display)");
		pCapturePin = NULL;
		break;
	}

	// Render the video port pin. We're supposed to do this in any case
	// according to the DirectShow docs for Video Port pins. We HAVE to do
	// this regardless of whether we actually want to use the overlay as
	// otherwise the ATI All-in-Wonder RADEON driver locks up and zombies
	// our process.

	if (pVideoPortPin) {
		DS_VERIFY(mpGraphBuilder->Render(pVideoPortPin), "render video port pin");
		pVideoPortPin = NULL;
	}

	// Add the audio filter back into the graph, if it exists.
	if (mpAudioCapFilt) {
		DS_VERIFY(mpGraphBuilder->AddFilter(mpAudioCapFilt, L"Audio capture"), "add audio capture filter to graph");
	}

	VDASSERT(!mpAudioPin || !VDIsPinConnectedDShow(mpAudioPin));

	// Terminate the capture pin with a null renderer if it has not
	// already been terminated.
	//
	//            +-------------+
	//   -------->|null renderer|
	//            +-------------+
	//
	if (pCapturePin) {
		IBaseFilterPtr pNullRenderer;
		IPinPtr pPinNRIn;

		DS_VERIFY(pNullRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create null renderer");
		DS_VERIFY(mpGraphBuilder->AddFilter(pNullRenderer, L"Video sink"), "add null renderer");
		mExtraFilters.push_back(pNullRenderer);
		DS_VERIFY(mpCapGraphBuilder2->FindPin(pNullRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinNRIn), "find null renderer input");
		DS_VERIFY(mpGraphBuilder->Connect(pCapturePin, pPinNRIn), "connect grabber -> sink");
		pCapturePin = NULL;
	}

	///////////////////////////////////////////////////////////////////////
	//
	// AUDIO PORTION
	//
	///////////////////////////////////////////////////////////////////////

	IPinPtr pAudioPin = mpAudioPin;

	VDASSERT(!pAudioPin || !VDIsPinConnectedDShow(pAudioPin));
	if (bNeedCapture || (bEnableAudio && mbAudioAnalysisEnabled)) {
		// We need to do the audio now.

		if (pAudioPin && bEnableAudio) {
			IPinPtr pPinSGIn, pPinSGOut;
			IBaseFilterPtr pAudioPullFilt;

			DS_VERIFY(pAudioPullFilt.CreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER), "create sample grabber");
			DS_VERIFY(mpGraphBuilder->AddFilter(pAudioPullFilt, L"Audio pulldown"), "add sample grabber");
			mExtraFilters.push_back(pAudioPullFilt);

			// Set the sample grabber to continuous mode.

			DS_VERIFY(pAudioPullFilt->QueryInterface(IID_ISampleGrabber, (void **)~mpAudioGrabber), "find sample grabber if");

			if (mAudioFormat.empty())
				return false;

			VDWaveFormatAsDShowMediaType amt(mAudioFormat.data(), mAudioFormat.size());

			DS_VERIFY(mpAudioGrabber->SetMediaType(&amt), "set media type");
			DS_VERIFY(mpAudioGrabber->SetOneShot(FALSE), "switch sample grabber to continuous");

			// Attach the grabber to the capture pin.

			DS_VERIFY(mpCapGraphBuilder2->FindPin(pAudioPullFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinSGIn), "find sample grabber input");
			DS_VERIFY(mpCapGraphBuilder2->FindPin(pAudioPullFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinSGOut), "find sample grabber output");

			// If audio analysis is enabled, try to keep the latency down to at least 1/30th of a second.
			if (mbAudioAnalysisEnabled) {
				ALLOCATOR_PROPERTIES allocProp;

				allocProp.cbAlign = -1;
				allocProp.cbBuffer = mAudioFormat->nAvgBytesPerSec/30;
				if (!allocProp.cbBuffer)
					allocProp.cbBuffer = 1;
				allocProp.cbBuffer += mAudioFormat->nBlockAlign - 1;
				allocProp.cbBuffer -= allocProp.cbBuffer % mAudioFormat->nBlockAlign;
				allocProp.cbPrefix = -1;
				allocProp.cBuffers = -1;

				IAMBufferNegotiation *pBN;

				if (SUCCEEDED(pAudioPin->QueryInterface(IID_IAMBufferNegotiation, (void **)&pBN))) {
					if (FAILED(pBN->SuggestAllocatorProperties(&allocProp))) {
						VDDEBUG("Riza/DShow: Warning: Unable to suggest buffer size of %u bytes to audio capture pin\n", (unsigned)allocProp.cbBuffer);
					}
					pBN->Release();
				}

				if (SUCCEEDED(pPinSGIn->QueryInterface(IID_IAMBufferNegotiation, (void **)&pBN))) {
					if (FAILED(pBN->SuggestAllocatorProperties(&allocProp))) {
						VDDEBUG("Riza/DShow: Warning: Unable to suggest buffer size of %u bytes to sample grabber pin\n", (unsigned)allocProp.cbBuffer);
					}
					pBN->Release();
				}
			}

			hr = mpGraphBuilder->Connect(pAudioPin, pPinSGIn);
			if (FAILED(hr)) {
				VDLogF(kVDLogWarning, L"CapDShow: Failed to build filter graph: cannot connect sample grabber filter to audio capture filter (error code: %08x)\n", hr);

				// see if we can tell what formats are supported by the pin
				vdrefptr<IEnumMediaTypes> pEnumMediaTypes;
				if (SUCCEEDED(pAudioPin->EnumMediaTypes(~pEnumMediaTypes))) {
					for(;;) {
						AM_MEDIA_TYPE *pMediaType = NULL;
						if (S_OK != pEnumMediaTypes->Next(1, &pMediaType, NULL))
							break;

						VDLogF(kVDLogInfo, L"Supported media type: major {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} subtype {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x} formattype {%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n"
							, pMediaType->majortype.Data1
							, pMediaType->majortype.Data2
							, pMediaType->majortype.Data3
							, pMediaType->majortype.Data4[0]
							, pMediaType->majortype.Data4[1]
							, pMediaType->majortype.Data4[2]
							, pMediaType->majortype.Data4[3]
							, pMediaType->majortype.Data4[4]
							, pMediaType->majortype.Data4[5]
							, pMediaType->majortype.Data4[6]
							, pMediaType->majortype.Data4[7]
							, pMediaType->subtype.Data1
							, pMediaType->subtype.Data2
							, pMediaType->subtype.Data3
							, pMediaType->subtype.Data4[0]
							, pMediaType->subtype.Data4[1]
							, pMediaType->subtype.Data4[2]
							, pMediaType->subtype.Data4[3]
							, pMediaType->subtype.Data4[4]
							, pMediaType->subtype.Data4[5]
							, pMediaType->subtype.Data4[6]
							, pMediaType->subtype.Data4[7]
							, pMediaType->formattype.Data1
							, pMediaType->formattype.Data2
							, pMediaType->formattype.Data3
							, pMediaType->formattype.Data4[0]
							, pMediaType->formattype.Data4[1]
							, pMediaType->formattype.Data4[2]
							, pMediaType->formattype.Data4[3]
							, pMediaType->formattype.Data4[4]
							, pMediaType->formattype.Data4[5]
							, pMediaType->formattype.Data4[6]
							, pMediaType->formattype.Data4[7]
							);

						RizaDeleteMediaType(pMediaType);
					}
				}

				pEnumMediaTypes.clear();

				TearDownGraph();
				return false;
			}

			pAudioPin = pPinSGOut;
			VDASSERT(!VDIsPinConnectedDShow(pAudioPin));
		}
	}

	// Terminate the audio path with a null renderer or a sound output. We do this
	// even if we are just previewing, because the Plextor PX-M402U doesn't output
	// frames reliably otherwise.
	bool bUseDefaultClock = true;

	if (pAudioPin) {
		VDASSERT(!VDIsPinConnectedDShow(pAudioPin));

		if (mbAudioPlaybackEnabled) {
			//HRESULT hrRender = mpGraphBuilder->Render(pAudioPin);
			mpAudioMask = new VDAudioMaskFilter;
			mpAudioMask->AddRef();
			mpAudioMask->SetParam(audioMask);
			IUnknown* pMask;
			mpAudioMask->QueryInterface(IID_IUnknown,(void**)&pMask);
			IPinPtr pPinMIn;
			IPinPtr pPinMOut;
			DS_VERIFY(mpGraphBuilder->AddFilter(mpAudioMask, L"Audio mask"), "add audio mask");
			DS_VERIFY(mpCapGraphBuilder2->FindPin(pMask, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinMIn), "find audio mask input");
			DS_VERIFY(mpGraphBuilder->Connect(pAudioPin, pPinMIn), "connect audio mask");
			mExtraFilters.push_back(mpAudioMask);
			DS_VERIFY(mpCapGraphBuilder2->FindPin(pMask, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinMOut), "find audio mask output");
			HRESULT hrRender = mpGraphBuilder->Render(pPinMOut);
			pMask->Release();

			// Reset the filter graph clock. We have to do this because when we
			// create a capture graph a different filter may end up being the
			// clock. For some reason the DirectSound Renderer refuses to play
			// if we try using the system clock. (SAA713x)
			if (mbForceAudioRendererClock) {
				IMediaFilterPtr pGraphMF;

				if (SUCCEEDED(mpGraphBuilder->QueryInterface(IID_IMediaFilter, (void **)~pGraphMF))) {
					IPinPtr pAudioPinConnect;
					if (SUCCEEDED(pAudioPin->ConnectedTo(~pAudioPinConnect))) {
						PIN_INFO pi;

						if (SUCCEEDED(pAudioPinConnect->QueryPinInfo(&pi))) {
							bUseDefaultClock = !SetClockFromDownstream(pi.pFilter, pGraphMF);
							pi.pFilter->Release();
						}
					}
				}
			}

			if (SUCCEEDED(hrRender))
				pAudioPin = NULL;
		}

		if (pAudioPin) {
			IPinPtr pPinRIn;
			IBaseFilterPtr pRenderer;
			VDASSERT(!VDIsPinConnectedDShow(pAudioPin));
			DS_VERIFY(pRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create audio null renderer");
			DS_VERIFY(mpGraphBuilder->AddFilter(pRenderer, L"Audio sink"), "add null renderer");
			mExtraFilters.push_back(pRenderer);
			DS_VERIFY(mpCapGraphBuilder2->FindPin(pRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinRIn), "find audio renderer input");
			DS_VERIFY(mpGraphBuilder->Connect(pAudioPin, pPinRIn), "connect audio null renderer");
		}
	}

	// If the capture filter has an audio pin that's not being used, terminate that too.
	if (mpRealAudioPin && mpRealAudioPin != mpAudioPin) {
		IPinPtr pPinRIn;
		IBaseFilterPtr pRenderer;
		DS_VERIFY(pRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create audio null renderer");
		DS_VERIFY(mpGraphBuilder->AddFilter(pRenderer, L"Audio sink"), "add null renderer");
		mExtraFilters.push_back(pRenderer);
		DS_VERIFY(mpCapGraphBuilder2->FindPin(pRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinRIn), "find audio renderer input");
		DS_VERIFY(mpGraphBuilder->Connect(mpRealAudioPin, pPinRIn), "connect audio null renderer to real audio cap pin");
	}

	// Set the graph clock
	if (mbDisableClockForPreview && !bNeedCapture) {
		IMediaFilterPtr pGraphMF;

		if (SUCCEEDED(mpGraphBuilder->QueryInterface(IID_IMediaFilter, (void **)~pGraphMF)))
			pGraphMF->SetSyncSource(NULL);
	} else if (bUseDefaultClock) {
		IMediaFilterPtr pGraphMF;

		if (SUCCEEDED(mpGraphBuilder->QueryInterface(IID_IMediaFilter, (void **)~pGraphMF))) {
			IReferenceClockPtr pGraphClock;

			if (SUCCEEDED(pGraphClock.CreateInstance(CLSID_SystemClock, NULL, CLSCTX_INPROC_SERVER)))
				pGraphMF->SetSyncSource(pGraphClock);
		}
	}

	// Dump the graph to stdout.
	VDDumpFilterGraphDShow(mpGraphBuilder);

	// Check for a window and return.
	CheckForWindow();

	// Broadcast events if values changed.
	CheckForChanges();

	return true;
}

//
//	TearDownGraph()
//
//	This rips up everything in the filter graph beyond the capture filter.
//
void VDCaptureDriverDS::TearDownGraph() {
	StopGraph();

	// drop pointers to graph components
	if (mpAudioGrabber)
		mpAudioGrabber->SetCallback(NULL, 0);

	mpVideoGrabber			= NULL;
	mpVideoWindow			= NULL;
	mVideoWindows.clear();
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	mpAudioGrabber = NULL;

	if (mpAudioMask) {
		mpAudioMask->Release();
		mpAudioMask = NULL;
	}

	// reset capture clock
	IMediaFilterPtr pGraphMF;

	if (SUCCEEDED(mpGraphBuilder->QueryInterface(IID_IMediaFilter, (void **)~pGraphMF))) {
		pGraphMF->SetSyncSource(NULL);
	}

	// destroy downstreams
	DestroySubgraph(mpGraphBuilder, mpCapFilt, mpCapSplitFilt, mpCapTransformFilt);
	DestroySubgraph(mpGraphBuilder, mpAudioCapFilt, NULL, NULL);

	// yank any loose filters
	while(!mExtraFilters.empty()) {
		IBaseFilterPtr filt = mExtraFilters.back();
		mExtraFilters.pop_back();

		FILTER_INFO finfo;
		if (SUCCEEDED(filt->QueryFilterInfo(&finfo))) {
			if (finfo.pGraph) {
				finfo.pGraph->RemoveFilter(filt);
				finfo.pGraph->Release();
			}
		}
	}

	VDASSERT(!mpRealAudioPin || !VDIsPinConnectedDShow(mpRealAudioPin));
	VDASSERT(!mpAudioPin || !VDIsPinConnectedDShow(mpAudioPin));
}

//
//	CheckForWindow()
//
//	Look for a video window in the capture graph, and make it ours if
//	there is one.
//
void VDCaptureDriverDS::CheckForWindow() {
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	// Sweep the filter graph and pick up all video windows, along with
	// the first IVideoQualProp interface we can find.
	IEnumFiltersPtr pEnumFilters;

	mVideoWindows.clear();
	if (SUCCEEDED(mpGraphBuilder->EnumFilters(~pEnumFilters))) {
		IBaseFilterPtr pFilter;

		while(S_OK == pEnumFilters->Next(1, ~pFilter, NULL)) {
			if (!mpVideoQualProp)
				pFilter->QueryInterface(IID_IQualProp, (void **)&mpVideoQualProp);	// OK for this to fail.

			IVideoWindowPtr pVW;
			if (SUCCEEDED(pFilter->QueryInterface(IID_IVideoWindow, (void **)~pVW))) {
				// hide the video window
				pVW->put_Visible(OAFALSE);
				pVW->put_AutoShow(OAFALSE);

				// add it to our list
				mVideoWindows.push_back(IVideoWindowPtr());
				mVideoWindows.back().Swap(pVW);
			}
		}
	}

	VDDEBUG("Riza/CapDShow: %u windows found.\n", mVideoWindows.size());

	// Only overlay and preview modes use DirectShow displays
	if (mDisplayMode == kDisplayNone || mDisplayMode == kDisplayAnalyze)
		return;

	// Look for the one video window we do want to show. The order is capture, then
	// preview, then video port. We'll always have at least one if display is enabled,
	// and possibly two if we hide the video port (we always have to have it).

	IBaseFilterPtr pFilter;
	bool success = false;

	// try capture pin first
	if (mpRealCapturePin) {
		success = VDGetFilterConnectedToPinDShow(mpRealCapturePin, ~pFilter);
		if (success) {
			HRESULT hr = mpCapGraphBuilder2->FindInterface(&LOOK_DOWNSTREAM_ONLY, NULL, pFilter, IID_IVideoWindow, (void **)~mpVideoWindow);
			success = SUCCEEDED(hr);
		}
	}

	// next, try preview pin
	if (!success && mpRealPreviewPin) {
		success = VDGetFilterConnectedToPinDShow(mpRealPreviewPin, ~pFilter);
		if (success) {
			HRESULT hr = mpCapGraphBuilder2->FindInterface(&LOOK_DOWNSTREAM_ONLY, NULL, pFilter, IID_IVideoWindow, (void **)~mpVideoWindow);
			success = SUCCEEDED(hr);
		}
	}

	// try video port
	if (!success && mpCapFiltVideoPortPin) {
		success = VDGetFilterConnectedToPinDShow(mpCapFiltVideoPortPin, ~pFilter);
		if (success) {
			HRESULT hr = mpCapGraphBuilder2->FindInterface(&LOOK_DOWNSTREAM_ONLY, NULL, pFilter, IID_IVideoWindow, (void **)~mpVideoWindow);
			success = SUCCEEDED(hr);
		}
	}

	// okay... use any others
	if (!success && !mVideoWindows.empty())
		mpVideoWindow = mVideoWindows.front();

	// if we have a video window, configure it
	if (mpVideoWindow) {
		long styles;

		mpVideoWindow->put_Visible(OAFALSE);
		mpVideoWindow->put_AutoShow(mbDisplayVisible ? OATRUE : OAFALSE);

		if (SUCCEEDED(mpVideoWindow->get_WindowStyle(&styles))) {
			mpVideoWindow->put_WindowStyle(styles & ~(WS_CAPTION|WS_THICKFRAME));
		}

		// Add WS_EX_NOPARENTNOTIFY to try to avoid certain kinds of deadlocks,
		// since the video window is in a different thread than its parent.

		if (SUCCEEDED(mpVideoWindow->get_WindowStyleEx(&styles))) {
			mpVideoWindow->put_WindowStyleEx(styles | WS_EX_NOPARENTNOTIFY);
		}

		SetDisplayRect(mDisplayRect);
		mpVideoWindow->put_Owner((OAHWND)mhwndParent);
	}
}

void VDCaptureDriverDS::CheckForChanges() {
	// check for crossbar changes
	if (mpAudioCrossbar) {
		int newAudioSource = UpdateCrossbarSource(mAudioSources, mpAudioCrossbar, mAudioCrossbarOutput);

		// Sigh... DirectShow crossbars seem to have the general problem that after stopping the graph,
		// get_IsRoutedTo() starts returning -1 even though the crossbar is actually still routing
		// audio. You can see this in GraphEdit if you start and stop the graph -- afterward the crossbar
		// property page shows Mute In (-1) for the audio input. As such, we ignore the return value if
		// -1 is returned.
		//
		// Seen directly on the Adaptec GameBridge; similar behavior reported for WinFast and Usenet
		// hints it happens on ATI too.

		if (mCurrentAudioSource != newAudioSource && newAudioSource != -1) {
			mCurrentAudioSource = newAudioSource;

			DS_VERBOSE_LOGF(L"DShow: Detected audio source change: %d", 1, &newAudioSource);

			if (mpCB)
				mpCB->CapEvent(kEventAudioSourceChanged, newAudioSource);
		}
	}

	if (mpVideoCrossbar) {
		int newVideoSource = UpdateCrossbarSource(mVideoSources, mpVideoCrossbar, mVideoCrossbarOutput);

		if (mCurrentVideoSource != newVideoSource) {
			mCurrentVideoSource = newVideoSource;

			if (mpCB)
				mpCB->CapEvent(kEventVideoSourceChanged, newVideoSource);
		}
	}

	// check for video frame or format changes
	if (mpCB) {
		bool keepGoing = true;

		sint32 fp = GetFramePeriod();

		if (mTrackedFramePeriod != fp) {
			mTrackedFramePeriod = fp;

			keepGoing &= mpCB->CapEvent(kEventVideoFrameRateChanged, 0);
		}

		vdstructex<BITMAPINFOHEADER> vf;
		GetVideoFormat(vf);

		if (mTrackedVideoFormat != vf) {
			mTrackedVideoFormat = vf;

			keepGoing &= mpCB->CapEvent(kEventVideoFormatChanged, 0);
		}

		if (!keepGoing && mCaptureThread)
			CaptureStop();
	}
}

bool VDCaptureDriverDS::DisplayPropertyPages(IUnknown *ptr, HWND hwndParent, const GUID *pDisablePages, int nDisablePages) {
	if (!ptr)
		return false;

	HRESULT hr;
	ISpecifyPropertyPages *pPages;
	hr = ptr->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pPages);
	if (FAILED(hr))
		return false;

	CAUUID cauuid;
	hr = pPages->GetPages(&cauuid);
	pPages->Release();

	if (FAILED(hr))
		return false;

	bool success = false;
	if (cauuid.cElems) {
		uint32 n = 0;
		for(uint32 i = 0; i < cauuid.cElems; ++i) {
			const GUID& guid = cauuid.pElems[i];
			bool copy = true;

			for(int j=0; j<nDisablePages; ++j) {
				if (guid == pDisablePages[j]) {
					copy = false;
					break;
				}
			}

			if (copy)
				cauuid.pElems[n++] = guid;
		}

		if (n) {
			if (hwndParent) {
				HRESULT hr = OleCreatePropertyFrame(hwndParent, 0, 0, NULL, 1,
					&ptr, n, cauuid.pElems, 0, 0, NULL);

				success = SUCCEEDED(hr);
			} else
				success = true;
		}
	}

	CoTaskMemFree(cauuid.pElems);

	return success;
}

int VDCaptureDriverDS::EnumerateCrossbarSources(InputSources& sources, IAMCrossbar *pCrossbar, int output) {
	// If there is an audio crossbar, scan it for sources.
	long inputs, outputs;
	int currentSourceIndex = -1;

	if (SUCCEEDED(pCrossbar->get_PinCounts(&outputs, &inputs))) {
		long currentSource = -1;

		VDVERIFY(SUCCEEDED(pCrossbar->get_IsRoutedTo(output, &currentSource)));

		for(int pin=0; pin<inputs; ++pin) {
			// get pin type
			long relatedPin;
			long physType;
			if (FAILED(pCrossbar->get_CrossbarPinInfo(TRUE, pin, &relatedPin, &physType)))
				continue;

			// check if this input can be routed to the audio output
			if (S_OK != pCrossbar->CanRoute(output, pin))
				continue;

			// check if this is the current pin
			if (pin == currentSource)
				currentSourceIndex = sources.size();

			// add entry for this pin
			sources.push_back(InputSource(pin, physType, VDStringW(VDGetNameForPhysicalConnectorTypeDShow((PhysicalConnectorType)physType))));
		}

		// If the crossbar is current set to -1, we force it to the first source for two reasons.
		// One is so that we get audio/video by default. The other is so that we're sure that
		// the source index we return is consistent. get_IsRoutedTo() has a nasty habit of
		// returning -1 when the graph is stopped or paused, even if a route actually exists.
		if (currentSource < 0 && !sources.empty()) {
			HRESULT hr = pCrossbar->Route(output, sources.front().mCrossbarPin);
			if (SUCCEEDED(hr))
				currentSourceIndex = 0;
		}
	}

	return currentSourceIndex;
}

int VDCaptureDriverDS::UpdateCrossbarSource(InputSources& sources, IAMCrossbar *pCrossbar, int output) {
	// If there is an audio crossbar, scan it for sources.
	long inputs, outputs;
	HRESULT hr = pCrossbar->get_PinCounts(&outputs, &inputs);

	if (FAILED(hr)) {
		VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to retrieve crossbar pin counts.\n"));
		return -1;
	}

	long currentSource = -1;

	hr = pCrossbar->get_IsRoutedTo(output, &currentSource);
	if (FAILED(hr)) {
		VDLog(kVDLogWarning, VDStringW(L"CapDShow: Unable to retrieve crossbar output pin routing.\n"));
		return -1;
	}

	if (hr == S_FALSE || currentSource == -1)
		return -1;

	InputSources::const_iterator it(sources.begin()), itEnd(sources.end());
	int index = 0;
	for(; it!=itEnd; ++it, ++index) {
		const InputSource& src = *it;

		if (src.mCrossbarPin == currentSource)
			return index;
	}

	VDLog(kVDLogWarning, VDStringW(L"CapDShow: Current crossbar pin does not correspond to a known source.\n"));
	return -1;
}

void VDCaptureDriverDS::DoEvents() {
	long evCode;
	LONG_PTR param1, param2;

	while(SUCCEEDED(mpMediaEventEx->GetEvent(&evCode, &param1, &param2, 0))) {
#if 0
		if (mpEventCallback)
			switch(evCode) {
			case EC_VIDEO_SIZE_CHANGED:
				mpEventCallback->EventVideoSizeChanged(LOWORD(param1), HIWORD(param1));
				break;
			}
#endif
		switch(evCode) {
		case EC_ERRORABORT:
			VDDEBUG("Cap/DShow: Error abort detected: hr=%08x (%s)\n", (int)param1, GetDXErrorName((HRESULT)param1));
			break;
		default:
			VDDEBUG("Cap/DShow: Unknown event %08x detected\n", (int)evCode);
			break;
		}

		mpMediaEventEx->FreeEventParams(evCode, param1, param2);
	}
}

LRESULT CALLBACK VDCaptureDriverDS::StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE)
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);

	VDCaptureDriverDS *pThis = (VDCaptureDriverDS *)GetWindowLongPtr(hwnd, 0);

	return pThis ? pThis->MessageSinkWndProc(hwnd, msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDCaptureDriverDS::MessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_APP)
		DoEvents();
	else if (msg == WM_APP+1)
		CaptureStop();
	else {
		VideoWindows::const_iterator it(mVideoWindows.begin()), itEnd(mVideoWindows.end());
		for(; it!=itEnd; ++it) {
			const IVideoWindowPtr& p = *it;

			p->NotifyOwnerMessage((OAHWND)hwnd, msg, wParam, lParam);
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool VDCaptureDriverDS::CapTryEnterCriticalSection() {
	return mGraphStateLock.TryWait();
}

void VDCaptureDriverDS::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key) {
	if (mpCaptureError)
		return;

	if (mpCB) {
		sint64 reltime = (sint64)(VDGetAccurateTick() - mCaptureStart) * 1000;

		try {
			if (!mCaptureStopQueued) {
				if (mCaptureThread) {
					if (!mpCB->CapEvent(kEventCapturing, 0)) {
						if (!mCaptureStopQueued.xchg(1))
							PostMessage(mhwndEventSink, WM_APP+1, 0, 0);

						goto skip_processing;
					}
				}

				mpCB->CapProcessData(stream, data, size, timestamp, key, reltime);
skip_processing:
				;
			}
		} catch(MyError& e) {
			MyError *e2 = new MyError;
			e2->TransferFrom(e);
			delete mpCaptureError.xchg(e2);

			if (!mCaptureStopQueued.xchg(1))
				PostMessage(mhwndEventSink, WM_APP+1, 0, 0);
		}
	}
}

void VDCaptureDriverDS::CapLeaveCriticalSection() {
	mGraphStateLock.Post();
}

///////////////////////////////////////////////////////////////////////////
//
//	capture system: DirectShow
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemDS : public IVDCaptureSystem {
public:
	VDCaptureSystemDS();
	~VDCaptureSystemDS();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	int mDriverCount;

	tDeviceVector mVideoDevices;
};

IVDCaptureSystem *VDCreateCaptureSystemDS() {
	return new VDCaptureSystemDS;
}

VDCaptureSystemDS::VDCaptureSystemDS()
	: mDriverCount(0)
{
	CoInitialize(NULL);
}

VDCaptureSystemDS::~VDCaptureSystemDS() {
	mVideoDevices.clear();		// must clear this first before COM goes away
	CoUninitialize();
}

void VDCaptureSystemDS::EnumerateDrivers() {
	mDriverCount = 0;
	mVideoDevices.clear();

	Enumerate(mVideoDevices, CLSID_VideoInputDeviceCategory);
	mDriverCount = mVideoDevices.size();
}

int VDCaptureSystemDS::GetDeviceCount() {
	return mDriverCount;
}

const wchar_t *VDCaptureSystemDS::GetDeviceName(int index) {
	if ((unsigned)index >= (unsigned)mDriverCount)
		return NULL;

	return mVideoDevices[index].second.c_str();
}

IVDCaptureDriver *VDCaptureSystemDS::CreateDriver(int index) {
	if ((unsigned)index >= (unsigned)mDriverCount)
		return NULL;

	return new VDCaptureDriverDS(mVideoDevices[index].first);
}

class SimpleStream: public IStream{
public:
	vdfastvector<char> memory;
	int rpos;

	SimpleStream(){ rpos=0; }

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject){ *ppvObject = 0; return 0; }
	virtual ULONG STDMETHODCALLTYPE AddRef(void){ return 0; }
	virtual ULONG STDMETHODCALLTYPE Release(void){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE Revert( void){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE Stat(__RPC__out STATSTG *pstatstg, DWORD grfStatFlag){ return 0; }
	virtual HRESULT STDMETHODCALLTYPE Clone(__RPC__deref_out_opt IStream **ppstm){ return 0; }

	virtual HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead) { 
		int s1 = memory.size()-rpos;
		if((int)cb<s1) s1 = cb;
		memcpy(pv,memory.data()+rpos,s1);
		rpos+=s1;
		if(pcbRead) *pcbRead = s1;
		return S_OK;
	}
	
	virtual HRESULT STDMETHODCALLTYPE Write(const void *pv, ULONG cb, ULONG *pcbWritten) {
		int pos = memory.size();
		memory.resize(pos+cb);
		memcpy(memory.data()+pos,pv,cb);
		if(pcbWritten) *pcbWritten = cb;
		return S_OK;
	}
};

void VDCaptureDriverDS::SaveVideoConfig(VDRegistryAppKey& key) {
	if(!mpCapFilt) return;
	IPersistStream* str;
	mpCapFilt->QueryInterface(IID_IPersistStream, (void**)&str);
	if(!str) return;
	SimpleStream d;
	str->Save(&d,false);
	str->Release();
	key.setBinary("VideoCapConfig",d.memory.data(),d.memory.size());
}

void VDCaptureDriverDS::LoadVideoConfig(VDRegistryAppKey& key) {
	if(!mpCapFilt) return;
	IPersistStream* str;
	mpCapFilt->QueryInterface(IID_IPersistStream, (void**)&str);
	if(!str) return;

	SimpleStream d;
	int n = key.getBinaryLength("VideoCapConfig");
	if(n!=-1){
		d.memory.resize(n);
		key.getBinary("VideoCapConfig",d.memory.data(),d.memory.size());
		str->Load(&d);
	}
}

void VDCaptureDriverDS::SaveAudioConfig(VDRegistryAppKey& key) {
	if(!mpAudioCapFilt) return;
	IPersistStream* str;
	mpAudioCapFilt->QueryInterface(IID_IPersistStream, (void**)&str);
	if(!str) return;
	SimpleStream d;
	str->Save(&d,false);
	str->Release();
	key.setBinary("AudioCapConfig",d.memory.data(),d.memory.size());
}

void VDCaptureDriverDS::LoadAudioConfig(VDRegistryAppKey& key) {
	if(!mpAudioCapFilt) return;
	IPersistStream* str;
	mpAudioCapFilt->QueryInterface(IID_IPersistStream, (void**)&str);
	if(!str) return;

	SimpleStream d;
	int n = key.getBinaryLength("AudioCapConfig");
	if(n!=-1){
		d.memory.resize(n);
		key.getBinary("AudioCapConfig",d.memory.data(),d.memory.size());
		str->Load(&d);
	}
}
