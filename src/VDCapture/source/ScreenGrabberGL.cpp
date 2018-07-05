//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#include <stdafx.h>
#include <vd2/VDCapture/capdriver.h>
#include <vd2/Riza/opengl.h>
#include <vd2/system/binary.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDCapture/ScreenGrabberGL.h>
#include <windows.h>
#include <tchar.h>

using namespace nsVDCapture;

namespace {
	#include "cap_screen_glshaders.inl"
}

VDScreenGrabberGL::VDScreenGrabberGL()
	: mhwndParent(NULL)
	, mhwndGL(NULL)
	, mhwndGLDraw(NULL)
	, mbCapBuffersInited(false)
	, mOffscreenSize(0)
	, mOffscreenPitch(0)
	, mGLShaderBase(0)
	, mGLTextureW(1)
	, mGLTextureH(1)
	, mGLCursorCacheTexture(0)
	, mCachedCursor(NULL)
	, mCachedCursorHotspotX(0)
	, mCachedCursorHotspotY(0)
	, mhdcCursorBuffer(NULL)
	, mhbmCursorBuffer(NULL)
	, mhbmCursorBufferOld(NULL)
	, mpCursorBuffer(NULL)
	, mbCaptureFramePending(false)
	, mbVisible(false)
	, mbDisplayPreview(false)
	, mDisplayArea(0, 0, 0, 0)
	, mTrackX(0)
	, mTrackY(0)
	, mbRescaleImage(false)
	, mbDrawMousePointer(true)
	, mbRemoveDuplicates(true)
	, mFramePeriod(10000000 / 30)
	, mCaptureSrcWidth(320)
	, mCaptureSrcHeight(240)
	, mCaptureDstWidth(320)
	, mCaptureDstHeight(240)
	, mCaptureFormat(kVDScreenGrabberFormat_XRGB32)
	, mpCB(NULL)
	, mWndClassGL(NULL)
{
	mGLOcclusionQueries[0] = 0;
	mGLOcclusionQueries[1] = 0;
	mbGLOcclusionValid[0] = false;
	mbGLOcclusionValid[1] = false;
	mbGLOcclusionPrevFrameValid = false;

	memset(mGLTextures, 0, sizeof mGLTextures);
	memset(mGLBuffers, 0, sizeof mGLBuffers);
	memset(mbFrameValid, 0, sizeof mbFrameValid);
}

VDScreenGrabberGL::~VDScreenGrabberGL() {
	Shutdown();
}

bool VDScreenGrabberGL::Init(IVDScreenGrabberCallback *cb) {
	mpCB = cb;

	const HINSTANCE hInst = VDGetLocalModuleHandleW32();

	if (!mWndClassGL) {
		VDStringA className;

		className.sprintf("VDScreenGrabberGL[%p]", this);

		WNDCLASS wcgl = { CS_OWNDC, StaticWndProcGL, 0, sizeof(VDScreenGrabberGL *), hInst, NULL, NULL, NULL, NULL, className.c_str() };

		mWndClassGL = RegisterClass(&wcgl);

		if (!mWndClassGL) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDScreenGrabberGL::Shutdown() {
	ShutdownDisplay();
	ShutdownCapture();

	if (mWndClassGL) {
		UnregisterClassA((LPCTSTR)mWndClassGL, VDGetLocalModuleHandleW32());
		mWndClassGL = NULL;
	}
}

bool VDScreenGrabberGL::InitCapture(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDScreenGrabberFormat format) {
	switch(format) {
		case kVDScreenGrabberFormat_YUY2:
			if (dstw & 1)
				return false;

			mOffscreenSize = dstw * dsth * 2;
			mOffscreenPitch = (dstw + 3) & ~3;
			break;

		case kVDScreenGrabberFormat_YV12:
			if ((dstw & 7) || (dsth & 1))
				return false;
			mOffscreenSize = dstw * dsth * 3 / 2;
			mOffscreenPitch = (dstw + 3) & ~3;
			break;

		case kVDScreenGrabberFormat_XRGB32:
			mOffscreenSize = dstw * dsth * 4;
			mOffscreenPitch = dstw*4;
			break;

		default:
			return false;
	}

	ShutdownCapture();

	mbRescaleImage = (srcw != dstw || srch != dsth);

	mCaptureSrcWidth = srcw;
	mCaptureSrcHeight = srch;
	mCaptureDstWidth = dstw;
	mCaptureDstHeight = dsth;
	mCaptureFormat = format;

	if (!mGL.Init())
		return false;

	const HINSTANCE hInst = VDGetLocalModuleHandleW32();

	if (!(mhwndGL = CreateWindow((LPCTSTR)mWndClassGL, _T(""), WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, hInst, this)))
		return false;

	HDC hdc = GetDC(mhwndGL);
	if (!mGL.Attach(hdc, 24, 8, 0, 0, true)) {
		ReleaseDC(mhwndGL, hdc);
		return false;
	}

	int buffersize = mCaptureDstWidth * mCaptureDstHeight * 4;

	mGL.Begin(hdc);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glGenBuffersARB(2, mGLBuffers);
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffersize, NULL, GL_STREAM_READ_ARB);
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[1]);
		mGL.glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffersize, NULL, GL_STREAM_READ_ARB);
	} else {
		mGLReadBuffer.resize((buffersize + 3) >> 2);
	}

	VDASSERT(!mGL.glGetError());

	mGL.glGenTextures(2, mGLTextures);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

	int texw = srcw * 2 - 1;
	int texh = srch * 2 - 1;

	while(int t = texw & (texw-1))
		texw = t;

	while(int t = texh & (texh-1))
		texh = t;

	mGLTextureW = texw;
	mGLTextureH = texh;

	mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texw, texh, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
	VDASSERT(!mGL.glGetError());

	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[1]);
	mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, texw, texh, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
	VDASSERT(!mGL.glGetError());

	int cxcursor = GetSystemMetrics(SM_CXCURSOR);
	int cycursor = GetSystemMetrics(SM_CYCURSOR);

	mCachedCursorWidth = cxcursor;
	mCachedCursorHeight = cycursor;

	HDC hdcScreen = GetDC(NULL);
	mhdcCursorBuffer = CreateCompatibleDC(hdcScreen);
	const BITMAPINFOHEADER bihCursor={
		sizeof(BITMAPINFOHEADER),
		cxcursor,
		cycursor * 2,
		1,
		32,
		BI_RGB,
		0,
		0,
		0,
		0,
		0
	};
	ReleaseDC(NULL, hdcScreen);

	mhbmCursorBuffer = CreateDIBSection(mhdcCursorBuffer, (const BITMAPINFO *)&bihCursor, DIB_RGB_COLORS, (void **)&mpCursorBuffer, NULL, 0);
	mhbmCursorBufferOld = SelectObject(mhdcCursorBuffer, mhbmCursorBuffer);

	cxcursor = cxcursor * 2 - 1;
	while(int t = cxcursor & (cxcursor - 1))
		cxcursor = t;

	cycursor = cycursor * 2 - 1;
	while(int t = cycursor & (cycursor - 1))
		cycursor = t;

	mCachedCursor = NULL;
	mGLCursorCacheTextureInvW = 1.0f / (float)cxcursor;
	mGLCursorCacheTextureInvH = 1.0f / (float)cycursor;

	mGL.glGenTextures(1, &mGLCursorCacheTexture);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
	mGL.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, cxcursor, cycursor, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);

	if (mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates)
		mGL.glGenOcclusionQueriesNV(2, mGLOcclusionQueries);

	// initialize shaders
	mGLShaderBase = mGL.InitTechniques(g_techniques, sizeof g_techniques / sizeof g_techniques[0]);

	VDASSERT(!mGL.glGetError());

	FlushFrameQueue();

	mGL.End();

	ReleaseDC(mhwndGL, hdc);

	mTimestampIndex = 0;
	mTimestampDelay = 0;
	if (mGL.EXT_pixel_buffer_object)
		++mTimestampDelay;
	if (mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates)
		++mTimestampDelay;

	mbCapBuffersInited = true;
	return true;
}

