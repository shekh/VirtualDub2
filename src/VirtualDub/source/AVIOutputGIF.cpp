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

#include <vd2/system/error.h>
#include <vd2/system/fileasync.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include "AVIOutput.h"
#include "AVIOutputGIF.h"

extern uint32 VDPreferencesGetFileAsyncDefaultMode();
extern uint32& VDPreferencesGetRenderOutputBufferSize();

////////////////////////////////////

class AVIVideoGIFOutputStream : public AVIOutputStream {
public:
	AVIVideoGIFOutputStream(IVDFileAsync *pAsync);
	~AVIVideoGIFOutputStream();

	void init(int loopCount);
	void finalize();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

private:
	void FlushFrame();

	IVDFileAsync *mpAsync;
	vdblock<uint8>	mPrevFrameBuffer;
	vdblock<uint8>	mCurFrameBuffer;
	vdblock<uint8>	mPackBuffer;
	uint32			mExtraLeader;
	uint32			mFrameCount;
	int				mLoopCount;

	uint32			mPrevTimestamp;

	VDPixmapLayout	mSrcLayout;
	VDPixmapLayout	mDstLayout;

	VDPixmapBuffer	mPreviousFrame;
	VDPixmapBuffer	mConvertBuffer;
	VDPixmapBuffer	mPrevConvertBuffer;
	vdfastvector<uint8> mFrameData;
};

AVIVideoGIFOutputStream::AVIVideoGIFOutputStream(IVDFileAsync *pAsync)
	: mpAsync(pAsync)
	, mFrameCount(0)
	, mPrevTimestamp(0)
{
}

AVIVideoGIFOutputStream::~AVIVideoGIFOutputStream() {
}

void AVIVideoGIFOutputStream::init(int loopCount) {
	mLoopCount = loopCount;

	const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)getFormat();
	int variant;

	int format = VDBitmapFormatToPixmapFormat(*bih, variant);

	if (!format)
		throw MyError("The current output format is not an uncompressed format that can be converted to an animated GIF.");

	VDMakeBitmapCompatiblePixmapLayout(mSrcLayout, bih->biWidth, bih->biHeight, format, variant);

	mConvertBuffer.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);
	mPrevConvertBuffer.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);

	mPreviousFrame.init(bih->biWidth, bih->biHeight, nsVDPixmap::kPixFormat_XRGB8888);

	uint32 size = VDPixmapCreateLinearLayout(mDstLayout, nsVDPixmap::kPixFormat_Pal8, bih->biWidth, bih->biHeight, 1);
	mPrevFrameBuffer.resize(size);
	mCurFrameBuffer.resize(size);
	mPackBuffer.resize(size);

	// Write header and logical screen descriptor.
	struct Header {
		// header
		uint8	mSignature[6];		// GIF89a

		// logical screen descriptor
		uint8	mWidth[2];
		uint8	mHeight[2];
		uint8	mFlags;
		uint8	mBackgroundColorIndex;
		uint8	mPixelAspectRatio;
	} hdr;

	hdr.mSignature[0] = 'G';
	hdr.mSignature[1] = 'I';
	hdr.mSignature[2] = 'F';
	hdr.mSignature[3] = '8';
	hdr.mSignature[4] = '9';
	hdr.mSignature[5] = 'a';
	VDWriteUnalignedLEU16(&hdr.mWidth, (uint16)(bih->biWidth));
	VDWriteUnalignedLEU16(&hdr.mHeight, (uint16)(bih->biHeight));
//	hdr.mFlags = 0xF7;		// global color table, 256 original, unsorted, 256 now
	hdr.mFlags = 0x70;		// no global color table, 256 original, unsorted, GCT size=0
	hdr.mPixelAspectRatio		= 0x31;
	hdr.mBackgroundColorIndex	= 0;
	mpAsync->FastWrite(&hdr, sizeof hdr);

#if 0
	// write global color table
	for(int i=0; i<256; ++i) {
		uint8 c[3];

		c[0] = (uint8)((i / 42) * 51);
		c[1] = (uint8)((((i / 6) % 7) * 85) >> 1);
		c[2] = (uint8)((i % 6) * 51);
		mpAsync->FastWrite(c, 3);
	}
