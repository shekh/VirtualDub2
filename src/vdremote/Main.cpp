#include <crtdbg.h>
#include <objbase.h>

#include "clsid.h"
#include "vdremote.h"
#include "vdserver.h"

#pragma warning(disable: 4355)		// warning C4355: 'this' : used in base member initializer list

long CAVIFileRemote::gRefCnt = 0;
long CAVIStreamRemote::gRefCnt = 0;

////////////////////////////////////////////////////////////

const GUID CDECL CLSID_AVIFile
	= { 0x00020000, 0, 0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

const GUID CDECL another_IID_IAVIFile
	= { 0x00020020, 0, 0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

const GUID CDECL another_IID_IAVIStream
	= { 0x00020021, 0, 0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};

const GUID CDECL CLSID_Avisynth
	= { 0xE6D6B700, 0x124D, 0x11D4, 0x86, 0xF3, 0xDB, 0x80, 0xAF, 0xD9, 0x87, 0x78};

////////////////////////////////////////////////////////////

BOOL APIENTRY DllMain(HANDLE hModule, ULONG ulReason, LPVOID lpReserved) {

	switch(ulReason) {
	case DLL_PROCESS_ATTACH:
		CoInitialize(NULL);
		_RPT0(0,"Process attach\n");
		break;

	case DLL_PROCESS_DETACH:
		CoUninitialize();
		_RPT0(0,"Process detach\n");
		break;
	}

    return TRUE;
}

const GUID CDECL CLSID_CAVIFileRemote
	= {0x894288e0,0x0948,0x11d2,{0x81,0x09,0x00,0x48,0x45,0x00,0x0e,0xb5}};
const GUID CDECL CLSID_CAVIStreamRemote
	= {0x91379540,0x0948,0x11d2,{0x81,0x09,0x00,0x48,0x45,0x00,0x0e,0xb5}};

// From the Microsoft AVIFile docs.  Dense code...

extern "C" STDAPI DllGetClassObject(const CLSID& rclsid, const IID& riid, void **ppv);

STDAPI DllGetClassObject(const CLSID& rclsid, const IID& riid, void **ppv) {
	HRESULT hresult;

	_RPT0(0,"DllGetClassObject()\n");

	if (rclsid == CLSID_CAVIStreamRemote) {
		_RPT0(0,"\tCLSID: CAVIStreamRemote\n");
		hresult = CAVIStreamRemote::Create(rclsid, riid, ppv);
	} else { // if (rclsid == CLSID_CAVIStreamRemote) {
		_RPT0(0,"\tCLSID: CAVIFileRemote (default) \n");
		hresult = CAVIFileRemote::Create(rclsid, riid, ppv);
	}

	_RPT0(0,"DllGetClassObject() exit\n");

	return hresult;
}

extern "C" STDAPI DllCanUnloadNow();

STDAPI DllCanUnloadNow() {
	_RPT2(0,"DllCanUnloadNow(): CAVIFileRemote %ld, CAVIStreamRemote %ld\n", CAVIFileRemote::gRefCnt, CAVIStreamRemote::gRefCnt);

	return (CAVIFileRemote::gRefCnt || CAVIStreamRemote::gRefCnt) ? S_FALSE : S_OK;
}


///////////////////////////////////////////////////////////////////////////
//
//	CAVIFileRemote
//
///////////////////////////////////////////////////////////////////////////


HRESULT CAVIFileRemote::CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) {
	if (pUnkOuter) return CLASS_E_NOAGGREGATION;

	return Create(CLSID_CAVIFileRemote, riid, ppvObj);
}

HRESULT CAVIFileRemote::LockServer (BOOL fLock) {
	_RPT1(0,"%p->CAVIFileRemote::LockServer()\n", this);
	return S_OK;
}

STDMETHODIMP CAVIFileRemote::GetClassID(LPCLSID lpClassID) {
	_RPT1(0,"%p->CAVIFileRemote::GetClassID()\n", this);

	*lpClassID = CLSID_CAVIFileRemote;

	return S_OK;
}

STDMETHODIMP CAVIFileRemote::IsDirty() {
	_RPT1(0,"%p->CAVIFileRemote::IsDirty()\n", this);
	return S_FALSE;
}

STDMETHODIMP CAVIFileRemote::Load(LPCOLESTR lpszFileName, DWORD grfMode) {
	char filename[MAX_PATH];

	_RPT1(0,"%p->CAVIFileRemote::Load()\n", this);

	WideCharToMultiByte(AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0, lpszFileName, -1, filename, sizeof filename, NULL, NULL); 

	return Open(filename, grfMode, lpszFileName);
}

STDMETHODIMP CAVIFileRemote::Save(LPCOLESTR lpszFileName, BOOL fRemember) {
	_RPT1(0,"%p->CAVIFileRemote::Save()\n", this);
	return E_FAIL;
}

STDMETHODIMP CAVIFileRemote::SaveCompleted(LPCOLESTR lpszFileName) {
	_RPT1(0,"%p->CAVIFileRemote::SaveCompleted()\n", this);
	return S_OK;
}

STDMETHODIMP CAVIFileRemote::GetCurFile(LPOLESTR *lplpszFileName) {
	_RPT1(0,"%p->CAVIFileRemote::GetCurFile()\n", this);
	*lplpszFileName = NULL;

	return E_FAIL;
}

///////////////////////////////////////////////////

HRESULT CAVIFileRemote::Create(const CLSID& rclsid, const IID& riid, void **ppv) {
	CAVIFileRemote *pAVIFileRemote;
	HRESULT hresult;

	_RPT0(0,"CAVIFileRemote::Create()\n");

	pAVIFileRemote = new CAVIFileRemote(rclsid);

	if (!pAVIFileRemote) return ResultFromScode(E_OUTOFMEMORY);

	hresult = pAVIFileRemote->QueryInterface(riid, ppv);
	pAVIFileRemote->Release();
	if (FAILED(GetScode(hresult))) {
		_RPT0(0,"failed!\n");
	}

	_RPT0(0,"CAVIFileRemote::Create() exit\n");

	return hresult;
}

STDMETHODIMP CAVIFileRemote::QueryInterface(const IID& iid, void **ppv) {
	_RPT1(0,"%08lx->CAVIFileRemote::QueryInterface()\n", this);

	_RPT3(0,"\tGUID: {%08lx-%04x-%04x-", iid.Data1, iid.Data2, iid.Data3);
	_RPT4(0,"%02x%02x-%02x%02x", iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3]);
	_RPT4(0,"%02x%02x%02x%02x} (", iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);

	if (iid == IID_IUnknown) {
		*ppv = (IUnknown *)(IAVIFile *)this;
		_RPT0(0,"IUnknown)\n");
	} else if (iid == IID_IClassFactory) {
		*ppv = (IClassFactory *)this;
		_RPT0(0,"IClassFactory)\n");
	} else if (iid == IID_IPersist) {
		*ppv = (IPersist *)this;
		_RPT0(0,"IPersist)\n");
	} else if (iid == IID_IPersistFile) {
		*ppv = (IPersistFile *)this;
		_RPT0(0,"IPersistFile)\n");
	} else if (iid == another_IID_IAVIFile) {
		*ppv = (IAVIFile *)this;
		_RPT0(0,"IAVIFile)\n");
	} else {
		_RPT0(0,"unknown!)\n");
		*ppv = NULL;
		return ResultFromScode(E_NOINTERFACE);
	}

	AddRef();

	return NULL;
}