void VDScreenGrabberGL::ShutdownCapture() {
	ShutdownDisplay();

	mbCapBuffersInited = false;

	if (mhbmCursorBufferOld) {
		SelectObject(mhdcCursorBuffer, mhbmCursorBufferOld);
		mhbmCursorBufferOld = NULL;
	}

	if (mhbmCursorBuffer) {
		DeleteObject(mhbmCursorBuffer);
		mhbmCursorBuffer = NULL;
	}

	if (mhdcCursorBuffer) {
		DeleteDC(mhdcCursorBuffer);
		mhdcCursorBuffer = NULL;
	}

	if (mGL.IsInited()) {
		if (HDC hdc = GetDC(mhwndGL)) {
			if (mGL.Begin(hdc)) {
				if (mGL.NV_occlusion_query) {
					for(int i=0; i<2; ++i) {
						if (mbGLOcclusionValid[i]) {
							GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[i]);
							mbGLOcclusionValid[i] = false;
						}
					}
					mGL.glDeleteOcclusionQueriesNV(2, mGLOcclusionQueries);
				}
				mGL.End();
			}
			ReleaseDC(mhwndGL, hdc);
		}

		mGL.glDeleteLists(mGLShaderBase, sizeof g_techniques / sizeof g_techniques[0]);

		if (mGLTextures[0]) {
			mGL.glDeleteTextures(2, mGLTextures);
			mGLTextures[0] = 0;
		}

		if (mGLCursorCacheTexture) {
			mGL.glDeleteTextures(1, &mGLCursorCacheTexture);
			mGLCursorCacheTexture = 0;
			mCachedCursor = NULL;
		}

		if (mGL.EXT_pixel_buffer_object)
			mGL.glDeleteBuffersARB(2, mGLBuffers);

		mGL.Shutdown();
	}

	if (mhwndGL) {
		DestroyWindow(mhwndGL);
		mhwndGL = NULL;
	}
}

void VDScreenGrabberGL::SetCaptureOffset(int x, int y) {
	mTrackX = x;
	mTrackY = y;
}

void VDScreenGrabberGL::SetCapturePointer(bool enable) {
	mbDrawMousePointer = enable;
}

uint64 VDScreenGrabberGL::GetCurrentTimestamp() {
	return ComputeGlobalTime();
}

sint64 VDScreenGrabberGL::ConvertTimestampDelta(uint64 t, uint64 base) {
	return (sint32)(uint32)(t - base) * (sint64)1000;
}

