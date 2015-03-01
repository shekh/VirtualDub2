#ifndef f_VD2_F_RESIZE_CONFIG_H
#define f_VD2_F_RESIZE_CONFIG_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vectors.h>

class IVDPixmapResampler;
class IVDXFilterPreview2;

struct VDResizeFilterData {
	enum {
		FILTER_NONE				= 0,
		FILTER_BILINEAR			= 1,
		FILTER_BICUBIC			= 2,
		FILTER_TABLEBILINEAR	= 3,
		FILTER_TABLEBICUBIC075	= 4,
		FILTER_TABLEBICUBIC060	= 5,
		FILTER_TABLEBICUBIC100	= 6,
		FILTER_LANCZOS3			= 7,
		FILTER_COUNT
	};

	enum {
		kFrameModeNone,
		kFrameModeToSize,
		kFrameModeARCrop,
		kFrameModeARLetterbox,
		kFrameModeCount
	};

	enum {
		kImageAspectNone,
		kImageAspectUseSource,
		kImageAspectCustom,
		kImageAspectModeCount
	};

	double	mImageW;
	double	mImageH;
	double	mImageRelW;
	double	mImageRelH;
	bool	mbUseRelative;

	double	mImageAspectNumerator;
	double	mImageAspectDenominator;
	uint32	mImageAspectMode;
	uint32	mFrameW;
	uint32	mFrameH;
	double	mFrameAspectNumerator;
	double	mFrameAspectDenominator;
	uint32	mFrameMode;
	uint32	mFilterMode;
	uint32	mAlignment;
	uint32	mFillColor;

	bool	mbInterlaced;

	vdrect32f	mDstRect;

	static const char *const kFilterNames[];

	VDResizeFilterData();

	const char *Validate() const;

	void ComputeSizes(uint32 srcw, uint32 srch, double& imgw, double& imgh, uint32& framew, uint32& frameh, bool useAlignment, bool widthHasPriority, int format);
	void ComputeDestRect(uint32 outw, uint32 outh, double dstw, double dsth);
	void UpdateInformativeFields(uint32 srcw, uint32 srch, bool widthHasPriority);

	bool ExchangeWithRegistry(bool write);
};

bool VDFilterResizeActivateConfigDialog(VDResizeFilterData& mfd, IVDXFilterPreview2 *ifp2, uint32 w, uint32 h, VDGUIHandle hParent);

#endif	// f_VD2_F_RESIZE_CONFIG_H
