#include "stdafx.h"
#include <vd2/system/w32assist.h>
#include <vd2/Dita/accel.h>

void VDUIExtractAccelerator(VDAccelTableEntry& ent, const ACCEL& accel) {
	ent.mCommandId = accel.cmd;

	ent.mAccel.mVirtKey = accel.key;
	ent.mAccel.mModifiers = 0;

	if (!(accel.fVirt & FVIRTKEY))
		ent.mAccel.mModifiers |= VDUIAccelerator::kAscii;

	if (accel.fVirt & FNOINVERT)
		ent.mAccel.mModifiers |= VDUIAccelerator::kNoinvert;

	if (accel.fVirt & FALT)
		ent.mAccel.mModifiers |= VDUIAccelerator::kModAlt;

	if (accel.fVirt & FCONTROL)
		ent.mAccel.mModifiers |= VDUIAccelerator::kModCtrl;

	if (accel.fVirt & FSHIFT)
		ent.mAccel.mModifiers |= VDUIAccelerator::kModShift;

	switch(accel.key) {
		case VK_INSERT:
		case VK_DELETE:
		case VK_HOME:
		case VK_END:
		case VK_NEXT:
		case VK_PRIOR:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
			ent.mAccel.mModifiers |= VDUIAccelerator::kModExtended;
			break;
	}
}

void VDUIExtractAcceleratorTableW32(VDAccelTableDefinition& dst, HACCEL haccel, const VDAccelToCommandEntry *pCommands, uint32 nCommands) {
	uint32 n = CopyAcceleratorTable(haccel, NULL, 0);
	vdfastvector<ACCEL> accels(n);

	n = CopyAcceleratorTable(haccel, accels.data(), n);

	dst.Clear();
	for(uint32 i=0; i<n; ++i) {
		const ACCEL& accel = accels[i];

		bool found = false;
		for(uint32 j=0; j<nCommands; ++j) {
			if (pCommands[j].mId == accel.cmd) {
				VDAccelTableEntry ent;
				VDUIExtractAccelerator(ent,accel);
				ent.mpCommand = pCommands[j].mpName;

				dst.Add(ent);
				found = true;
				break;
			}
		}

		VDASSERT(found);
	}
}

void VDUIMergeAcceleratorTableW32(VDAccelTableDefinition& dst, HACCEL haccel, const int *pCommands, uint32 nCommands, VDAccelTableDefinition& src) {
	uint32 n = CopyAcceleratorTable(haccel, NULL, 0);
	vdfastvector<ACCEL> accels(n);

	n = CopyAcceleratorTable(haccel, accels.data(), n);

	dst.Clear();

	{for(uint32 i=0; i<n; ++i) {
		const ACCEL& accel = accels[i];
		VDAccelTableEntry ent;
		VDUIExtractAccelerator(ent,accel);
		ent.mpCommand = "";
		dst.Add(ent);
	}}

	{for(uint32 i=0; i<nCommands; ++i) {
		int cmd = pCommands[i];

		bool found = false;
		for(uint32 j=0; j<src.GetSize(); ++j) {
			if (src[j].mCommandId == cmd) {
				dst.Add(src[j]);
				found = true;
			}
		}

		VDASSERT(found);
	}}
}

void VDUIGetAcceleratorStringInternal(const VDUIAccelerator& accel, VDStringW& s) {
	union {
		wchar_t w[1024];
		char a[1024];
	} buf;

	UINT scanCode = MapVirtualKey(accel.mVirtKey, 0);
	if (!scanCode)
		return;

	LPARAM lParam = (scanCode << 16) | (1 << 25);

	if (accel.mModifiers & VDUIAccelerator::kModExtended)
		lParam |= (1 << 24);

	if (VDIsWindowsNT()) {
		if (GetKeyNameTextW(lParam, buf.w, 1024))
			s.append(buf.w);
	} else {
		if (GetKeyNameTextA(lParam, buf.a, 1024))
			s.append(VDTextAToW(buf.a));
	}
}

