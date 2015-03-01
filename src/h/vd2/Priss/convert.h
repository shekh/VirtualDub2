//	VirtualDub - Video processing and capture application
//	Audio processing library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_PRISS_CONVERT_H
#define f_VD2_PRISS_CONVERT_H

#include <vd2/system/vdtypes.h>

/////// PCM conversion

typedef void (VDAPIENTRY *tpVDConvertPCM)(void *dst, const void *src, uint32 samples);

void VDConvertPCM32FToPCM8	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM32FToPCM16	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM16ToPCM8	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM16ToPCM32F	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM8ToPCM16	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM8ToPCM32F	(void *dst0, const void *src0, uint32 samples);

void VDConvertPCM16ToPCM8_MMX	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM8ToPCM16_MMX	(void *dst0, const void *src0, uint32 samples);

void VDConvertPCM32FToPCM8_SSE	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM32FToPCM16_SSE	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM16ToPCM32F_SSE	(void *dst0, const void *src0, uint32 samples);
void VDConvertPCM8ToPCM32F_SSE	(void *dst0, const void *src0, uint32 samples);

typedef const tpVDConvertPCM (*tpVDConvertPCMVtbl)[3];

enum VDAudioSampleType {
	kVDAudioSampleType8U,
	kVDAudioSampleType16S,
	kVDAudioSampleType32F,
	kVDAudioSampleTypeCount
};

tpVDConvertPCMVtbl VDGetPCMConversionVtable();

/////// FIR filtering

struct VDAudioFilterVtable {
	sint16 (VDAPIENTRY *FilterPCM16)(const sint16 *src, const sint16 *filter, uint32 filterquadsize);
	void (VDAPIENTRY *FilterPCM16End)();
	void (VDAPIENTRY *FilterPCM16SymmetricArray)(sint16 *dst, ptrdiff_t dst_stride, const sint16 *src_center, uint32 count, const sint16 *filter, uint32 filterquadsizeminus1);
};

const VDAudioFilterVtable *VDGetAudioFilterVtable(uint32 taps = 0);

#endif
