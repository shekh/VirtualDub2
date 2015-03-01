//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#include <vd2/system/vdtypes.h>
#include <vd2/system/strutil.h>
#include <vd2/system/Error.h>
#include <vd2/system/protscope.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Riza/w32audiocodec.h>

namespace {
	// Need to take care of this at some point.
	void SafeCopyWaveFormat(vdstructex<VDWaveFormat>& dst, const VDWaveFormat *src) {
		VDASSERTCT(sizeof(VDWaveFormat) == sizeof(WAVEFORMATEX));
		if (src->mTag == WAVE_FORMAT_PCM) {
			dst.resize(sizeof(VDWaveFormat));
			dst->mExtraSize = 0;
			memcpy(dst.data(), src, sizeof(PCMWAVEFORMAT));
		} else
			dst.assign((const VDWaveFormat *)src, sizeof(VDWaveFormat) + src->mExtraSize);
	}

	const char *VDGetNameOfMMSYSTEMErrorW32(MMRESULT res) {
		switch(res) {
		case MMSYSERR_NOERROR:		return "MMSYSERR_NOERROR";
		case MMSYSERR_ERROR:		return "MMSYSERR_ERROR";
		case MMSYSERR_BADDEVICEID:	return "MMSYSERR_BADDEVICEID";
		case MMSYSERR_NOTENABLED:	return "MMSYSERR_NOTENABLED";
		case MMSYSERR_ALLOCATED:	return "MMSYSERR_ALLOCATED";
		case MMSYSERR_INVALHANDLE:	return "MMSYSERR_INVALHANDLE";
		case MMSYSERR_NODRIVER:		return "MMSYSERR_NODRIVER";
		case MMSYSERR_NOMEM:		return "MMSYSERR_NOMEM";
		case MMSYSERR_NOTSUPPORTED:	return "MMSYSERR_NOTSUPPORTED";
		case MMSYSERR_BADERRNUM:	return "MMSYSERR_BADERRNUM";
		case MMSYSERR_INVALFLAG:	return "MMSYSERR_INVALFLAG";
		case MMSYSERR_INVALPARAM:	return "MMSYSERR_INVALPARAM";
		case MMSYSERR_HANDLEBUSY:	return "MMSYSERR_HANDLEBUSY";
		case MMSYSERR_INVALIDALIAS:	return "MMSYSERR_INVALIDALIAS";
		case MMSYSERR_BADDB:		return "MMSYSERR_BADDB";
		case MMSYSERR_KEYNOTFOUND:	return "MMSYSERR_KEYNOTFOUND";
		case MMSYSERR_READERROR:	return "MMSYSERR_READERROR";
		case MMSYSERR_WRITEERROR:	return "MMSYSERR_WRITEERROR";
		case MMSYSERR_DELETEERROR:	return "MMSYSERR_DELETEERROR";
		case MMSYSERR_VALNOTFOUND:	return "MMSYSERR_VALNOTFOUND";
		case MMSYSERR_NODRIVERCB:	return "MMSYSERR_NODRIVERCB";
		case MMSYSERR_MOREDATA:		return "MMSYSERR_MOREDATA";
		default:
			return "unknown";
		}
	}

	const char *VDGetNameOfACMErrorW32(MMRESULT res) {
		switch(res) {
			case ACMERR_NOTPOSSIBLE:	return "ACMERR_NOTPOSSIBLE";
			case ACMERR_BUSY:			return "ACMERR_BUSY";
			case ACMERR_UNPREPARED:		return "ACMERR_UNPREPARED";
			case ACMERR_CANCELED:		return "ACMERR_CANCELED";
			default:
				return VDGetNameOfMMSYSTEMErrorW32(res);
		}
	}
}

IVDAudioCodec *VDCreateAudioCompressorW32(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, const char *pShortNameDriverHint, bool throwIfNotFound) {
	vdautoptr<VDAudioCodecW32> codec(new VDAudioCodecW32);

	if (!codec->Init((const WAVEFORMATEX *)srcFormat, (const WAVEFORMATEX *)dstFormat, true, pShortNameDriverHint, throwIfNotFound))
		return NULL;

	return codec.release();
}

IVDAudioCodec *VDCreateAudioDecompressorW32(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, const char *pShortNameDriverHint, bool throwIfNotFound) {
	vdautoptr<VDAudioCodecW32> codec(new VDAudioCodecW32);

	if (!codec->Init((const WAVEFORMATEX *)srcFormat, (const WAVEFORMATEX *)dstFormat, false, pShortNameDriverHint, throwIfNotFound))
		return NULL;

	return codec.release();
}

