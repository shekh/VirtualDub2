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
#include <vfw.h>

#include "command.h"

///////////////////////////////////////////////////////////////////////////

#define R(x) ((((x)&0xFF000000)>>24) | (((x)&0x00FF0000)>>8) | (((x)&0x0000FF00)<<8) | (((x)&0x000000FF)<<24))

const struct CodecEntry {
	FOURCC fcc;
	const char *name;
} codec_entries[]={
	{ R('VCR1'), "ATI video 1" },
	{ R('VCR2'), "ATI video 2" },
	{ R('TR20'), "Duck TrueMotion 2.0" },
	{ R('dvsd'), "DV" },
	{ R('HFYU'), "Huffyuv" },
	{ R('I263'), "Intel H.263" },
	{ R('I420'), "LifeView YUV12 codec" },
	{ R('IR21'), "Indeo Video 2.1" },
	{ R('IV31'), "Indeo Video 3.1" },
	{ R('IV32'), "Indeo Video 3.2" },
	{ R('IV41'), "Indeo Video 4.1" },
	{ R('IV50'), "Indeo Video 5.x" },
	{ R('UCOD'), "Iterated Systems' ClearVideo" },
	{ R('mjpg'), "Motion JPEG" },
	{ R('MJPG'), "Motion JPEG" },
	{ R('dmb1'), "Motion JPEG (Matrox)" },
	{ R('MPG4'), "Microsoft High-Speed MPEG-4 " },
	{ R('MP42'), "Microsoft High-Speed MPEG-4 V2" },
	{ R('MP43'), "Microsoft High-Speed MPEG-4 V3" },
	{ R('DIV3'), "Microsoft High-Speed MPEG-4 V3 [Hack: DivX Low-Motion]" },
	{ R('DIV4'), "Microsoft High-Speed MPEG-4 V3 [Hack: DivX Fast-Motion]" },
	{ R('AP41'), "Microsoft High-Speed MPEG-4 V3 [Hack: AngelPotion Definitive]" },
	{ R('MRLE'), "Microsoft RLE" },
	{ R('MSVC'), "Microsoft Video 1" },
	{ R('CRAM'), "Microsoft Video 1" },
	{ R('DIVX'), "DivX 4+" },
	{ R('CVID'), "Radius Cinepak" },
	{ R('VIVO'), "VivoActive" },

	{ R('vifp'), "VFAPI reader codec" },
	{ R('VDST'), "VirtualDub frameclient driver" },
};

#undef R

const char *LookupVideoCodec(uint32 fccType) {
	int i;

	for(i=0; i<3; i++) {
		int c = (int)((fccType>>(8*i)) & 255);

		if (isalpha(c))
			fccType = (fccType & ~(FOURCC)(0xff << (i*8))) | (toupper(c) << (i*8));
	}

	for(i=0; i<sizeof codec_entries/sizeof codec_entries[0]; i++)
		if (codec_entries[i].fcc == fccType)
			return codec_entries[i].name;

	return NULL;
}
