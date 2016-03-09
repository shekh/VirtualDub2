#include "stdafx.h"
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "FilterChainDesc.h"
#include "FilterFrameVideoSource.h"
#include "FilterSystem.h"
#include "ClippingControl.h"
#include "PositionControl.h"
#include "filters.h"
#include "resource.h"
#include "gui.h"
#include "command.h"
#include "VideoSource.h"

class VDFilterClippingDialog : public VDDialogFrameW32 {
public:
	VDFilterClippingDialog(FilterInstance *pFiltInst, VDFilterChainDesc *pFilterChainDesc, sint64 initialTimeUS);

protected:
	void OnDataExchange(bool write);
	bool OnLoaded();

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void UpdateFrame(VDPosition pos);

	VDFilterChainDesc *mpFilterChainDesc;
	FilterInstance	*mpFilterInst;
	FilterSystem	mFilterSys;
	IVDClippingControl	*mpClipCtrl;
	IVDPositionControl	*mpPosCtrl;

	int mPreCropW;
	int mPreCropH;
	int mMinSizeW;
	int mMinSizeH;

	sint64			mInitialTimeUS;

	double			mFilterFramesToSourceFrames;
	double			mSourceFramesToFilterFrames;

	vdrefptr<VDFilterFrameVideoSource>	mpFrameSource;

	VDDialogResizerW32	mResizer;
};

VDFilterClippingDialog::VDFilterClippingDialog(FilterInstance *pFiltInst, VDFilterChainDesc *pFilterChainDesc, sint64 initialTimeUS)
	: VDDialogFrameW32(IDD_FILTER_CLIPPING)
	, mpFilterChainDesc(pFilterChainDesc)
	, mpFilterInst(pFiltInst)
	, mMinSizeW(0)
	, mMinSizeH(0)
	, mInitialTimeUS(initialTimeUS)
{
}

void VDFilterClippingDialog::OnDataExchange(bool write) {
	if (write) {
		vdrect32 r;
		mpClipCtrl->GetClipBounds(r);

		mpFilterInst->SetCrop(r.left, r.top, r.right, r.bottom, IsButtonChecked(IDC_CROP_PRECISE));
	} else {
		const vdrect32& r = mpFilterInst->GetCropInsets();
		mpClipCtrl->SetClipBounds(r);

		bool precise = mpFilterInst->IsPreciseCroppingEnabled();
		CheckButton(IDC_CROP_PRECISE, precise);
		CheckButton(IDC_CROP_FAST, !precise);
	}
}

