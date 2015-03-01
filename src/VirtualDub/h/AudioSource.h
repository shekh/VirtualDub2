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

#ifndef f_AUDIOSOURCE_H
#define f_AUDIOSOURCE_H

#include <vd2/Riza/audiocodec.h>
#include "DubSource.h"

class IAVIReadHandler;
class IAVIReadStream;

class AudioSource : public DubSource {
public:
	const VDWaveFormat *getWaveFormat() const {
		return (const VDWaveFormat *)getFormat();
	}
};

AudioSource *VDCreateAudioSourceWAV(const wchar_t *fn, uint32 inputBufferSize);

#endif
