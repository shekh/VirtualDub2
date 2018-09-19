//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#ifdef _DEBUG
	#include <vd2/VDXFrame/VideoFilter.h>
	#include <vd2/Kasumi/text.h>
	#include <vd2/Kasumi/region.h>

	class VDVideoFilterShowInfo : public VDXVideoFilter {
	public:
		uint32 GetParams();
		void Start();
		void Run();

	protected:
		VDStringA		mTextLine;
		VDPixmapRegion	mTextRegion;
		VDPixmapRegion	mShadowRegion;
		VDPixmapPathRasterizer	mRasterizer;
		VDPixmapRegion	mShadowBrush;
	};

	uint32 VDVideoFilterShowInfo::GetParams() {
		return 0;
	}

	void VDVideoFilterShowInfo::Start() {
		VDPixmapCreateRoundRegion(mShadowBrush, 16.0f);
	}

	void VDVideoFilterShowInfo::Run() {
		const VDXFBitmap& dst = *fa->mpOutputFrames[0];
		const VDXPixmap& pxdst = *dst.mpPixmap;

		const float size = 8.0f * 8.0f * 20.0f;

		VDPixmap pxdst2;
		pxdst2.data = pxdst.data;
		pxdst2.data2 = pxdst.data2;
		pxdst2.data3 = pxdst.data3;
		pxdst2.pitch = pxdst.pitch;
		pxdst2.pitch2 = pxdst.pitch2;
		pxdst2.pitch3 = pxdst.pitch3;
		pxdst2.format = pxdst.format;
		pxdst2.w = pxdst.w;
		pxdst2.h = pxdst.h;
		pxdst2.palette = pxdst.palette;

		float y = 20.0f;

		for(int lines=0; lines<3; ++lines) {
			switch(lines) {
			case 0:
				mTextLine.sprintf("Source frame: %d (%.3fs)", (int)fa->pfsi->lCurrentSourceFrame, (double)fa->pfsi->lSourceFrameMS / 1000.0);
				break;
			case 1:
				mTextLine.sprintf("Output frame: %d", (int)fa->pfsi->mOutputFrame);
				break;
			case 2:
				mTextLine.sprintf("Sequence frame: %d (%.3fs)", (int)fa->pfsi->lCurrentFrame, (double)fa->pfsi->lDestFrameMS / 1000.0);
				break;
			}

			mRasterizer.Clear();
			VDPixmapConvertTextToPath(mRasterizer, NULL, size, 8.0f * 8.0f * 10.0f, 8.0f * 8.0f * y, mTextLine.c_str());		
			mRasterizer.ScanConvert(mTextRegion);
			VDPixmapConvolveRegion(mShadowRegion, mTextRegion, mShadowBrush);
			VDPixmapFillPixmapAntialiased8x(pxdst2, mShadowRegion, 0, 0, 0xFF000000);
			VDPixmapFillPixmapAntialiased8x(pxdst2, mTextRegion, 0, 0, 0xFFFFFF00);

			y += 20.0f;
		}
	}

	extern const VDXFilterDefinition filterDef_showinfo = VDXVideoFilterDefinition<VDVideoFilterShowInfo>(
		NULL,
		"__show info",
		"Shows frame metadata for debugging purposes.");

	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{44,{flat}}' }'' : unreferenced local function has been removed
#endif