bool VDFilterClippingDialog::OnLoaded()  {
	RECT rw, rc;

	VDSetDialogDefaultIcons(mhdlg);

	// try to init filters
	mFilterFramesToSourceFrames = 1.0;
	mSourceFramesToFilterFrames = 1.0;

	mPreCropW = 320;
	mPreCropH = 240;

	if (mpFilterChainDesc && inputVideo) {
		IVDStreamSource *pVSS = inputVideo->asStream();
		const VDPixmap& px = inputVideo->getTargetFormat();

		try {
			// halt the main filter system
			filters.DeinitFilters();
			filters.DeallocateBuffers();

			mFilterSys.prepareLinearChain(
					mpFilterChainDesc,
					px.w,
					px.h,
					px,
					pVSS->getRate(),
					pVSS->getLength(),
					inputVideo->getPixelAspectRatio());

			mpFrameSource = new VDFilterFrameVideoSource;
			mpFrameSource->Init(inputVideo, mFilterSys.GetInputLayout());

			// start private filter system
			mFilterSys.initLinearChain(
					NULL,
					VDXFilterStateInfo::kStatePreview,
					mpFilterChainDesc,
					mpFrameSource,
					px.w,
					abs(px.h),
					px,
					px.palette,
					pVSS->getRate(),
					pVSS->getLength(),
					inputVideo->getPixelAspectRatio());

			mFilterSys.ReadyFilters();

			double srcRate = pVSS->getRate().asDouble();
			double dstRate = mpFilterInst->GetSourceDesc().mFrameRate.asDouble();

			mFilterFramesToSourceFrames = srcRate / dstRate;
			mSourceFramesToFilterFrames = dstRate / srcRate;

			mPreCropW = mpFilterInst->mPrepareInfo.mStreams[0].mExternalSrc.w;
			mPreCropH = mpFilterInst->mPrepareInfo.mStreams[0].mExternalSrc.h;
		} catch(const MyError&) {
			// eat the error
		}
	}

	HWND hwndClipping = GetDlgItem(mhdlg, IDC_BORDERS);
	mpClipCtrl = VDGetIClippingControl((VDGUIHandle)hwndClipping);

	mpClipCtrl->SetBitmapSize(mPreCropW, mPreCropH);

	const VDFilterStreamDesc srcDesc(mpFilterInst->GetSourceDesc());

	mpPosCtrl = VDGetIPositionControlFromClippingControl((VDGUIHandle)hwndClipping);
	mpPosCtrl->SetAutoStep(true);
	mpPosCtrl->SetRange(0, srcDesc.mFrameCount < 0 ? 1000 : srcDesc.mFrameCount);
	mpPosCtrl->SetFrameRate(srcDesc.mFrameRate);

	if (mFilterSys.isRunning()) {
		const VDFraction& dstfr = mFilterSys.GetOutputFrameRate();
		VDPosition timelineFrame = VDRoundToInt64(dstfr.asDouble() * (double)mInitialTimeUS * (1.0 / 1000000.0));

		IVDFilterFrameSource *src = mpFilterInst->GetSource(0);
		if (src) {
			VDPosition localFrame = mFilterSys.GetSymbolicFrame(timelineFrame, src);

			if (localFrame >= 0)
				mpPosCtrl->SetPosition(localFrame);
		}
	}

	GetWindowRect(mhdlg, &rw);
	GetWindowRect(hwndClipping, &rc);
	const int origW = rw.right - rw.left;
	const int origH = rw.bottom - rw.top;
	int padW = (rw.right - rw.left) - (rc.right - rc.left);
	int padH = origH - (rc.bottom - rc.top);

	mResizer.Init(mhdlg);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_STATIC_YCCCROP, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_CROP_PRECISE, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_CROP_FAST, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_BORDERS, VDDialogResizerW32::kMC);

	mpClipCtrl->AutoSize(padW, padH);

	GetWindowRect(hwndClipping, &rc);
	MapWindowPoints(NULL, mhdlg, (LPPOINT)&rc, 2);

	int newH = std::max<int>(origH, (rc.bottom - rc.top) + padH);
	int newW = std::max<int>(origW, (rc.right - rc.left) + padW);

	SetWindowPos(hwndClipping, NULL, 0, 0, newW - padW, newH - padH, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	if (origW != newW || origH != newH)
		SetWindowPos(mhdlg, NULL, 0, 0, newW, newH, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);

	mMinSizeW = newW;
	mMinSizeH = newH;

	SendMessage(mhdlg, DM_REPOSITION, 0, 0);

	// render first frame
	UpdateFrame(mpPosCtrl->GetPosition());

	return VDDialogFrameW32::OnLoaded();
}

