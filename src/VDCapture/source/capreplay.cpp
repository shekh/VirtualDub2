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
#include <vd2/system/file.h>
#include <vd2/system/math.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/error.h>
#include <vd2/VDCapture/capdriver.h>
#include <vd2/VDCapture/capreplay.h>

///////////////////////////////////////////////////////////////////////////

class VDCaptureReplayDriver : public IVDCaptureReplayDriver {
public:
	VDCaptureReplayDriver();
	~VDCaptureReplayDriver();

	void SetChildCallback(IVDCaptureDriverCallback *pChild);

	void Init(const wchar_t *filename);
	bool ReplayNext();

protected:
	IVDCaptureDriverCallback *mpCB;

	struct Event {
		bool	audio;
		bool	key;
		sint64	captime;
		sint64	globaltime;
		uint32	bytes;
	};

	struct ReverseEventSorter {
		bool operator()(const Event& ev1, const Event& ev2) const {
			return ev1.globaltime > ev2.globaltime;
		}
	};

	vdfastvector<Event> mEvents;
	vdblock<char> mDummyData;
};

VDCaptureReplayDriver::VDCaptureReplayDriver() {
}

VDCaptureReplayDriver::~VDCaptureReplayDriver() {
}

IVDCaptureReplayDriver *VDCreateCaptureReplayDriver() {
	return new VDCaptureReplayDriver;
}

void VDCaptureReplayDriver::SetChildCallback(IVDCaptureDriverCallback *pChild) {
	mpCB = pChild;
}

void VDCaptureReplayDriver::Init(const wchar_t *filename) {
	VDTextInputFile ifile(filename);

	// skip first line
	ifile.GetNextLine();

	uint32 largestDataBlock = 0;

	while(const char *txt = ifile.GetNextLine()) {
		const char *ranges[9][2];

		for(int i=0; i<9; ++i) {
			ranges[i][0] = txt;
			ranges[i][1] = strchr(txt, ',');
			if (!ranges[i][1])
				ranges[i][1] = txt + strlen(txt);

			txt = ranges[i][1];
			if (*txt)
				++txt;
		}

		if (ranges[1][0] != ranges[1][1]) {
			Event vev;

			vev.audio = false;
			vev.key = atoi(ranges[4][0]) > 0;
			vev.captime = VDRoundToInt64(strtod(ranges[1][0], NULL) * 1000.0);
			vev.globaltime = VDRoundToInt64(strtod(ranges[2][0], NULL) * 1000.0);
			vev.bytes = atoi(ranges[3][0]);

			if (largestDataBlock < vev.bytes)
				largestDataBlock = vev.bytes;

			mEvents.push_back(vev);
		}

		if (ranges[5][0] != ranges[5][1]) {
			Event aev;

			aev.audio = true;
			aev.key = false;
			aev.globaltime = VDRoundToInt64(strtod(ranges[7][0], NULL) * 1000.0);
			aev.captime = aev.globaltime;
			aev.bytes = atoi(ranges[8][0]);

			if (largestDataBlock < aev.bytes)
				largestDataBlock = aev.bytes;

			mEvents.push_back(aev);
		}
	}

	mDummyData.resize(largestDataBlock);

	std::sort(mEvents.begin(), mEvents.end(), ReverseEventSorter());

	mpCB->CapBegin(0);
}

bool VDCaptureReplayDriver::ReplayNext() {
	if (mEvents.empty())
		return false;

	Event& ev = mEvents.back();

	try {
		mpCB->CapProcessData(ev.audio ? 1 : 0, mDummyData.data(), ev.bytes, ev.captime, ev.key, ev.globaltime);
	} catch(const MyError& e) {
		mpCB->CapEnd(&e);
		return false;
	}

	mEvents.pop_back();

	return true;
}