VDAudioCodecW32::VDAudioCodecW32()
	: mhDriver(NULL)
	, mhStream(NULL)
	, mOutputReadPt(0)
{
	mDriverName[0] = 0;
	mDriverFilename[0] = 0;

	memset(&mBufferHdr, 0, sizeof mBufferHdr);	// Do this so we can detect whether the buffer is prepared or not.
}

VDAudioCodecW32::~VDAudioCodecW32() {
	Shutdown();
}

namespace {
	struct ACMDriverList {
		typedef const HACMDRIVERID *const_iterator;

		ACMDriverList(const char *hint)
			: mpHint(hint)
		{
			acmDriverEnum(Callback, (DWORD_PTR)this, 0);
		}

		const_iterator begin() const { return mDriverIds.begin(); }
		const_iterator end() const { return mDriverIds.end(); }

	protected:
		static BOOL CALLBACK Callback(HACMDRIVERID hadid, DWORD_PTR dwInstance, DWORD fdwSupport) {
			ACMDriverList *pThis = (ACMDriverList *)dwInstance;

			// If we have a hint, check if the driver matches. If so, push it at the front of
			// the list instead of the back.
			if (pThis->mpHint) {
				ACMDRIVERDETAILS add = {sizeof(ACMDRIVERDETAILS)};

				if (!acmDriverDetails(hadid, &add, 0) && !_stricmp(add.szShortName, pThis->mpHint)) {
					pThis->mDriverIds.insert(pThis->mDriverIds.begin(), hadid);
					pThis->mpHint = NULL;
					return TRUE;
				}
			}

			pThis->mDriverIds.push_back(hadid);
			return TRUE;
		}

		const char *mpHint;
		vdfastvector<HACMDRIVERID> mDriverIds;
	};
}