STDMETHODIMP_(ULONG) CAVIFileRemote::AddRef() {
	_RPT1(0,"%p->CAVIFileRemote::AddRef()\n", this);
	++gRefCnt;
	return ++m_refs;
}

STDMETHODIMP_(ULONG) CAVIFileRemote::Release() {
	_RPT1(0,"%p->CAVIFileRemote::Release()\n", this);

	if (!--m_refs)
		delete this;

	--gRefCnt;

	_RPT0(0,"CAVIFileRemote::Release() exit\n");

	return m_refs;
}

///////////////////////////////////////////////////////////////////////////
//
//	CAVIStreamRemote - binding glue
//
///////////////////////////////////////////////////////////////////////////

CAVIStreamClassFactory::CAVIStreamClassFactory(CAVIStreamRemote *af) {
	avifile = af;
}

HRESULT CAVIStreamClassFactory::QueryInterface (REFIID riid, void * * ppvObj) {
	return avifile->QueryInterface(riid, ppvObj);
}

unsigned long  CAVIStreamClassFactory::AddRef () {
	return avifile->AddRef();
}

unsigned long  CAVIStreamClassFactory::Release () {
	return avifile->Release();
}

HRESULT CAVIStreamClassFactory::CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) {
	if (pUnkOuter) return CLASS_E_NOAGGREGATION;

	return avifile->Create(CLSID_CAVIFileRemote, riid, ppvObj);
}

