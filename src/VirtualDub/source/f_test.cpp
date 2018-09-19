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
	#include <windows.h>
	#include <vd2/system/memory.h>
	#include <vd2/VDXFrame/VideoFilter.h>
	#include <vd2/Kasumi/text.h>
	#include <vd2/Kasumi/region.h>
	#include <vd2/VDLib/Dialog.h>
	#include "resource.h"

	struct VDVideoFilterTestConfig {
		bool	mbSwapBuffers;
		bool	mbSourceHDC;
		bool	mbDestHDC;
	};

	class VDVideoFilterTestDialog : public VDDialogFrameW32 {
	public:
		VDVideoFilterTestDialog(VDVideoFilterTestConfig& config);

	protected:
		void OnDataExchange(bool write);

		VDVideoFilterTestConfig& mConfig;
	};

	VDVideoFilterTestDialog::VDVideoFilterTestDialog(VDVideoFilterTestConfig& config)
		: VDDialogFrameW32(IDD_FILTER_TEST)
		, mConfig(config)
	{
	}

	void VDVideoFilterTestDialog::OnDataExchange(bool write) {
		ExchangeControlValueBoolCheckbox(write, IDC_SWAP_BUFFERS, mConfig.mbSwapBuffers);
		ExchangeControlValueBoolCheckbox(write, IDC_SOURCE_HDC, mConfig.mbSourceHDC);
		ExchangeControlValueBoolCheckbox(write, IDC_DEST_HDC, mConfig.mbDestHDC);
	}

	class VDVideoFilterTest : public VDXVideoFilter {
	public:
		bool Configure(VDXHWND hwnd);
		uint32 GetParams();
		void Start();
		void Run();

	protected:
		VDStringA		mTextLine;
		VDPixmapRegion	mTextRegion;
		VDPixmapRegion	mShadowRegion;
		VDPixmapPathRasterizer	mRasterizer;
		VDPixmapRegion	mShadowBrush;

		VDVideoFilterTestConfig	mConfig;
	};

	bool VDVideoFilterTest::Configure(VDXHWND hwnd) {
		VDVideoFilterTestDialog dlg(mConfig);

		return dlg.ShowDialog((VDGUIHandle)hwnd) != 0;
	}

	uint32 VDVideoFilterTest::GetParams() {
		if (mConfig.mbSourceHDC)
			fa->src.dwFlags |= VDXFBitmap::NEEDS_HDC;

		if (mConfig.mbDestHDC)
			fa->dst.dwFlags |= VDXFBitmap::NEEDS_HDC;

		return mConfig.mbSwapBuffers ? FILTERPARAM_SWAP_BUFFERS : 0;
	}

	void VDVideoFilterTest::Start() {
		VDPixmapCreateRoundRegion(mShadowBrush, 16.0f);
	}

	void VDVideoFilterTest::Run() {
		const VDXFBitmap& src = *fa->mpSourceFrames[0];
		const VDXFBitmap& dst = *fa->mpOutputFrames[0];

		if (mConfig.mbSwapBuffers) {
			if (mConfig.mbSourceHDC && mConfig.mbDestHDC) {
				::BitBlt((HDC)dst.hdc, 0, 0, dst.w, dst.h, (HDC)src.hdc, 0, 0, NOTSRCCOPY);
			} else {
				VDMemcpyRect(dst.data, dst.pitch, src.data, src.pitch, dst.w, dst.h);
			}
		}

		if (mConfig.mbDestHDC) {
			int sdc = ::SaveDC((HDC)dst.hdc);

			if (sdc) {
				::SetTextAlign((HDC)dst.hdc, TA_TOP | TA_LEFT);
				::SetBkMode((HDC)dst.hdc, TRANSPARENT);

				static const char kGDIText[]="Drawing with dest HDC";

				::SetTextColor((HDC)dst.hdc, RGB(0, 0, 0));
				for(int i=0; i<9; ++i) {
					if (i == 4)
						continue;

					::TextOut((HDC)dst.hdc, 10 + (i % 3), 10 + (i / 3), kGDIText, sizeof kGDIText - 1);
				}

				::SetTextColor((HDC)dst.hdc, RGB(255, 0, 255));
				::TextOut((HDC)dst.hdc, 11, 11, kGDIText, sizeof kGDIText - 1);
				::RestoreDC((HDC)dst.hdc, sdc);
			}
		}

		::GdiFlush();
		const VDXPixmap& pxdst = *dst.mpPixmap;

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

		const float size = 8.0f * 8.0f * 20.0f;

		float y = 40.0f;
		mTextLine = "Drawing directly into pixmap";

		mRasterizer.Clear();
		VDPixmapConvertTextToPath(mRasterizer, NULL, size, 8.0f * 8.0f * 10.0f, 8.0f * 8.0f * y, mTextLine.c_str());		
		mRasterizer.ScanConvert(mTextRegion);
		VDPixmapConvolveRegion(mShadowRegion, mTextRegion, mShadowBrush);
		VDPixmapFillPixmapAntialiased8x(pxdst2, mShadowRegion, 0, 0, 0xFF000000);
		VDPixmapFillPixmapAntialiased8x(pxdst2, mTextRegion, 0, 0, 0xFFFFFF00);
	}

	extern const VDXFilterDefinition filterDef_test = VDXVideoFilterDefinition<VDVideoFilterTest>(
		NULL,
		"__test",
		"Tests various filter system features (debug only).");

	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{44,{flat}}' }'' : unreferenced local function has been removed
#endif