bool VDAudioCodecW32::Init(const WAVEFORMATEX *pSrcFormat, const WAVEFORMATEX *pDstFormat, bool isCompression, const char *pDriverShortNameHint, bool throwOnError) {
	Shutdown();

	SafeCopyWaveFormat(mSrcFormat, (const VDWaveFormat *)pSrcFormat);

	if (pDstFormat)
		SafeCopyWaveFormat(mDstFormat, (const VDWaveFormat *)pDstFormat);

	// enumerate IDs for all installed codecs
	ACMDriverList driverList(pDriverShortNameHint);

	// try one driver at a time
	MMRESULT res = 0;

	for(ACMDriverList::const_iterator it(driverList.begin()), itEnd(driverList.end());
		it != itEnd;
		++it)
	{
		const HACMDRIVERID driverId = *it;

		// open driver
		HACMDRIVER hDriver = NULL;
		if (acmDriverOpen(&hDriver, *it, 0))
			continue;

		if (!pDstFormat) {
			VDASSERT(!isCompression);
		
			DWORD dwDstFormatSize = 0;

			VDVERIFY(!acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, (LPVOID)&dwDstFormatSize));

			if (dwDstFormatSize < sizeof(WAVEFORMATEX))
				dwDstFormatSize = sizeof(WAVEFORMATEX);

			mDstFormat.resize(dwDstFormatSize);
			memset(mDstFormat.data(), 0, dwDstFormatSize);
			mDstFormat->mTag = WAVE_FORMAT_PCM;

			if (acmFormatSuggest(hDriver, (WAVEFORMATEX *)pSrcFormat, (WAVEFORMATEX *)mDstFormat.data(), dwDstFormatSize, ACM_FORMATSUGGESTF_WFORMATTAG)) {
				acmDriverClose(hDriver, NULL);
				continue;
			}

			// sanitize the destination format a bit

			if (mDstFormat->mSampleBits != 8 && mDstFormat->mSampleBits != 16)
				mDstFormat->mSampleBits = 16;

			if (mDstFormat->mChannels != 1 && mDstFormat->mChannels !=2)
				mDstFormat->mChannels = 2;

			mDstFormat->mBlockSize		= (uint16)((mDstFormat->mSampleBits >> 3) * mDstFormat->mChannels);
			mDstFormat->mDataRate		= mDstFormat->mBlockSize * mDstFormat->mSamplingRate;
			mDstFormat->mExtraSize		= 0;
			mDstFormat.resize(sizeof(WAVEFORMATEX));
		}

		// open conversion stream
		res = acmStreamOpen(&mhStream, hDriver, (WAVEFORMATEX *)pSrcFormat, (WAVEFORMATEX *)mDstFormat.data(), NULL, 0, 0, ACM_STREAMOPENF_NONREALTIME);
		if (!res) {
			mhDriver = hDriver;
			break;
		}

		// Aud-X accepts PCM/6ch but not WAVE_FORMAT_EXTENSIBLE/PCM/6ch. Argh. We attempt to work
		// around this by trying a PCM version if WFE doesn't work.
		if (isCompression) {
			// Need to put this somewhere.
			struct WaveFormatExtensibleW32 {
				WAVEFORMATEX mFormat;
				union {
					uint16 mBitDepth;
					uint16 mSamplesPerBlock;		// may be zero, according to MSDN
				};
				uint32	mChannelMask;
				GUID	mGuid;
			};

			static const GUID local_KSDATAFORMAT_SUBTYPE_PCM={	// so we don't have to bring in ksmedia.h
				WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71
			};

			if (pSrcFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pSrcFormat->cbSize >= sizeof(WaveFormatExtensibleW32) - sizeof(WAVEFORMATEX)) {
				const WaveFormatExtensibleW32& wfexex = *(const WaveFormatExtensibleW32 *)pSrcFormat;

				if (wfexex.mGuid == local_KSDATAFORMAT_SUBTYPE_PCM) {
					// Rewrite the format to be straight PCM and try again.
					vdstructex<VDWaveFormat> srcFormat2(mSrcFormat.data(), sizeof(VDWaveFormat));
					srcFormat2->mExtraSize	= 0;
					srcFormat2->mTag		= WAVE_FORMAT_PCM;
					MMRESULT res2 = acmStreamOpen(&mhStream, hDriver, (WAVEFORMATEX *)srcFormat2.data(), (WAVEFORMATEX *)mDstFormat.data(), NULL, 0, 0, ACM_STREAMOPENF_NONREALTIME);

					if (!res2) {
						res = res2;
						mSrcFormat = srcFormat2;
						pSrcFormat = (WAVEFORMATEX *)mSrcFormat.data();

						mhDriver = hDriver;
						break;
					}
				}
			}
		}

		acmDriverClose(hDriver, 0);
	}

	if (!mhStream) {
		Shutdown();
		
		if (!throwOnError)
			return false;

		if (isCompression) {
				throw MyError(
							"Error initializing audio stream compression:\n"
							"No installed audio codec could compress the source audio to the desired format.\n"
							"\n"
							"Check that the sampling rate and number of channels in the source is compatible with the selected compressed audio format."
						);
		} else {
			throw MyError(
						"Error initializing audio stream decompression:\n"
						"No installed audio codec could be found to decompress the compressed source audio.\n"
						"\n"
						"Check to make sure you have the required codec%s."
						,
						(pSrcFormat->wFormatTag&~1)==0x160 ? " (Microsoft Audio Codec)" : ""
					);
		}
	}

	DWORD dwSrcBufferSize = mSrcFormat->mDataRate / 5;
	DWORD dwDstBufferSize = mDstFormat->mDataRate / 5;

	if (!dwSrcBufferSize)
		dwSrcBufferSize = 1;

	dwSrcBufferSize += mSrcFormat->mBlockSize - 1;
	dwSrcBufferSize -= dwSrcBufferSize % mSrcFormat->mBlockSize;

	if (!dwDstBufferSize)
		dwDstBufferSize = 1;

	dwDstBufferSize += mDstFormat->mBlockSize - 1;
	dwDstBufferSize -= dwDstBufferSize % mDstFormat->mBlockSize;

	if (acmStreamSize(mhStream, dwSrcBufferSize, &dwDstBufferSize, ACM_STREAMSIZEF_SOURCE)) {
		Shutdown();
		throw MyError("Error initializing audio stream output size.");
	}

	mInputBuffer.resize(dwSrcBufferSize);
	mOutputBuffer.resize(dwDstBufferSize);

	mBufferHdr.cbStruct		= sizeof(ACMSTREAMHEADER);
	mBufferHdr.pbSrc		= (LPBYTE)&mInputBuffer.front();
	mBufferHdr.cbSrcLength	= mInputBuffer.size();
	mBufferHdr.pbDst		= (LPBYTE)&mOutputBuffer.front();
	mBufferHdr.cbDstLength	= mOutputBuffer.size();

	if (acmStreamPrepareHeader(mhStream, &mBufferHdr, 0)) {
		// make sure buffer header is invalidated so we don't try to unprepare it
		memset(&mBufferHdr, 0, sizeof mBufferHdr);

		Shutdown();
		throw MyError("Error preparing audio decompression buffers.");
	}

	Restart();

	// try to get driver name for debugging purposes (OK to fail)
	mDriverName[0] = mDriverFilename[0] = 0;

	HACMDRIVERID hDriverID;
	if (!acmDriverID((HACMOBJ)mhStream, &hDriverID, 0)) {
		ACMDRIVERDETAILS add = { sizeof(ACMDRIVERDETAILS) };
		if (!acmDriverDetails(hDriverID, &add, 0)) {
			strncpyz(mDriverName, add.szLongName, sizeof mDriverName);
			strncpyz(mDriverFilename, add.szShortName, sizeof mDriverFilename);
		}
	}

	return true;
}