bool VDScreenGrabberGL::AcquireFrame(bool dispatch) {
	if (!mbCapBuffersInited)
		return false;

	sint64 globalTime;

	int w = mCaptureDstWidth;
	int h = mCaptureDstHeight;
	int srcw = mCaptureSrcWidth;
	int srch = mCaptureSrcHeight;

	globalTime = ComputeGlobalTime();

	// Check for cursor update.
	CURSORINFO ci = {sizeof(CURSORINFO)};
	bool cursorImageUpdated = false;

	if (mbDrawMousePointer) {
		if (!::GetCursorInfo(&ci)) {
			ci.hCursor = NULL;
		}

		if (ci.hCursor) {
			cursorImageUpdated = UpdateCachedCursor(ci);

			ci.ptScreenPos.x -= mCachedCursorHotspotX;
			ci.ptScreenPos.y -= mCachedCursorHotspotY;
		}
	}

	RECT r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};

	GetWindowRect(mhwndGL, &r);

	if (HDC hdc = GetDC(mhwndGL)) {
		if (mGL.Begin(hdc)) {
			// init state
			VDASSERT(!mGL.glGetError());
			mGL.glDrawBuffer(GL_BACK);
			mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

			// update cursor if necessary
			if (cursorImageUpdated) {
				mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
				mGL.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mCachedCursorWidth, mCachedCursorHeight, GL_BGRA_EXT, GL_UNSIGNED_BYTE, mpCursorBuffer);
			}

			// read screen into texture
			if (mGL.ARB_multitexture)
				mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
			mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

			int srcx = mTrackX - r.left;
			int srcy = r.bottom - (mTrackY + srch);

			if (srcx > r.right - r.left - srcw)
				srcx = r.right - r.left - srcw;
			if (srcy > r.bottom - r.top - srch)
				srcy = r.bottom - r.top - srch;
			if (srcx < 0)
				srcx = 0;
			if (srcy < 0)
				srcy = 0;

			// Enable depth test. We don't actually want the depth test, but we need it
			// on for occlusion query to work on ATI.
			mGL.glEnable(GL_DEPTH_TEST);
			mGL.glDepthFunc(GL_ALWAYS);
			mGL.glDepthMask(GL_FALSE);

			// shrink image
			VDASSERT(!mGL.glGetError());
			mGL.glDisable(GL_LIGHTING);
			mGL.glDisable(GL_CULL_FACE);
			mGL.glDisable(GL_BLEND);
			mGL.glDisable(GL_ALPHA_TEST);
			mGL.glDisable(GL_STENCIL_TEST);
			mGL.glDisable(GL_SCISSOR_TEST);
			mGL.glEnable(GL_TEXTURE_2D);

			mGL.DisableFragmentShaders();

			int fbw = srcw < w ? w : srcw;
			int fbh = srch < h ? h : srch;

			mGL.glViewport(0, 0, fbw, fbh);
			mGL.glMatrixMode(GL_MODELVIEW);
			mGL.glLoadIdentity();
			mGL.glMatrixMode(GL_PROJECTION);
			mGL.glLoadIdentity();
			mGL.glOrtho(0, fbw, 0, fbh, -1.0f, 1.0f);

			VDASSERT(!mGL.glGetError());
			mGL.glReadBuffer(GL_FRONT);

			VDPROFILEBEGIN("GL:ReadScreen");
			if (!mbRescaleImage || mbDrawMousePointer) {
				mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srcx, srcy, srcw, srch);
				mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

				float u = (float)srcw / (float)mGLTextureW;
				float v = (float)srch / (float)mGLTextureH;

				mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				mGL.glBegin(GL_QUADS);
					mGL.glTexCoord2f(0.0f, 0.0f);
					mGL.glVertex2f(0, 0);
					mGL.glTexCoord2f(u, 0.0f);
					mGL.glVertex2f((float)srcw, 0);
					mGL.glTexCoord2f(u, v);
					mGL.glVertex2f((float)srcw, (float)srch);
					mGL.glTexCoord2f(0.0f, v);
					mGL.glVertex2f(0, (float)srch);
				mGL.glEnd();
				mGL.glReadBuffer(GL_BACK);

				if (ci.hCursor) {
					mGL.glBindTexture(GL_TEXTURE_2D, mGLCursorCacheTexture);
					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

					int curx = ci.ptScreenPos.x - srcx;
					int cury = (r.bottom - (ci.ptScreenPos.y + mCachedCursorHeight)) - srcy;
					float curu = (float)mCachedCursorWidth * mGLCursorCacheTextureInvW;
					float curv = (float)mCachedCursorHeight * mGLCursorCacheTextureInvH;
					mGL.glEnable(GL_BLEND);

					if (mbCachedCursorXORMode && mGL.EXT_texture_env_combine) {
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_EXT, GL_SRC_ALPHA);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
						mGL.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
						mGL.glBegin(GL_QUADS);
							mGL.glTexCoord2f(0.0f, 0.0f);
							mGL.glVertex2i(curx, cury);
							mGL.glTexCoord2f(curu, 0.0f);
							mGL.glVertex2i(curx + mCachedCursorWidth, cury);
							mGL.glTexCoord2f(curu, curv);
							mGL.glVertex2i(curx + mCachedCursorWidth, cury + mCachedCursorHeight);
							mGL.glTexCoord2f(0.0f, curv);
							mGL.glVertex2i(curx, cury + mCachedCursorHeight);
						mGL.glEnd();
						mGL.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
						mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					} else
						mGL.glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

					mGL.glBegin(GL_QUADS);
						mGL.glTexCoord2f(0.0f, 0.0f);
						mGL.glVertex2i(curx, cury);
						mGL.glTexCoord2f(curu, 0.0f);
						mGL.glVertex2i(curx + mCachedCursorWidth, cury);
						mGL.glTexCoord2f(curu, curv);
						mGL.glVertex2i(curx + mCachedCursorWidth, cury + mCachedCursorHeight);
						mGL.glTexCoord2f(0.0f, curv);
						mGL.glVertex2i(curx, cury + mCachedCursorHeight);
					mGL.glEnd();
					mGL.glDisable(GL_BLEND);
					mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
				}

				srcx = 0;
				srcy = 0;
			}

			if (mbRescaleImage) {
				do {
					int dstw = std::max<int>(w, (srcw+1) >> 1);
					int dsth = std::max<int>(h, (srch+1) >> 1);

					mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, srcx, srcy, srcw, srch);
					srcx = 0;
					srcy = 0;

					mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

					float u = (float)srcw / (float)mGLTextureW;
					float v = (float)srch / (float)mGLTextureH;

					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
					mGL.glBegin(GL_QUADS);
						mGL.glTexCoord2f(0.0f, 0.0f);
						mGL.glVertex2f(0, 0);
						mGL.glTexCoord2f(u, 0.0f);
						mGL.glVertex2f((float)dstw, 0);
						mGL.glTexCoord2f(u, v);
						mGL.glVertex2f((float)dstw, (float)dsth);
						mGL.glTexCoord2f(0.0f, v);
						mGL.glVertex2f(0, (float)dsth);
					mGL.glEnd();

					mGL.glReadBuffer(GL_BACK);
					srcw = dstw;
					srch = dsth;
				} while(srcw != w || srch != h);
			}
			VDPROFILEEND();

			VDASSERT(!mGL.glGetError());

			// compute texturing parameters
			float u = (float)w / (float)mGLTextureW;
			float v = (float)h / (float)mGLTextureH;

			bool removeDuplicates = mGL.NV_occlusion_query && mGL.ARB_multitexture && mbRemoveDuplicates;
			if (mCaptureFormat || removeDuplicates) {
				mGL.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);

				if (removeDuplicates) {
					if (!mbGLOcclusionPrevFrameValid) {
						mbGLOcclusionPrevFrameValid = true;
						mbFrameValid[0] = true;
					} else {
						VDPROFILEBEGIN("GL:OcclusionQuery");
						mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
						mGL.glEnable(GL_TEXTURE_2D);
						mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[1]);

						mGL.glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
						mGL.glEnable(GL_ALPHA_TEST);
						mGL.glAlphaFunc(GL_GREATER, 0.0f);
						
						mGL.glBeginOcclusionQueryNV(mGLOcclusionQueries[0]);
						if (mGL.NV_register_combiners)
							mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_difference_NV1x);
						else if (mGL.ATI_fragment_shader)
							mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_difference_ATIFS);

						mGL.glBegin(GL_QUADS);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, 0.0f);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, 0.0f);
							mGL.glVertex2f(0, 0);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, 0.0f);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, 0.0f);
							mGL.glVertex2f((float)w, 0);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, v);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, v);
							mGL.glVertex2f((float)w, (float)h);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, 0.0f, v);
							mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, 0.0f, v);
							mGL.glVertex2f(0, (float)h);
						mGL.glEnd();
						mGL.glEndOcclusionQueryNV();
						mGL.glFlush();

						mbGLOcclusionValid[0] = true;

						mGL.glDisable(GL_ALPHA_TEST);
						mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

						std::swap(mGLTextures[0], mGLTextures[1]);
						std::swap(mGLOcclusionQueries[0], mGLOcclusionQueries[1]);
						std::swap(mbGLOcclusionValid[0], mbGLOcclusionValid[1]);
						mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
						mGL.glDisable(GL_TEXTURE_2D);
						mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
						mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

						GLint pixelCount = 0;

						if (mbGLOcclusionValid[0]) {
							pixelCount = GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[0]);
							mbGLOcclusionValid[0] = false;
						}

						mbFrameValid[0] = (pixelCount > 0);

						VDASSERT(!mGL.glGetError());
						std::swap(mbFrameValid[0], mbFrameValid[1]);
						VDPROFILEEND();
					}
				} else
					mbFrameValid[0] = mbFrameValid[1] = true;
			} else
				mbFrameValid[0] = mbFrameValid[1] = true;

			VDPROFILEBEGIN("GL:ConvertAndRead");
			switch(mCaptureFormat) {
			case kVDScreenGrabberFormat_YV12:
				if (mGL.ATI_fragment_shader)
					ConvertToYV12_GL_ATIFS(w, h, u, v);
				else if (mGL.NV_register_combiners) {
					if (mGL.NV_register_combiners2)
						ConvertToYV12_GL_NV2x(w, h, u, v);
					else
						ConvertToYV12_GL_NV1x(w, h, u, v);
				}
				break;
			case kVDScreenGrabberFormat_YUY2:
				if (mGL.ATI_fragment_shader)
					ConvertToYUY2_GL_NV2x_ATIFS(w, h, u, v, true);
				else if (mGL.NV_register_combiners) {
					if (mGL.NV_register_combiners2)
						ConvertToYUY2_GL_NV2x_ATIFS(w, h, u, v, false);
					else
						ConvertToYUY2_GL_NV1x(w, h, u, v);
				}
				break;
			default:
				mGL.glReadBuffer(GL_BACK);
				if (mGL.EXT_pixel_buffer_object) {
					mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
					mGL.glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
				} else
					mGL.glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)mGLReadBuffer.data());

				break;
			}
			VDPROFILEEND();

			// readback!

			mTimestampQueue[mTimestampIndex & 3] = globalTime;
			globalTime = mTimestampQueue[(mTimestampIndex + mTimestampDelay) & 3];
			++mTimestampIndex;

			if (globalTime >= 0 && dispatch) {
				if (mGL.EXT_pixel_buffer_object) {
					if (mbFrameValid[0]) {
						mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[1]);
						VDPROFILEBEGIN("GL:MapBuffer");
						void *p = mGL.glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
						VDPROFILEEND();
						DispatchFrame(p, mOffscreenPitch, globalTime);
						mGL.glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
					} else
						DispatchFrame(NULL, 0, globalTime);

					std::swap(mGLBuffers[0], mGLBuffers[1]);
				} else {
					if (mbFrameValid[0])
						DispatchFrame(mGLReadBuffer.data(), mOffscreenPitch, globalTime);
					else
						DispatchFrame(NULL, 0, globalTime);
				}
			}

			VDASSERT(!mGL.glGetError());

			// necessary for ATI to work
			if (mGL.ATI_fragment_shader) {
				if (mGL.EXT_swap_control)
					mGL.wglSwapIntervalEXT(0);
				mGL.wglSwapBuffers(hdc);
			}

			mGL.End();
		}
		ReleaseDC(mhwndGL, hdc);
	}
	
	if (mhwndGLDraw && mbVisible) {
		RECT rdraw;
		GetClientRect(mhwndGLDraw, &rdraw);

		if (rdraw.right && rdraw.bottom) {
			VDPROFILEBEGIN("Overlay (OpenGL)");
			if (HDC hdcDraw = GetDC(mhwndGLDraw)) {
				if (mGL.Begin(hdcDraw)) {
					VDASSERT(!mGL.glGetError());
					mGL.glDisable(GL_LIGHTING);
					mGL.glDisable(GL_CULL_FACE);
					mGL.glDisable(GL_BLEND);
					mGL.glDisable(GL_ALPHA_TEST);
					mGL.glDisable(GL_DEPTH_TEST);
					mGL.glDisable(GL_STENCIL_TEST);
					mGL.glDisable(GL_SCISSOR_TEST);
					mGL.glEnable(GL_TEXTURE_2D);

					VDASSERT(!mGL.glGetError());
					mGL.DisableFragmentShaders();

					float dstw = (float)rdraw.right;
					float dsth = (float)rdraw.bottom;

					VDASSERT(!mGL.glGetError());
					mGL.glViewport(0, 0, rdraw.right, rdraw.bottom);
					mGL.glMatrixMode(GL_MODELVIEW);
					mGL.glLoadIdentity();
					mGL.glMatrixMode(GL_PROJECTION);
					mGL.glLoadIdentity();
					mGL.glOrtho(0, dstw, 0, dsth, -1.0f, 1.0f);

					mGL.glDrawBuffer(GL_BACK);
					mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					mGL.glClearColor(0.5f, 0.0f, 0.0f, 0.0f);
					mGL.glClear(GL_COLOR_BUFFER_BIT);
					VDASSERT(!mGL.glGetError());

					mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
					VDASSERT(!mGL.glGetError());

					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					mGL.glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					mGL.glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

					float u = (float)srcw / (float)mGLTextureW;
					float v = (float)srch / (float)mGLTextureH;

					VDASSERT(!mGL.glGetError());
					mGL.glBegin(GL_QUADS);
						mGL.glTexCoord2f(0.0f, 0.0f);
						mGL.glVertex2f(0, 0);
						mGL.glTexCoord2f(u, 0.0f);
						mGL.glVertex2f(dstw, 0);
						mGL.glTexCoord2f(u, v);
						mGL.glVertex2f(dstw, dsth);
						mGL.glTexCoord2f(0.0f, v);
						mGL.glVertex2f(0, dsth);
					mGL.glEnd();
					VDASSERT(!mGL.glGetError());

					if (mGL.EXT_swap_control)
						mGL.wglSwapIntervalEXT(0);

					mGL.wglSwapBuffers(hdcDraw);
					mGL.End();
				}
				ReleaseDC(mhwndGLDraw, hdcDraw);
			}
			VDPROFILEEND();
		}
	}

	return true;
}

