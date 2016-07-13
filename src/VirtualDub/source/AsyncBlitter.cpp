//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"

#include "AsyncBlitter.h"
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/profile.h>
#include <vd2/system/tls.h>

#define USE_DRAWDIBDRAW
//#define USE_STRETCHDIBITS
//#define USE_SETDIBITSTODEVICE


#ifdef _DEBUG
#define LOCK_SET(x)		(lock_state |= (x))
#define LOCK_CLEAR(x)	(lock_state &= ~(x))
#define LOCK_RESET		(lock_state = LOCK_NONE)
#else
#define LOCK_SET(x)
#define LOCK_CLEAR(x)
#define LOCK_RESET
#endif

class VDAsyncBlitter : public VDThread, public IVDAsyncBlitter {
public:
	VDAsyncBlitter();
	VDAsyncBlitter(int max_requests);
	~VDAsyncBlitter();

	enum {
		PCR_OKAY,
		PCR_NOBLIT,
		PCR_WAIT,
	};

#ifdef _DEBUG
	enum {
		LOCK_NONE		= 0,
		LOCK_DESTROY	= 0x00000001L,
		LOCK_LOCK		= 0x00000002L,
		LOCK_PULSE		= 0x00000004L,
		LOCK_POST		= 0x00000008L,
		LOCK_ASYNC_EXIT	= 0x00000010L,
	};
	
	VDAtomicInt lock_state;
#endif

	void enablePulsing(bool);
	void setPulseCallback(PulseCallback pc, void *pcd);
	void pulse(int delta);
	void setPulseClock(uint32 clk);
	uint32 getPulseClock() const;
	bool lock(uint32, sint32 timeout = -1);
	void unlock(uint32);
	void nextFrame(long adv=1);
	long getFrameDelta();
	void sendAFC(uint32 id, AFC pFunc, void *pData);
	void postAPC(uint32 id, APC pFunc, void *pData1, void *pData2);
	void postAPC(uint32 id, uint32 t, APC pFunc, void *pData1, void *pData2);
	void abort();
	void beginFlush();

	bool ServiceRequests(bool fWait);

	VDSignal *getFlushCompleteSignal() {
		return mRequests ? &mEventAbort : NULL;
	}

private:
	class AsyncBlitRequestAFC {
	public:
		AFC		pFunc;
		void	*pData;
	};

	class AsyncBlitRequestAPC {
	public:
		APC		pFunc;
		int		pass;
		void *pData1, *pData2;
	};

	class AsyncBlitRequest {
	public:
		enum {
			REQTYPE_AFC,
			REQTYPE_APC,
		} type;
		volatile uint32	bufferID;
		uint32	framenum;

		union {
			AsyncBlitRequestAFC afc;
			AsyncBlitRequestAPC apc;
		};
	};

	AsyncBlitRequest *mRequests;
	int max_requests;

	VDRTProfiler	*mpRTProfiler;
	int				mProfileChannel;
	VDSignal		mEventDraw;
	VDSignal		mEventDrawReturn;
	VDSignal		mEventAbort;
	VDAtomicInt		dwLockedBuffers;
	VDAtomicInt		dwPulseFrame;
	uint32			dwDrawFrame;
	volatile bool	fAbort;
	bool			fPulsed;
	volatile bool	fFlush;

	PulseCallback	mpPulseCallback;
	void			*mpPulseCallbackData;

	void release(uint32);
	bool waitPulse(uint32);
	bool DoRequest(AsyncBlitRequest *req);
	void ThreadRun();
};

IVDAsyncBlitter *VDCreateAsyncBlitter() {
	return new VDAsyncBlitter;
}

IVDAsyncBlitter *VDCreateAsyncBlitter(int maxRequests) {
	return new VDAsyncBlitter(maxRequests);
}

VDAsyncBlitter::VDAsyncBlitter() : VDThread("VDAsyncBlitter") {
	max_requests		= 0;
	mRequests			= NULL;
	dwLockedBuffers		= 0;
	fAbort				= false;
	fFlush				= false;
	fPulsed				= false;
	mpPulseCallback		= NULL;
	dwPulseFrame		= 0;
	dwDrawFrame			= 0;

	LOCK_RESET;
}

VDAsyncBlitter::VDAsyncBlitter(int maxreq) : VDThread("VDAsyncBlitter") {

	max_requests		= maxreq;
	mRequests			= new AsyncBlitRequest[max_requests];
	memset(mRequests,0,sizeof(AsyncBlitRequest)*max_requests);
	dwLockedBuffers		= 0;
	fAbort				= false;
	fFlush				= false;
	fPulsed				= false;
	mpPulseCallback		= NULL;
	dwPulseFrame		= 0;
	dwDrawFrame			= 0;

	LOCK_RESET;

	if (!ThreadStart())
		throw MyError("Couldn't create draw thread!");

	SetThreadPriority(getThreadHandle(), THREAD_PRIORITY_HIGHEST);
}