void VDAudioCodecW32::Shutdown() {
	mDstFormat.clear();

	if (mhStream) {
		if (mBufferHdr.fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED) {
			mBufferHdr.cbSrcLength = mInputBuffer.size();
			mBufferHdr.cbDstLength = mOutputBuffer.size();
			acmStreamUnprepareHeader(mhStream, &mBufferHdr, 0);

			memset(&mBufferHdr, 0, sizeof mBufferHdr);
		}

		acmStreamClose(mhStream, 0);
		mhStream = NULL;
	}

	if (mhDriver) {
		acmDriverClose(mhDriver, 0);
		mhDriver = NULL;
	}

	mDriverName[0] = 0;
	mDriverFilename[0] = 0;
}

void *VDAudioCodecW32::LockInputBuffer(unsigned& bytes) {
	unsigned space = mInputBuffer.size() - mBufferHdr.cbSrcLength;
	VDASSERT((int)space >= 0);

	bytes = space;
	return &mInputBuffer[mBufferHdr.cbSrcLength];
}

void VDAudioCodecW32::UnlockInputBuffer(unsigned bytes) {
	mBufferHdr.cbSrcLength += bytes;
}

void VDAudioCodecW32::Restart() {
	mBufferHdr.cbSrcLength = 0;
	mBufferHdr.cbDstLengthUsed = 0;
	mbFirst	= true;
	mbFlushing = false;
	mbEnded = false;
	mOutputReadPt = 0;
}

