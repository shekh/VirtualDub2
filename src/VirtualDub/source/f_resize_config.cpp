#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>
#include <vd2/system/registry.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/plugin/vdvideofilt.h>
#include "gui.h"
#include "resource.h"

#include "f_resize_config.h"

extern HINSTANCE g_hInst;
extern const char g_szError[];

namespace {
	int revcolor(int c) {
		return ((c>>16)&0xff) | (c&0xff00) | ((c&0xff)<<16);
	}
}

const char *const VDResizeFilterData::kFilterNames[]={
	"Nearest neighbor",
	"Bilinear (interpolation only)",
	"Bicubic (interpolation only)",
	"Precise bilinear",
	"Precise bicubic (A=-0.75)",
	"Precise bicubic (A=-0.60)",
	"Precise bicubic (A=-1.00)",
	"Lanczos3"
};

class VDDataExchangeRegistry {
public:
	VDDataExchangeRegistry(const char *path, bool write);
	~VDDataExchangeRegistry();

	bool WasChangeDetected() const { return mbChangeDetected; }

	void Exchange(const char *name, sint32& value);
	void Exchange(const char *name, uint32& value);
	void Exchange(const char *name, bool& value);
	void Exchange(const char *name, double& value);

protected:
	bool mbWrite;
	bool mbChangeDetected;
	VDRegistryAppKey mKey;
};

VDDataExchangeRegistry::VDDataExchangeRegistry(const char *path, bool write)
	: mbWrite(write)
	, mbChangeDetected(false)
	, mKey(path)
{
}

VDDataExchangeRegistry::~VDDataExchangeRegistry()
{
}

void VDDataExchangeRegistry::Exchange(const char *name, sint32& value) {
	if (mbWrite) {
		mKey.setInt(name, (int)value);
	} else {
		sint32 newValue = (sint32)mKey.getInt(name, (int)value);
		if (newValue != value) {
			value = newValue;
			mbChangeDetected = true;
		}
	}
}

void VDDataExchangeRegistry::Exchange(const char *name, uint32& value) {
	if (mbWrite) {
		mKey.setInt(name, (uint32)value);
	} else {
		uint32 newValue = (uint32)mKey.getInt(name, (int)value);
		if (newValue != value) {
			value = newValue;
			mbChangeDetected = true;
		}
	}
}

void VDDataExchangeRegistry::Exchange(const char *name, bool& value) {
	if (mbWrite) {
		mKey.setInt(name, (uint32)value);
	} else {
		bool newValue = 0 != mKey.getInt(name, (int)value);
		if (newValue != value) {
			value = newValue;
			mbChangeDetected = true;
		}
	}
}

