//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include <vfw.h>

#include "resource.h"
#include "vbitmap.h"
#include "auxdlg.h"
#include "oshelper.h"

#include <vd2/system/w32assist.h>

extern "C" unsigned long version_num;
extern "C" char version_time[];

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

static HDC g_hdcAboutDisplay;
static HBITMAP g_hbmAboutDisplay;
static HGDIOBJ g_hgoAboutDisplay;
static BITMAPINFOHEADER g_bihAboutDisplay;
static void *g_pvAboutDisplay;
static void *g_pvAboutDisplayBack;
static VBitmap g_vbAboutSrc, g_vbAboutDst;

///////////////////////////////////////////////////////////////////////////

struct TriPt {
	float x, y;
	float u, v;
};

typedef unsigned short Pixel16;

// Triangle setup based on Chris Hecker's GDM article on texture mapping.
// We define pixel centers on the display to be at integers.

void RenderTriangle(Pixel16 *dst, long dstpitch, Pixel16 *tex, TriPt *pt1, TriPt *pt2, TriPt *pt3, uint32 s) {
	TriPt *pt, *pl, *pr;

	// Find top point

	if (pt1->y < pt2->y)		// 1 < 2
		if (pt1->y < pt3->y) {	// 1 < 2,3
			pt = pt1;
			pr = pt2;
			pl = pt3;
		} else {				// 3 < 1 < 2
			pt = pt3;
			pr = pt1;
			pl = pt2;
		}
	else						// 2 < 1
		if (pt2->y < pt3->y) {	// 2 < 1,3
			pt = pt2;
			pr = pt3;
			pl = pt1;
		} else {				// 3 < 2 < 1
			pt = pt3;
			pr = pt1;
			pl = pt2;
		}

	if (pl->y == pt->y && pt->y == pr->y)
		return;

	// Compute gradients

	float A;
	float one_over_A;
	float dudx, dvdx;
	float dudy, dvdy;
	uint32 dudxi, dvdxi;

	A = (pt->y - pl->y) * (pr->x - pl->x) - (pt->x - pl->x) * (pr->y - pl->y);
	one_over_A = 1.0f / A;
	dudx = ((pr->u - pl->u) * (pt->y - pl->y) - (pt->u - pl->u) * (pr->y - pl->y)) * one_over_A;
	dvdx = ((pr->v - pl->v) * (pt->y - pl->y) - (pt->v - pl->v) * (pr->y - pl->y)) * one_over_A;
	dudy = ((pt->u - pl->u) * (pr->x - pl->x) - (pr->u - pl->u) * (pt->x - pl->x)) * one_over_A;
	dvdy = ((pt->v - pl->v) * (pr->x - pl->x) - (pr->v - pl->v) * (pt->x - pl->x)) * one_over_A;

	dudxi = (int)(dudx * 16777216.0f) << 3;
	dvdxi = (int)(dvdx * 16777216.0f) << 3;
	
	// Compute edge walking parameters

	float dxl1=0, dxr1=0, dul1=0, dvl1=0;
	float dxl2=0, dxr2=0, dul2=0, dvl2=0;

	// Compute left-edge interpolation parameters for first half.

	if (pl->y != pt->y) {
		dxl1 = (pl->x - pt->x) / (pl->y - pt->y);

		dul1 = dudy + dxl1 * dudx;
		dvl1 = dvdy + dxl1 * dvdx;
	}

	// Compute right-edge interpolation parameters for first half.

	if (pr->y != pt->y) {
		dxr1 = (pr->x - pt->x) / (pr->y - pt->y);
	}

	// Reject backfaces.

	if (dxl1 >= dxr1)
		return;

	// Compute third-edge interpolation parameters.

	if (pr->y != pl->y) {
		dxl2 = (pr->x - pl->x) / (pr->y - pl->y);

		dul2 = dudy + dxl2 * dudx;
		dvl2 = dvdy + dxl2 * dvdx;

		dxr2 = dxl2;
	}

	// Initialize parameters for first half.
	//
	// We place pixel centers at (x+0.5, y+0.5).

	float xl, xr, ul, vl, yf;
	int y, y1, y2;

	// y_start < y+0.5 to include pixel y.

	y = (int)floor(pt->y + 0.5f);
	yf = (y+0.5f) - pt->y;

	xl = pt->x + dxl1 * yf;
	xr = pt->x + dxr1 * yf;
	ul = pt->u + dul1 * yf;
	vl = pt->v + dvl1 * yf;

	// Initialize parameters for second half.

	float xl2, xr2, ul2, vl2;

	if (pl->y > pr->y) {		// Left edge is long side
		dxl2 = dxl1;
		dul2 = dul1;
		dvl2 = dvl1;

		y1 = (int)floor(pr->y + 0.5f);
		y2 = (int)floor(pl->y + 0.5f);

		yf = (y1+0.5f) - pr->y;

		// Step left edge.

		xl2 = xl + dxl1 * (y1 - y);
		ul2 = ul + dul1 * (y1 - y);
		vl2 = vl + dvl1 * (y1 - y);

		// Prestep right edge.

		xr2 = pr->x + dxr2 * yf;
	} else {					// Right edge is long side
		dxr2 = dxr1;

		y1 = (int)floor(pl->y + 0.5f);
		y2 = (int)floor(pr->y + 0.5f);

		yf = (y1+0.5f) - pl->y;

		// Prestep left edge.

		xl2 = pl->x + dxl2 * yf;
		ul2 = pl->u + dul2 * yf;
		vl2 = pl->v + dvl2 * yf;

		// Step right edge.

		xr2 = xr + dxr1 * (y1 - y);
	}

	// Rasterize!

	int u_correct=0, v_correct=0;

	if (dudx < 0)			u_correct = -1;
	else if (dudx > 0)		u_correct = 0;
	else if (dul1 < 0)		u_correct = -1;

	if (dvdx < 0)			v_correct = -1;
	else if (dvdx > 0)		v_correct = 0;
	else if (dvl1 < 0)		v_correct = -1;

	s = (s>>3) + (s>>7);

/*	if (y < 0)
		y = 0;

	if (y2 > 160)
		y2 = 160;*/

	dst += dstpitch * y;

	while(y < y2) {
		if (y == y1) {
			xl = xl2;
			xr = xr2;
			ul = ul2;
			vl = vl2;
			dxl1 = dxl2;
			dxr1 = dxr2;
			dul1 = dul2;
			dvl1 = dvl2;
		}

		int x1, x2;
		float xf;
		unsigned u, v;

		// x_left must be less than (x+0.5) to include pixel x.

		x1		= (int)floor(xl + 0.5f);
		x2		= (int)floor(xr + 0.5f);
		xf		= (x1+0.5f) - xl;
		
		u		= ((int)((ul + xf * dudx)*16777216.0f) << 3) + u_correct;
		v		= ((int)((vl + xf * dvdx)*16777216.0f) << 3) + v_correct;

		while(x1 < x2) {
			uint32 A = tex[(u>>27) + (v>>27)*32];
			uint32 B = dst[x1];

			uint32 p0 = (((A^B ) & 0x7bde)>>1) + (A&B);
			uint32 p  = (((A^p0) & 0x7bde)>>1) + (A&B);
			dst[x1] = (uint16)(((((p&0x007c1f)*s + 0x004010) & 0x0f83e0) + (((p&0x0003e0)*s + 0x000200) & 0x007c00)) >> 5);

			++x1;

			u += dudxi;
			v += dvdxi;
		}

		dst += dstpitch;
		xl += dxl1;
		xr += dxr1;
		ul += dul1;
		vl += dvl1;

		++y;
	}
}

