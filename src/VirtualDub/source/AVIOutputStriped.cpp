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
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>

#include "AVIOutputStriped.h"
#include "AVIOutputFile.h"
#include "AVIStripeSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	output streams
//
///////////////////////////////////////////////////////////////////////////

class AVIStripedAudioOutputStream : public AVIOutputStream {
public:
	AVIStripedAudioOutputStream(AVIOutputStriped *);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
		throw MyError("Partial writes are not supported for video streams.");
	}

	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}

protected:
	AVIOutputStriped *const mpParent;
};

AVIStripedAudioOutputStream::AVIStripedAudioOutputStream(AVIOutputStriped *pParent) : mpParent(pParent) {
}

void AVIStripedAudioOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	VDXAVIStreamHeader& hdr = streamInfo.aviHeader;
	mpParent->writeChunk(TRUE, flags, pBuffer, cbBuffer, hdr.dwLength, lSamples);
	hdr.dwLength += lSamples;
}

////////////////////////////////////

class AVIStripedVideoOutputStream : public AVIOutputStream {
public:
	AVIStripedVideoOutputStream(AVIOutputStriped *);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
		throw MyError("Partial writes are not supported for video streams.");
	}

	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}

protected:
	AVIOutputStriped *const mpParent;
};

AVIStripedVideoOutputStream::AVIStripedVideoOutputStream(AVIOutputStriped *pParent) : mpParent(pParent) {
}

void AVIStripedVideoOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	VDXAVIStreamHeader& hdr = streamInfo.aviHeader;
	mpParent->writeChunk(FALSE, flags, pBuffer, cbBuffer, hdr.dwLength, lSamples);
	hdr.dwLength += lSamples;
}



///////////////////////////////////////////////////////////////////////////
//
//	AVIOutputStriped
//
///////////////////////////////////////////////////////////////////////////

class AVIOutputStripeState {
public:
	__int64		size;
	long		audio_sample, video_sample;
};



AVIOutputStriped::AVIOutputStriped(AVIStripeSystem *stripesys) {
	this->stripesys		= stripesys;

	stripe_files		= NULL;
	stripe_data			= NULL;

	audio_index_cache_point		= 0;
	video_index_cache_point		= 0;

	f1GbMode = false;
}

AVIOutputStriped::~AVIOutputStriped() {
	int i;

	if (stripe_files) {
		for(i=0; i<stripe_count; i++)
			delete stripe_files[i];

		delete[] stripe_files;
	}
	delete[] stripe_data;
}

//////////////////////////////////

IVDMediaOutputStream *AVIOutputStriped::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new AVIStripedVideoOutputStream(this)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputStriped::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new AVIStripedAudioOutputStream(this)))
		throw MyMemoryError();
	return audioOut;
}

void AVIOutputStriped::set_1Gb_limit() {
	f1GbMode = true;
}

bool AVIOutputStriped::init(const wchar_t *szFile) {
	int i;

	stripe_count = stripesys->getStripeCount();

	if (!(stripe_data = new AVIOutputStripeState [stripe_count]))
		throw MyMemoryError();

	memset(stripe_data, 0, sizeof(AVIOutputStripeState)*stripe_count);

	if (!(stripe_files = new IVDMediaOutputAVIFile *[stripe_count]))
		throw MyMemoryError();

	bool fFoundIndex = false;

	for(i=0; i<stripe_count; i++) {
		stripe_files[i] = NULL;
		if (stripesys->getStripeInfo(i)->isIndex())
			fFoundIndex = true;
	}

	if (!fFoundIndex)
		throw MyError("Cannot create output: stripe system has no index stripe");

	for(i=0; i<stripe_count; i++) {
		AVIStripe *sinfo = stripesys->getStripeInfo(i);

		stripe_files[i] = VDCreateMediaOutputAVIFile();

		if (f1GbMode)
			stripe_files[i]->set_1Gb_limit();

		IVDMediaOutput *pOutput = stripe_files[i];

		if (videoOut && (sinfo->isVideo() || sinfo->isIndex())) {
			IVDMediaOutputStream *pStripeVideoOut = pOutput->createVideoStream();

			if (sinfo->isIndex()) {
				BITMAPINFOHEADER bmih = *(BITMAPINFOHEADER *)videoOut->getFormat();
				bmih.biSize			= sizeof(BITMAPINFOHEADER);
				bmih.biCompression		= 'TSDV';

				pStripeVideoOut->setFormat(&bmih, sizeof(BITMAPINFOHEADER));
			} else {
				pStripeVideoOut->setFormat(videoOut->getFormat(), videoOut->getFormatLen());
			}

			VDXStreamInfo vsi(videoOut->getStreamInfo());
			VDXAVIStreamHeader& vhdr = vsi.aviHeader;

			if (sinfo->isIndex()) {
				vhdr.fccHandler		= 'TSDV';
				vhdr.dwSampleSize	= 16;

				index_file = stripe_files[i];
			}

			pStripeVideoOut->setStreamInfo(vsi);
		}
		if (audioOut && (sinfo->isAudio() || sinfo->isIndex())) {
			IVDMediaOutputStream *pStripeAudioOut = pOutput->createAudioStream();

			pStripeAudioOut->setFormat(audioOut->getFormat(), audioOut->getFormatLen());

			VDXStreamInfo asi(audioOut->getStreamInfo());
			VDXAVIStreamHeader& ahdr = asi.aviHeader;

			if (!sinfo->isAudio()) {
				ahdr.fccType		= 'idua';
				ahdr.fccHandler		= 'TSDV';
				ahdr.dwSampleSize	= 16;

				index_file = stripe_files[i];
			}

			pStripeAudioOut->setStreamInfo(asi);
		}

		stripe_files[i]->disable_os_caching();
		stripe_files[i]->setBuffering(sinfo->lBufferSize, sinfo->lBufferSize >> 2);

		if (!pOutput->init(VDTextAToW(sinfo->szName).c_str()))
			throw MyError("Error initializing stripe #%d", i+1);
	}

	return true;
}