void VDDataExchangeRegistry::Exchange(const char *name, double& value) {
	if (mbWrite) {
		mKey.setString(name, VDswprintf(L"%g", 1, &value).c_str());
	} else {
		VDStringW s;

		if (mKey.getString(name, s)) {
			double newValue;
			char dummy;

			if (1 == swscanf(s.c_str(), L" %lg%C", &newValue, &dummy)) {
				if (newValue != value) {
					value = newValue;
					mbChangeDetected = true;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

VDResizeFilterData::VDResizeFilterData()
	: mImageW(320)
	, mImageH(240)
	, mImageRelW(100)
	, mImageRelH(100)
	, mbUseRelative(true)
	, mImageAspectNumerator(4.0f)
	, mImageAspectDenominator(3.0f)
	, mImageAspectMode(kImageAspectUseSource)
	, mFrameW(320)
	, mFrameH(240)
	, mFrameAspectNumerator(4.0f)
	, mFrameAspectDenominator(3.0f)
	, mFrameMode(kFrameModeNone)
	, mFilterMode(FILTER_TABLEBICUBIC075)
	, mAlignment(1)
	, mFillColor(0)
	, mbInterlaced(false)
{
}

const char *VDResizeFilterData::Validate() const {
	if (mbUseRelative) {
		if (!(mImageW > 0 && mImageW <= 1048576))
			return "The target image width is invalid (not within 1-1048576).";
	} else {
		if (!(mImageRelW > 0.0 && mImageRelW <= 1000000.0))
			return "The target image width is invalid (not within 1-1000000%).";
	}

	switch(mImageAspectMode) {
	case kImageAspectNone:
		if (mbUseRelative) {
			if (!(mImageRelH > 0.0 && mImageRelH <= 1000000.0))
				return "The target image height is invalid (not within 0-1000000%).";
		} else {
			if (!(mImageH > 0 && mImageH <= 1048576))
				return "The target image height is invalid (not within 1-1048576).";
		}
		break;

	case kImageAspectUseSource:
		break;

	case kImageAspectCustom:
		if (!(mImageAspectDenominator >= 0.001 && mImageAspectDenominator < 1000.0)
			|| !(mImageAspectNumerator >= 0.001 && mImageAspectNumerator < 1000.0))
			return "The target image aspect ratio is invalid (values must be within 0.001 to 1000.0).";
		break;

	default:
		return "The target image size mode is invalid.";
	}

	switch(mFrameMode) {
		case kFrameModeNone:
			break;

		case kFrameModeToSize:
			if (!(mFrameW >= 0 && mFrameW < 1048576) || !(mFrameH >= 0 && mFrameH < 1048576))
				return "The target frame size is invalid.";
			break;

		case kFrameModeARCrop:
		case kFrameModeARLetterbox:
			if (!(mFrameAspectDenominator >= 0.001 && mFrameAspectDenominator < 1000.0)
				|| !(mFrameAspectNumerator >= 0.001 && mFrameAspectNumerator < 1000.0))
				return "The target image aspect ratio is invalid (values must be within 0.1 to 10.0).";
			break;

		default:
			return "The target image size mode is invalid.";
	}

	return NULL;
}

void VDResizeFilterData::ComputeSizes(uint32 srcw, uint32 srch, double& imgw, double& imgh, uint32& framew, uint32& frameh, bool useAlignment, bool widthHasPriority, int format) {
	if (mbUseRelative) {
		imgw = srcw * (mImageRelW * (1.0 / 100.0));
		imgh = srch * (mImageRelH * (1.0 / 100.0));
	} else {
		imgw = mImageW;
		imgh = mImageH;
	}

	switch(mImageAspectMode) {
		case kImageAspectNone:
			break;

		case kImageAspectUseSource:
			if (widthHasPriority)
				imgh = imgw * ((double)srch / (double)srcw);
			else
				imgw = imgh * ((double)srcw / (double)srch);
			break;

		case kImageAspectCustom:
			if (widthHasPriority)
				imgh = imgw * (mImageAspectDenominator / mImageAspectNumerator);
			else
				imgw = imgh * (mImageAspectNumerator / mImageAspectDenominator);
			break;
	}

	// clamp image size to prevent excessive ratios
	if (imgw < 0.125f)
		imgw = 0.125f;

	if (imgh < 0.125f)
		imgh = 0.125f;

	double framewf;
	double framehf;

	switch(mFrameMode) {
		case kFrameModeNone:
			framewf = imgw;
			framehf = imgh;
			break;

		case kFrameModeToSize:
			framewf = mFrameW;
			framehf = mFrameH;
			break;

		case kFrameModeARCrop:
			framewf = imgw;
			framehf = imgw * (mFrameAspectDenominator / mFrameAspectNumerator);
			if (framehf > imgh) {
				framewf = imgh * (mFrameAspectNumerator / mFrameAspectDenominator);
				framehf = imgh;
			}
			break;
		case kFrameModeARLetterbox:
			framewf = imgw;
			framehf = imgw * (mFrameAspectDenominator / mFrameAspectNumerator);
			if (framehf < imgh) {
				framewf = imgh * (mFrameAspectNumerator / mFrameAspectDenominator);
				framehf = imgh;
			}
			break;
	}

	framew = (uint32)VDRoundToInt(framewf);
	frameh = (uint32)VDRoundToInt(framehf);

	if (framew < 1)
		framew = 1;
	if (frameh < 1)
		frameh = 1;

	int alignmentH = 1;
	int alignmentW = 1;

	if (format && format < nsVDPixmap::kPixFormat_Max_Standard) {
		const VDPixmapFormatInfo& info = VDPixmapGetInfo(format);

		alignmentW = 1 << (info.qwbits + info.auxwbits);
		alignmentH = 1 << (info.qhbits + info.auxhbits);
	}

	if (useAlignment) {
		if (alignmentH < mAlignment)
			alignmentH = mAlignment;

		if (alignmentW < mAlignment)
			alignmentW = mAlignment;
	}

	// if alignment is present, round frame sizes down to a multiple of alignment.
	if (alignmentW > 1 || alignmentH > 1) {
		framew -= framew % alignmentW;
		frameh -= frameh % alignmentH;

		if (!framew)
			framew = alignmentW;

		if (!frameh)
			frameh = alignmentH;

		// Detect if letterboxing is occurring. If so, we should enlarge the image
		// to fit alignment requirements. We have to enlarge because it will often
		// be impossible to meet alignment requirements in both axes if we shrink.

		if (imgw > 1e-5 && imgh > 1e-5) {
			if (imgw < framew) {
				if (imgh < frameh) {			// Bars all around.

					// If we have bars on all sides, we're basically screwed
					// as there is no way we can expand the image and maintain
					// aspect ratio. So we do nothing.

				} else {						// Bars on left and right only -- expand to horizontal alignment.

					double imginvaspect = imgh / imgw;

					uint32 tmpw = ((uint32)VDRoundToInt(imgw) + alignmentW - 1);
					tmpw -= tmpw % alignmentW;

					imgw = tmpw;
					imgh = imgw * imginvaspect;

				}
			} else if (imgh < frameh) {			// Bars on top and bottom only -- expand to vertical alignment.

				double imgaspect = imgw / imgh;

				uint32 tmph = ((uint32)VDRoundToInt(imgh) + alignmentH - 1);
				tmph -= tmph % alignmentH;

				imgh = tmph;
				imgw = imgh * imgaspect;

			}

			if (!imgw)
				imgw = alignmentW;

			if (!imgh)
				imgh = alignmentH;
		}
	}
}

void VDResizeFilterData::ComputeDestRect(uint32 outw, uint32 outh, double dstw, double dsth) {
	float dx = ((float)outw - (float)dstw) * 0.5f;
	float dy = ((float)outh - (float)dsth) * 0.5f;

	if (mAlignment > 1) {
		if (dx > 0.0) {
			int x1 = VDCeilToInt(dx - 0.5f);
			x1 -= x1 % (int)mAlignment;
			dx = (float)x1;
		}

		if (dy > 0.0) {
			int y1 = VDCeilToInt(dy - 0.5f);
			y1 -= y1 % (int)mAlignment;
			dy = (float)y1;
		}
	}

	mDstRect.set(dx, dy, dx + (float)dstw, dy + (float)dsth);
}

void VDResizeFilterData::UpdateInformativeFields(uint32 srcw, uint32 srch, bool widthHasPriority) {
	double imgw, imgh;
	uint32 framew, frameh;
	ComputeSizes(srcw, srch, imgw, imgh, framew, frameh, false, widthHasPriority, 0);

	if (mbUseRelative) {
		mImageW = imgw;
		mImageH = imgh;
		if (mImageAspectMode != VDResizeFilterData::kImageAspectNone) {
			if (widthHasPriority)
				mImageRelH = 100.0 * imgh / (double)srch;
			else
				mImageRelW = 100.0 * imgw / (double)srcw;
		}
	} else {
		mImageRelW = 100.0 * imgw / (double)srcw;
		mImageRelH = 100.0 * imgh / (double)srch;
		if (mImageAspectMode != VDResizeFilterData::kImageAspectNone) {
			if (widthHasPriority)
				mImageH = imgh;
			else
				mImageW = imgw;
		}
	}
}

bool VDResizeFilterData::ExchangeWithRegistry(bool write) {
	VDDataExchangeRegistry ex("Video filters\\resize", write);

	ex.Exchange("Image width", mImageW);
	ex.Exchange("Image height", mImageH);
	ex.Exchange("Image relative width", mImageRelW);
	ex.Exchange("Image relative height", mImageRelH);
	ex.Exchange("Use relative image size", mbUseRelative);
	ex.Exchange("Image aspect N", mImageAspectNumerator);
	ex.Exchange("Image aspect D", mImageAspectDenominator);
	ex.Exchange("Frame width", mFrameW);
	ex.Exchange("Frame height", mFrameH);
	ex.Exchange("Frame aspect N", mFrameAspectNumerator);
	ex.Exchange("Frame aspect D", mFrameAspectDenominator);
	ex.Exchange("Filter mode", mFilterMode);
	ex.Exchange("Interlaced", mbInterlaced);
	ex.Exchange("Image aspect mode", mImageAspectMode);
	ex.Exchange("Frame mode", mFrameMode);
	ex.Exchange("Codec-friendly alignment", mAlignment);

	return ex.WasChangeDetected();
}

///////////////////////////////////////////////////////////////////////////////

VDCanvasFilterData::VDCanvasFilterData()
	: mFrameW(320)
	, mFrameH(240)
	, mFrameAspectNumerator(4.0f)
	, mFrameAspectDenominator(3.0f)
	, mFrameMode(kFrameModeNone)
	, mFillColor(0)
{
	mAnchorX = 1;
	mAnchorY = 1;
	mFrameDW = 0;
	mFrameDH = 0;
	mFrameX = 0;
	mFrameY = 0;
}

const char *VDCanvasFilterData::Validate() const {
	switch(mFrameMode) {
		case kFrameModeNone:
		case kFrameModeRelative:
			break;

		case kFrameModeToSize:
			if (!(mFrameW >= 0 && mFrameW < 1048576) || !(mFrameH >= 0 && mFrameH < 1048576))
				return "The target frame size is invalid.";
			break;

		case kFrameModeARCrop:
		case kFrameModeARLetterbox:
			if (!(mFrameAspectDenominator >= 0.001 && mFrameAspectDenominator < 1000.0)
				|| !(mFrameAspectNumerator >= 0.001 && mFrameAspectNumerator < 1000.0))
				return "The target image aspect ratio is invalid (values must be within 0.1 to 10.0).";
			break;

		default:
			return "The target image size mode is invalid.";
	}

	return NULL;
}

void VDCanvasFilterData::ComputeSizes(uint32 imgw, uint32 imgh, uint32& framew, uint32& frameh) {
	double framewf;
	double framehf;

	switch(mFrameMode) {
		case kFrameModeNone:
			framewf = imgw;
			framehf = imgh;
			break;

		case kFrameModeToSize:
			framewf = mFrameW;
			framehf = mFrameH;
			break;

		case kFrameModeRelative:
			framewf = imgw + mFrameDW;
			framehf = imgh + mFrameDH;
			if (mAnchorX==1) framewf += mFrameDW;
			if (mAnchorY==1) framehf += mFrameDH;
			if (framewf<0) framewf = 0;
			if (framehf<0) framehf = 0;
			break;

		case kFrameModeARCrop:
			framewf = imgw;
			framehf = imgw * (mFrameAspectDenominator / mFrameAspectNumerator);
			if (framehf > imgh) {
				framewf = imgh * (mFrameAspectNumerator / mFrameAspectDenominator);
				framehf = imgh;
			}
			break;
		case kFrameModeARLetterbox:
			framewf = imgw;
			framehf = imgw * (mFrameAspectDenominator / mFrameAspectNumerator);
			if (framehf < imgh) {
				framewf = imgh * (mFrameAspectNumerator / mFrameAspectDenominator);
				framehf = imgh;
			}
			break;
	}

	framew = (uint32)VDRoundToInt(framewf);
	frameh = (uint32)VDRoundToInt(framehf);
}

void VDCanvasFilterData::ComputeDestRect(uint32 outw, uint32 outh, double dstw, double dsth) {
	float dx = (float)outw - (float)dstw;
	float dy = (float)outh - (float)dsth;
	if (mAnchorX==0) dx = 0;
	if (mAnchorX==1) dx = (float)(dx*0.5);
	if (mAnchorY==0) dy = 0;
	if (mAnchorY==1) dy = (float)(dy*0.5);

	dx += (float)mFrameX;
	dy += (float)mFrameY;

	mDstRect.set(dx, dy, dx + (float)dstw, dy + (float)dsth);
}

///////////////////////////////////////////////////////////////////////////////

class VDDataExchangeDialogW32 {
public:
	VDDataExchangeDialogW32(HWND hdlg, bool write);
	~VDDataExchangeDialogW32();

	bool WasChangeDetected() const { return mbChangeDetected; }
	uint32 GetFirstErrorPos() const { return mErrorPos; }

	void ExchangeEdit(uint32 id, sint32& value);
	void ExchangeEdit(uint32 id, uint32& value);
	void ExchangeEdit(uint32 id, double& value);
	void ExchangeOption(uint32 id, bool& value, bool optionValue);
	void ExchangeOption(uint32 id, uint32& value, uint32 optionValue);
	void ExchangeCombo(uint32 id, uint32& value);
	void ExchangeButton(uint32 id, bool& value);

protected:
	bool mbWrite;
	bool mbChangeDetected;
	uint32 mErrorPos;
	HWND mhdlg;
};

VDDataExchangeDialogW32::VDDataExchangeDialogW32(HWND hdlg, bool write)
	: mbWrite(write)
	, mbChangeDetected(false)
	, mErrorPos(0)
	, mhdlg(hdlg)
{
}

VDDataExchangeDialogW32::~VDDataExchangeDialogW32()
{
}

void VDDataExchangeDialogW32::ExchangeEdit(uint32 id, sint32& value) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		VDSetWindowTextW32(hwnd, VDswprintf(L"%d", 1, &value).c_str());
	} else {
		VDStringW s(VDGetWindowTextW32(hwnd));
		int newValue;
		char dummy;

		if (1 == swscanf(s.c_str(), L" %d%C", &newValue, &dummy)) {
			if (newValue != value) {
				value = (sint32)newValue;
				mbChangeDetected = true;
			}
		} else {
			if (!mErrorPos && !(GetWindowLong(hwnd, GWL_STYLE) & WS_DISABLED))
				mErrorPos = id;
		}
	}
}

void VDDataExchangeDialogW32::ExchangeEdit(uint32 id, uint32& value) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		VDSetWindowTextW32(hwnd, VDswprintf(L"%u", 1, &value).c_str());
	} else {
		VDStringW s(VDGetWindowTextW32(hwnd));
		unsigned newValue;
		char dummy;

		if (1 == swscanf(s.c_str(), L" %u%C", &newValue, &dummy)) {
			if (newValue != value) {
				value = (uint32)newValue;
				mbChangeDetected = true;
			}
		} else {
			if (!mErrorPos && !(GetWindowLong(hwnd, GWL_STYLE) & WS_DISABLED))
				mErrorPos = id;
		}
	}
}

void VDDataExchangeDialogW32::ExchangeEdit(uint32 id, double& value) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		VDSetWindowTextW32(hwnd, VDswprintf(L"%g", 1, &value).c_str());
	} else {
		VDStringW s(VDGetWindowTextW32(hwnd));
		double newValue;
		char dummy;

		if (1 == swscanf(s.c_str(), L" %lg%C", &newValue, &dummy)) {
			if (newValue != value) {
				value = newValue;
				mbChangeDetected = true;
			}
		} else {
			if (!mErrorPos && !(GetWindowLong(hwnd, GWL_STYLE) & WS_DISABLED))
				mErrorPos = id;
		}
	}
}

