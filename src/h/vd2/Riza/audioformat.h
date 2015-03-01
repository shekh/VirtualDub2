#ifndef f_VD2_RIZA_AUDIOFORMAT_H
#define f_VD2_RIZA_AUDIOFORMAT_H

#include <vd2/system/vdtypes.h>

#ifdef _MSC_VER
	#pragma pack(push, 1)
#endif

namespace nsVDWinFormats {
	struct Guid {
		uint32	mData1;
		uint16	mData2;
		uint16	mData3;
		uint8	mData4[8];

		bool operator==(const Guid&) const;
	};

	/// Mirror of WAVEFORMATEX.
	struct WaveFormatEx {
		uint16	mFormatTag;
		uint16	mChannels;
		uint32	mSamplesPerSec;
		uint32	mAvgBytesPerSec;
		uint16	mBlockAlign;
		uint16	mBitsPerSample;
		uint16	mSize;
	};

	/// Mirror of WAVEFORMATEXTENSIBLE
	struct WaveFormatExtensible {
		WaveFormatEx mFormat;
		union {
			uint16 mBitDepth;
			uint16 mSamplesPerBlock;		// may be zero, according to MSDN
		};
		uint32		mChannelMask;
		Guid		mGuid;
	};

	enum {
		kWAVE_FORMAT_PCM = 1,
		kWAVE_FORMAT_EXTENSIBLE = 0xfffe
	};

	extern const Guid kKSDATAFORMAT_SUBTYPE_PCM;

	// Helper class.
	struct WaveFormatExPCM : public WaveFormatEx{
		WaveFormatExPCM(uint32 samplingRate, uint32 channels, uint32 precision) {
			mFormatTag = kWAVE_FORMAT_PCM;
			mChannels = (uint16)channels;
			mSamplesPerSec = samplingRate;
			mBitsPerSample = (uint16)precision;
			mBlockAlign = (uint16)(mChannels * ((precision + 7) >> 3));
			mAvgBytesPerSec = samplingRate * mBlockAlign;
			mSize = 0;
		}
	};
}

#ifdef _MSC_VER
	#pragma pack(pop)
#endif

#endif