#endif

	// write loop extension
	if (mLoopCount != 1) {
		struct LoopExtension {
			uint8	mExtensionCode;
			uint8	mAppExtensionLabel;
			uint8	mAppBlockLength;
			uint8	mSignature[11];
			uint8	mSubBlockLength;
			uint8	mExtensionType;
			uint8	mIterations[2];
			uint8	mSubBlockTerminator;
		} loopext;
		VDASSERTCT(sizeof(loopext) == 19);

		loopext.mExtensionCode = 0x21;
		loopext.mAppExtensionLabel = 0xFF;
		loopext.mAppBlockLength = 11;
		memcpy(loopext.mSignature, "NETSCAPE2.0", 11);
		loopext.mSubBlockLength = 3;
		loopext.mExtensionType = 1;
		VDWriteUnalignedLEU16(loopext.mIterations, (uint16)(mLoopCount ? mLoopCount - 1 : 0));
		loopext.mSubBlockTerminator = 0;
		mpAsync->FastWrite(&loopext, sizeof loopext);
	}
}

void AVIVideoGIFOutputStream::finalize() {
	// Flush out any remaining frame.
	FlushFrame();

	// Write GIF terminator byte.
	static const uint8 kTerminator = 0x3B;
	mpAsync->FastWrite(&kTerminator, 1);

	sint64 pos = mpAsync->GetFastWritePos();
	mpAsync->FastWriteEnd();
	mpAsync->Truncate(pos);
	mpAsync->Close();
}

namespace {
	class VDPixmapColorQuantizerOctree {
		struct Node {
			sint16	mListNext;
			uint32	mBlueTotal;
			uint32	mGreenTotal;
			uint32	mRedTotal;
			uint32	mColorTotal;
			bool	mbInternalNode;
			bool	mbLeaf;
			sint16	mNext[8];
		};
	public:
		void Init();
		void InsertColor(uint8 r, uint8 g, uint8 b);
		void CreatePalette();
		uint8 SearchOctree(uint8 r, uint8 g, uint8 b);

		const uint32 *GetPalette() const { return mPalette; }
		uint32 GetPaletteSize() const { return mLeafCount; }

	protected:
		sint16	ReduceNode(sint16 nodeIndex);
		uint8	AssignColors(Node *oct, uint8 index);

		int		mReductionLevel;
		int		mLeafCount;
		vdfastvector<Node>	mNodes;

		sint16	mNextFree;
		sint16	mLevelInternalNodes[8];

		uint32	mPalette[256];
	};

	void VDPixmapColorQuantizerOctree::Init() {
		Node temp = {0};
		mNodes.resize(2048, temp);

		for(int i=0; i<8; ++i)
			mLevelInternalNodes[i] = 0;

		mReductionLevel = 0;
		mNextFree = 1;
		mLeafCount = 1;

		for(int i=1; i<2047; ++i)
			mNodes[i].mListNext = (sint16)(i+1);
	}

	void VDPixmapColorQuantizerOctree::InsertColor(uint8 r, uint8 g, uint8 b) {
		sint16 nodeIndex = 0;

		int depth = 0;
		do {
			Node *node = &mNodes[nodeIndex];
			uint8 lv = (uint8)(0x80 >> depth);
			int i = 0;
			if (r & lv)
				i = 4;

			if (g & lv)
				i += 2;

			if (b & lv)
				++i;

			sint16 childIndex = node->mNext[i];
			if (!childIndex) {
				if (node->mbLeaf)
					break;

				if (!mNextFree) {
					sint16 nodeIndex2 = ReduceNode(nodeIndex);

					if (nodeIndex2) {
						nodeIndex = nodeIndex2;
						goto addit;
					}
				}

				VDASSERT(mNextFree);
				childIndex = mNextFree;

				VDASSERT((uint32)i < 8);
				node->mNext[i] = childIndex;

				Node *child = &mNodes[childIndex];
				mNextFree = child->mListNext;

				child->mRedTotal = 0;
				child->mGreenTotal = 0;
				child->mBlueTotal = 0;
				child->mColorTotal = 0;
				child->mbInternalNode = false;
				child->mbLeaf = false;
				for(int j=0; j<8; ++j)
					child->mNext[j] = 0;

				++mLeafCount;

				if (!node->mbInternalNode) {
					node->mbInternalNode = true;
					--mLeafCount;

					VDASSERT(depth < 7);
					node->mListNext = mLevelInternalNodes[depth];
					mLevelInternalNodes[depth] = nodeIndex;

					if (mReductionLevel < depth)
						mReductionLevel = depth;
				}
			}

			nodeIndex = childIndex;
			++depth;
		} while(depth < 7);

addit:
		Node *node = &mNodes[nodeIndex];
		node->mBlueTotal	+= b;
		node->mGreenTotal	+= g;
		node->mRedTotal		+= r;
		++node->mColorTotal;
	}