bool VDScreenGrabberGL::InitDisplay(HWND hwndParent, bool preview) {
	mbDisplayPreview = preview;

	DWORD dwFlags = mbVisible ? WS_CHILD | WS_VISIBLE : WS_CHILD;

	if (!(mhwndGLDraw = CreateWindow((LPCTSTR)mWndClassGL, _T(""), dwFlags, mDisplayArea.left, mDisplayArea.top, mDisplayArea.width(), mDisplayArea.height(), hwndParent, NULL, VDGetLocalModuleHandleW32(), this))) {
		ShutdownDisplay();
		return false;
	}

	HDC hdc2 = GetDC(mhwndGLDraw);
	if (!hdc2) {
		ShutdownDisplay();
		return false;
	}

	mGL.AttachAux(hdc2, 24, 8, 0, 0, true);
	ReleaseDC(mhwndGLDraw, hdc2);
	return true;
}

void VDScreenGrabberGL::ShutdownDisplay() {
	if (mhwndGLDraw) {
		DestroyWindow(mhwndGLDraw);
		mhwndGLDraw = NULL;
	}
}

void VDScreenGrabberGL::SetDisplayArea(const vdrect32& r) {
	mDisplayArea = r;

	if (mhwndGLDraw)
		SetWindowPos(mhwndGLDraw, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

void VDScreenGrabberGL::SetDisplayVisible(bool vis) {
	if (vis == mbVisible)
		return;

	mbVisible = vis;

	if (mhwndGLDraw)
		ShowWindow(mhwndGLDraw, vis ? SW_SHOWNA : SW_HIDE);
}

void VDScreenGrabberGL::SetFramePeriod(sint32 ms) {
	mFramePeriod = ms;
}

void VDScreenGrabberGL::FlushFrameQueue() {
	for(int i=0; i<2; ++i) {
		if (mbGLOcclusionValid[i]) {
			GetOcclusionQueryPixelCountSafe(mGLOcclusionQueries[i]);
			mbGLOcclusionValid[i] = false;
		}
	}

	for(int i=0; i<sizeof(mTimestampQueue)/sizeof(mTimestampQueue[0]); ++i)
		mTimestampQueue[i] = -1;

	mbFrameValid[0] = mbFrameValid[1] = false;
	mbGLOcclusionValid[0] = mbGLOcclusionValid[1] = false;
	mbGLOcclusionPrevFrameValid = false;
}

sint64 VDScreenGrabberGL::ComputeGlobalTime() {
	return VDGetAccurateTick();
}

bool VDScreenGrabberGL::UpdateCachedCursor(const CURSORINFO& ci) {
	if (mCachedCursor == ci.hCursor)
		return false;

	mCachedCursor = ci.hCursor;

	ICONINFO ii;
	if (!::GetIconInfo(ci.hCursor, &ii))
		return false;

	mCachedCursorHotspotX = ii.xHotspot;
	mCachedCursorHotspotY = ii.yHotspot;

	bool mergeMask = false;
	bool cursorImageUpdated = false;

	HDC hdc = GetDC(NULL);
	if (hdc) {
		mbCachedCursorXORMode = false;

		if (!ii.hbmColor) {
			mbCachedCursorXORMode = true;

			// Query bitmap format.
			BITMAPINFOHEADER maskFormat = {sizeof(BITMAPINFOHEADER)};
			if (::GetDIBits(hdc, ii.hbmMask, 0, 0, NULL, (LPBITMAPINFO)&maskFormat, DIB_RGB_COLORS)) {
				// Validate cursor size. This shouldn't change since SM_CXCURSOR and SM_CYCURSOR are constant.
				if (maskFormat.biWidth == mCachedCursorWidth && maskFormat.biHeight == mCachedCursorHeight * 2) {
					// Retrieve bitmap bits.
					BITMAPINFOHEADER hdr = {};
					hdr.biSize			= sizeof(BITMAPINFOHEADER);
					hdr.biWidth			= maskFormat.biWidth;
					hdr.biHeight		= maskFormat.biHeight;
					hdr.biPlanes		= 1;
					hdr.biBitCount		= 32;
					hdr.biCompression	= BI_RGB;
					hdr.biSizeImage		= maskFormat.biWidth * maskFormat.biHeight * 4;
					hdr.biXPelsPerMeter	= 0;
					hdr.biYPelsPerMeter	= 0;
					hdr.biClrUsed		= 0;
					hdr.biClrImportant	= 0;

					::GetDIBits(hdc, ii.hbmMask, 0, maskFormat.biHeight, mpCursorBuffer, (LPBITMAPINFO)&hdr, DIB_RGB_COLORS);
				}
			}

			uint32 numPixels = mCachedCursorWidth * mCachedCursorHeight;
			uint32 *pXORMask = mpCursorBuffer;
			uint32 *pANDMask = pXORMask + numPixels;

			for(uint32 i=0; i<numPixels; ++i)
				pXORMask[i] = (pXORMask[i] & 0xFFFFFF) + (~pANDMask[i] << 24);
		} else {
			RECT r1 = {0, 0, mCachedCursorWidth, mCachedCursorHeight};
			RECT r2 = {0, mCachedCursorHeight, mCachedCursorWidth, mCachedCursorHeight*2};
			FillRect(mhdcCursorBuffer, &r1, (HBRUSH)GetStockObject(BLACK_BRUSH));
			FillRect(mhdcCursorBuffer, &r2, (HBRUSH)GetStockObject(WHITE_BRUSH));
			DrawIcon(mhdcCursorBuffer, 0, 0, ci.hCursor);
			DrawIcon(mhdcCursorBuffer, 0, mCachedCursorHeight, ci.hCursor);
			GdiFlush();

			uint32 numPixels = mCachedCursorWidth * mCachedCursorHeight;
			uint32 *pWhiteMask = mpCursorBuffer;
			uint32 *pBlackMask = pWhiteMask + numPixels;

			for(uint32 i=0; i<numPixels; ++i) {
				uint32 pixelOnWhite = pWhiteMask[i];
				uint32 pixelOnBlack = pBlackMask[i];
				int alpha = 255 - (int)(pWhiteMask[i] & 255) + (int)(pBlackMask[i] & 255);
				if ((unsigned)alpha >= 256)
					alpha = ~alpha >> 31;
				pWhiteMask[i] = (pBlackMask[i] & 0xffffff) + (alpha << 24);
			}
		}

		cursorImageUpdated = true;
		ReleaseDC(NULL, hdc);
	}

	if (ii.hbmColor)
		VDVERIFY(::DeleteObject(ii.hbmColor));
	if (ii.hbmMask)
		VDVERIFY(::DeleteObject(ii.hbmMask));

	return cursorImageUpdated;
}

void VDScreenGrabberGL::DispatchFrame(const void *data, ptrdiff_t pitch, sint64 timestamp) {
	if (mpCB) {
		uint32 h = mCaptureDstHeight;

		if (mCaptureFormat == kVDScreenGrabberFormat_YV12)
			h += (h >> 1);

		mpCB->ReceiveFrame(timestamp, data, pitch, pitch, h);
	}
}

void VDScreenGrabberGL::ConvertToYV12_GL_NV1x(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	for(int phase=0; phase<4; phase += 2) {
		float u0 = ((float)phase - 1.5f) / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = ((float)phase - 0.5f) / (float)mGLTextureW;
		float u3 = u2 + u;

		mGL.glCallList(mGLShaderBase + (phase ? kVDOpenGLTechIndex_YV12_NV1x_Y_ra : kVDOpenGLTechIndex_YV12_NV1x_Y_gb));
		mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(0.0f, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w4, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w4, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_NV1x_Cb : kVDOpenGLTechIndex_YV12_NV1x_Cr));
		for(int phase=0; phase<4; phase += 2) {
			float u0 = (float)(phase * 2 - 3.0f) / (float)mGLTextureW;
			float v0 = 0;
			float u1 = u0 + u;
			float v1 = v0 + v;

			float u2 = (float)(phase * 2 - 1.0f) / (float)mGLTextureW;
			float u3 = u0 + u;

			mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

			float y0 = y;
			float y1 = y + (float)h2;

			mGL.glBegin(GL_QUADS);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(w4, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w4+w8, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w4+w8, y1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(w4, y1);
			mGL.glEnd();
			VDASSERT(!mGL.glGetError());
		}

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDScreenGrabberGL::ConvertToYV12_GL_NV2x(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners2 path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	{
		float u0 = -1.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = -0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		float u4 = +0.5f / (float)mGLTextureW;
		float u5 = u4 + u;

		float u6 = +1.5f / (float)mGLTextureW;
		float u7 = u6 + u;

		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YV12_NV2x_Y);
		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glVertex2f(w4, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glVertex2f(w4, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_NV2x_Cb : kVDOpenGLTechIndex_YV12_NV2x_Cr));
		for(int phase=0; phase<4; phase += 2) {
			float u0 = (float)(phase * 2 - 4.0f) / (float)mGLTextureW;
			float v0 = 0;
			float u1 = u0 + u;
			float v1 = v0 + v;

			float u2 = (float)(phase * 2 - 2.0f) / (float)mGLTextureW;
			float u3 = u2 + u;

			float u4 = (float)(phase * 2 - 0.0f) / (float)mGLTextureW;
			float u5 = u4 + u;

			mGL.glColorMask(GL_TRUE, phase==0, phase==0, GL_TRUE);

			float y0 = y;
			float y1 = y + (float)h2;

			mGL.glBegin(GL_QUADS);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
				mGL.glVertex2f(w4, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
				mGL.glVertex2f(w4+w8, y0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
				mGL.glVertex2f(w4+w8, y1);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
				mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
				mGL.glVertex2f(w4, y1);
			mGL.glEnd();
			VDASSERT(!mGL.glGetError());
		}

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDScreenGrabberGL::ConvertToYV12_GL_ATIFS(int w, int h, float u, float v) {
	// YV12 conversion - NV_register_combiners2 path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	float y = 0.0f;
	float w4 = (float)w * 0.25f;
	{
		float u0 = -1.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = -0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		float u4 = +0.5f / (float)mGLTextureW;
		float u5 = u4 + u;

		float u6 = +1.5f / (float)mGLTextureW;
		float u7 = u6 + u;

		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YV12_ATIFS_Y);
		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glVertex2f(w4, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glVertex2f(w4, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE4_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);

	y = 0.0f;

	float w8 = (float)w * 0.125f;
	int h2 = h >> 1;
	mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	for(int cmode=0; cmode<2; ++cmode) {
		mGL.glCallList(mGLShaderBase + (cmode ? kVDOpenGLTechIndex_YV12_ATIFS_Cb : kVDOpenGLTechIndex_YV12_ATIFS_Cr));
		float u0 = -4.5f / (float)mGLTextureW;		float u1 = u0 + u;
		float u2 = -2.5f / (float)mGLTextureW;		float u3 = u2 + u;
		float u4 = -0.5f / (float)mGLTextureW;		float u5 = u4 + u;
		float u6 = +0.5f / (float)mGLTextureW;		float u7 = u6 + u;
		float u8 = +2.5f / (float)mGLTextureW;		float u9 = u8 + u;

		float v0 = 0;
		float v1 = v0 + v;

		float y0 = y;
		float y1 = y + (float)h2;

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u8, v1);
			mGL.glVertex2f(w4, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u9, v1);
			mGL.glVertex2f(w4+w8, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u5, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u7, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u9, v0);
			mGL.glVertex2f(w4+w8, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u4, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u6, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE4_ARB, u8, v0);
			mGL.glVertex2f(w4, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());

		y += h2;
	}

	mGL.glActiveTextureARB(GL_TEXTURE4_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	// readback!
	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);

	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)(w * h * 5 / 4));
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 2, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
		mGL.glReadPixels(w >> 2, 0, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h));
		mGL.glReadPixels(w >> 2, h >> 1, w >> 3, h >> 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst + (w * h * 5 / 4));
	}
}

void VDScreenGrabberGL::ConvertToYUY2_GL_NV1x(int w, int h, float u, float v) {
	// YUY2 conversion - NV_register_combiners path
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
	mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YUY2_NV1x_Y);
	VDASSERT(!mGL.glGetError());

	float y = 0.0f;
	float w2 = (float)w * 0.5f;
	{
		float u0 = -0.5f / (float)mGLTextureW;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		float u2 = +0.5f / (float)mGLTextureW;
		float u3 = u2 + u;

		mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glColor4f(0.0f, 0.0f, 1.0f, 0.0f);
		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v1);	mGL.glVertex2f(0.0f, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v1);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v1);	mGL.glVertex2f(w2, y0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u1, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u3, v0);	mGL.glVertex2f(w2, y1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);	mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u2, v0);	mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	{
		mGL.glCallList(mGLShaderBase + kVDOpenGLTechIndex_YUY2_NV1x_C);

		float u0 = 0;
		float v0 = 0;
		float u1 = u0 + u;
		float v1 = v0 + v;

		mGL.glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);

		float y0 = 0;
		float y1 = (float)h;

		mGL.glBegin(GL_QUADS);
			mGL.glTexCoord2f(u0, v1);	mGL.glVertex2f(0, y0);
			mGL.glTexCoord2f(u1, v1);	mGL.glVertex2f(w2, y0);
			mGL.glTexCoord2f(u1, v0);	mGL.glVertex2f(w2, y1);
			mGL.glTexCoord2f(u0, v0);	mGL.glVertex2f(0, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);
	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
	}
}

