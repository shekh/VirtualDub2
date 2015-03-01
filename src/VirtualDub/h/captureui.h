//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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

#ifndef f_CAPTUREUI_H
#define f_CAPTUREUI_H

class IVDCaptureProject;

class VDINTERFACE IVDCaptureProjectUI : public IVDRefCount {
public:
	virtual bool	Attach(VDGUIHandle hwnd, IVDCaptureProject *pProject) = 0;
	virtual void	Detach() = 0;

	virtual bool	SetDriver(const wchar_t *s) = 0;
	virtual void	SetCaptureFile(const wchar_t *s) = 0;
	virtual void	PreallocateCaptureFile(sint64 size) = 0;
	virtual bool	SetTunerChannel(int ch) = 0;
	virtual bool	SetTunerExactFrequency(uint32 freq) = 0;
	virtual void	SetTunerInputMode(bool cable) = 0;
	virtual void	SetTimeLimit(int limitsecs) = 0;
	virtual void	SetAudioCaptureEnabled(bool enable) = 0;
	virtual void	SetAudioPlaybackEnabled(bool enable) = 0;
	virtual void	Capture() = 0;
};

IVDCaptureProjectUI *VDCreateCaptureProjectUI();

#endif