void AVIOutputStriped::finalize() {
	int i;

	FlushCache(FALSE);
	FlushCache(TRUE);

	for(i=0; i<stripe_count; i++)
		stripe_files[i]->finalize();
}

void AVIOutputStriped::writeChunk(bool is_audio, uint32 flags, const void *pBuffer, uint32 cbBuffer,
										uint32 lSampleFirst, uint32 lSampleCount) {
	AVIStripeIndexEntry *asie;
	int s, best_stripe=-1;
	uint32 lBufferSize;
	BOOL best_will_block = TRUE;

	// Pick a stripe to send the data to.
	//
	// Prioritize stripes that can hold the data without blocking,
	// then on total data fed to that stripe at that point.

	for(s=0; s<stripe_count; s++) {

		if (is_audio) {
			if (!stripesys->getStripeInfo(s)->isAudio())
				continue;
		} else {
			if (!stripesys->getStripeInfo(s)->isVideo())
				continue;
		}

		if (stripe_files[s]->bufferStatus(&lBufferSize) >= ((cbBuffer+11)&-4)) {
			// can accept without blocking

			if (best_will_block) { // automatic win
				best_will_block = FALSE;
				best_stripe = s;
				continue;
			}
		} else {
			// will block

			if (!best_will_block)	// automatic loss
				continue;
		}

		// compare total data sizes

		if (best_stripe<0 || stripe_data[best_stripe].size > stripe_data[s].size)
			best_stripe = s;
	}

	// Write data to stripe.

	if (best_stripe >= 0) {
		IVDMediaOutput *pOutput = stripe_files[best_stripe];

		if (is_audio)
			pOutput->getAudioOutput()->write(flags, pBuffer, cbBuffer, lSampleCount);
		else
			pOutput->getVideoOutput()->write(flags, pBuffer, cbBuffer, lSampleCount);

		stripe_data[best_stripe].size += cbBuffer+8;
	}

	// Write lookup chunk to index stream.
	//
	// NOTE: Do not write index marks to the same stream the data
	//       was written to!

	if (index_file != stripe_files[best_stripe]) {
		if (is_audio) {
			if (audio_index_cache_point >= CACHE_SIZE)
				FlushCache(TRUE);

			asie = &audio_index_cache[audio_index_cache_point++];
		} else {
			if (video_index_cache_point >= CACHE_SIZE)
				FlushCache(FALSE);

			asie = &video_index_cache[video_index_cache_point++];
		}

		asie->lSampleFirst	= lSampleFirst;
		asie->lSampleCount	= lSampleCount;
		asie->lStripe		= best_stripe;
		asie->lStripeSample	= 0;

		if (is_audio) {
			if (best_stripe >= 0) {
				asie->lStripeSample = stripe_data[best_stripe].audio_sample;

				stripe_data[best_stripe].audio_sample += lSampleCount;
			}
			audio_index_flags[audio_index_cache_point-1] = flags;
			audio_index_count[audio_index_cache_point-1] = lSampleCount;
		} else {
			if (best_stripe >= 0) {
				asie->lStripeSample = stripe_data[best_stripe].video_sample;

				stripe_data[best_stripe].video_sample += lSampleCount;
			}
			video_index_flags[video_index_cache_point-1] = flags;
			video_index_count[video_index_cache_point-1] = lSampleCount;
		}
	}
}

void AVIOutputStriped::FlushCache(BOOL fAudio) {
	int i;

	if (fAudio) {
		IVDMediaOutputStream *pIndexAudio = index_file->getAudioOutput();

		for(i=0; i<audio_index_cache_point; i++)
			pIndexAudio->write(audio_index_flags[i], &audio_index_cache[i], sizeof AVIStripeIndexEntry, audio_index_count[i]);

		audio_index_cache_point = 0;
	} else {
		IVDMediaOutputStream *pIndexVideo = index_file->getVideoOutput();

		for(i=0; i<video_index_cache_point; i++)
			pIndexVideo->write(video_index_flags[i], &video_index_cache[i], sizeof AVIStripeIndexEntry, video_index_count[i]);

		video_index_cache_point = 0;
	}
}