void VDDataExchangeDialogW32::ExchangeOption(uint32 id, bool& value, bool optionValue) {
	uint32 v = value;

	ExchangeOption(id, v, optionValue);

	value = (v != 0);
}

void VDDataExchangeDialogW32::ExchangeOption(uint32 id, uint32& value, uint32 optionValue) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		SendMessage(hwnd, BM_SETCHECK, (value == optionValue) ? BST_CHECKED : BST_UNCHECKED, 0);
	} else {
		if (value != optionValue) {
			if (SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
				mbChangeDetected = true;
				value = optionValue;
			}
		}
	}
}

void VDDataExchangeDialogW32::ExchangeCombo(uint32 id, uint32& value) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		SendMessage(hwnd, CB_SETCURSEL, value, 0);
	} else {
		int result = (int)SendMessage(hwnd, CB_GETCURSEL, 0, 0);

		if (result != CB_ERR && (uint32)result != value) {
			mbChangeDetected = true;
			value = (uint32)result;
		}
	}
}

void VDDataExchangeDialogW32::ExchangeButton(uint32 id, bool& value) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		if (!mErrorPos)
			mErrorPos = id;
		return;
	}

	if (mbWrite) {
		SendMessage(hwnd, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
	} else {
		bool result = (SendMessage(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED);

		if (result != value) {
			mbChangeDetected = true;
			value = result;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

namespace {
	void VDSetDlgItemFloatW32(HWND hdlg, UINT id, double v) {
		char buf[512];

		sprintf(buf, "%g", v);
		SetDlgItemText(hdlg, id, buf);
	}

	double VDGetDlgItemFloatW32(HWND hdlg, UINT id, BOOL *success) {
		char buf[512];

		*success = FALSE;

		if (GetDlgItemText(hdlg, id, buf, sizeof buf)) {
			double v;
			if (1 == sscanf(buf, " %lg", &v)) {
				*success = TRUE;
				return v;
			}
		}

		return 0;
	}
};

class VDVF1ResizeDlg : public VDDialogBaseW32 {
public:
	VDVF1ResizeDlg(VDResizeFilterData& config, IVDXFilterPreview2 *ifp, uint32 w, uint32 h);

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void InitDialog();
	uint32 ExchangeWithDialog(bool write);
	void UpdateDialogAbsoluteSize();
	void UpdateDialogRelativeSize();
	void UpdateEnables();
	void UpdatePreview();
	bool ApplyChanges();
	void MarkDirty();

	VDResizeFilterData& mConfig;
	VDResizeFilterData mNewConfig;
	bool		mbConfigDirty;
	bool		mbWidthPriority;
	HBRUSH		mhbrColor;
	IVDXFilterPreview2 *mifp;
	uint32		mWidth;
	uint32		mHeight;
	int			mRecursionLock;
};

VDVF1ResizeDlg::VDVF1ResizeDlg(VDResizeFilterData& config, IVDXFilterPreview2 *ifp, uint32 w, uint32 h)
	: VDDialogBaseW32(IDD_FILTER_RESIZE)
	, mConfig(config)
	, mbConfigDirty(false)
	, mbWidthPriority(true)
	, mhbrColor(NULL)
	, mifp(ifp)
	, mWidth(w)
	, mHeight(h)
	, mRecursionLock(0)
{
}

INT_PTR VDVF1ResizeDlg::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		InitDialog();
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_APPLY:
			ApplyChanges();
			return TRUE;

		case IDC_PREVIEW:
			if (mifp->IsPreviewDisplayed()) {
				EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), FALSE);
			} else {
				if (mbConfigDirty) {
					if (!ApplyChanges())
						return TRUE;
				}
			}
			mifp->Toggle((VDXHWND)mhdlg);
			return TRUE;

		case IDOK:
			if (uint32 badid = ExchangeWithDialog(false)) {
				SetFocus(GetDlgItem(mhdlg, badid));
				MessageBeep(MB_ICONERROR);
			} else if (const char *err = mNewConfig.Validate()) {
				MessageBox(mhdlg, err, g_szError, MB_ICONERROR | MB_OK);
			} else {
				mifp->Close();
				mConfig = mNewConfig;
				End(true);
			}
			return TRUE;

		case IDCANCEL:
			mifp->Close();
			End(false);
			return TRUE;

		case IDC_SAVE_AS_DEFAULT:
			if (uint32 badid = ExchangeWithDialog(false)) {
				SetFocus(GetDlgItem(mhdlg, badid));
				MessageBeep(MB_ICONERROR);
			} else if (const char *err = mNewConfig.Validate())
				MessageBox(mhdlg, err, g_szError, MB_ICONERROR | MB_OK);
			else
				mNewConfig.ExchangeWithRegistry(true);
			return TRUE;

		case IDC_WIDTH:
		case IDC_HEIGHT:
			if (!mRecursionLock && (HIWORD(wParam) == EN_KILLFOCUS || HIWORD(wParam) == EN_CHANGE)) {
				if (HIWORD(wParam) == EN_CHANGE) {
					mbWidthPriority = (LOWORD(wParam) == IDC_WIDTH);
					++mRecursionLock;
					CheckDlgButton(mhdlg, IDC_SIZE_ABSOLUTE, BST_CHECKED);
					CheckDlgButton(mhdlg, IDC_SIZE_RELATIVE, BST_UNCHECKED);
					--mRecursionLock;
				}
				ExchangeWithDialog(false);
			}
			break;

		case IDC_RELWIDTH:
		case IDC_RELHEIGHT:
			if (!mRecursionLock && (HIWORD(wParam) == EN_KILLFOCUS || HIWORD(wParam) == EN_CHANGE)) {
				if (HIWORD(wParam) == EN_CHANGE) {
					mbWidthPriority = (LOWORD(wParam) == IDC_RELWIDTH);
					++mRecursionLock;
					CheckDlgButton(mhdlg, IDC_SIZE_ABSOLUTE, BST_UNCHECKED);
					CheckDlgButton(mhdlg, IDC_SIZE_RELATIVE, BST_CHECKED);
					--mRecursionLock;
				}
				ExchangeWithDialog(false);
			}
			break;

		case IDC_FRAMEWIDTH:
		case IDC_FRAMEHEIGHT:
		case IDC_ASPECT_RATIO1:
		case IDC_ASPECT_RATIO2:
		case IDC_FRAME_ASPECT1:
		case IDC_FRAME_ASPECT2:
			if (!mRecursionLock && (HIWORD(wParam) == EN_KILLFOCUS || HIWORD(wParam) == EN_CHANGE)) {
				ExchangeWithDialog(false);
			}
			return TRUE;

		case IDC_FILTER_MODE:
			if (!mRecursionLock && HIWORD(wParam) == CBN_SELCHANGE) {
				ExchangeWithDialog(false);
			}
			return TRUE;

		case IDC_SIZE_ABSOLUTE:
		case IDC_SIZE_RELATIVE:
		case IDC_INTERLACED:
		case IDC_AR_NONE:
		case IDC_AR_USE_SOURCE:
		case IDC_AR_USE_RATIO:
		case IDC_FRAME_NONE:
		case IDC_FRAME_TO_SIZE:
		case IDC_FRAME_AR_CROP:
		case IDC_FRAME_AR_LETTERBOX:
		case IDC_LETTERBOX:
		case IDC_ALIGNMENT_1:
		case IDC_ALIGNMENT_2:
		case IDC_ALIGNMENT_4:
		case IDC_ALIGNMENT_8:
		case IDC_ALIGNMENT_16:
			if (!mRecursionLock && HIWORD(wParam) == BN_CLICKED) {
				ExchangeWithDialog(false);
			}
			return TRUE;

		case IDC_PICKCOLOR:
			{
				COLORREF rgbColor = revcolor(mNewConfig.mFillColor);

				if (guiChooseColor(mhdlg, rgbColor)) {
					mNewConfig.mFillColor = revcolor(rgbColor);

					DeleteObject(mhbrColor);
					mhbrColor = CreateSolidBrush(rgbColor);
					RedrawWindow(GetDlgItem(mhdlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
					MarkDirty();
				}
			}
			break;
		}
		break;

	case WM_CTLCOLORSTATIC:
		if (GetWindowLong((HWND)lParam, GWL_ID) == IDC_COLOR)
			return (INT_PTR)mhbrColor;
		break;

	case WM_DESTROY:
		if (mhbrColor) {
			DeleteObject(mhbrColor);
			mhbrColor = NULL;
		}

		break;
	}
	return FALSE;
}

void VDVF1ResizeDlg::InitDialog() {
	HWND hwndItem = GetDlgItem(mhdlg, IDC_FILTER_MODE);
	for(int i=0; i<VDResizeFilterData::FILTER_COUNT; i++)
		SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)VDResizeFilterData::kFilterNames[i]);

	if (mWidth && mHeight)
		mConfig.UpdateInformativeFields(mWidth, mHeight, true);

	mNewConfig = mConfig;
	ExchangeWithDialog(true);

	mbConfigDirty = false;

	UpdateEnables();

	mhbrColor = CreateSolidBrush(revcolor(mConfig.mFillColor));
	mifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));

	UINT id = IDC_WIDTH;
	if (mConfig.mbUseRelative)
		id = IDC_RELWIDTH;

	hwndItem = GetDlgItem(mhdlg, id);
	if (hwndItem) {
		SetFocus(hwndItem);
		SendMessage(hwndItem, EM_SETSEL, 0, -1);
	}
}

