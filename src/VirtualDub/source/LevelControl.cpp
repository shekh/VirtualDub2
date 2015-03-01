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

#define f_POSITIONCONTROL_CPP

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "oshelper.h"

#include "LevelControl.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY LevelControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

extern const char g_szLevelControlName[]="birdyLevelControl";

#define		MAX_TABS		(8)

////////////////////////////

typedef struct LevelControlData {
	int			iPos[MAX_TABS];
	int			iPixPos[MAX_TABS];
	COLORREF	rgbTab[MAX_TABS];
	int			nTabs;
	int			iMovingTab;
	int			iMovingOffset;
	int			iTabW, iTabH;
	RECT		rColorBand;
	RECT		rTabBand;
	COLORREF	rgbLow, rgbHigh;
	int			w;
} LevelControlData;

////////////////////////////

ATOM RegisterLevelControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= LevelControlWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(LevelControlData *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);	//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= VIDEOLEVELCONTROLCLASS;

	return RegisterClass(&wc);
}

static void LevelControlResize(HWND hwnd, LevelControlData *const lcd) {
	RECT r;

	GetClientRect(hwnd, &r);

	lcd->iTabH		= r.bottom/2;
	lcd->iTabW		= (lcd->iTabH*6+8)/16;

	lcd->rColorBand.left	= r.left+lcd->iTabW;
	lcd->rColorBand.right	= r.right-lcd->iTabW;
	lcd->rColorBand.top		= r.top;
	lcd->rColorBand.bottom	= r.top+lcd->iTabH-1;

	lcd->rTabBand.left		= r.left;
	lcd->rTabBand.right		= r.right;
	lcd->rTabBand.top		= lcd->rColorBand.bottom;
	lcd->rTabBand.bottom	= r.bottom;

	lcd->w					= lcd->rColorBand.right - lcd->rColorBand.left;
}

static int LevelRatioToPixel(int rat, int w) {
	return (w * rat) >> 16;
}

static int LevelPixelToRatio(int pix, int w) {
	return (pix * 65535) / (w-1);
}