	sint16 VDPixmapColorQuantizerOctree::ReduceNode(sint16 nodeIndex) {
		sint16 reduceIndex;

		for(;;) {
			reduceIndex = mLevelInternalNodes[mReductionLevel];
			if (reduceIndex) {
				Node *reduceNode = &mNodes[reduceIndex];
				VDASSERT(reduceNode->mbInternalNode);

				mLevelInternalNodes[mReductionLevel] = reduceNode->mListNext;

				for(int j=0; j<8; ++j) {
					sint16 reduceChildIndex = reduceNode->mNext[j];
					if (reduceChildIndex) {
						reduceNode->mNext[j] = 0;

						if (reduceChildIndex == nodeIndex)
							nodeIndex = reduceIndex;

						Node *reduceChildNode = &mNodes[reduceChildIndex];
						reduceNode->mRedTotal	+= reduceChildNode->mRedTotal;
						reduceNode->mGreenTotal	+= reduceChildNode->mGreenTotal;
						reduceNode->mBlueTotal	+= reduceChildNode->mBlueTotal;
						reduceNode->mColorTotal	+= reduceChildNode->mColorTotal;

						VDASSERT(!reduceChildNode->mbInternalNode);

						reduceChildNode->mListNext = mNextFree;
						mNextFree = reduceChildIndex;
						--mLeafCount;
					}
				}

				++mLeafCount;
				reduceNode->mbLeaf = true;
				reduceNode->mbInternalNode = false;
				VDASSERT(mNextFree);

				if (reduceIndex == nodeIndex)
					return nodeIndex;

				return 0;
			}

			if (!mReductionLevel)
				break;

			--mReductionLevel;
		}

		return nodeIndex;
	}

	void VDPixmapColorQuantizerOctree::CreatePalette() {
		memset(mPalette, 0, sizeof mPalette);

		while(mLeafCount > 255)
			ReduceNode(0);

		AssignColors(&mNodes[0], 0);
	}

	uint8 VDPixmapColorQuantizerOctree::AssignColors(Node *node, uint8 palidx) {
		bool has_leaves = false;

		for(int i=0; i<8; ++i) {
			sint16 index = node->mNext[i];

			if (index) {
				palidx = AssignColors(&mNodes[index], palidx);
				has_leaves = true;
			}
		}

		if (!has_leaves) {
			uint32 n = node->mColorTotal;

			if (n) {
				uint8 r = (uint8)(((node->mRedTotal*2+1) / n) >> 1);
				uint8 g = (uint8)(((node->mGreenTotal*2+1) / n) >> 1);
				uint8 b = (uint8)(((node->mBlueTotal*2+1) / n) >> 1);

				mPalette[palidx] = ((uint32)r << 16) + ((uint32)g << 8) + (uint32)b;

				node->mColorTotal = palidx;

				++palidx;
			}
		}

		return palidx;
	}

	uint8 VDPixmapColorQuantizerOctree::SearchOctree(uint8 r, uint8 g, uint8 b) {
		const Node *const nodes = mNodes.data();
		const Node *node = nodes;
		uint8 lv = 0x80;

		for(;;) {
			int t=0;
			if (r & lv) t+=4;
			if (g & lv) t+=2;
			if (b & lv) ++t;

			sint16 childIndex = node->mNext[t];

			if (!childIndex)
				break;

			lv >>= 1;
			node = nodes + childIndex;
		}

		return (uint8)node->mColorTotal;
	}

