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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#ifdef _DEBUG
	#include <vd2/VDXFrame/VideoFilter.h>

	class VDVideoFilterDebugCrop : public VDXVideoFilter {
	public:
		uint32 GetParams();
		void Run();
	};

	uint32 VDVideoFilterDebugCrop::GetParams() {
		if (fa->dst.mFrameCount > 10)
			fa->dst.mFrameCount -= 10;
		else
			fa->dst.mFrameCount = 0;

		return 0;
	}

	void VDVideoFilterDebugCrop::Run() {
	}

	extern const VDXFilterDefinition filterDef_debugcrop = VDXVideoFilterDefinition<VDVideoFilterDebugCrop>(
		NULL,
		"__debug crop",
		"Removes output frame count debugging purposes.");

	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{44,{flat}}' }'' : unreferenced local function has been removed
#endif