///////////////////////////////////////////////////////////////////////////

#pragma optimize("t", off)
#pragma optimize("s", on)

static BOOL CALLBACK HideAllButOKCANCELProc(HWND hwnd, LPARAM lParam) {
	UINT id = GetWindowLong(hwnd, GWL_ID);

	if (id != IDOK && id != IDCANCEL)
		ShowWindow(hwnd, SW_HIDE);

	return TRUE;
}

static void CALLBACK AboutTimerProc(UINT uID, UINT, DWORD_PTR dwUser, DWORD_PTR, DWORD_PTR) {
	PostMessage((HWND)dwUser, WM_APP+0, 0, 0);
}

static void AboutSetCompilerBuild(HWND hwnd) {
	VDStringW s(VDGetWindowTextW32(hwnd));
	VDSubstituteStrings(s);
	VDSetWindowTextW32(hwnd, s.c_str());
}

INT_PTR APIENTRY AboutDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool bTimerSet;
	static bool bRender;
	static MMRESULT mmTimer;
	static Pixel16 *tex;
	static RECT rBounce;
	static RECT rDirtyLast;
	static int xpos, ypos, xvel, yvel;
	static float cubeside;
	static float vx[8][3];
	static const int faces[6][4] = {
		{ 0, 1, 4, 5 },
		{ 2, 6, 3, 7 },
		{ 0, 4, 2, 6 },
		{ 1, 3, 5, 7 },
		{ 0, 2, 1, 3 },
		{ 4, 5, 6, 7 },
	};

    switch (message)
    {
        case WM_INITDIALOG:
			{
				char buf[128];

				AboutSetCompilerBuild(GetDlgItem(hDlg, IDC_STATIC_VERSION));

				wsprintf(buf, "Build %d/"
#ifdef _DEBUG
					"debug"
#else
					"release"
#endif
					" (%s)", version_num, version_time);

				SetDlgItemText(hDlg, IDC_FINALS_SUCK, buf);

				HRSRC hrsrc;

				if (hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_CREDITS), "STUFF")) {
					HGLOBAL hGlobal;
					if (hGlobal = LoadResource(NULL, hrsrc)) {
						const char *pData, *pLimit;

						if (pData = (const char *)LockResource(hGlobal)) {
							HWND hwndItem = GetDlgItem(hDlg, IDC_CREDITS);
							const INT tab = 80;

							pLimit = pData + SizeofResource(NULL, hrsrc);

							SendMessage(hwndItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
							SendMessage(hwndItem, LB_SETTABSTOPS, 1, (LPARAM)&tab);

							while(pData < pLimit) {
								char *t = buf;

								while(pData < pLimit && *pData!='\r' && *pData!='\n')
									*t++ = *pData++;

								while(pData < pLimit && (*pData=='\r' || *pData=='\n'))
									++pData;

								*t = 0;

								if (t > buf)
									SendMessage(GetDlgItem(hDlg, IDC_CREDITS), LB_ADDSTRING, 0, (LPARAM)buf);
							}

							FreeResource(hGlobal);
						}
						FreeResource(hGlobal);
					}
				}

				// Showtime!  Invalidate the entire window, force an update, and show the window.

				ShowWindow(hDlg, SW_SHOW);
				InvalidateRect(hDlg, NULL, TRUE);
				UpdateWindow(hDlg);

				// Grab the client area.

				HDC hdc;
				RECT r;

				GetClientRect(hDlg, &r);

				g_bihAboutDisplay.biSize			= sizeof(BITMAPINFOHEADER);
				g_bihAboutDisplay.biWidth			= r.right;
				g_bihAboutDisplay.biHeight			= r.bottom;
				g_bihAboutDisplay.biBitCount		= 16;
				g_bihAboutDisplay.biPlanes			= 1;
				g_bihAboutDisplay.biCompression		= BI_RGB;
				g_bihAboutDisplay.biXPelsPerMeter	= 80;
				g_bihAboutDisplay.biYPelsPerMeter	= 80;
				g_bihAboutDisplay.biClrUsed			= 0;
				g_bihAboutDisplay.biClrImportant	= 0;

				if (hdc = GetDC(hDlg)) {
					if (g_hdcAboutDisplay = CreateCompatibleDC(hdc)) {
						if (g_hbmAboutDisplay = CreateDIBSection(g_hdcAboutDisplay, (const BITMAPINFO *)&g_bihAboutDisplay, DIB_RGB_COLORS, &g_pvAboutDisplay, NULL, 0)) {
							g_hgoAboutDisplay = SelectObject(g_hdcAboutDisplay, g_hbmAboutDisplay);

							BitBlt(g_hdcAboutDisplay, 0, 0, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
							GdiFlush();

							if (tex = new Pixel16[32*32]) {
								g_pvAboutDisplayBack = malloc(((r.right+3)&~3)*2*r.bottom);

								if (g_pvAboutDisplayBack) {
									g_vbAboutSrc.init(g_pvAboutDisplayBack, r.right, r.bottom, 16);
									g_vbAboutDst.init(g_pvAboutDisplay, &g_bihAboutDisplay);
									g_vbAboutSrc.BitBlt(0, 0, &g_vbAboutDst, 0, 0, -1, -1);

									// Hide all controls.

									EnumChildWindows(hDlg, HideAllButOKCANCELProc, 0);

									// Grab the VirtualDub icon.

									HICON hico = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_VIRTUALDUB));

									RECT rFill = {0,0,32,32};
									FillRect(g_hdcAboutDisplay, &rFill, (HBRUSH)(COLOR_3DFACE+1));
									DrawIcon(g_hdcAboutDisplay, 0, 0, hico);

									GdiFlush();

									VBitmap(tex, 32, 32, 16).BitBlt(0, 0, &g_vbAboutDst, 0, 0, 32, 32);

									g_vbAboutDst.BitBlt(0, 0, &g_vbAboutSrc, 0, 0, 32, 32);

									// Initialize cube vertices.

									{
										int i;
										float rs;

										if (r.right > r.bottom)
											rs = r.bottom / (3.6f*3.0f);
										else
											rs = r.right / (3.6f*3.0f);

										for(i=0; i<8; i++) {
											vx[i][0] = i&1 ? -rs : +rs;
											vx[i][1] = i&2 ? -rs : +rs;
											vx[i][2] = i&4 ? -rs : +rs;
										}

										cubeside = rs + rs;

										rBounce.left = rBounce.top = (int)ceil(rs*1.8);
										rBounce.right = r.right - rBounce.left;
										rBounce.bottom = r.bottom - rBounce.top;

										xpos = rand()%(rBounce.right-rBounce.left) + rBounce.left;
										ypos = rand()%(rBounce.bottom-rBounce.top) + rBounce.top;
										xvel = (rand()&2)-1;
										yvel = (rand()&2)-1;

										rDirtyLast.top = rDirtyLast.left = 0;
										rDirtyLast.right = rDirtyLast.bottom = 0;
									}

									InvalidateRect(hDlg, NULL, TRUE);

									if (TIMERR_NOERROR == timeBeginPeriod(10)) {
										bTimerSet = true;
										bRender = true;

										mmTimer = timeSetEvent(10, 10, AboutTimerProc, (DWORD_PTR)hDlg, TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
									}
								}
							}
						}
					}
				}

			}
            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
				if (tex) {
					delete[] tex;
					tex = 0;
				}

				if (g_pvAboutDisplayBack) {
					free(g_pvAboutDisplayBack);
					g_pvAboutDisplayBack = 0;
				}

				if (g_hbmAboutDisplay) {
					DeleteObject(SelectObject(g_hdcAboutDisplay, g_hgoAboutDisplay));
					g_hbmAboutDisplay = NULL;
				}

				if (g_hdcAboutDisplay) {
					DeleteDC(g_hdcAboutDisplay);
					g_hdcAboutDisplay = NULL;
				}

				if (mmTimer)
					timeKillEvent(mmTimer);

				if (bTimerSet)
					timeEndPeriod(33);

                EndDialog(hDlg, TRUE);  
                return TRUE;
            }
            break;

		case WM_ERASEBKGND:
			if (g_pvAboutDisplayBack) {
				SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
				return TRUE;
			} else
				return FALSE;

		case WM_PAINT:
			if (g_pvAboutDisplayBack) {
				HDC hdc;
				PAINTSTRUCT ps;

				if (hdc = BeginPaint(hDlg, &ps)) {
					BitBlt(hdc, 0, 0, g_vbAboutDst.w, g_vbAboutDst.h,
						g_hdcAboutDisplay, 0, 0, SRCCOPY);

					bRender = true;

					EndPaint(hDlg, &ps);
				}
				return TRUE;
			}
			return FALSE;

		case WM_APP:
			if (g_pvAboutDisplayBack && bRender) {
				static float theta = 0.0;
				float xt, yt, zt;

				bRender = false;

				xt = sinf(theta) / 80.0f;
				yt = sinf(theta + 3.1415926535f * 2.0f / 3.0f) / 80.0f;
				zt = sinf(theta + 3.1415926535f * 4.0f / 3.0f) / 80.0f;
				theta = theta + 0.005f;

				xpos += xvel;
				ypos += yvel;

				if (xpos<rBounce.left) { xpos = rBounce.left; xvel = +1; }
				if (xpos>rBounce.right) { xpos = rBounce.right; xvel = -1; }
				if (ypos<rBounce.top) { ypos = rBounce.top; yvel = +1; }
				if (ypos>rBounce.bottom) { ypos = rBounce.bottom; yvel = -1; }

				RECT rDirty = { 0x7fffffff, 0x7fffffff, -1, -1 };
				RECT rDirty2;

				for(int i=0; i<8; i++) {
					float x0 = vx[i][0];
					float y0 = vx[i][1];
					float z0 = vx[i][2];

					float x1 = x0 * cosf(zt) - y0 * sinf(zt);
					float y1 = x0 * sinf(zt) + y0 * cosf(zt);
					float z1 = z0;

					float x2 = x1 * cosf(yt) - z1 * sinf(yt);
					float y2 = y1;
					float z2 = x1 * sinf(yt) + z1 * cosf(yt);

					float x3 = x2;
					float y3 = y2 * cosf(xt) - z2 * sinf(xt);
					float z3 = y2 * sinf(xt) + z2 * cosf(xt);

					vx[i][0] = x3;
					vx[i][1] = y3;
					vx[i][2] = z3;

					int ix1 = (int)floor(x3);
					int iy1 = (int)floor(y3);

					if (rDirty.left   > ix1) rDirty.left   = ix1;
					if (rDirty.right  < ix1) rDirty.right  = ix1;
					if (rDirty.top    > iy1) rDirty.top    = iy1;
					if (rDirty.bottom < iy1) rDirty.bottom = iy1;
				}

				OffsetRect(&rDirty, xpos, ypos);
				UnionRect(&rDirty2, &rDirty, &rDirtyLast);
				rDirtyLast = rDirty;

				++rDirty2.right;
				++rDirty2.bottom;

				GdiFlush();

				g_vbAboutDst.BitBlt(rDirty2.left, rDirty2.top, &g_vbAboutSrc, rDirty2.left, rDirty2.top,
					rDirty2.right+1-rDirty2.left, rDirty2.bottom+1-rDirty2.top);

				TriPt v[4];

				v[0].u =  0;	v[0].v = 0;
				v[1].u =  0;	v[1].v = 32;
				v[2].u = 32;	v[2].v = 0;
				v[3].u = 32;	v[3].v = 32;

				const float lightdir[3]={
					0.57735026918962576450914878050196f / (cubeside*cubeside),
					0.57735026918962576450914878050196f / (cubeside*cubeside),
					0.57735026918962576450914878050196f / (cubeside*cubeside),
				};

				for(int f=0; f<6; f++) {
					const int f0 = faces[f][0];
					const int f1 = faces[f][1];
					const int f2 = faces[f][2];
					const int f3 = faces[f][3];

					v[0].x = vx[f0][0] + xpos;
					v[0].y = vx[f0][1] + ypos;
					v[1].x = vx[f1][0] + xpos;
					v[1].y = vx[f1][1] + ypos;
					v[2].x = vx[f2][0] + xpos;
					v[2].y = vx[f2][1] + ypos;
					v[3].x = vx[f3][0] + xpos;
					v[3].y = vx[f3][1] + ypos;

					const float side0[3]={
						vx[f1][0] - vx[f0][0],
						vx[f1][1] - vx[f0][1],
						vx[f1][2] - vx[f0][2]
					};

					const float side1[3]={
						vx[f2][0] - vx[f0][0],
						vx[f2][1] - vx[f0][1],
						vx[f2][2] - vx[f0][2]
					};

					const float normal[3]={
						side0[1]*side1[2] - side0[2]*side1[1],
						side0[2]*side1[0] - side0[0]*side1[2],
						side0[0]*side1[1] - side0[1]*side1[0]
					};

					float shade = normal[0]*lightdir[0] + normal[1]*lightdir[1] + normal[2]*lightdir[2];

					uint32 alpha = VDRoundToInt((1.5f + 0.5f*shade) * 127.5f);

					RenderTriangle((Pixel16 *)g_vbAboutDst.Address32(0,0), -g_vbAboutDst.pitch/2, tex, v+0, v+1, v+2, alpha);
					RenderTriangle((Pixel16 *)g_vbAboutDst.Address32(0,0), -g_vbAboutDst.pitch/2, tex, v+2, v+1, v+3, alpha);
				}

				InvalidateRect(hDlg, &rDirty2, FALSE);
			}
			return TRUE;
    }
    return FALSE;
}
