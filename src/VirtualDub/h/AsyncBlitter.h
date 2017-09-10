//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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

#ifndef f_ASYNCBLITTER_H
#define f_ASYNCBLITTER_H

class VDSignal;

class VDINTERFACE IVDAsyncBlitter {
public:
	typedef int (*PulseCallback)(void *context, uint32 frame);
	typedef void (*AFC)(void *context);
	typedef bool (*APC)(int pass, sint64 timelinePos, void *data1, void *data2, bool aborting);

	enum {
		PCR_OKAY,
		PCR_NOBLIT,
		PCR_WAIT,
	};

	virtual ~IVDAsyncBlitter() {}

	virtual void enablePulsing(bool) = 0;
	virtual void setPulseCallback(PulseCallback fn, void *context) = 0;
	virtual void pulse(int delta) = 0;
	virtual void setPulseClock(uint32 clk) = 0;
	virtual uint32 getPulseClock() const = 0;
	virtual bool lock(uint32, sint32 timeout = -1) = 0;
	virtual void unlock(uint32) = 0;
	virtual void nextFrame(long adv=1) = 0;
	virtual long getFrameDelta() = 0;
	virtual void sendAFC(uint32 id, AFC, void *context) = 0;
	virtual void postAPC(uint32 id, sint64 timelinePos, APC func, void *pData1, void *pData2) = 0;
	virtual void postAPC(uint32 id, sint64 timelinePos, uint32 t, APC func, void *pData1, void *pData2) = 0;
	virtual void abort() = 0;
	virtual void beginFlush() = 0;
	virtual void stop() = 0;

	virtual bool ServiceRequests(bool fWait) = 0;

	virtual VDSignal *getFlushCompleteSignal() = 0;
};

IVDAsyncBlitter *VDCreateAsyncBlitter();
IVDAsyncBlitter *VDCreateAsyncBlitter(int maxRequests);

#endif
