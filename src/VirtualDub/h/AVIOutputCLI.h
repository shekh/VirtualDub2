//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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

#ifndef f_AVIOUTPUTCLI_H
#define f_AVIOUTPUTCLI_H

#include <vd2/system/unknown.h>
#include "ExternalEncoderProfile.h"

struct VDPixmapLayout;

struct VDAVIOutputCLITemplate {
	vdrefptr<VDExtEncProfile> mpVideoEncoderProfile;
	vdrefptr<VDExtEncProfile> mpAudioEncoderProfile;
	vdrefptr<VDExtEncProfile> mpMultiplexerProfile;
	bool mbUseOutputPathAsTemp;
};

class IAVIOutputCLI : public IVDUnknown {
public:
	enum { kTypeID = 'aocl' };

	virtual ~IAVIOutputCLI() {}
	virtual void SetInputLayout(const VDPixmapLayout& layout) = 0;
	virtual void SetOpt(DubOptions& opt) = 0;
	virtual void SetBufferSize(sint32 nBytes) = 0;
	virtual void CloseWithoutFinalize() = 0;
};

IAVIOutputCLI *VDCreateAVIOutputCLI(const VDAVIOutputCLITemplate& templ);

#endif