uint32 VDVF1ResizeDlg::ExchangeWithDialog(bool write) {
	VDDataExchangeDialogW32 ex(mhdlg, write);

	++mRecursionLock;
	ex.ExchangeOption(IDC_SIZE_ABSOLUTE, mNewConfig.mbUseRelative, false);
	ex.ExchangeOption(IDC_SIZE_RELATIVE, mNewConfig.mbUseRelative, true);

	bool doboth = mNewConfig.mImageAspectMode == VDResizeFilterData::kImageAspectNone || write;
	bool dowidth = doboth || mbWidthPriority;
	bool doheight = doboth || !mbWidthPriority;

	if (write || mNewConfig.mbUseRelative) {
		if (dowidth)
			ex.ExchangeEdit(IDC_RELWIDTH, mNewConfig.mImageRelW);
		if (doheight)
			ex.ExchangeEdit(IDC_RELHEIGHT, mNewConfig.mImageRelH);
	}
	
	if (write || !mNewConfig.mbUseRelative) {
		if (dowidth)
			ex.ExchangeEdit(IDC_WIDTH, mNewConfig.mImageW);
		if (doheight)
			ex.ExchangeEdit(IDC_HEIGHT, mNewConfig.mImageH);
	}

	ex.ExchangeEdit(IDC_ASPECT_RATIO1, mNewConfig.mImageAspectNumerator);
	ex.ExchangeEdit(IDC_ASPECT_RATIO2, mNewConfig.mImageAspectDenominator);
	ex.ExchangeEdit(IDC_FRAMEWIDTH, mNewConfig.mFrameW);
	ex.ExchangeEdit(IDC_FRAMEHEIGHT, mNewConfig.mFrameH);
	ex.ExchangeEdit(IDC_FRAME_ASPECT1, mNewConfig.mFrameAspectNumerator);
	ex.ExchangeEdit(IDC_FRAME_ASPECT2, mNewConfig.mFrameAspectDenominator);
	ex.ExchangeCombo(IDC_FILTER_MODE, mNewConfig.mFilterMode);
	ex.ExchangeButton(IDC_INTERLACED, mNewConfig.mbInterlaced);
	ex.ExchangeOption(IDC_AR_NONE, mNewConfig.mImageAspectMode, VDResizeFilterData::kImageAspectNone);
	ex.ExchangeOption(IDC_AR_USE_SOURCE, mNewConfig.mImageAspectMode, VDResizeFilterData::kImageAspectUseSource);
	ex.ExchangeOption(IDC_AR_USE_RATIO, mNewConfig.mImageAspectMode, VDResizeFilterData::kImageAspectCustom);
	ex.ExchangeOption(IDC_FRAME_NONE, mNewConfig.mFrameMode, VDResizeFilterData::kFrameModeNone);
	ex.ExchangeOption(IDC_FRAME_TO_SIZE, mNewConfig.mFrameMode, VDResizeFilterData::kFrameModeToSize);
	ex.ExchangeOption(IDC_FRAME_AR_CROP, mNewConfig.mFrameMode, VDResizeFilterData::kFrameModeARCrop);
	ex.ExchangeOption(IDC_FRAME_AR_LETTERBOX, mNewConfig.mFrameMode, VDResizeFilterData::kFrameModeARLetterbox);
	ex.ExchangeOption(IDC_ALIGNMENT_1, mNewConfig.mAlignment, 1);
	ex.ExchangeOption(IDC_ALIGNMENT_2, mNewConfig.mAlignment, 2);
	ex.ExchangeOption(IDC_ALIGNMENT_4, mNewConfig.mAlignment, 4);
	ex.ExchangeOption(IDC_ALIGNMENT_8, mNewConfig.mAlignment, 8);
	ex.ExchangeOption(IDC_ALIGNMENT_16, mNewConfig.mAlignment, 16);
	--mRecursionLock;

	uint32 error = ex.GetFirstErrorPos();
	bool changeDetected = ex.WasChangeDetected();

	if (changeDetected) {
		UpdateEnables();

		if (!write && !error && mWidth && mHeight) {
			VDDataExchangeDialogW32 wex(mhdlg, true);

			mNewConfig.UpdateInformativeFields(mWidth, mHeight, mbWidthPriority);

			++mRecursionLock;
			if (mNewConfig.mbUseRelative) {
				wex.ExchangeEdit(IDC_WIDTH, mNewConfig.mImageW);
				wex.ExchangeEdit(IDC_HEIGHT, mNewConfig.mImageH);
				if (mNewConfig.mImageAspectMode != VDResizeFilterData::kImageAspectNone) {
					if (mbWidthPriority)
						wex.ExchangeEdit(IDC_RELHEIGHT, mNewConfig.mImageRelH);
					else
						wex.ExchangeEdit(IDC_RELWIDTH, mNewConfig.mImageRelW);
				}
			} else {
				wex.ExchangeEdit(IDC_RELWIDTH, mNewConfig.mImageRelW);
				wex.ExchangeEdit(IDC_RELHEIGHT, mNewConfig.mImageRelH);
				if (mNewConfig.mImageAspectMode != VDResizeFilterData::kImageAspectNone) {
					if (mbWidthPriority)
						wex.ExchangeEdit(IDC_HEIGHT, mNewConfig.mImageH);
					else
						wex.ExchangeEdit(IDC_WIDTH, mNewConfig.mImageW);
				}
			}
			--mRecursionLock;
		}
	}

	if (changeDetected || error) {
		MarkDirty();
	}

	return error;
}