	bool istrans(uint32 a, uint32 b, uint32 c) {
		sint32 ar = (a >> 16) & 0xff;
		sint32 ag = (a >>  8) & 0xff;
		sint32 ab = (a >>  0) & 0xff;
		sint32 br = (b >> 16) & 0xff;
		sint32 bg = (b >>  8) & 0xff;
		sint32 bb = (b >>  0) & 0xff;
		sint32 cr = (c >> 16) & 0xff;
		sint32 cg = (c >>  8) & 0xff;
		sint32 cb = (c >>  0) & 0xff;

		// istrans(a, b, c) => length(a-b) <= length(c-b)

		sint32 dr = ar - br;
		sint32 dg = ag - bg;
		sint32 db = ab - bb;
		sint32 er = cr - br;
		sint32 eg = cg - bg;
		sint32 eb = cb - bb;

		return (dr*dr + dg*dg + db*db) <= (er*er + eg*eg + eb*eb);
	}
}

void AVIVideoGIFOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	if (!cbBuffer) {
		++mFrameCount;
		return;
	}

	mPrevFrameBuffer.swap(mCurFrameBuffer);
	mPrevConvertBuffer.swap(mConvertBuffer);

	if (cbBuffer)
		VDPixmapBlt(mConvertBuffer, VDPixmapFromLayout(mSrcLayout, (void *)pBuffer));

	VDPixmap pxsrc = mConvertBuffer;

	VDPixmap pxdst(VDPixmapFromLayout(mDstLayout, mCurFrameBuffer.data()));

	VDPixmapColorQuantizerOctree quant;
	quant.Init();

	if (!mFrameCount) {
		for(uint32 y=0; y<pxdst.h; ++y) {
			const uint8 *src = (const uint8 *)vdptroffset(pxsrc.data, y * pxsrc.pitch);

			for(uint32 x=0; x<pxdst.w; ++x) {
				quant.InsertColor(src[2], src[1], src[0]);
				src += 4;
			}
		}

		quant.CreatePalette();

		for(uint32 y=0; y<pxdst.h; ++y) {
			const uint8 *src = (const uint8 *)vdptroffset(pxsrc.data, y * pxsrc.pitch);
			uint8 *dst = (uint8 *)vdptroffset(pxdst.data, y * pxdst.pitch);

			for(uint32 x=0; x<pxdst.w; ++x) {
				*dst++ = quant.SearchOctree(src[2], src[1], src[0]);
				src += 4;
			}
		}
	} else {
		for(uint32 y=0; y<pxdst.h; ++y) {
			const uint8 *src = (const uint8 *)vdptroffset(pxsrc.data, y * pxsrc.pitch);
			const uint8 *prev = (const uint8 *)vdptroffset(mPrevConvertBuffer.data, y * mPrevConvertBuffer.pitch);

			for(uint32 x=0; x<pxdst.w; ++x) {
				if (src[0] != prev[0] || src[1] != prev[1] || src[2] != prev[2])
					quant.InsertColor(src[2], src[1], src[0]);

				prev += 4;
				src += 4;
			}
		}

		quant.CreatePalette();

		for(uint32 y=0; y<pxdst.h; ++y) {
			const uint8 *src = (const uint8 *)vdptroffset(pxsrc.data, y * pxsrc.pitch);
			const uint8 *prev = (const uint8 *)vdptroffset(mPrevConvertBuffer.data, y * mPrevConvertBuffer.pitch);
			uint8 *dst = (uint8 *)vdptroffset(pxdst.data, y * pxdst.pitch);

			for(uint32 x=0; x<pxdst.w; ++x) {
				if (src[0] != prev[0] || src[1] != prev[1] || src[2] != prev[2])
					*dst++ = quant.SearchOctree(src[2], src[1], src[0]);
				else
					*dst++ = 255;

				prev += 4;
				src += 4;
			}
		}
	}

	const uint32 *pal = quant.GetPalette();

	uint32 w = pxdst.w;
	uint32 h = pxdst.h;
	uint32 winx1 = 0;
	uint32 winy1 = 0;
	uint32 winx2 = w;
	uint32 winy2 = h;

	if (!mFrameCount) {
		memcpy(mPackBuffer.data(), mCurFrameBuffer.data(), w*h);
		VDPixmapBlt(mPreviousFrame, pxsrc);
	} else {
		const uint8 *srcp = mPrevFrameBuffer.data();
		const uint8 *srcc = mCurFrameBuffer.data();
		const uint32 *curp = (const uint32 *)pxsrc.data;
		uint32 *prevp = (uint32 *)mPreviousFrame.data;
		uint8 *dst = mPackBuffer.data();

		bool topRegion = true;
		winx1 = w;
		winx2 = 0;

		for(uint32 y=0; y<h; ++y, (srcc += w), (srcp += w), (dst += w)) {
			uint32 x;

//			for(x=0; x<w && srcp[x] == srcc[x]; ++x)
//				dst[x] = 255;
			for(x=0; x<w && (curp[x] == 255 || istrans(prevp[x], curp[x], pal[srcc[x]])); ++x)
				dst[x] = 255;

			if (x >= w) {
				if (topRegion)
					winy1 = y+1;

				vdptrstep(prevp, mPreviousFrame.pitch);
				vdptrstep(curp, pxsrc.pitch);
				continue;
			}

			topRegion = false;

			uint32 x2 = x;
			for(; x<w; ++x) {
				uint8 c = srcc[x];

//				if (c == srcp[x])
//					c = 255;
//				else
				if (c != 255) {
					if (istrans(prevp[x], curp[x], pal[c]))
						c = 255;
					else {
						x2 = x+1;
						prevp[x] = pal[c];
					}
				}

				dst[x] = c;
			}

			// determine min/max
			uint32 xr = w;
			while(xr && dst[xr-1] == 255)
				--xr;

			uint32 xl = 0;
			while(xl < xr && dst[xl] == 255)
				++xl;

			// widen window as necessary
			if (winx1 > xl)
				winx1 = xl;

			if (winx2 < xr)
				winx2 = xr;

			winy2 = y+1;
			vdptrstep(prevp, mPreviousFrame.pitch);
			vdptrstep(curp, pxsrc.pitch);
		}

		if (topRegion)
			winx1 = winx2 = winy1 = winy2 = 0;
	}

	// bail if the frame is empty and we're not on frame 0
	if (winx1 >= winx2 || winy1 >= winy2) {
		++mFrameCount;
		return;
	}

	// flush previous frame now that we know the delay
	FlushFrame();

	// Write GIF graphic control extension.
	struct GraphicControlExtension {
		uint8	mExtensionSignature;
		uint8	mExtensionCode;
		uint8	mBlockSize;
		uint8	mFlags;
		uint8	mDelayTime[2];
		uint8	mTransparentIndex;
		uint8	mBlockTerminator;
	} gconext;

	gconext.mExtensionSignature = 0x21;
	gconext.mExtensionCode = 0xF9;
	gconext.mBlockSize = 4;
	gconext.mFlags = 0x04;		// do not dispose; no user input; no transparent color

	// Note that we will update this in FlushFrame() when we know the next non-empty delta frame.
	gconext.mDelayTime[0] = 0;
	gconext.mDelayTime[1] = 0;

	gconext.mTransparentIndex = 0;
	gconext.mBlockTerminator = 0;

	uint32 palsize = 2;
	int palbits = 1;
	while(palbits < 8 && palsize < quant.GetPaletteSize() + 1) {
		palsize += palsize;
		++palbits;
	}

	if (mFrameCount) {
		gconext.mFlags |= 0x01;
		gconext.mTransparentIndex = (uint8)(palsize - 1);
	}

	mFrameData.insert(mFrameData.end(), (const uint8 *)&gconext, (const uint8 *)(&gconext + 1));

	// Write GIF image descriptor.
	struct ImageDescriptor {
		uint8	mImageSeparator;
		uint8	mX[2];
		uint8	mY[2];
		uint8	mWidth[2];
		uint8	mHeight[2];
		uint8	mFlags;
	} imgdesc;

	imgdesc.mImageSeparator = 0x2C;
	VDWriteUnalignedLEU16(&imgdesc.mX, (uint16)winx1);
	VDWriteUnalignedLEU16(&imgdesc.mY, (uint16)winy1);
	VDWriteUnalignedLEU16(&imgdesc.mWidth, (uint16)(winx2 - winx1));
	VDWriteUnalignedLEU16(&imgdesc.mHeight, (uint16)(winy2 - winy1));