INT_PTR VDFilterClippingDialog::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_BORDERS:
				{
					sint64 pos = -1;

					if (inputVideo) {
						switch(HIWORD(wParam)) {
							case PCN_KEYPREV:
								{
									pos = mpPosCtrl->GetPosition();
									sint64 srcPos = VDFloorToInt64(((double)pos + 0.5) * mFilterFramesToSourceFrames);

									for(;;) {
										sint64 newSrcPos = inputVideo->prevKey(srcPos);

										if (newSrcPos < 0) {
											pos = 0;
											break;
										}

										sint64 newPos = VDFloorToInt64(((double)newSrcPos + 0.5) * mSourceFramesToFilterFrames);
										if (pos != newPos) {
											pos = newPos;
											break;
										}

										srcPos = newSrcPos;
									}

									mpPosCtrl->SetPosition(pos);
								}
								break;
							case PCN_KEYNEXT:
								{
									pos = mpPosCtrl->GetPosition();
									sint64 srcPos = VDFloorToInt64(((double)pos + 0.5) * mFilterFramesToSourceFrames);

									for(;;) {
										sint64 newSrcPos = inputVideo->nextKey(srcPos);

										if (newSrcPos < 0) {
											pos = mpPosCtrl->GetRangeEnd();
											break;
										}

										sint64 newPos = VDFloorToInt64(((double)newSrcPos + 0.5) * mSourceFramesToFilterFrames);
										if (pos != newPos) {
											pos = newPos;
											break;
										}

										srcPos = newSrcPos;
									}

									mpPosCtrl->SetPosition(pos);
								}
								break;
						}

						UpdateFrame(mpPosCtrl->GetPosition());
					}
				}
				return TRUE;
			}
            break;

		case WM_NOTIFY:
			if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_BORDERS) {
				VDPosition pos = guiPositionHandleNotify(lParam, mpPosCtrl);

				if (pos >= 0)
					UpdateFrame(pos);
			}
			break;

		case WM_MOUSEWHEEL:
			// Windows forwards all mouse wheel messages down to us, which we then forward
			// to the clipping control.  Obviously for this to be safe the position control
			// MUST eat the message, which it currently does.
			{
				HWND hwndClipping = GetDlgItem(mhdlg, IDC_BORDERS);
				if (hwndClipping)
					return SendMessage(hwndClipping, WM_MOUSEWHEEL, wParam, lParam);
			}
			break;

		case WM_SIZE:
			mResizer.Relayout();
			return 0;

		case WM_GETMINMAXINFO:
			{
				MINMAXINFO& mmi = *(MINMAXINFO *)lParam;

				if (mmi.ptMinTrackSize.x < mMinSizeW)
					mmi.ptMinTrackSize.x = mMinSizeW;

				if (mmi.ptMinTrackSize.y < mMinSizeH)
					mmi.ptMinTrackSize.y = mMinSizeH;
			}
			return TRUE;
    }

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

void VDFilterClippingDialog::UpdateFrame(VDPosition pos) {
	if (mFilterSys.isRunning()) {
		bool success = false;

		sint64 frameCount = mFilterSys.GetOutputFrameCount();
		if (pos >= 0 && pos < frameCount) {
			try {
				vdrefptr<IVDFilterFrameClientRequest> req;

				IVDFilterFrameSource *src = mpFilterInst->GetSource(0);
				const VDPixmapLayout& srcLayout = src->GetOutputLayout();

				if (src->CreateRequest(pos, false, 0, ~req)) {
					bool canBlock = false;
					do {
						if (IVDFilterFrameSource::kRunResult_Running == mpFrameSource->RunRequests(NULL)) {
							canBlock = false;
							continue;
						}

						if (FilterSystem::kRunResult_Running == mFilterSys.Run(NULL, false)) {
							canBlock = false;
						} else {
							if (canBlock) {
								mFilterSys.Block();
								canBlock = false;
							} else {
								canBlock = true;
							}
						}
					} while(!req->IsCompleted());

					if (req->IsSuccessful()) {
						VDFilterFrameBuffer *buf = req->GetResultBuffer();
						VDPixmap px(VDPixmapFromLayout(srcLayout, (void *)buf->LockRead()));
						px.info = buf->info;
						mpClipCtrl->BlitFrame(&px);
						buf->Unlock();
					} else {
						mpClipCtrl->BlitFrame(NULL);
					}

					success = true;
				}
			} catch(const MyError&) {
				// eat the error
			}
		}

		if (!success)
			mpClipCtrl->BlitFrame(NULL);
	} else
		guiPositionBlit(GetDlgItem(mhdlg, IDC_BORDERS), pos, mPreCropW, mPreCropH);
}

bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, VDFilterChainDesc *pFilterChainDesc, sint64 initialTimeUS) {
	VDFilterClippingDialog dlg(pFiltInst, pFilterChainDesc, initialTimeUS);

	return 0 != dlg.ShowDialog(hParent);
}