void VDVF1ResizeDlg::UpdateEnables() {
	EnableWindow(GetDlgItem(mhdlg, IDC_ASPECT_RATIO1), mNewConfig.mImageAspectMode == VDResizeFilterData::kImageAspectCustom);
	EnableWindow(GetDlgItem(mhdlg, IDC_ASPECT_RATIO2), mNewConfig.mImageAspectMode == VDResizeFilterData::kImageAspectCustom);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAMEWIDTH), mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeToSize);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAMEHEIGHT), mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeToSize);

	bool frameToAspect = (mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeARCrop) || (mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeARLetterbox);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAME_ASPECT1), frameToAspect);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAME_ASPECT2), frameToAspect);

	bool letterbox = (mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeToSize) || (mNewConfig.mFrameMode == VDResizeFilterData::kFrameModeARLetterbox);
	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_FILLCOLOR), letterbox);
	EnableWindow(GetDlgItem(mhdlg, IDC_COLOR), letterbox);
	EnableWindow(GetDlgItem(mhdlg, IDC_PICKCOLOR), letterbox);
}

void VDVF1ResizeDlg::UpdatePreview() {
	mifp->RedoSystem();
}

bool VDVF1ResizeDlg::ApplyChanges() {
	if (!mbConfigDirty)
		return false;

	if (uint32 badid = ExchangeWithDialog(false)) {
		SetFocus(GetDlgItem(mhdlg, badid));
		MessageBeep(MB_ICONERROR);
		return false;
	}

	mbConfigDirty = false;
	mifp->UndoSystem();

	mConfig = mNewConfig;
	UpdatePreview();
	EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), FALSE);
	return true;
}

