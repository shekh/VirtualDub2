//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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

#ifndef f_VD2_DITA_CONTROLS_H
#define f_VD2_DITA_CONTROLS_H

class IVDUIWindow;

extern IVDUIWindow *VDCreateUISplitter();
extern IVDUIWindow *VDCreateUISplitSet();
extern IVDUIWindow *VDCreateUISplitBar();
extern IVDUIWindow *VDCreateUISet();
extern IVDUIWindow *VDCreateUIPageSet();
extern IVDUIWindow *VDCreateUIGrid();
extern IVDUIWindow *VDCreateUILabel();
extern IVDUIWindow *VDCreateUINumericLabel();
extern IVDUIWindow *VDCreateUITextEdit();
extern IVDUIWindow *VDCreateUITextArea();
extern IVDUIWindow *VDCreateUIComboBox();
extern IVDUIWindow *VDCreateUIListBox();
extern IVDUIWindow *VDCreateUIListView();
extern IVDUIWindow *VDCreateUIBaseWindow();
extern IVDUIWindow *VDCreateUIWindow();
extern IVDUIWindow *VDCreateUIButton();
extern IVDUIWindow *VDCreateUIGroup();
extern IVDUIWindow *VDCreateUICheckbox();
extern IVDUIWindow *VDCreateUIOption();
extern IVDUIWindow *VDCreateUITrackbar();
extern IVDUIWindow *VDCreateUIHotkey();

#endif