static LRESULT APIENTRY LevelControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LevelControlData *lcd = (LevelControlData *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {

	case WM_NCCREATE:
		if (!(lcd = new LevelControlData))
			return FALSE;
		memset(lcd,0,sizeof(LevelControlData));

		lcd->nTabs		= 3;
		lcd->iPos[0]	= 0;
		lcd->rgbTab[0]	= 0xFFFFFF;
		lcd->rgbLow		= 0x000000;
		lcd->rgbHigh	= 0xFFFFFF;
		lcd->iMovingTab	= -1;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)lcd);
		return TRUE;

	case WM_CREATE:
	case WM_SIZE:
		LevelControlResize(hwnd, lcd);
		break;

	case WM_DESTROY:
		delete lcd;
		SetWindowLongPtr(hwnd, 0, 0);
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;
			int i, x, w, h;
			RECT r;

			hdc = BeginPaint(hwnd, &ps);

			w = lcd->w;
			h = lcd->rColorBand.bottom - lcd->rColorBand.top;

			if (RectVisible(hdc, &lcd->rColorBand)) {
				int dr, dg, db;
				int xlo, xhi;
				RECT rClip;

				dr = (signed long)(lcd->rgbHigh&0xff) - (signed long)(lcd->rgbLow&0xff);
				dg = ((signed long)(lcd->rgbHigh&0xff00) - (signed long)(lcd->rgbLow&0xff00))>>8;
				db = ((signed long)(lcd->rgbHigh&0xff0000) - (signed long)(lcd->rgbLow&0xff0000))>>16;

				r = lcd->rColorBand;
				xlo = 0;
				xhi = w;

				i = GetClipBox(hdc, &rClip);

				if (i!=ERROR && i!=NULLREGION) {
					RECT rClip2;

					IntersectRect(&rClip2, &r, &rClip);

					xlo = rClip2.left - r.left;
					xhi = rClip2.right - r.left;
				}

				for(x=xlo; x<xhi; x++) {
					HBRUSH hbr;
					COLORREF cr;

					r.left = lcd->rColorBand.left + x;
					r.right = r.left+1;

					cr	= lcd->rgbLow
						+  ((dr*x + w/2)/w)
						+ (((dg*x + w/2)/w) << 8)
						+ (((db*x + w/2)/w) << 16);

					if (hbr = CreateSolidBrush(cr)) {
						FillRect(hdc, &r, hbr);
						DeleteObject(hbr);
					}
				}
			}

			for(i=0; i<lcd->nTabs; i++) {
				POINT pt[3];
				HBRUSH hbr;
				HGDIOBJ hgoOld;

				pt[0].x = lcd->iPixPos[i];
				pt[1].x = lcd->iPixPos[i]+lcd->iTabW;
				pt[2].x = lcd->iPixPos[i]+2*lcd->iTabW;
				pt[0].y = pt[2].y = lcd->rColorBand.bottom+lcd->iTabH-1;
				pt[1].y = lcd->rColorBand.bottom;

				if (hbr = CreateSolidBrush(lcd->rgbTab[i]))
					hgoOld = SelectObject(hdc, hbr);

				Polygon(hdc, pt, 3);

				if (hbr)
					DeleteObject(SelectObject(hdc, hgoOld));
			}

			EndPaint(hwnd, &ps);
		}
		break;

	case WM_LBUTTONDOWN:
		{
			int x = LOWORD(lParam);
			int y = HIWORD(lParam);

			if (y >= lcd->rColorBand.bottom && y<lcd->rColorBand.bottom+lcd->iTabH) {
				int i;
				int best_tab = -1, best_dx = 100;

				for(i=0; i<lcd->nTabs; i++) {
					int dx = (x-lcd->iTabW) - lcd->iPixPos[i];
					int adx = abs(dx);

					// Pick the closest tab to cursor; if tabs are on the same
					// position, pick the lowest tab if mouse is to the left and
					// the highest tab if mouse is to the right

					if (adx < 8 && (best_tab == -1 || adx < best_dx ||
						(adx == best_dx && dx>0))) {

						best_dx = adx;
						best_tab = i;
					}

				}
				if (best_tab >= 0) {
					SetCapture(hwnd);
					lcd->iMovingTab = best_tab;
					lcd->iMovingOffset = x - lcd->iPixPos[best_tab];
				}
			}
		}
		break;

	case WM_LBUTTONUP:
		if (lcd->iMovingTab>=0) {
			ReleaseCapture();
			lcd->iMovingTab = -1;
		}
		break;

	case WM_MOUSEMOVE:
		if (lcd->iMovingTab>=0) {
			int pos, pixpos;
			RECT r;

			pixpos = (int)(signed short)LOWORD(lParam) - lcd->iMovingOffset;
			pos = LevelPixelToRatio(pixpos, lcd->w);

			// clip position against spectrum

			if (pixpos<0) {
				pos = 0;
				pixpos = 0;
			} else if (pixpos >= lcd->rColorBand.right - lcd->rColorBand.left) {
				pixpos = lcd->rColorBand.right - lcd->rColorBand.left - 1;
				pos = 0xFFFF;
			}

			// don't let tabs pass each other

			if (lcd->iMovingTab>0 && pos < lcd->iPos[lcd->iMovingTab-1]) {
				pos = lcd->iPos[lcd->iMovingTab-1];
				pixpos = lcd->iPixPos[lcd->iMovingTab-1];
			} else if (lcd->iMovingTab < lcd->nTabs-1 && pos > lcd->iPos[lcd->iMovingTab+1]) {
				pos = lcd->iPos[lcd->iMovingTab+1];
				pixpos = lcd->iPixPos[lcd->iMovingTab+1];
			}

			// render changes

			r.left = lcd->iPixPos[lcd->iMovingTab];
			r.right = lcd->iPixPos[lcd->iMovingTab]+2*lcd->iTabW+1;
			r.top = lcd->rColorBand.bottom;
			r.bottom = r.top + lcd->iTabH;

			InvalidateRect(hwnd, &r, TRUE);

			lcd->iPos[lcd->iMovingTab] = pos;
			lcd->iPixPos[lcd->iMovingTab] = pixpos;

			r.left = pixpos;
			r.right = pixpos+2*lcd->iTabW+1;

			InvalidateRect(hwnd, &r, TRUE);

			UpdateWindow(hwnd);

			// send notification message to parent window

			NMVLTABCHANGE nmvltc;

			nmvltc.hdr.code		= VLCN_TABCHANGE;
			nmvltc.hdr.hwndFrom	= hwnd;
			nmvltc.hdr.idFrom	= GetWindowLong(hwnd, GWL_ID);
			nmvltc.iTab			= lcd->iMovingTab;
			nmvltc.iNewPos		= pos;

			SendMessage(GetParent(hwnd), WM_NOTIFY, nmvltc.hdr.idFrom, (LPARAM)&nmvltc);
		}
		break;

	case VLCM_SETTABCOUNT:
		lcd->nTabs = lParam;
		if (wParam)
			InvalidateRect(hwnd, &lcd->rTabBand, TRUE);
		break;

	case VLCM_SETTABCOLOR:
		lcd->rgbTab[LOWORD(wParam)] = lParam;
		if (HIWORD(wParam))
			InvalidateRect(hwnd, &lcd->rTabBand, TRUE);
		break;

	case VLCM_MOVETABPOS:
	case VLCM_SETTABPOS:
		lcd->iPos[LOWORD(wParam)] = lParam;
		lcd->iPixPos[LOWORD(wParam)] = LevelRatioToPixel(lParam, lcd->w);
		if (HIWORD(wParam))
			InvalidateRect(hwnd, &lcd->rTabBand, TRUE);

		if (msg == VLCM_MOVETABPOS) {
			NMVLTABCHANGE nmvltc;

			nmvltc.hdr.code		= VLCN_TABCHANGE;
			nmvltc.hdr.hwndFrom	= hwnd;
			nmvltc.hdr.idFrom	= GetWindowLong(hwnd, GWL_ID);
			nmvltc.iTab			= LOWORD(wParam);
			nmvltc.iNewPos		= lParam;

			SendMessage(GetParent(hwnd), WM_NOTIFY, nmvltc.hdr.idFrom, (LPARAM)&nmvltc);
		}
		break;

	case VLCM_SETGRADIENT:
		lcd->rgbLow = wParam;
		lcd->rgbHigh = lParam;
		InvalidateRect(hwnd, &lcd->rColorBand, FALSE);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return FALSE;
}
