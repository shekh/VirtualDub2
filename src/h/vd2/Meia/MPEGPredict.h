//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_VD2_MEIA_MPEGPREDICT_H
#define f_VD2_MEIA_MPEGPREDICT_H

#include <stddef.h>

// MPEG prediction
//
// Motion prediction involves extracting an 8x8 block (Cr/Cb) or 16x16 block (Y)
// from reference frames into the current image.  The complicating factors are that
// predictions may occur on half-pel boundaries, thus requiring four different
// routines per block size, and that bidirectional predictions require averaging
// two predictions, necessitating both 'copy' and 'add' routines.  Thus there are
// 4 offsets x 2 block sizes x 2 modes = 16 routines.
//
// MMX is assumed to be active on entry and exit to the predictors; no EMMS is
// necessary unless x87 floating point is desired (yuck!!).  Destination scanlines
// must be aligned to a 16 byte boundary and the source pitch must be a multiple
// of 16 bytes.

typedef void (*tVDMPEGPredictor)(unsigned char *dst, unsigned char *src, ptrdiff_t pitch);

struct VDMPEGPredictorSet {
	tVDMPEGPredictor	Y_predictors[2][2];
	tVDMPEGPredictor	C_predictors[2][2];
	tVDMPEGPredictor	Y_adders[2][2];
	tVDMPEGPredictor	C_adders[2][2];
};

extern const VDMPEGPredictorSet g_VDMPEGPredict_reference;
extern "C" const VDMPEGPredictorSet g_VDMPEGPredict_scalar;
extern "C" const VDMPEGPredictorSet g_VDMPEGPredict_mmx;
extern "C" const VDMPEGPredictorSet g_VDMPEGPredict_isse;
extern "C" const VDMPEGPredictorSet g_VDMPEGPredict_sse2;

#endif
