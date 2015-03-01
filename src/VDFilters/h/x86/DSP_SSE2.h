//	VirtualDub - Video processing and capture application
//	Internal filter library
//	Copyright (C) 1998-2011 Avery Lee
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

#ifndef f_VD2_VDFILTERS_X86_DSP_SSE2_H
#define f_VD2_VDFILTERS_X86_DSP_SSE2_H

void VDDSPBlend8_LerpConst_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16, uint8 factor);
void VDDSPBlend8_Min_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Max_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Add_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Multiply_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_LinearBurn_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Screen_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_HardLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_LinearLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_PinLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_HardMix_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Overlay_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);
void VDDSPBlend8_Difference_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16);

void VDDSPBlend8_Select_SSE2(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16);
void VDDSPBlend8_Lerp_SSE2(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16);

#endif
