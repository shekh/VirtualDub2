//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
//
//	As a special exemption to the normal GPL license that normally applies
//	to VirtualDub, this specific file (MPEGIDCT.h) is placed into the public
//	domain as of 06/07/2003.  The reason I have done so is that the test
//	harness (idct_test.cpp) uses this file and wouldn't be usable otherwise.
//
//	Note that the IDCT routines themselves are not in the public domain at
//	this time.

#ifndef f_VD2_MEIA_MPEGIDCT_H
#define f_VD2_MEIA_MPEGIDCT_H

// IDCT support
//
// The 2D inverse discrete cosine transform (2D-IDCT) is the biggest pain in the butt with
// regard to MPEG-1 decoding.  As such there are a number of arcane tricks here that one
// must be careful when playing with this module.
//
// There are two types of MPEG-1 blocks that must pass through the IDCT -- intra and
// non-intra.  Intra blocks decode to [0,255] and are simply plunked directly into the
// Y, Cb, or Cr plane after decoding.  Non-intra blocks decode to [-256,255] and are
// added to the plane with subtraction.  For speed, the IDCT routines incorporate the
// addition, clip, and store to the planes as part of the transform process.
//
// The MPEG decoder must pass the final stored coefficient sequence position to the
// decoder.  ('5' would indicate that the 6th coefficient along the zigzag order is
// the final non-zero coefficient.)  This is used for fast pruning of the transforms.
// Most transforms are composed of partially merged row/column sequences; the last
// coefficient is used to prune transform of all-zero rows.  If the non-zero region
// is limited to the top-left 4x4, some also use a form of vector-radix pruning to
// simplify the column transform.  All elements after the indicated end element must
// still be set to zero; it is always acceptable for an IDCT to set last_pos=63 on
// entry (this is useful for debugging).
//
// IDCTs generally fall into two types:
//
// o  AAN-derived IDCTs take advantage of the quantization matrix to hide some of the
//    transform multiplications for free.  These include the original Arai-Agui-Nakajima
//    IDCT as well as Feig-Winograd.  These are among the fastest 2D IDCTs but
//    unfortunately are not 100% compatible with MPEG-1 oddification; also, the folded
//    prescaling matrix requires greater input precision.
//
//    This type of IDCT is identified by a non-null pPrescaler member, and takes
//    32-bit coefficients in the 'src' parameter.
//
// o  Non-AAN-derived IDCTs generally are row-column based; these include plain
//    row-column transforms such as Lee and Hou, as well as Intel's AP-922 MMX algorithm.
//    These do not require a prescaling matrix, and are identified by a NULL pPrescaler.
//    These take 16-bit coefficients in the 'src' parameter.
//
// Essentially all IDCTs used by VirtualDub require an input permutation; this is handled
// via the pAltScan member, which allows an IDCT to specify a zigzag order other than
// MPEG/JPEG standard.
//
// All IDCTs must assume that MMX may be active and that x87 floating-point arithmetic
// cannot be used without a preceding EMMS instruction.  There are no such issues with
// SSE arithmetic.  MMX may be freely used in an IDCT without EMMS at the end.

typedef void (*tVDMPEGIDCT)(unsigned char *dst, int pitch, void *src, int last_pos);
typedef void (*tVDMPEGIDCTTest)(void *src, int last_pos);
typedef void (*tVDMPEGIDCTFinalizer)();

struct VDMPEGIDCTSet {
	tVDMPEGIDCT				pIntra;			// clip & place
	tVDMPEGIDCT				pNonintra;		// add, clip & place
	tVDMPEGIDCTTest			pTest;			// just compute - does not leave MMX state active
	const int				*pPrescaler;	// if non-null, a 64-entry, 8:8 fixed point matrix for AAN-derived
	const int				*pAltScan;		// if non-null, specifies alternative scanning order
	tVDMPEGIDCT				pIntra4x2;		// clip & place (4x2 for DV)
};

// idct_test.cpp

struct VDIDCTTestResult {
	int		mRangeLow, mRangeHigh, mSign, mIterations;
	int		mMaxErrors[64];				// worst per-cell absolute error
	double	mMeanErrors[64];			// per-cell bias
	double	mSquaredErrors[64];			// per-cell variance

	double	mAverageError;				// overall bias
	double	mWorstError;				// worst cell bias
	double	mAverageSquaredError;		// overall variance
	double	mWorstSquaredError;			// worst cell variance
	int		mMaximumError;				// worst error encountered

	bool	mbMaxErrorOK;
	bool	mbAverageErrorOK;
	bool	mbWorstErrorOK;
	bool	mbAverageSquaredErrorOK;
	bool	mbWorstSquaredErrorOK;
};

struct VDIDCTComplianceResult {
	VDIDCTTestResult tests[6];
	bool	mbZeroTestOK;
	bool	mbPassed;
};

bool VDTestVideoIDCTCompliance(const VDMPEGIDCTSet& idct, VDIDCTComplianceResult& result);

// idct_*.cpp

extern const VDMPEGIDCTSet g_VDMPEGIDCT_reference;				// Hou row/column

#ifndef _M_AMD64
extern const VDMPEGIDCTSet g_VDMPEGIDCT_scalar;					// Feig-Winograd (scalar)
extern const VDMPEGIDCTSet g_VDMPEGIDCT_mmx;					// Intel AP-922 (MMX)
extern const VDMPEGIDCTSet g_VDMPEGIDCT_isse;					// Intel AP-922 (MMX2) with 4x4 VR pruning
#endif
extern const VDMPEGIDCTSet g_VDMPEGIDCT_sse2;					// Intel AP-922 (SSE2) with 4x4 VR pruning

#endif