bool VDAudioCodecW32::Convert(bool flush, bool requireOutput) {
	if (mOutputReadPt < mBufferHdr.cbDstLengthUsed)
		return true;

	if (mbEnded)
		return false;

	mBufferHdr.cbSrcLengthUsed = 0;
	mBufferHdr.cbDstLengthUsed = 0;

	const bool isCompression = mDstFormat->mTag != WAVE_FORMAT_PCM;

	// Run the message queue and clear out any MM_STREAM_DONE messages. We need to do
	// this in order to work around a severe bug in the Creative MP3 codec (ctmp3.acm),
	// which has a loop like this:
	//
	//	do {
	//		Sleep(10);
	//	} while(!PostThreadMessage(GetCurrentThreadId(), MM_STREAM_DONE, id, 0));
	//
	// Since we're not required to run a message pump to use ACM without a window handle,
	// this causes the window message queue to fill up.

	MSG msg;
	int messageCount = 0;
	for(; messageCount < 500; ++messageCount) {
		if (!PeekMessage(&msg, (HWND)-1, MM_STREAM_DONE, MM_STREAM_DONE, PM_REMOVE | PM_NOYIELD))
			break;
	}

	if (messageCount > 0) {
		static bool wtf = false;

		if (!wtf) {
			wtf = true;

			VDDEBUG("AudioCodec: MM_STREAM_DONE thread messages found in message queue!\n");
		}

		if (messageCount >= 500) {
			VDDEBUG("AudioCodec: Too many messages found!\n");
		}
	}


	if (mBufferHdr.cbSrcLength || flush) {
		vdprotected2(isCompression ? "compressing audio" : "decompressing audio", const char *, mDriverName, const char *, mDriverFilename) {
			DWORD flags = ACM_STREAMCONVERTF_BLOCKALIGN;

			if (flush && !mBufferHdr.cbSrcLength)
				mbFlushing = true;

			if (mbFlushing)
				flags = ACM_STREAMCONVERTF_END;

			if (mbFirst)
				flags |= ACM_STREAMCONVERTF_START;

			if (MMRESULT res = acmStreamConvert(mhStream, &mBufferHdr, flags))
				throw MyError(
					isCompression
						? "The audio codec reported an error while compressing audio data.\n\nError code: %d (%s)"
						: "The audio codec reported an error while decompressing audio data.\n\nError code: %d (%s)"
					, res
					, VDGetNameOfACMErrorW32(res));

			mbFirst = false;
		}

		// If the codec didn't do anything....
		if (!mBufferHdr.cbSrcLengthUsed && !mBufferHdr.cbDstLengthUsed) {
			if (flush) {
				if (mbFlushing)
					mbEnded = true;
				else
					mbFlushing = true;
			} else if (requireOutput) {
				// Check for a jam condition to try to trap that damned 9995 frame
				// hang problem.
				const VDWaveFormat& wfsrc = *mSrcFormat;
				const VDWaveFormat& wfdst = *mDstFormat;

				throw MyError("The operation cannot continue as the target audio codec has jammed and is not %scompressing data.\n"
								"Codec state for driver \"%.64s\":\n"
								"    source buffer size: %d bytes\n"
								"    destination buffer size: %d bytes\n"
								"    source format: tag %04x, %dHz/%dch/%d-bit, %d bytes/sec\n"
								"    destination format: tag %04x, %dHz/%dch/%d-bit, %d bytes/sec\n"
								, isCompression ? "" : "de"
								, mDriverName
								, mBufferHdr.cbSrcLength
								, mBufferHdr.cbDstLength
								, wfsrc.mTag, wfsrc.mSamplingRate, wfsrc.mChannels, wfsrc.mSampleBits, wfsrc.mDataRate
								, wfdst.mTag, wfdst.mSamplingRate, wfdst.mChannels, wfdst.mSampleBits, wfdst.mDataRate);
			}
		}
	}

	mOutputReadPt = 0;

	// if ACM didn't use all the source data, copy the remainder down
	if (mBufferHdr.cbSrcLengthUsed < mBufferHdr.cbSrcLength) {
		long left = mBufferHdr.cbSrcLength - mBufferHdr.cbSrcLengthUsed;

		memmove(&mInputBuffer.front(), &mInputBuffer[mBufferHdr.cbSrcLengthUsed], left);

		mBufferHdr.cbSrcLength = left;
	} else
		mBufferHdr.cbSrcLength = 0;

	return mBufferHdr.cbSrcLengthUsed || mBufferHdr.cbDstLengthUsed;
}

const void *VDAudioCodecW32::LockOutputBuffer(unsigned& bytes) {
	bytes = mBufferHdr.cbDstLengthUsed - mOutputReadPt;
	return mOutputBuffer.data() + mOutputReadPt;
}

void VDAudioCodecW32::UnlockOutputBuffer(unsigned bytes) {
	mOutputReadPt += bytes;
	VDASSERT(mOutputReadPt <= mBufferHdr.cbDstLengthUsed);
}

unsigned VDAudioCodecW32::CopyOutput(void *dst, unsigned bytes) {
	bytes = std::min<unsigned>(bytes, mBufferHdr.cbDstLengthUsed - mOutputReadPt);

	if (dst)
		memcpy(dst, &mOutputBuffer[mOutputReadPt], bytes);

	mOutputReadPt += bytes;
	return bytes;
}

///////////////////////////////////////////////////////////////////////////

IVDAudioCodec *VDLocateAudioDecompressor(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, bool preferInternalCodecs, const char *pShortNameDriverHint) {
	IVDAudioCodec *codec = NULL;

	if (preferInternalCodecs) {
		codec = VDCreateAudioDecompressor(srcFormat, dstFormat);
		if (codec)
			return codec;
	}

	codec = VDCreateAudioDecompressorW32(srcFormat, dstFormat, pShortNameDriverHint, false);
	if (codec)
		return codec;

	if (!preferInternalCodecs) {
		codec = VDCreateAudioDecompressor(srcFormat, dstFormat);
		if (codec)
			return codec;
	}

	throw MyError(
			"No audio decompressor could be found to decompress the source audio format.\n"
			"(source format tag: %04x)"
			, (uint16)srcFormat->mTag
		);
}
