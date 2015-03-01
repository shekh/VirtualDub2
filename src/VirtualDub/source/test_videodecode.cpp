#include <stdafx.h>
#if 0
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Dita/interface.h>
#include "ProgressDialog.h"
#include "VideoSource.h"
#include "project.h"

namespace {
	uint32 checksum(const void *src0, size_t len) {
		uint32 sum = 0;

		if (len >= 4) {
			const uint32 *src4 = (const uint32 *)src0;

			do {
				uint32 t = sum + *src4++;

				sum = t + (t < sum);
			} while((len-=4) >= 4);

			src0 = src4;
		}

		uint32 last = 0;

		switch(len) {
		case 3:
			last = *(const uint16 *)src0 + ((uint32)((const uint8 *)src0)[2] << 16);
			break;
		case 2:
			last = *(const uint16 *)src0;
			break;
		case 1:
			last = *(const uint8 *)src0;
			break;
		}

		uint32 t = sum + last;
		return t + (t < sum);
	}
}

void VDTestVideoDecodeRandomAccess(IVDVideoSource *pSource, VDProject *proj) {
	CPUEnableExtensions(CPUCheckForExtensions());

	IVDStreamSource *pStream = pSource->asStream();

	pSource->setTargetFormat(nsVDPixmap::kPixFormat_RGB888);

	const VDPosition len = pStream->getLength();

	vdblock<uint32> checksums((uint32)len);
	const VDPixmap& px = pSource->getTargetFormat();
	uint32 frameSize = ((px.w*3+3)&~3) * px.h;

	{
		ProgressDialog pd(NULL, "Random access test", "Forward scan", (long)len, false);

		for(VDPosition i=0; i<len; ++i) {
			pd.check();
			pd.advance((long)i);

			proj->MoveToFrame(i);
			while(proj->Tick())
				;

			checksums[i] = checksum(pSource->getFrameBuffer(), frameSize);
		}
	}

	{
		ProgressDialog pd(NULL, "Random access test", "Reverse scan", (long)len, false);

		for(VDPosition i=len-1; i>=0; --i) {
			pd.check();
			pd.advance((long)((len-1) - i));

			proj->MoveToFrame(i);
			while(proj->Tick())
				;

			uint32 expected = checksums[i];
			uint32 actual = checksum(pSource->getFrameBuffer(), frameSize);

			VDASSERT(expected == actual);
		}
	}

	{
		ProgressDialog pd(NULL, "Random access test", "Random access scan", 100000, false);

		for(int i=0; i<100000; ++i) {
			pd.check();
			pd.advance(i);

			uint32 f = rand() + (rand() << 15);

			f %= (uint32)len;

			proj->MoveToFrame(f);
			while(proj->Tick())
				;

			uint32 expected = checksums[f];
			uint32 actual = checksum(pSource->getFrameBuffer(), frameSize);

			VDASSERT(expected == actual);
		}
	}
}
#endif