HRESULT CAVIStreamClassFactory::LockServer (BOOL fLock) {
	return S_OK;
}

///////////////////////

HRESULT CAVIStreamRemote::Create(const CLSID& rclsid, const IID& riid, void **ppv) {
	CAVIStreamRemote *pAVIStreamRemote;
	IUnknown *pUnknown;
	HRESULT hresult;

	_RPT0(0,"CAVIStreamRemote::Create()\n");

	pAVIStreamRemote = new CAVIStreamRemote(rclsid, (void **)&pUnknown);

	if (!pAVIStreamRemote) return ResultFromScode(E_OUTOFMEMORY);

	hresult = pUnknown->QueryInterface(riid, ppv);
	pAVIStreamRemote->Release();
	if (FAILED(GetScode(hresult))) {
		_RPT0(0,"Failed!\n");
	}

	_RPT0(0,"CAVIStreamRemote::Create() exit\n");

	return hresult;
}

STDMETHODIMP CAVIStreamRemote::QueryInterface(const IID& iid, void **ppv) {
	_RPT1(0,"%08lx->CAVIStreamRemote::QueryInterface()\n", this);

	_RPT3(0,"\tGUID: {%08lx-%04x-%04x-", iid.Data1, iid.Data2, iid.Data3);
	_RPT4(0,"%02x%02x-%02x%02x", iid.Data4[0], iid.Data4[1], iid.Data4[2], iid.Data4[3]);
	_RPT4(0,"%02x%02x%02x%02x} (", iid.Data4[4], iid.Data4[5], iid.Data4[6], iid.Data4[7]);

	if (iid == IID_IUnknown) {
		*ppv = (IUnknown *)this;
		_RPT0(0,"IUnknown)\n");
	} else if (iid == IID_IClassFactory) {
		*ppv = (IClassFactory *)&iclassfactory;
		_RPT0(0,"IClassFactory)\n");
	} else if (iid == IID_IAVIStream) {
		*ppv = (IAVIStream *)this;
		_RPT0(0,"IAVIStream)\n");
	} else {
		_RPT0(0,"unknown)\n");
		*ppv = NULL;
		return ResultFromScode(E_NOINTERFACE);
	}

	AddRef();

	return NULL;
}

STDMETHODIMP_(ULONG) CAVIStreamRemote::AddRef() {
	_RPT0(0,"CAVIStreamRemote::AddRef()\n");

	++gRefCnt;
	return ++m_refs;
}

STDMETHODIMP_(ULONG) CAVIStreamRemote::Release() {
	_RPT0(0,"CAVIStreamRemote::Release()\n");

	if (!--m_refs)
		delete this;

	--gRefCnt;

	_RPT0(0,"CAVIStreamRemote::Release() exit\n");

	return m_refs;
}

////////////////////////////////////////////////////////////////////////
//
//		CAVIFileRemote
//
////////////////////////////////////////////////////////////////////////