VDAsyncBlitter::~VDAsyncBlitter() {
	if (isThreadAttached()) {
		fAbort = true;
		mEventDraw.signal();

		ThreadWait();
	}

	delete[] mRequests;
}



///////////////////////////////////////////



void VDAsyncBlitter::enablePulsing(bool p) {
	dwPulseFrame = 0;
	dwDrawFrame = 0;
	fPulsed = p;
}

void VDAsyncBlitter::pulse(int delta) {
	dwPulseFrame += delta;
	mEventDraw.signal();
}

void VDAsyncBlitter::setPulseClock(uint32 clk) {
	if (clk <= dwPulseFrame)
		return;

	dwPulseFrame = clk;
	mEventDraw.signal();
}

uint32 VDAsyncBlitter::getPulseClock() const {
	return dwPulseFrame;
}

bool VDAsyncBlitter::lock(uint32 id, sint32 timeout) {
	if (!mRequests)
		return true;
	
	if (fAbort)
		return false;

	if (dwLockedBuffers & id) {
		while((dwLockedBuffers & id) && !fAbort) {
			LOCK_SET(LOCK_LOCK);
			DWORD result = WaitForSingleObject(mEventDrawReturn.getHandle(), timeout < 0 ? INFINITE : timeout);
			LOCK_CLEAR(LOCK_LOCK);
			if (WAIT_TIMEOUT == result)
				return false;
		}
	}
	dwLockedBuffers |= id;

	return true;
}

void VDAsyncBlitter::unlock(uint32 id) {
	if (!mRequests) return;

	dwLockedBuffers &= ~id;
}

void VDAsyncBlitter::setPulseCallback(PulseCallback pc, void *pcd) {
	mpPulseCallback = pc;
	mpPulseCallbackData = pcd;
}

bool VDAsyncBlitter::waitPulse(uint32 framenum) {
	if (fPulsed) {
		int pcret = PCR_OKAY;

		do {
			if (mpPulseCallback) {
				pcret = mpPulseCallback(mpPulseCallbackData, framenum);
				if (pcret == PCR_WAIT) {
					LOCK_SET(LOCK_PULSE);
					mEventDraw.wait();
					LOCK_CLEAR(LOCK_PULSE);
				}
			} else
				while(!fAbort && !fFlush) {
					sint32 diff = (sint32)(dwPulseFrame-framenum);

					if (diff >= 0)
						break;

					LOCK_SET(LOCK_PULSE);
					mEventDraw.wait();
					LOCK_CLEAR(LOCK_PULSE);
				}
		} while(pcret == PCR_WAIT && !fAbort && !fFlush);

		if (pcret == PCR_NOBLIT) return false;
	}

	return fAbort;
}

void VDAsyncBlitter::nextFrame(long adv) {
	dwDrawFrame += adv;
}

long VDAsyncBlitter::getFrameDelta() {
	return dwPulseFrame - dwDrawFrame;
}

void VDAsyncBlitter::sendAFC(uint32 id, AFC pFunc, void *pData) {
	int i;

	if (fAbort) {
		return;
	}

	lock(id);

	if (!mRequests) {
		pFunc(pData);
		return;
	}

	for(;;) {
		for(i=0; i<max_requests; i++)
			if (!mRequests[i].bufferID) break;

		if (i < max_requests) break;

		LOCK_SET(LOCK_POST);
		mEventDrawReturn.wait();
		LOCK_CLEAR(LOCK_POST);

		if (fAbort) {
			unlock(id);
			return;
		}
	}

	mRequests[i].type			= AsyncBlitRequest::REQTYPE_AFC;

	mRequests[i].afc.pFunc		= pFunc;
	mRequests[i].afc.pData		= pData;

	mRequests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();

	// wait for request to complete

	lock(id);
	unlock(id);
}

void VDAsyncBlitter::postAPC(uint32 id, APC pFunc, void *pData1, void *pData2) {
	postAPC(id, dwDrawFrame, pFunc, pData1, pData2);
}