void VDVF1ResizeDlg::MarkDirty() {
	if (!mbConfigDirty) {
		mbConfigDirty = true;

		if (mifp->IsPreviewDisplayed())
			EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), TRUE);
	}
}

bool VDFilterResizeActivateConfigDialog(VDResizeFilterData& mfd, IVDXFilterPreview2 *ifp2, uint32 w, uint32 h, VDGUIHandle hParent) {
	VDVF1ResizeDlg dlg(mfd, ifp2, w, h);

	return dlg.ActivateDialogDual(hParent) != 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////

class VDVFCanvasDlg : public VDDialogBaseW32 {
public:
	VDVFCanvasDlg(VDCanvasFilterData& config, IVDXFilterPreview2 *ifp, uint32 w, uint32 h);

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void InitDialog();
	uint32 ExchangeWithDialog(bool write, bool changeMode);
	void UpdateDialogAbsoluteSize();
	void UpdateDialogRelativeSize();
	void UpdateEnables();
	void UpdatePreview();
	void ApplyAnchor(int x, int y);
	bool ApplyChanges();
	void MarkDirty();

	VDCanvasFilterData& mConfig;
	VDCanvasFilterData mNewConfig;
	bool		mbConfigDirty;
	HBRUSH		mhbrColor;
	IVDXFilterPreview2 *mifp;
	uint32		mWidth;
	uint32		mHeight;
	int			mRecursionLock;
};

VDVFCanvasDlg::VDVFCanvasDlg(VDCanvasFilterData& config, IVDXFilterPreview2 *ifp, uint32 w, uint32 h)
	: VDDialogBaseW32(IDD_FILTER_CANVAS)
	, mConfig(config)
	, mbConfigDirty(false)
	, mhbrColor(NULL)
	, mifp(ifp)
	, mWidth(w)
	, mHeight(h)
	, mRecursionLock(0)
{
}

INT_PTR VDVFCanvasDlg::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		InitDialog();
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_APPLY:
			ApplyChanges();
			return TRUE;

		case IDC_PREVIEW:
			if (mifp->IsPreviewDisplayed()) {
				EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), FALSE);
			} else {
				if (mbConfigDirty) {
					if (!ApplyChanges())
						return TRUE;
				}
			}
			mifp->Toggle((VDXHWND)mhdlg);
			return TRUE;

		case IDOK:
			if (uint32 badid = ExchangeWithDialog(false, false)) {
				SetFocus(GetDlgItem(mhdlg, badid));
				MessageBeep(MB_ICONERROR);
			} else if (const char *err = mNewConfig.Validate()) {
				MessageBox(mhdlg, err, g_szError, MB_ICONERROR | MB_OK);
			} else {
				mifp->Close();
				mConfig = mNewConfig;
				End(true);
			}
			return TRUE;

		case IDCANCEL:
			mifp->Close();
			End(false);
			return TRUE;

		case IDC_FRAMEWIDTH:
		case IDC_FRAMEHEIGHT:
		case IDC_FRAME_ASPECT1:
		case IDC_FRAME_ASPECT2:
		case IDC_XPOS:
		case IDC_YPOS:
			if (!mRecursionLock && (HIWORD(wParam) == EN_KILLFOCUS || HIWORD(wParam) == EN_CHANGE)) {
				ExchangeWithDialog(false, false);
			}
			return TRUE;

		case IDC_FRAME_NONE:
		case IDC_FRAME_TO_SIZE:
		case IDC_FRAME_AR_CROP:
		case IDC_FRAME_AR_LETTERBOX:
		case IDC_FRAME_RELATIVE:
			if (!mRecursionLock && HIWORD(wParam) == BN_CLICKED) {
				ExchangeWithDialog(false, true);
			}
			return TRUE;

		case IDC_PICKCOLOR:
			{
				COLORREF rgbColor = revcolor(mNewConfig.mFillColor);

				if (guiChooseColor(mhdlg, rgbColor)) {
					mNewConfig.mFillColor = revcolor(rgbColor);

					DeleteObject(mhbrColor);
					mhbrColor = CreateSolidBrush(rgbColor);
					RedrawWindow(GetDlgItem(mhdlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
					MarkDirty();
				}
			}
			break;

		case IDC_DIR_TOPLEFT:				ApplyAnchor(0,0); return true;
		case IDC_DIR_TOPCENTER:			ApplyAnchor(1,0); return true;
		case IDC_DIR_TOPRIGHT:			ApplyAnchor(2,0); return true;
		case IDC_DIR_MIDDLELEFT:		ApplyAnchor(0,1); return true;
		case IDC_DIR_MIDDLECENTER:	ApplyAnchor(1,1); return true;
		case IDC_DIR_MIDDLERIGHT:		ApplyAnchor(2,1); return true;
		case IDC_DIR_BOTTOMLEFT:		ApplyAnchor(0,2); return true;
		case IDC_DIR_BOTTOMCENTER:	ApplyAnchor(1,2); return true;
		case IDC_DIR_BOTTOMRIGHT:		ApplyAnchor(2,2); return true;
		}
		break;

	case WM_CTLCOLORSTATIC:
		if (GetWindowLong((HWND)lParam, GWL_ID) == IDC_COLOR)
			return (INT_PTR)mhbrColor;
		break;

	case WM_DESTROY:
		if (mhbrColor) {
			DeleteObject(mhbrColor);
			mhbrColor = NULL;
		}

		break;
	}
	return FALSE;
}

