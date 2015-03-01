#ifndef f_VDREMOTE_VDREMOTE_H
#define f_VDREMOTE_VDREMOTE_H

#include <vfw.h>

#include "vdserver.h"

class CAVIFileRemote;

class CAVIFileRemote: public IAVIFile, public IPersistFile, public IClassFactory {
private:
	long m_refs;

	char *szPath;
	IVDubServerLink *ivdsl;
	BOOL fHasAudio;
	BITMAPINFOHEADER *vFormat;
	LONG vFormatLen, vSampleFirst, vSampleLast;
	AVISTREAMINFO vStreamInfo;
	WAVEFORMATEX *aFormat;
	LONG aFormatLen, aSampleFirst, aSampleLast;
	AVISTREAMINFO aStreamInfo;
	PAVIFILE pafTunnel;

	CRITICAL_SECTION csPort;

public:
	IVDubAnimConnection *ivdac;
	static long gRefCnt;

	CAVIFileRemote(const CLSID& rclsid);
	~CAVIFileRemote();

	////////////

	static HRESULT Create(const CLSID& rclsid, const IID& riid, void **ppv);
	STDMETHODIMP QueryInterface(const IID& iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	////////////

	STDMETHODIMP CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) ;
	STDMETHODIMP LockServer (BOOL fLock) ;

	////////////

	STDMETHODIMP GetClassID(LPCLSID lpClassID);

	STDMETHODIMP IsDirty();
	STDMETHODIMP Load(LPCOLESTR lpszFileName, DWORD grfMode);
	STDMETHODIMP Save(LPCOLESTR lpszFileName, BOOL fRemember);
	STDMETHODIMP SaveCompleted(LPCOLESTR lpszFileName);
	STDMETHODIMP GetCurFile(LPOLESTR *lplpszFileName);

	////////////

	STDMETHODIMP CreateStream(PAVISTREAM *ppStream, AVISTREAMINFOW *psi);
	STDMETHODIMP EndRecord();
	STDMETHODIMP GetStream(PAVISTREAM *ppStream, DWORD fccType, LONG lParam);
	STDMETHODIMP Info(AVIFILEINFOW *psi, LONG lSize);
	STDMETHODIMP Open(LPCSTR szFile, UINT mode, LPCOLESTR lpszFileName);
    STDMETHODIMP Save(LPCSTR szFile, AVICOMPRESSOPTIONS FAR *lpOptions,
				AVISAVECALLBACK lpfnCallback);
	STDMETHODIMP ReadData(DWORD fcc, LPVOID lp, LONG *lpcb);
	STDMETHODIMP WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer);
	STDMETHODIMP DeleteStream(DWORD fccType, LONG lParam);

	void LockPort();
	void UnlockPort();

};

///////////////////////////////////

class CAVIStreamRemote;

class CAVIStreamClassFactory: public IClassFactory {
private:
	CAVIStreamRemote *avifile;

public:
	CAVIStreamClassFactory(CAVIStreamRemote *af);

	STDMETHODIMP QueryInterface (REFIID riid, void * * ppvObj) ;
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	STDMETHODIMP CreateInstance (LPUNKNOWN pUnkOuter, REFIID riid,  void * * ppvObj) ;
	STDMETHODIMP LockServer (BOOL fLock) ;
};

class CAVIStreamRemote: public IAVIStream {
private:
	CAVIStreamClassFactory iclassfactory;

	long m_refs;

	CAVIFileRemote *parent;
	AVISTREAMINFO *streamInfo;
	WAVEFORMATEX *wfexFormat;
	BITMAPINFOHEADER *bmihFormat;
	LONG lFormatLen, lSampleFirst, lSampleLast;
	BOOL fAudio;

public:
	static long gRefCnt;

	static HRESULT Create(const CLSID& rclsid, const IID& riid, void **ppv);
	STDMETHODIMP QueryInterface(const IID& iid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	CAVIStreamRemote(const CLSID& rclsid, void **pUnknown);
	CAVIStreamRemote(CAVIFileRemote *parentPtr, BOOL isAudio, AVISTREAMINFO *asi, void *format, long format_len, long sample_first, long sample_last);
	~CAVIStreamRemote();

	STDMETHODIMP Create(LPARAM lParam1, LPARAM lParam2);
	STDMETHODIMP Delete(LONG lStart, LONG lSamples);
	STDMETHODIMP_(LONG) Info(AVISTREAMINFOW *psi, LONG lSize);
	STDMETHODIMP_(LONG) FindSample(LONG lPos, LONG lFlags);
	STDMETHODIMP Read(LONG lStart, LONG lSamples, LPVOID lpBuffer, LONG cbBuffer, LONG *plBytes, LONG *plSamples);
	STDMETHODIMP ReadData(DWORD fcc, LPVOID lp, LONG *lpcb);
	STDMETHODIMP ReadFormat(LONG lPos, LPVOID lpFormat, LONG *lpcbFormat);
	STDMETHODIMP SetFormat(LONG lPos, LPVOID lpFormat, LONG cbFormat);
	STDMETHODIMP Write(LONG lStart, LONG lSamples, LPVOID lpBuffer,
		LONG cbBuffer, DWORD dwFlags, LONG FAR *plSampWritten, 
		LONG FAR *plBytesWritten);
	STDMETHODIMP WriteData(DWORD fcc, LPVOID lpBuffer, LONG cbBuffer);
	STDMETHODIMP SetInfo(AVISTREAMINFOW *psi, LONG lSize);
};

#endif


