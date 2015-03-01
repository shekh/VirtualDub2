//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"

#include <windows.h>
#include <vd2/system/registry.h>
#include "resource.h"

// There are some truly lame people out there who would try to rip off
// an open source program by hacking the binary!

#if _MSC_VER >= 1400
	#pragma optimize("s", on)
#else
	#pragma optimize("as", on)
#endif

extern const unsigned char fht_tab[];

typedef unsigned char byte;

class Licensor {
public:
	Licensor(HWND, bool conditional);

	static INT_PTR APIENTRY DProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	static void depack(HWND hDlg);
};

// Wouldn't be fun without some good old-fashioned string expansion and LZ77 encoding.

void Licensor::depack(HWND hDlg) {
	unsigned char depackbuf[15073], textbuf[18317];
	HRSRC hRSRC;

	if (hRSRC = FindResource(NULL, (LPSTR)IDR_VIRUS, "STUFF")) {
		HGLOBAL hGlobal;
		if (hGlobal = LoadResource(NULL, hRSRC)) {
			LPVOID lpData;

			if (lpData = LockResource(hGlobal)) {
				byte *s = (byte *)lpData;
				byte *t = depackbuf;
				int sum = 0;
				unsigned long code = 0;

				while(sum != 0xfffff609) {
					if (!(code >> 24)) {
						code = *s++ | 0xff000000;
					}

					if (code & 0x80) {
						sum += *t++ = *s++;
						sum = ~sum;
					} else {
						int off = *(unsigned short *)s;
						int count = (off>>12)+3;
						off &= 0xfff;
						s += 2;

						do {
							sum += *t = t[off - 4096];
							sum = ~sum;
							++t;
						} while(--count);
					}

					code += code;
				}

				s = depackbuf;
				t = textbuf;

				while(t != textbuf + 18316) {
					if (*s >= 0xa0) {
						*t++ = ' ';
						*t++ = (char)(*s & 0x7f);
					} else if (*s >= 0x80)
						while((*s)-->=0x80)
							*t++ = ' ';
					else {
						if (*s == '\n')
							*t++ = '\r';
						*t++ = *s;
					}
					++s;
				}
				*t = 0;

				SendMessage(GetDlgItem(hDlg, IDC_LICENSE), WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), MAKELPARAM(TRUE, 0));
				SendMessage(GetDlgItem(hDlg, IDC_LICENSE), WM_SETTEXT, 0, (LPARAM)textbuf);
				FreeResource(hGlobal);
				return;
			}
			FreeResource(hGlobal);
		}
	}
	EndDialog(hDlg, 0);
}

INT_PTR APIENTRY Licensor::DProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			depack(hDlg);

			// Force visible even if we were started with nCmdShow == SW_HIDE.
			ShowWindow(hDlg, SW_SHOWNA);
            return TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
                EndDialog(hDlg, 1);  
                return TRUE;
            }
            break;
    }
    return FALSE;
}

Licensor::Licensor(HWND hwndParent, bool conditional) {
	if (conditional) {
		char str[11];
		int i;

		for(i=0; i<10; i++)
			str[i] = (char)(fht_tab[i]^0xaa);
		str[i] = 0;

		VDRegistryAppKey key;
		if (key.getInt(str, 0))
			return;

		key.setInt(str, 1);
	}

	if (!DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_LICENSE), hwndParent, DProc))
		throw new int('budv');
}

void VDDisplayLicense(HWND hwndParent, bool conditional) {
	Licensor l(hwndParent, conditional);
}