//	imgdesc.mFlags = 0x00;		// no local color table, no interlace, unsorted, LCT size=0
	imgdesc.mFlags = 0x80 + (uint8)(palbits - 1);		// use local color table, no interlace, unsorted, LCT size=2-256

	mFrameData.insert(mFrameData.end(), (const uint8 *)&imgdesc, (const uint8 *)(&imgdesc + 1));

	// write local color table
	for(int i=0; i<palsize; ++i) {
		uint32 rgb = pal[i];

		uint8 c[3] = {
			(uint8)(rgb >> 16),
			(uint8)(rgb >> 8),
			(uint8)rgb,
		};

		mFrameData.push_back(c[0]);
		mFrameData.push_back(c[1]);
		mFrameData.push_back(c[2]);
	}

	// 1-bit images must use 2 bits per GIF89a spec.
	const uint8 initialCodeSize = palbits > 1 ? (uint8)palbits : 2;
	mFrameData.push_back(initialCodeSize);

	// LZW compress image
	struct DictionaryEntry {
		sint32	mPrevAndLastChar;
		sint32	mHashNext;
	} dict[4096];

	sint32 hash[256];
	for(int i=0; i<256; ++i)
		hash[i] = -1;

	for(int i=0; i<palsize; ++i)
		dict[i].mPrevAndLastChar = (-1 << 16) + i;

	for(int i=palsize; i<256; ++i)
		dict[i].mPrevAndLastChar = -1;

	const uint32 clearCode = 1 << initialCodeSize;
	int nextEntry = clearCode + 2;
	uint32 codebits = initialCodeSize + 1;
	sint32 nextBump = 1 << codebits;

	uint8 buf[260];
	uint32 dstidx = 1;
	uint32 accum = clearCode;
	uint32 accbits = codebits;
	uint8 colorMask = (uint8)(palsize - 1);

	if (winy2 > winy1) {
		const uint8 *src = &mPackBuffer[winx1 + w*winy1];
		uint32 winx = 0;
		uint32 winw = winx2 - winx1;
		uint32 winh = winy2 - winy1;

		sint32 code = (src[winx] & colorMask) << 16;
		if (++winx >= winw) {
			src += w;
			winx = 0;
			--winh;
		}

		while(winh) {
			uint8 c = src[winx] & colorMask;
			if (++winx >= winw) {
				src += w;
				winx = 0;
				--winh;
			}

			// combine current string with next character
			sint32 nextCode = code + c;

			// search hash table for (s,c)
			uint32 hidx = ((uint32)nextCode + ((uint32)nextCode >> 16)) & 255;
			sint32 hpos;
			for(hpos = hash[hidx]; hpos >= 0; hpos = dict[hpos].mHashNext) {
				if (dict[hpos].mPrevAndLastChar == nextCode)
					break;
			}

			// did we find the new code?
			if (hpos < 0) {
				// no, emit the existing code.
				accum += (code >> 16) << accbits;
				accbits += codebits;

				// flush bytes as necessary
				while(accbits >= 8) {
					buf[dstidx++] = (uint8)accum;
					accum >>= 8;
					accbits -= 8;
				}

				// write a full block if we can
				if (dstidx >= 256) {
					// set block length
					buf[0] = 255;

					// write block length + data
					mFrameData.insert(mFrameData.end(), buf, buf + 256);

					// move remaining data down
					memcpy(buf+1, buf+256, dstidx-256);
					dstidx -= 255;
				}

				hpos = c;

				if (nextEntry < 4096) {
					dict[nextEntry].mPrevAndLastChar = nextCode;
					dict[nextEntry].mHashNext = hash[hidx];
					hash[hidx] = nextEntry;

					// check if we need to bump the code point size
					if (nextEntry == nextBump) {
						++codebits;
						nextBump += nextBump;

						// make sure we never bump beyond 12 bits
						if (nextBump == 4096)
							++nextBump;
					}

					++nextEntry;
				} else {
					// Flush the table. This isn't generally advantageous for compression ratio,
					// but we have to do it as some buggy decoders overflow their table and crash
					// otherwise (MS GIF Animator 1.01).

					// emit flush code
					accum += clearCode << accbits;
					accbits += codebits;

					// flush bytes as necessary
					while(accbits >= 8) {
						buf[dstidx++] = (uint8)accum;
						accum >>= 8;
						accbits -= 8;
					}

					// reset
					codebits = initialCodeSize + 1;
					nextBump = 1 << codebits;
					nextEntry = clearCode + 2;
					for(int i=0; i<256; ++i)
						hash[i] = -1;
				}
			}

			code = hpos << 16;
		}

		// flush remaining code -- there's always one.
		accum += (code >> 16) << accbits;
		accbits += codebits;

		// flush bytes as necessary
		while(accbits >= 8) {
			buf[dstidx++] = (uint8)accum;
			accum >>= 8;
			accbits -= 8;
		}

		// add eos code
		accum += (clearCode + 1) << accbits;
		accbits += codebits;

		// flush bytes as necessary
		while(accbits >= 8) {
			buf[dstidx++] = (uint8)accum;
			accum >>= 8;
			accbits -= 8;
		}

		// flush partial byte
		if (accbits)
			buf[dstidx++] = (uint8)accum;

		// write remaining blocks (always at least one, may be two).
		for(;;) {
			// set block length
			uint32 tc = dstidx > 256 ? 256 : dstidx;
			buf[0] = (uint8)(tc - 1);

			// write block length + data
			mFrameData.insert(mFrameData.end(), buf, buf + tc);

			// move remaining data down
			if (dstidx <= 256)
				break;

			memcpy(buf+1, buf+256, dstidx-256);
			dstidx -= 255;
		}
	}

	// Write terminator
	const uint8 kTerminator = 0x00;
	mFrameData.push_back(kTerminator);

	++mFrameCount;
}

void AVIVideoGIFOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial write operations are not supported for video streams.");
}

void AVIVideoGIFOutputStream::partialWrite(const void *pBuffer, uint32 cbBuffer) {
}

void AVIVideoGIFOutputStream::partialWriteEnd() {
}

void AVIVideoGIFOutputStream::FlushFrame() {
	if (mFrameData.empty())
		return;

	// compute timestamp for this frame in centiseconds
	double frameToTicksFactor = (double)streamInfo.dwScale / (double)streamInfo.dwRate * 100.0;
	uint32 timestamp = VDRoundToInt32((sint32)mFrameCount * frameToTicksFactor);

	// poke delay into graphic control structure of previous frame (offset 4) and write it
	uint32 delay = timestamp - mPrevTimestamp;
	mPrevTimestamp = timestamp;

	VDWriteUnalignedLEU16(&mFrameData[4], (uint16)delay);

	mpAsync->FastWrite(mFrameData.data(), (uint32)mFrameData.size());

	// wipe frame data buffer for next frame
	mFrameData.clear();
}

//////////////////////////////////

class AVIOutputGIF : public AVIOutput, public IVDAVIOutputGIF {
public:
	AVIOutputGIF();

	AVIOutput *AsAVIOutput() { return this; }
	void SetLoopCount(int count) {
		mLoopCount = count;
	}

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();

protected:
	vdautoptr<IVDFileAsync> mpAsync;
	int mLoopCount;
};

IVDAVIOutputGIF *VDCreateAVIOutputGIF() { return new AVIOutputGIF; }

AVIOutputGIF::AVIOutputGIF()
	: mpAsync(VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode()))
	, mLoopCount(0)
{
}

AVIOutput *VDGetAVIOutputGIF() {
	return new AVIOutputGIF;
}

IVDMediaOutputStream *AVIOutputGIF::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoGIFOutputStream(mpAsync)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputGIF::createAudioStream() {
	return NULL;
}

bool AVIOutputGIF::init(const wchar_t *szFile) {
	uint32 bufsize = VDPreferencesGetRenderOutputBufferSize();
	mpAsync->Open(szFile, 2, bufsize >> 1);

	if (!videoOut)
		return false;

	static_cast<AVIVideoGIFOutputStream *>(videoOut)->init(mLoopCount);

	return true;
}

void AVIOutputGIF::finalize() {
	if (videoOut)
		static_cast<AVIVideoGIFOutputStream *>(videoOut)->finalize();
}
