#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"

class VDUIShutdownDialog : public VDDialogFrameW32 {
public:
	VDUIShutdownDialog();

protected:
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	bool OnLoaded();
	bool OnTimer();

	int mPos;
};

VDUIShutdownDialog::VDUIShutdownDialog()
	: VDDialogFrameW32(IDD_JOBFINISH)
{
}

VDZINT_PTR VDUIShutdownDialog::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_TIMER:
			return OnTimer();
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

bool VDUIShutdownDialog::OnLoaded() {
	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_SETRANGE, TRUE, MAKELONG(0, 40));
	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_SETSTEP, 1, 0);
	SetTimer(mhdlg, 1, 250, NULL);

	mPos = 0;

	return VDDialogFrameW32::OnLoaded();
}

bool VDUIShutdownDialog::OnTimer() {
	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_STEPIT, 0, 0);

	if (++mPos >= 40)
		End(true);

	return true;
}

bool VDUIRequestSystemShutdown(VDGUIHandle hParent) {
	VDUIShutdownDialog dlg;

	return dlg.ShowDialog(hParent) != 0;
}
