//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#include <stdafx.h>
#include <vd2/VDCapture/caplog.h>
#include <vd2/system/file.h>
#include <vd2/system/thread.h>

namespace {
	struct CapEntry {
		sint64	global_clock;
		sint64	timestamp;
		uint32	size;
		uint8	stream;
		bool	key;
	};

	struct CapBlock {
		enum { kCount = 1024 };
		CapEntry mEntries[kCount];
		CapBlock *mpNext;
	};

	struct CapBlockIterator {
		CapBlock *mpCurrent;
		int		mNext;
		int		mLimit;
		int		mLastLimit;

		CapBlockIterator(CapBlock *head, int lastLimit) : mpCurrent(head), mNext(0), mLimit(head && head->mpNext ? CapBlock::kCount : lastLimit), mLastLimit(lastLimit) {}

		bool operator!() const {
			return !mpCurrent;
		}

		const CapEntry& operator*() const {
			return mpCurrent->mEntries[mNext];
		}

		void operator++() {
			if (++mNext >= mLimit) {
				mNext = 0;
				mpCurrent = mpCurrent->mpNext;
				if (mpCurrent)
					mLimit = mpCurrent->mpNext ? CapBlock::kCount : mLastLimit;
			}
		}
	};
}

class VDCaptureLogFilter : public IVDCaptureLogFilter {
public:
	VDCaptureLogFilter();
	~VDCaptureLogFilter();

	void Shutdown();

	void SetChildCallback(IVDCaptureDriverCallback *pChild);

	void WriteLog(const wchar_t *pszName);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapEvent(nsVDCapture::DriverEvent event, int data);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	VDCriticalSection mLock;
	CapBlock *mpBlockHead;
	CapBlock *mpBlockTail;
	int		mNextEntry;
	IVDCaptureDriverCallback *mpCB;
};

VDCaptureLogFilter::VDCaptureLogFilter()
	: mpBlockHead(NULL)
	, mpBlockTail(NULL)
{
}

VDCaptureLogFilter::~VDCaptureLogFilter() {
	Shutdown();
}

IVDCaptureLogFilter *VDCreateCaptureLogFilter() {
	return new VDCaptureLogFilter;
}

void VDCaptureLogFilter::Shutdown() {
	while(CapBlock *p = mpBlockHead) {
		mpBlockHead = p->mpNext;
		free(p);
	}
	mNextEntry = CapBlock::kCount;
}

void VDCaptureLogFilter::SetChildCallback(IVDCaptureDriverCallback *pChild) {
	mpCB = pChild;
}

void VDCaptureLogFilter::WriteLog(const wchar_t *pszName) {
	VDFileStream stream(pszName, nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);
	VDTextOutputStream fout(&stream);

	fout.PutLine("VFrames,VCapTime,VGlobalTime,VSize,VKey,AFrames,ABytes,AGlobalTime,ASize");

	CapBlockIterator itV(mpBlockHead, mNextEntry);
	CapBlockIterator itA(mpBlockHead, mNextEntry);

	char buf[512];
	uint32 vframes = 0;
	uint32 aframes = 0;
	sint64 abytes = 0;

	for(;;) {
		bool videoOK = false;
		bool audioOK = false;
		char *s = buf;

		while(!!itV) {
			const CapEntry& vent = *itV;
			++itV;

			if (!vent.stream) {
				videoOK = true;
				s += sprintf(s, "%u,%.3f,%.3f,%u,%d,", ++vframes, vent.timestamp / 1000.0, vent.global_clock / 1000.0, vent.size, vent.key);
				break;
			}
		}

		if (!videoOK)
			s += sprintf(s, ",,,,,");

		while(!!itA) {
			const CapEntry& aent = *itA;
			++itA;

			if (aent.stream == 1) {
				audioOK = true;
				abytes += aent.size;
				s += sprintf(s, "%u,%I64d,%.3f,%u", ++aframes, abytes, aent.global_clock / 1000.0, aent.size);
				break;
			}
		}

		if (!audioOK)
			s += sprintf(s, ",,,,");

		if (!videoOK && !audioOK)
			break;

		fout.PutLine(buf);
	}

	fout.Flush();
}

void VDCaptureLogFilter::CapBegin(sint64 global_clock) {
	Shutdown();

	mpCB->CapBegin(global_clock);
}

void VDCaptureLogFilter::CapEnd(const MyError *pError) {
	mpCB->CapEnd(pError);
}

bool VDCaptureLogFilter::CapEvent(nsVDCapture::DriverEvent event, int data) {
	return mpCB->CapEvent(event, data);
}

void VDCaptureLogFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock)  {
	if (stream >= 0) {
		vdsynchronized(mLock) {
			do {
				if (mNextEntry >= CapBlock::kCount) {
					CapBlock *p = new_nothrow CapBlock;

					if (!p)
						break;

					if (mpBlockTail)
						mpBlockTail->mpNext = p;
					p->mpNext = NULL;
					if (!mpBlockHead)
						mpBlockHead = p;
					mpBlockTail = p;
					mNextEntry = 0;
				}

				CapEntry& ent = mpBlockTail->mEntries[mNextEntry++];

				ent.global_clock	= global_clock;
				ent.timestamp		= timestamp;
				ent.size			= size;
				ent.stream			= stream;
				ent.key				= key;
			} while(false);
		}
	}

	mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
}