void VDAsyncBlitter::postAPC(uint32 id, uint32 time, APC pFunc, void *pData1, void *pData2) {
	int i;

	if (fAbort) {
		pFunc(0, pData1, pData2, true);
		return;
	}

	if (!mRequests) {
		for(int pass = 0; pFunc(pass, pData1, pData2, false); ++pass)
			;

		return;
	}

	for(;;) {
		for(i=0; i<max_requests; i++)
			if (!mRequests[i].bufferID) break;

		if (i < max_requests) break;

		LOCK_SET(LOCK_POST);
		mEventDrawReturn.wait();
		LOCK_CLEAR(LOCK_POST);

		if (fAbort) {
			unlock(id);
			return;
		}
	}

	mRequests[i].type			= AsyncBlitRequest::REQTYPE_APC;

	mRequests[i].apc.pFunc		= pFunc;
	mRequests[i].apc.pass		= 0;
	mRequests[i].apc.pData1		= pData1;
	mRequests[i].apc.pData2		= pData2;

	mRequests[i].framenum	= time;
	mRequests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();

//	VDDEBUG2("VDAsyncBlitter: posted %d (time=%d)\n", dwDrawFrame, dwPulseFrame);
}

void VDAsyncBlitter::release(uint32 id) {
	if (!mRequests) return;

	dwLockedBuffers &= ~id;
	mEventDrawReturn.signal();
}

void VDAsyncBlitter::abort() {
	fAbort = true;
}

void VDAsyncBlitter::beginFlush() {
	if (!mRequests)
		return;

	fFlush = true;
	mEventDraw.signal();
}

bool VDAsyncBlitter::DoRequest(AsyncBlitRequest *req) {
	switch(req->type) {
	case AsyncBlitRequest::REQTYPE_AFC:
		req->afc.pFunc(req->afc.pData);
		break;

	case AsyncBlitRequest::REQTYPE_APC:
		return req->apc.pFunc(req->apc.pass++, req->apc.pData1, req->apc.pData2, false);
	}

	return false;
}

bool VDAsyncBlitter::ServiceRequests(bool fWait) {
	AsyncBlitRequest *req;
	bool fRequestServiced = false;
	int i;

	if (fFlush) {
		req = mRequests;

		for(i=0; i<max_requests; ++i,++req) {
			if (req->bufferID) {
				switch (req->type) {
				case AsyncBlitRequest::REQTYPE_AFC:
					req->afc.pFunc(req->afc.pData);		// can't flush these
					break;
				case AsyncBlitRequest::REQTYPE_APC:
					req->apc.pFunc(req->apc.pass, req->apc.pData1, req->apc.pData2, true);	// request a quick exit
					break;
				}

				release(req->bufferID);
				req->bufferID = 0;
			}
		}

		mEventAbort.signal();

		return false;
	}

	req = mRequests;

	for(i=0; i<max_requests && !fAbort; ++i,++req) {
		if (req->bufferID) {
			if (!fFlush) {
				if (req->type == AsyncBlitRequest::REQTYPE_AFC) {
					DoRequest(req);
					fRequestServiced = true;
					release(req->bufferID);
					req->bufferID = 0;

				} else if (!fWait || !waitPulse(req->framenum)) {
					if ((uint32)dwPulseFrame < req->framenum) {
						continue;
					}

					fRequestServiced = true;
					if (mpRTProfiler)
						mpRTProfiler->BeginEvent(mProfileChannel, 0xe0ffe0, "Blit");
					bool bMore = DoRequest(req);
					if (mpRTProfiler)
						mpRTProfiler->EndEvent(mProfileChannel);

					if (bMore) {
						++req->framenum;
						continue;
					}

					release(req->bufferID);
					req->bufferID = 0;

				} else {
					// unreachable?
				}
			}

			fRequestServiced = true; // do not wait new input after flush?
			//! leaked here: APC must do cleanup
			//release(req->bufferID);
			//req->bufferID = 0;
		}
	}

	return fRequestServiced;
}

void VDAsyncBlitter::ThreadRun() {
	mpRTProfiler = VDGetRTProfiler();
	if (mpRTProfiler)
		mProfileChannel = mpRTProfiler->AllocChannel("Blitter");

	while(!fAbort) {
		if (!ServiceRequests(true) && !fAbort) {
			LOCK_SET(LOCK_ASYNC_EXIT);
			mEventDraw.wait();
			LOCK_CLEAR(LOCK_ASYNC_EXIT);
		}
	}

	// check if we have any AFC/APC requests that MUST be aborted cleanly
	AsyncBlitRequest *req = mRequests;
	for(int i=0; i<max_requests; ++i,++req) {
		if (req->bufferID) {
			switch (req->type) {
			case AsyncBlitRequest::REQTYPE_AFC:
				req->afc.pFunc(req->afc.pData);		// can't flush these
				break;
			case AsyncBlitRequest::REQTYPE_APC:
				req->apc.pFunc(req->apc.pass, req->apc.pData1, req->apc.pData2, true);	// request a quick exit
				break;
			}
			release(req->bufferID);
			req->bufferID = 0;
		}
	}

	GdiFlush();

	if (mpRTProfiler)
		mpRTProfiler->FreeChannel(mProfileChannel);

	dwLockedBuffers = 0;
	mEventDraw.signal();
	mEventAbort.signal();
}