void VDVFCanvasDlg::InitDialog() {
	mNewConfig = mConfig;
	ExchangeWithDialog(true, false);

	mbConfigDirty = false;

	if (mConfig.mFrameMode==VDCanvasFilterData::kFrameModeNone) {
		mNewConfig.mFrameMode = VDCanvasFilterData::kFrameModeToSize;
		mNewConfig.mFrameW = mWidth;
		mNewConfig.mFrameH = mHeight;
		ExchangeWithDialog(true, false);
	}

	UpdateEnables();

	mhbrColor = CreateSolidBrush(revcolor(mConfig.mFillColor));
	mifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));

	UINT id = IDC_FRAMEWIDTH;
	if (mConfig.mFrameMode==mConfig.kFrameModeARCrop)
		id = IDC_FRAME_ASPECT1;
	if (mConfig.mFrameMode==mConfig.kFrameModeARLetterbox)
		id = IDC_FRAME_ASPECT1;

	HWND hwndItem = GetDlgItem(mhdlg, id);
	if (hwndItem) {
		SetFocus(hwndItem);
		SendMessage(hwndItem, EM_SETSEL, 0, -1);
	}

	SendDlgItemMessage(mhdlg, IDC_SPIN_XOFFSET, UDM_SETRANGE, 0, MAKELONG((short)-(UD_MINVAL-1/2), (short)+(UD_MINVAL-1/2)));
	SendDlgItemMessage(mhdlg, IDC_SPIN_YOFFSET, UDM_SETRANGE, 0, MAKELONG((short)+(UD_MINVAL-1/2), (short)-(UD_MINVAL-1/2)));

	static const uint32 idbypos[3][3]={
		IDC_DIR_TOPLEFT,
		IDC_DIR_TOPCENTER,
		IDC_DIR_TOPRIGHT,
		IDC_DIR_MIDDLELEFT,
		IDC_DIR_MIDDLECENTER,
		IDC_DIR_MIDDLERIGHT,
		IDC_DIR_BOTTOMLEFT,
		IDC_DIR_BOTTOMCENTER,
		IDC_DIR_BOTTOMRIGHT
	};

	CheckDlgButton(mhdlg, idbypos[mConfig.mAnchorY][mConfig.mAnchorX], true);
}

