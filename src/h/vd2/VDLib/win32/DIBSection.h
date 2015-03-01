//	VirtualDub - Video processing and capture application
//	Application helper library
//	Copyright (C) 1998-2007 Avery Lee
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

#ifndef f_VD2_VDLIB_WIN32_DIBSECTION_H
#define f_VD2_VDLIB_WIN32_DIBSECTION_H

#ifdef _MSC_VER
	#pragma once
#endif

struct HBITMAP__;
struct HDC__;
struct tagBITMAPINFO;
struct VDPixmap;
class VDFileMappingW32;

class VDDIBSectionW32 {
public:
	VDDIBSectionW32();
	~VDDIBSectionW32();

	bool	Init(int w, int h, int depth, const VDFileMappingW32 *mapping = NULL, uint32 mapOffset = 0);
	bool	Init(const tagBITMAPINFO *bi, const VDFileMappingW32 *mapping = NULL, uint32 mapOffset = 0);
	void	Shutdown();

	void		*GetPointer()	const { return mpBits; }
	HDC__		*GetHDC()		const { return mhdc; }
	HBITMAP__	*GetHBITMAP()	const { return mhbm; }

	VDPixmap	GetPixmap()		const;

protected:
	void		*mpBits;
	HBITMAP__	*mhbm;
	HDC__		*mhdc;
	void		*mhgo;
	int			mWidth;
	int			mHeight;
	int			mDepth;
	ptrdiff_t	mPitch;
	void		*mpScan0;
	bool		mbForceUnmap;
};

#endif
