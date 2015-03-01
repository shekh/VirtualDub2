//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#include <vd2/Riza/audiocodec.h>

IVDAudioCodec *VDCreateAudioDecompressor(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat) {
	switch(srcFormat->mTag) {
		case VDWaveFormat::kTagCCITTMuLaw:
			return VDCreateAudioDecompressorMuLaw(srcFormat, dstFormat);

		case VDWaveFormat::kTagCCITTALaw:
			return VDCreateAudioDecompressorALaw(srcFormat, dstFormat);

		case VDWaveFormat::kTagMPEG1:
		case VDWaveFormat::kTagMPEGLayer3:
			return VDCreateAudioDecompressorMPEG(srcFormat, dstFormat);
	}

	return NULL;
}