uint32 VDVFCanvasDlg::ExchangeWithDialog(bool write, bool changeMode) {
	VDDataExchangeDialogW32 ex(mhdlg, write);

	++mRecursionLock;
	if (!changeMode) {
		if (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeToSize) {
			ex.ExchangeEdit(IDC_FRAMEWIDTH, mNewConfig.mFrameW);
			ex.ExchangeEdit(IDC_FRAMEHEIGHT, mNewConfig.mFrameH);
		}
		if (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeRelative) {
			ex.ExchangeEdit(IDC_FRAMEWIDTH, mNewConfig.mFrameDW);
			ex.ExchangeEdit(IDC_FRAMEHEIGHT, mNewConfig.mFrameDH);
		}
	}
	ex.ExchangeEdit(IDC_XPOS, mNewConfig.mFrameX);
	ex.ExchangeEdit(IDC_YPOS, mNewConfig.mFrameY);
	ex.ExchangeEdit(IDC_FRAME_ASPECT1, mNewConfig.mFrameAspectNumerator);
	ex.ExchangeEdit(IDC_FRAME_ASPECT2, mNewConfig.mFrameAspectDenominator);
	ex.ExchangeOption(IDC_FRAME_TO_SIZE, mNewConfig.mFrameMode, VDCanvasFilterData::kFrameModeToSize);
	ex.ExchangeOption(IDC_FRAME_AR_CROP, mNewConfig.mFrameMode, VDCanvasFilterData::kFrameModeARCrop);
	ex.ExchangeOption(IDC_FRAME_AR_LETTERBOX, mNewConfig.mFrameMode, VDCanvasFilterData::kFrameModeARLetterbox);
	ex.ExchangeOption(IDC_FRAME_RELATIVE, mNewConfig.mFrameMode, VDCanvasFilterData::kFrameModeRelative);
	--mRecursionLock;

	uint32 error = ex.GetFirstErrorPos();
	bool changeDetected = ex.WasChangeDetected();

	if (changeDetected) {
		UpdateEnables();

		if (changeMode) {
			VDDataExchangeDialogW32 wex(mhdlg, true);
			++mRecursionLock;
			if (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeToSize) {
				wex.ExchangeEdit(IDC_FRAMEWIDTH, mNewConfig.mFrameW);
				wex.ExchangeEdit(IDC_FRAMEHEIGHT, mNewConfig.mFrameH);
			}
			if (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeRelative) {
				wex.ExchangeEdit(IDC_FRAMEWIDTH, mNewConfig.mFrameDW);
				wex.ExchangeEdit(IDC_FRAMEHEIGHT, mNewConfig.mFrameDH);
			}
			--mRecursionLock;
		}
	}

	if (changeDetected || error) {
		MarkDirty();
	}

	return error;
}

void VDVFCanvasDlg::UpdateEnables() {
	bool frameToWH = (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeToSize) || (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeRelative);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAMEWIDTH), frameToWH);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAMEHEIGHT), frameToWH);

	bool frameToAspect = (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeARCrop) || (mNewConfig.mFrameMode == VDCanvasFilterData::kFrameModeARLetterbox);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAME_ASPECT1), frameToAspect);
	EnableWindow(GetDlgItem(mhdlg, IDC_FRAME_ASPECT2), frameToAspect);
}

void VDVFCanvasDlg::UpdatePreview() {
	mifp->RedoSystem();
}

void VDVFCanvasDlg::ApplyAnchor(int x, int y) {
	mNewConfig.mAnchorX = x;
	mNewConfig.mAnchorY = y;
	mNewConfig.mFrameX = 0;
	mNewConfig.mFrameY = 0;
	ExchangeWithDialog(true, false);
	MarkDirty();
}

bool VDVFCanvasDlg::ApplyChanges() {
	if (!mbConfigDirty)
		return false;

	if (uint32 badid = ExchangeWithDialog(false, false)) {
		SetFocus(GetDlgItem(mhdlg, badid));
		MessageBeep(MB_ICONERROR);
		return false;
	}

	mbConfigDirty = false;
	mifp->UndoSystem();

	mConfig = mNewConfig;
	UpdatePreview();
	EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), FALSE);
	return true;
}

void VDVFCanvasDlg::MarkDirty() {
	if (!mbConfigDirty) {
		mbConfigDirty = true;

		if (mifp->IsPreviewDisplayed())
			EnableWindow(GetDlgItem(mhdlg, IDC_APPLY), TRUE);
	}
}

bool VDFilterCanvasActivateConfigDialog(VDCanvasFilterData& mfd, IVDXFilterPreview2 *ifp2, uint32 w, uint32 h, VDGUIHandle hParent) {
	VDVFCanvasDlg dlg(mfd, ifp2, w, h);

	return dlg.ActivateDialogDual(hParent) != 0;
}