void VDUIGetAcceleratorString(const VDUIAccelerator& accel, VDStringW& s) {
	s.clear();

	if (accel.mModifiers & VDUIAccelerator::kModCtrl) {
		VDUIAccelerator accelCtrl;
		accelCtrl.mVirtKey = VK_CONTROL;
		accelCtrl.mModifiers = 0;
		VDUIGetAcceleratorStringInternal(accelCtrl, s);

		s += L"+";
	}

	if (accel.mModifiers & VDUIAccelerator::kModShift) {
		VDUIAccelerator accelShift;
		accelShift.mVirtKey = VK_SHIFT;
		accelShift.mModifiers = 0;
		VDUIGetAcceleratorStringInternal(accelShift, s);

		s += L"+";
	}

	if (accel.mModifiers & VDUIAccelerator::kModAlt) {
		VDUIAccelerator accelAlt;
		accelAlt.mVirtKey = VK_MENU;
		accelAlt.mModifiers = 0;
		VDUIGetAcceleratorStringInternal(accelAlt, s);

		s += L"+";
	}

	VDUIGetAcceleratorStringInternal(accel, s);
}

HACCEL VDUIBuildAcceleratorTableW32(const VDAccelTableDefinition& def) {
	uint32 n = def.GetSize();
	vdfastvector<ACCEL> accels(n);

	for(size_t i=0; i<n; ++i) {
		const VDAccelTableEntry& entry = def[i];

		ACCEL& accel = accels[i];

		accel.fVirt = 0;

		if (!(entry.mAccel.mModifiers & VDUIAccelerator::kAscii))
			accel.fVirt |= FVIRTKEY;

		if (entry.mAccel.mModifiers & VDUIAccelerator::kNoinvert)
			accel.fVirt |= FNOINVERT;

		if (entry.mAccel.mModifiers & VDUIAccelerator::kModCtrl)
			accel.fVirt |= FCONTROL;

		if (entry.mAccel.mModifiers & VDUIAccelerator::kModShift)
			accel.fVirt |= FSHIFT;

		if (entry.mAccel.mModifiers & VDUIAccelerator::kModAlt)
			accel.fVirt |= FALT;

		accel.key = (WORD)entry.mAccel.mVirtKey;
		accel.cmd = (WORD)entry.mCommandId;
	}

	return CreateAcceleratorTable(accels.data(), n);
}

void VDUIUpdateMenuAcceleratorsW32(HMENU hmenu, const VDAccelTableDefinition& def) {
	int n = GetMenuItemCount(hmenu);

	VDStringA bufa;
	VDStringW keystr;
	for(int i=0; i<n; ++i) {
		MENUITEMINFOA miia;

		miia.cbSize		= sizeof(MENUITEMINFOA);
		miia.fMask		= MIIM_ID | MIIM_SUBMENU | MIIM_FTYPE;
		miia.dwTypeData	= NULL;
		miia.cch		= 0;

		if (GetMenuItemInfoA(hmenu, i, TRUE, &miia)) {
			if (miia.hSubMenu) {
				VDUIUpdateMenuAcceleratorsW32(miia.hSubMenu, def);
			} else {
				const uint32 id = miia.wID;

				miia.fMask		= MIIM_STRING;
				miia.dwTypeData = NULL;
				miia.cch		= 0;
				if (GetMenuItemInfoA(hmenu, i, TRUE, &miia)) {
					++miia.cch;
					bufa.resize(miia.cch);
		
					miia.dwTypeData	= (LPSTR)bufa.data();

					if (GetMenuItemInfoA(hmenu, i, TRUE, &miia)) {
						VDStringA::size_type pos = bufa.find('\t');
						if (pos != VDStringA::npos)
							bufa.resize(pos);
						else
							bufa.resize(miia.cch);

						const uint32 m = def.GetSize();
						for(uint32 j=0; j<m; ++j) {
							const VDAccelTableEntry& ent = def[j];

							if (ent.mCommandId == id) {
								VDUIGetAcceleratorString(ent.mAccel, keystr);
					
								bufa.push_back('\t');
								bufa.append(VDTextWToA(keystr).c_str());
								break;
							}
						}

						miia.fMask = MIIM_STRING;
						miia.dwTypeData = (LPSTR)bufa.c_str();
						VDVERIFY(SetMenuItemInfoA(hmenu, i, TRUE, &miia));
					}
				}
			}
		}
	}
}