void VDScreenGrabberGL::ConvertToYUY2_GL_NV2x_ATIFS(int w, int h, float u, float v, bool atifs) {
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	mGL.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glEnable(GL_TEXTURE_2D);
	mGL.glBindTexture(GL_TEXTURE_2D, mGLTextures[0]);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);
	mGL.glCallList(mGLShaderBase + (atifs ? kVDOpenGLTechIndex_YUY2_ATIFS : kVDOpenGLTechIndex_YUY2_NV2x));
	VDASSERT(!mGL.glGetError());

	float y = 0.0f;
	float w2 = (float)w * 0.5f;

	mGL.glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	{
		float u0 = -0.5f / (float)mGLTextureW;		// Y1
		float u1 = +0.5f / (float)mGLTextureW;		// Y2
		float u2 = -1.0f / (float)mGLTextureW;		// chroma left
		float u3 =  0.0f / (float)mGLTextureW;		// chroma right
		float v0 = 0;
		float v1 = v0 + v;

		float y0 = y;
		float y1 = y + (float)h;

		mGL.glBegin(GL_QUADS);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3, v1);
			mGL.glVertex2f(0.0f, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+u, v1);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3+u, v1);
			mGL.glVertex2f(w2, y0);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2+u, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3+u, v0);
			mGL.glVertex2f(w2, y1);

			mGL.glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u0, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u1, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE2_ARB, u2, v0);
			mGL.glMultiTexCoord2fARB(GL_TEXTURE3_ARB, u3, v0);
			mGL.glVertex2f(0.0f, y1);
		mGL.glEnd();
		VDASSERT(!mGL.glGetError());
	}

	mGL.glActiveTextureARB(GL_TEXTURE3_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE2_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE1_ARB);
	mGL.glDisable(GL_TEXTURE_2D);
	mGL.glActiveTextureARB(GL_TEXTURE0_ARB);

	VDASSERT(!mGL.glGetError());
	mGL.glReadBuffer(GL_BACK);
	if (mGL.EXT_pixel_buffer_object) {
		mGL.glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, mGLBuffers[0]);
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (void *)0);
	} else {
		char *dst = (char *)mGLReadBuffer.data();
		mGL.glReadPixels(0, 0, w >> 1, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dst);
	}
}

GLuint VDScreenGrabberGL::GetOcclusionQueryPixelCountSafe(GLuint query) {
	GLint rv;
	mGL.glFlush();

	int iters = 1000;
	for(;;) {
		mGL.glGetOcclusionQueryivNV(query, GL_PIXEL_COUNT_AVAILABLE_NV, &rv);
		if (rv)
			break;

		if (iters)
			--iters;
		else
			::Sleep(1);
	}

	mGL.glGetOcclusionQueryivNV(query, GL_PIXEL_COUNT_NV, &rv);
	return rv;
}

LRESULT CALLBACK VDScreenGrabberGL::StaticWndProcGL(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

IVDScreenGrabber *VDCreateScreenGrabberGL() {
	return new VDScreenGrabberGL;
}