STDMETHODIMP CAVIFileRemote::CreateStream(PAVISTREAM *ppStream, AVISTREAMINFOW *psi) {
	_RPT1(0,"%p->CAVIFileRemote::CreateStream()\n", this);

	if (pafTunnel)
		return pafTunnel->CreateStream(ppStream, psi);

	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileRemote::EndRecord() {
	_RPT1(0,"%p->CAVIFileRemote::EndRecord()\n", this);

	if (pafTunnel)
		return pafTunnel->EndRecord();

	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileRemote::Save(LPCSTR szFile, AVICOMPRESSOPTIONS FAR *lpOptions,
				AVISAVECALLBACK lpfnCallback) {
	_RPT1(0,"%p->CAVIFileRemote::Save()\n", this);

	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileRemote::ReadData(DWORD fcc, LPVOID lp, LONG *lpcb) {
	_RPT1(0,"%p->CAVIFileRemote::ReadData()\n", this);

	if (pafTunnel)
		return pafTunnel->ReadData(fcc, lp, lpcb);

	return AVIERR_NODATA;
}

STDMETHODIMP CAVIFileRemote::WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer) {
	_RPT1(0,"%p->CAVIFileRemote::WriteData()\n", this);

	if (pafTunnel)
		return pafTunnel->WriteData(fcc, lpBuffer, cbBuffer);

	return AVIERR_READONLY;
}

STDMETHODIMP CAVIFileRemote::DeleteStream(DWORD fccType, LONG lParam) {
	_RPT1(0,"%p->CAVIFileRemote::DeleteStream()\n", this);

	if (pafTunnel)
		return pafTunnel->DeleteStream(fccType, lParam);

	return AVIERR_READONLY;
}




CAVIFileRemote::CAVIFileRemote(const CLSID& rclsid) {
	_RPT0(0,"CAVIFileRemote::CAVIFileRemote()\n");

	m_refs = 0; AddRef();

	szPath = NULL;
	ivdsl = NULL;
	ivdac = NULL;
	vFormat = NULL;
	aFormat = NULL;
	pafTunnel = NULL;

	InitializeCriticalSection(&csPort);
}

CAVIFileRemote::~CAVIFileRemote() {
	_RPT0(0,"CAVIFileRemote::~CAVIFileRemote()\n");

	DeleteCriticalSection(&csPort);

	if (ivdac) ivdsl->FrameServerDisconnect(ivdac);
	delete szPath;

	if (pafTunnel)
		pafTunnel->Release();
}


STDMETHODIMP CAVIFileRemote::Open(LPCSTR szFile, UINT mode, LPCOLESTR lpszFileName) {
	HMMIO hmmio = NULL;
	MMCKINFO mmiriff, mmi;
	MMRESULT mmerr;
	HRESULT final = S_OK;

	_RPT2(0,"CAVIFileRemote::Open(\"%s\", %08lx)\n", szFile, mode);

	if (pafTunnel) {
		pafTunnel->Release();
		pafTunnel = NULL;
	}

	if (mode & (OF_CREATE|OF_WRITE)) {
		IPersistFile *ppf;

		if (FAILED(CoCreateInstance(CLSID_AVIFile, NULL, CLSCTX_INPROC_SERVER, another_IID_IAVIFile, (void **)&pafTunnel)))
			return (HRESULT)E_FAIL;

		if (FAILED(pafTunnel->QueryInterface(IID_IPersistFile, (void **)&ppf)))
			return (HRESULT)E_FAIL;

		HRESULT hr = ppf->Load(lpszFileName, mode);

		ppf->Release();

		return hr;
	}

	if (!(hmmio = mmioOpen((char *)szFile, NULL, MMIO_READ)))
		return E_FAIL;

	_RPT0(0,"File opened.\n");

	// RIFF <size> VDRM { PATH <remote-path> }

	try {
		char buf[16];

		_RPT0(0,"Checking for Avisynth signature...\n");

		if (16==mmioRead(hmmio, buf, 16)) {
			buf[9] = 0;
			if (!_stricmp(buf, "#avisynth")) {

				mmioClose(hmmio, 0);

				// Hand it off to the Avisynth handler.

				IPersistFile *ppf;

				// Okay, it's not one of our files.  Now try passing it off
				// to the regular AVI handler!

				_RPT0(0,"Attempt avisynth tunnel create\n");

				if (FAILED(CoCreateInstance(CLSID_Avisynth, NULL, CLSCTX_INPROC_SERVER, another_IID_IAVIFile, (void **)&pafTunnel)))
					return (HRESULT)E_FAIL;

				_RPT0(0,"Attempt avisynth tunnel query -> IPersistFile\n");

				if (FAILED(pafTunnel->QueryInterface(IID_IPersistFile, (void **)&ppf)))
					return (HRESULT)E_FAIL;

				_RPT0(0,"Attempt avisynth tunnel load\n");

				HRESULT hr = ppf->Load(lpszFileName, mode);

				ppf->Release();

				return hr;
			}
		}

		mmioSeek(hmmio, 0, SEEK_SET);

		_RPT0(0,"Attempting to find 'VDRM'...\n");

		mmiriff.fccType = 'MRDV';
		mmerr = mmioDescend(hmmio, &mmiriff, NULL, MMIO_FINDRIFF);
		if (mmerr == MMIOERR_CHUNKNOTFOUND) {
			IPersistFile *ppf;

			mmioClose(hmmio, 0);

			// Okay, it's not one of our files.  Now try passing it off
			// to the regular AVI handler!

			_RPT0(0,"Attempt tunnel create\n");

			if (FAILED(CoCreateInstance(CLSID_AVIFile, NULL, CLSCTX_INPROC_SERVER, another_IID_IAVIFile, (void **)&pafTunnel)))
				return (HRESULT)E_FAIL;

			_RPT0(0,"Attempt tunnel query -> IPersistFile\n");

			if (FAILED(pafTunnel->QueryInterface(IID_IPersistFile, (void **)&ppf)))
				return (HRESULT)E_FAIL;

			_RPT0(0,"Attempt tunnel load\n");

			HRESULT hr = ppf->Load(lpszFileName, mode);

			ppf->Release();

			return hr;
		}

		else if (mmerr != MMSYSERR_NOERROR) throw (HRESULT)E_FAIL;

		_RPT0(0,"Attempting to find 'PATH'...\n");
		mmi.ckid = 'HTAP';
		mmerr = mmioDescend(hmmio, &mmi, &mmiriff, MMIO_FINDCHUNK);
		if (mmerr == MMIOERR_CHUNKNOTFOUND) throw (HRESULT)E_FAIL;
		else if (mmerr != MMSYSERR_NOERROR) throw (HRESULT)E_FAIL;

		_RPT0(0,"Allocate path memory...\n");
		if (!(szPath = new char[mmi.cksize+1]))
			throw (HRESULT)E_OUTOFMEMORY;

		szPath[mmi.cksize]=0;

		_RPT0(0,"Read in path...\n");

		if ((LONG)mmi.cksize != mmioRead(hmmio, szPath, mmi.cksize))
			throw (HRESULT)E_FAIL;

		_RPT1(0,"File parsed, remote-path: [%s]\n", szPath);

		// now attempt to open the link

		ivdsl = GetDubServerInterface();
		if (!ivdsl) throw (HRESULT)E_FAIL;

		_RPT0(0,"Have dub server interface.\n");

		ivdac = ivdsl->FrameServerConnect(szPath);
		if (!ivdac) throw (HRESULT)E_FAIL;

		// retrieve streaminfo and format information

		_RPT0(0,"Connected to frameserver.\n");

		fHasAudio = ivdac->hasAudio();

		_RPT0(0,"Reading video stream info...\n");

		if (!ivdac->readStreamInfo(&vStreamInfo, FALSE, &vSampleFirst, &vSampleLast))
			throw (HRESULT)E_FAIL;

		_RPT0(0,"Reading video format length...\n");

		if ((vFormatLen = ivdac->readFormat(NULL, FALSE))<=0)
			throw (HRESULT)E_FAIL;

		_RPT1(0,"Allocating video format (%ld bytes)...\n", vFormatLen);

		if (!(vFormat = (BITMAPINFOHEADER *)malloc(vFormatLen)))
			throw (HRESULT)E_OUTOFMEMORY;

		_RPT0(0,"Reading video format...\n");

		if (ivdac->readFormat(vFormat, FALSE)<=0)
			throw (HRESULT)E_FAIL;

		if (fHasAudio) {
			_RPT0(0,"Reading audio stream info...\n");

			if (!ivdac->readStreamInfo(&aStreamInfo, TRUE, &aSampleFirst, &aSampleLast))
				throw (HRESULT)E_FAIL;

			_RPT0(0,"Reading audio format length...\n");

			if ((aFormatLen = ivdac->readFormat(NULL, TRUE))<=0)
				throw (HRESULT)E_FAIL;

			_RPT1(0,"Allocating audio format (%ld bytes)...\n", aFormatLen);

			if (!(aFormat = (WAVEFORMATEX *)malloc(aFormatLen)))
				throw (HRESULT)E_OUTOFMEMORY;

			_RPT0(0,"Reading audio format...\n");

			if (ivdac->readFormat(aFormat, TRUE)<=0)
				throw (HRESULT)E_FAIL;
		}

	} catch(HRESULT res) {
		_RPT0(0,"*** failed!\n");

		final = res;
	}

	if (hmmio) mmioClose(hmmio, 0);

	_RPT0(0,"CAVIFileRemote::Open() exit\n");

	return final;
}

STDMETHODIMP CAVIFileRemote::Info(AVIFILEINFOW *psi, LONG lSize) {
	AVISTREAMINFO *asi;

	_RPT0(0,"CAVIFileRemote::Info()\n");

	if (pafTunnel)
		return pafTunnel->Info(psi, lSize);
	
	if (!psi || !ivdac) return E_FAIL;

	psi->dwMaxBytesPerSec		= 0;
	psi->dwFlags				= AVIFILEINFO_HASINDEX | AVIFILEINFO_ISINTERLEAVED;
	psi->dwCaps					= AVIFILECAPS_CANREAD | AVIFILECAPS_ALLKEYFRAMES | AVIFILECAPS_NOCOMPRESSION;
	psi->dwStreams				= fHasAudio ? 2 : 1;
	psi->dwSuggestedBufferSize	= 0;
	psi->dwWidth				= vFormat->biWidth;
	psi->dwHeight				= vFormat->biHeight;
	psi->dwEditCount			= 0;
	wcscpy(psi->szFileType, L"VirtualDub Remote AVI");

	// determine which stream is longer.
	// v_dwRate / v_dwScale < a_dwRate / a_dwScale
	// v_dwRate * a_dwScale < a_dwRate * v_dwScale
	//
	// DON'T: Panasonic uses AVISTREAMINFO.dwLength as the frame count!

	asi = &vStreamInfo;

//	if (fHasAudio && vStreamInfo.dwRate * aStreamInfo.dwScale < aStreamInfo.dwRate * vStreamInfo.dwScale)
//		asi = &aStreamInfo;

	psi->dwRate					= asi->dwRate;
	psi->dwScale				= asi->dwScale;
	psi->dwLength				= asi->dwLength;

	return 0;
}

STDMETHODIMP CAVIFileRemote::GetStream(PAVISTREAM *ppStream, DWORD fccType, LONG lParam) {
	CAVIStreamRemote *casr;

	_RPT4(0,"%p->CAVIFileRemote::GetStream(%p, %08lx, %ld)\n", this, ppStream, fccType, lParam);

	if (pafTunnel) {
		HRESULT hr;
		hr = pafTunnel->GetStream(ppStream, fccType, lParam);

		_RPT2(0,"%08lx %08lx\n", *ppStream, hr);

		return hr;
	}

	*ppStream = NULL;

	if (!fccType) {
		if (lParam==0) fccType = streamtypeVIDEO;
		else if (lParam==1) {
			lParam = 0;
			fccType = streamtypeAUDIO;
		}
	}

	if (lParam > 0) return AVIERR_NODATA;

	if (fccType == streamtypeVIDEO) {
		if (!(casr = new CAVIStreamRemote(this, FALSE, &vStreamInfo, vFormat, vFormatLen, vSampleFirst, vSampleLast)))
			return AVIERR_MEMORY;

		*ppStream = (IAVIStream *)casr;

	} else if (fccType == streamtypeAUDIO) {
		if (!fHasAudio) return AVIERR_NODATA;

		if (!(casr = new CAVIStreamRemote(this, TRUE, &aStreamInfo, aFormat, aFormatLen, aSampleFirst, aSampleLast)))
			return AVIERR_MEMORY;

		*ppStream = (IAVIStream *)casr;
	} else
		return AVIERR_NODATA;

	return 0;
}





void CAVIFileRemote::LockPort() {
	EnterCriticalSection(&csPort);
}

void CAVIFileRemote::UnlockPort() {
	LeaveCriticalSection(&csPort);
}



////////////////////////////////////////////////////////////////////////
//
//		CAVIStreamRemote
//
////////////////////////////////////////////////////////////////////////

STDMETHODIMP CAVIStreamRemote::Create(LPARAM lParam1, LPARAM lParam2) {
	_RPT1(0,"%p->CAVIStreamRemote::Create()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamRemote::Delete(LONG lStart, LONG lSamples) {
	_RPT1(0,"%p->CAVIStreamRemote::Delete()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamRemote::ReadData(DWORD fcc, LPVOID lp, LONG *lpcb) {
	_RPT1(0,"%p->CAVIStreamRemote::ReadData()\n", this);
	return AVIERR_NODATA;
}

STDMETHODIMP CAVIStreamRemote::SetFormat(LONG lPos, LPVOID lpFormat, LONG cbFormat) {
	_RPT1(0,"%p->CAVIStreamRemote::SetFormat()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamRemote::WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer) {
	_RPT1(0,"%p->CAVIStreamRemote::WriteData()\n", this);
	return AVIERR_READONLY;
}

STDMETHODIMP CAVIStreamRemote::SetInfo(AVISTREAMINFOW *psi, LONG lSize) {
	return AVIERR_READONLY;
}



CAVIStreamRemote::CAVIStreamRemote(const CLSID& rclsid, void **pUnknown) : iclassfactory(this) {
	_RPT1(0,"%p->CAVIStreamRemote()\n", this);
	m_refs = 0; AddRef();

	parent			= NULL;
	streamInfo		= NULL;
	wfexFormat		= NULL;
	bmihFormat		= NULL;
}

CAVIStreamRemote::CAVIStreamRemote(CAVIFileRemote *parentPtr, BOOL isAudio, AVISTREAMINFO *asi, void *format, long format_len, long sample_first, long sample_last) : iclassfactory(this) {
	_RPT2(0,"%p->CAVIStreamRemote(%s)\n", this, isAudio ? "audio" : "video");
	m_refs = 0; AddRef();

	parentPtr->AddRef();

	parent			= parentPtr;
	fAudio			= isAudio;
	streamInfo		= asi;
	wfexFormat		= (WAVEFORMATEX *)format;
	bmihFormat		= (BITMAPINFOHEADER *)format;
	lFormatLen		= format_len;
	lSampleFirst	= sample_first;
	lSampleLast		= sample_last;
}

CAVIStreamRemote::~CAVIStreamRemote() {
	_RPT1(0,"%p->~CAVIStreamRemote()\n", this);

	parent->Release();
}

STDMETHODIMP_(LONG) CAVIStreamRemote::Info(AVISTREAMINFOW *psi, LONG lSize) {
	AVISTREAMINFOW asiw;

	_RPT3(0,"%p->CAVIStreamRemote::Info(%p, %ld)\n", this, psi, lSize);

	_RPT1(0,"stream length: %ld\n", streamInfo->dwLength);
	memset(psi, 0, lSize);
	memcpy(&asiw, streamInfo, sizeof(AVISTREAMINFO));
	wcscpy(asiw.szName, fAudio ? L"VDub remote audio #1" : L"VDub remote video #1");
	if (lSize < sizeof(AVISTREAMINFOW)) {
		memcpy(psi, &asiw, lSize);
	} else {
		memcpy(psi, &asiw, sizeof(AVISTREAMINFOW));
	}

	return 0;
}

STDMETHODIMP_(LONG) CAVIStreamRemote::FindSample(LONG lPos, LONG lFlags) {
	_RPT3(0,"%p->CAVIStreamRemote::FindSample(%ld, %08lx)\n", this, lPos, lFlags);

	if (lFlags & FIND_FORMAT)
		return -1;

	if (lFlags & FIND_FROM_START)
		return 0;

	return lPos;
}

STDMETHODIMP CAVIStreamRemote::Read(LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG *plBytes, LONG *plSamples) {
	HRESULT hres = 0;
	int err;

	_RPT3(0,"%p->CAVIStreamRemote::Read(%ld samples at %ld)\n", this, lSamples, lStart);
	_RPT2(0,"\tbuffer: %ld bytes at %p\n", cbBuffer, lpBuffer);

	parent->LockPort();

	if (plSamples) *plSamples = 0;
	if (plBytes) *plBytes = 0;

	// Panasonic does some bad things if you give it fewer samples than it asks for,
	// particularly if cbBuffer is 0 and lpBuffer = NULL.

	if (fAudio) {
		LONG lABytes, lASamples;

		if (lSamples == AVISTREAMREAD_CONVENIENT)
			lSamples = cbBuffer;

			while(lSamples > 0) {
				err = parent->ivdac->readAudio(lStart, lSamples, lpBuffer, cbBuffer, &lABytes, &lASamples);

				if (err == VDSRVERR_TOOBIG || !lASamples)
					break;

				if (err != VDSRVERR_OK) {
					hres = AVIERR_FILEREAD;
					break;
				}

				if (plBytes)
					*plBytes += lABytes;

				if (plSamples)
					*plSamples += lASamples;

				if (lpBuffer)
					lpBuffer = (char *)lpBuffer + lABytes;

				cbBuffer -= lABytes;
				lStart += lASamples;
				lSamples -= lASamples;
			}
	} else {
		if (lSamples == AVISTREAMREAD_CONVENIENT)
			lSamples = 1;

		if (!lpBuffer) {
			if (plSamples) *plSamples = 1;
			if (plBytes) *plBytes = bmihFormat->biSizeImage;
		} else if (cbBuffer < (long)bmihFormat->biSizeImage) {
			_RPT1(0,"\tBuffer too small; should be %ld samples\n", bmihFormat->biSizeImage);
			hres = AVIERR_BUFFERTOOSMALL;
			if (plSamples) *plSamples = 1;
			if (plBytes) *plBytes = bmihFormat->biSizeImage;
		} else {
			_RPT0(0,"\tAttempting to read\n");
			err = parent->ivdac->readVideo(lStart, lpBuffer);
			if (!err) {
				_RPT0(0,"\tRead successful\n");
				if (plSamples) *plSamples = 1;
				if (plBytes) *plBytes = bmihFormat->biSizeImage;
			} else {
				_RPT0(0,"\tError!\n");
				hres = AVIERR_FILEREAD;
			}
		}
	}

	parent->UnlockPort();

	return hres;
}

STDMETHODIMP CAVIStreamRemote::ReadFormat(LONG lPos, LPVOID lpFormat, LONG *lpcbFormat) {
	_RPT1(0,"%p->CAVIStreamRemote::ReadFormat()\n", this);

	if (!lpFormat) {
		*lpcbFormat = lFormatLen;
		return S_OK;
	}

	if (*lpcbFormat < lFormatLen)
		memcpy(lpFormat, wfexFormat, *lpcbFormat);
	else {
		memcpy(lpFormat, wfexFormat, lFormatLen);
		*lpcbFormat = lFormatLen;
	}

	return S_OK;
}

STDMETHODIMP CAVIStreamRemote::Write(LONG lStart, LONG lSamples, LPVOID lpBuffer,
	LONG cbBuffer, DWORD dwFlags, LONG FAR *plSampWritten, 
    LONG FAR *plBytesWritten) {

	_RPT1(0,"%p->CAVIStreamRemote::Write()\n", this);

	return AVIERR_READONLY;
}

