#pragma warning(disable: 4786)

#include <crtdbg.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <list>
#include <set>

#include <vd2/system/vdtypes.h>
#include <vd2/system/atomic.h>
#include <vd2/system/file.h>
#include <vd2/system/Progress.h>

#include <vd2/Meia/MPEGFile.h>
#include "MPEGCache.h"

using namespace std;

#define printf sizeof

///////////////////////////////////////////////////////////////////////////

enum eParseResult {
	kParseContinue = 0,
	kParseAbort = 1,
	kParseComplete = 2,
	kParseRewind = 3,		// seek only
};

///////////////////////////////////////////////////////////////////////////

class VDINTERFACE IVDMPEGPacketReader {
public:
	virtual bool DoScanByPos(int64 pos, int active_stream, int64 lower_bound, int64 upper_bound)=0;
	virtual bool ReadPacket(int stream, int64 packet, int offset, char *dst, int length)=0;
};

class VDMPEGStream {
protected:
	// <pos, len>

	typedef std::map<int64, int> tPackList;

	tPackList				mPackList;
	tPackList::iterator		mLastPackEntry;

	IVDMPEGPacketReader		*mpReader;

	////////////////////////////////

	VDMPEGStream(IVDMPEGPacketReader *pReader) : mpReader(pReader) {
		mLastPackEntry = mPackList.begin();
	}

	void RecordPacket(int64 pos, int length) {
		mLastPackEntry = mPackList.insert(mLastPackEntry, tPackList::value_type(pos, length));
	}

	bool ReadPackets(int stream, int64 first_packet, int offset, char *dst, int length) {
		tPackList::iterator it = mPackList.find(first_packet);

		while(length > 0) {
			if (it == mPackList.end()) {
				VDASSERT(false);
				return false;
			}

			int blen = length;

			if (blen > (*it).second - offset)
				blen = (*it).second - offset;

			VDASSERT(blen>0);

			if (!mpReader->ReadPacket(stream, (*it).first, offset, dst, blen))
				return false;

			++it;

			dst += blen;
			length -= blen;

			offset = 0;
		}

		return true;
	}
};




///////////////////////////////////////////////////////////////////////////
//
//	Audio stream handler
//
///////////////////////////////////////////////////////////////////////////




class VDMPEGAudioStream : public VDMPEGStream, public IVDMPEGAudioStream {
private:
	int						mnStreamID;

	unsigned long master_header;
	VDAtomicInt refCount;

	typedef pair<unsigned long, int64> tFrameLocation;	// offset:size, packet_offset
	typedef map<long, tFrameLocation> tFrameMap;
	tFrameMap mFrameMap;

	long		mFrameSize;		// frame size in samples -- 576 for MPEG-2, 1152 for MPEG-1
	long		mSamplingRate;	// sampling rate in Hz
	long		mInitialFrame;

	/////////////////////

	bool isValidAudioHeader(unsigned long hdr) {
		// FFF00000 12 bits	sync mark
		//
		// 00080000  1 bit	version
		// 00060000  2 bits	layer (3 = layer I, 2 = layer II, 1 = layer III)
		// 00010000  1 bit	error protection (0 = enabled)
		//
		// 0000F000  4 bits	bitrate_index
		// 00000C00  2 bits	sampling_freq
		// 00000200  1 bit	padding
		// 00000100  1 bit	extension
		//
		// 000000C0  2 bits	mode (0=stereo, 1=joint stereo, 2=dual channel, 3=mono)
		// 00000030  2 bits	mode_ext
		// 00000008  1 bit	copyright
		// 00000004  1 bit	original
		// 00000003  2 bits	emphasis
		
		// sync bits must be on
		if ((hdr & 0xfff00000) != 0xfff00000)
			return false;
		
		// 00 for layer ("layer 4") is not valid
		if (!(hdr & 0x00060000))
			return false;
		
		// 1111 for bitrate is not valid
		if ((hdr & 0x0000F000) == 0x0000F000)
			return false;
		
		// 11 for sampling frequency is not valid
		if ((hdr & 0x00000C00) == 0x00000C00)
			return false;
		
		// Looks okay to me...
		return true;
	}

	bool isConsistentAudioHeader(unsigned long hdr) {
		return !((master_header ^ hdr) & 0xFFFEFC08);
	}

	unsigned getFrameLengthFromHeader(unsigned long hdr) {
		static const int bitrates[3][15] = {
				  {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
				  {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
				  {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}
				};

		unsigned bitrate = bitrates[3 - ((hdr&0x60000) >> 17)][(hdr&0xf000)>>12];
		unsigned padding = (hdr & 0x200) >> 9;

		// frame_size = floor(144000 * bitrate / sampling_rate) + padding;

		switch(hdr & 0xC00) {
		case 0x000:		return (1440 * bitrate / 441) + padding;
		case 0x400:		return 3*bitrate + padding;
		case 0x800:		return 9*bitrate/2 + padding;
		default:		VDNEVERHERE; return 0;
		}
	}

public:
	VDMPEGAudioStream(IVDMPEGPacketReader *pReader, int id)
			: VDMPEGStream(pReader)
			, refCount(0)
			, mnStreamID(id)
			, mParseDesiredFrame(-1)
			, mInitialFrame(-1)
	{
	}

	~VDMPEGAudioStream() {
	}
	
	int AddRef() { return refCount.inc(); }
	int Release() { int rc = refCount.dec(); if (!rc) delete this; return rc; }

	void getInitialFormat(VDMPEGAudioFormat &format) {
		format.sampling_rate = mSamplingRate;
		format.channels = ((master_header & 0xC0) != 0xC0) ? 2 : 1;
	}

	unsigned long getFrameCount() {
		VDASSERT(mFrameMap.begin() != mFrameMap.end());

		tFrameMap::iterator it = mFrameMap.end();

		--it;
		return (*it).first + 1;
	}

	long ReadFrames(long frame_num, int frame_count, void *buf, long maxbuffer, long& bytes_read) {
		if (frame_num < 0)
			return -1;

		long samples_read = 0;
		bytes_read = 0;

		while(frame_count>0) {
			tFrameMap::iterator it = mFrameMap.find(frame_num);

			if (it == mFrameMap.end()) {
				tFrameMap::iterator itLo = mFrameMap.lower_bound(frame_num);
				tFrameMap::iterator itHi = mFrameMap.upper_bound(frame_num);

				--itLo;

				VDDEBUG("MPEGAudio: Interpolating between %d(%I64x) and %d(%I64x)\n", itLo->first, itLo->second.second, itHi->first, itHi->second.second);

				int64 posLo = (*itLo).second.second;
				int64 posHi = (*itHi).second.second;
				int64 pos = posLo + (posHi-posLo) * (frame_num - (*itLo).first) / (frame_num - (*itHi).first) - 4096;

				if (pos<0)
					pos = 0;

				if (!mpReader->DoScanByPos(pos, mnStreamID, posLo, posHi))
					break;

				continue;
			}

			int length = (*it).second.first>>16;
			int64 packetpos = (*it).second.second;
			int offset = (*it).second.first&0xffff;

//			VDDEBUG("reading: %I64x+%ld (%d)\n", packetpos, offset, length);

			if (buf)
				if (!ReadPackets(mnStreamID, packetpos, offset, (char *)buf, length))
					break;

			++samples_read;
			bytes_read += length;
			buf = (char *)buf + length;

			++frame_num;
			--frame_count;
		}

		return samples_read;
	}


	////////////////////////////

	eParseResult ParseInit(const unsigned char *buf, int length, int64 pts, int64 pos) {
		if (!buf) {
			master_header = 0;
			ParseReset();
		} else {
			int t = -length;
			const unsigned char *buf2 = buf + length;

			do {
				master_header = (master_header<<8) + buf2[t];

				if (isValidAudioHeader(master_header)) {

					mFrameSize = 1152;

					switch(master_header & 0x00000C00) {
					case 0x00000000:	mSamplingRate = 44100; break;
					case 0x00000400:	mSamplingRate = 48000; break;
					case 0x00000800:	mSamplingRate = 32000; break;
					}

					if (!mFrameMap.empty())
						return kParseComplete;

					break;
				}
			} while(++t);

			if (mFrameMap.empty())
				if (kParseAbort == Parse(buf, length, pts, pos))
					return kParseAbort;
		}

		return kParseContinue;
	}

	////////////////////////

	long mParseFrame;
	unsigned long mParseHeader;
	unsigned long mParseDesiredFrame;
	int64			mParseLastPacket;
	unsigned long	mParseLastLength;
	int				mParseSkip;

	void ParseReset() {
		mParseFrame = -1;
		mParseHeader = 0;
		mParseSkip = 0;
	}

	// Frames may consist of 576 or 1152 samples, and the sampling rate
	// can be 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000.

	long getFrameFromPTS(int64 pts) {
		return (long)((pts * mSamplingRate + mFrameSize * 45000) / (mFrameSize * 90000));
	}

	eParseResult Parse(const unsigned char *buf, int length, int64 pts, int64 pos) {
		const unsigned char *buf0 = buf;
		const unsigned char *buflimit = buf + length;

		RecordPacket(pos, length);

		if (mParseFrame < 0) {
			if (pts >= 0) {
				mParseFrame = getFrameFromPTS(pts);
				if (mInitialFrame < 0) {
					mInitialFrame = mParseFrame;
					mParseFrame = 0;
				} else
					mParseFrame -= mInitialFrame;
			} else
				return kParseContinue;
		}

		while(buf < buflimit) {
			if (mParseSkip) {
				int tc = buflimit - buf;

				if (tc > mParseSkip)
					tc = mParseSkip;

				buf += tc;
				mParseSkip -= tc;
				continue;
			}

			mParseHeader = (mParseHeader<<8) + (*buf++);

//			if (isValidAudioHeader(mParseHeader))
//				VDDEBUG("%08lx\n", mParseHeader);

			if (isConsistentAudioHeader(mParseHeader)) {
				if (mParseFrame >= 0) {
					int offs = buf-buf0-4;
					int64 pktpos = pos;

					if (offs < 0) {
						pktpos = mParseLastPacket;
						offs += mParseLastLength;
					}

					unsigned framelen = getFrameLengthFromHeader(mParseHeader);

					mFrameMap[mParseFrame] = tFrameLocation((framelen<<16) + offs, pktpos);

					if (mParseFrame == mParseDesiredFrame)
						return kParseComplete;

					if (mParseFrame > (long)mParseDesiredFrame)
						return kParseRewind;

					++mParseFrame;

					mParseSkip = framelen - 4;
				}
				mParseHeader = 0;
			}
		}

		mParseLastPacket = pos;
		mParseLastLength = length;

		return kParseContinue;
	}
};




///////////////////////////////////////////////////////////////////////////
//
//	Video stream handler
//
///////////////////////////////////////////////////////////////////////////




class VDMPEGVideoStream : public VDMPEGStream, public IVDMPEGVideoStream {
private:

	struct GOPInfo {
		long	first_frame;
		long	frame_count;	// -1 means not yet scanned
	};

	typedef map<int64, GOPInfo> GOPMap;

	struct FrameInfo {
		int64		first_packet;
		int			packet_offset;
		int			length;
		long		display_order;
		FrameType	type;
	};

	struct TentativeFrameInfo {
		int64		first_packet;
		int			packet_offset;
		int			length;
		int			frameno;
		FrameType	type;
	};

	typedef map<long, FrameInfo> FrameMap;
	typedef list<TentativeFrameInfo> FrameList;

	////////////

	int						mnStreamID;

	VDMPEGSequenceHeader	mParsedHdr, mInitialSeqHdr;
	VDAtomicInt refCount;

	// mGOPTable maps byte positions to GOPs.

	GOPMap		mGOPTable;

	// mFrameTable maps frame numbers to GOPs and pack positions.
	// mFrameLimboTable stores frames that have been identified but haven't
	// been associated with a timestamped GOP, usually because the GOP has
	// been found but the timestamp hasn't.  If the timestamp is found before
	// the next GOP tag is found, the limbo pictures are timestamped and
	// added to the frame table.

	FrameMap	mFrameTable;
	FrameList	mFrameLimboTable;

	TentativeFrameInfo	mFrameInProgress;
	int64		mFrameInProgressStart;
	bool		mbFrameInProgress;

	long		mFirstFrame;

public:

	VDMPEGVideoStream(IVDMPEGPacketReader *pReader, int id)
			: VDMPEGStream(pReader)
			, refCount(0)
			, mnStreamID(id)
			, mFirstFrame(-1)
	{
	}

	~VDMPEGVideoStream() {
	}
	
	int AddRef() { return refCount.inc(); }
	int Release() { int rc = refCount.dec(); if (!rc) delete this; return rc; }

	void getInitialFormat(VDMPEGSequenceHeader& dst) {
		dst = mInitialSeqHdr;
	}

	long getFrameCount() {
		long frames = 0;

		if (mFrameTable.end() != mFrameTable.begin()) {
			FrameMap::iterator it = mFrameTable.end();

			--it;

			frames = (*it).first + 1;
		}

		return frames;
	}

	long getNearestI(long f) {
		while(f>0) {
			FrameType type;

			if (ReadPicture(f, NULL, 0, type)<0)
				return -1;

			if (type == kIFrame)
				break;

			--f;
		}

		return f;
	}

	long getPrevIP(long f) {
		while(f>0) {
			FrameType type;

			--f;

			if (ReadPicture(f, NULL, 0, type)<0)
				return -1;

			if (type == kIFrame || type == kPFrame)
				break;
		}

		return f;
	}

	long getNextIP(long f) {
		FrameMap::iterator itEnd = mFrameTable.end();

		if (itEnd == mFrameTable.begin())
			return -1;
		
		--itEnd;

		long lastframe = (*itEnd).first;
		
		for(;;) {
			FrameType type;

			++f;

			if (f > lastframe)
				return -1;

			if (ReadPicture(f, NULL, 0, type)<0)
				return -1;

			if (type == kIFrame || type == kPFrame)
				break;

		}

		return f;
	}

	long DisplayToStreamOrder(long f) {
		if (f<0)
			return -1;

		// Look for the GOP holding the frame.

		GOPMap::iterator it, itEnd;
		bool bAttemptedScan = false;

		for(;;) {
			it = mGOPTable.begin();
			itEnd = mGOPTable.end();

			for(; it!=itEnd; ++it) {
				long delta = f - (*it).second.first_frame;

				if (delta >= 0 && delta < (*it).second.frame_count)
					break;
			}

			if (it != itEnd)
				break;

			// If we haven't already done so, force a scan.  It doesn't
			// matter the particular frames (we're requesting the wrong
			// one anyway) as long as the GOP is pulled in.

			if (bAttemptedScan)
				return -1;

			FrameType t;
			ReadPicture(f, NULL, 0, t);

			bAttemptedScan = true;

		} while(it == itEnd);

		// Okay, find the start of the GOP in the frame table and sweep it.

		FrameMap::iterator itF = mFrameTable.lower_bound((*it).second.first_frame);
		FrameMap::iterator itFE = mFrameTable.end();
		long limit = (*it).second.first_frame + (*it).second.frame_count;

		for(; itF != itFE && (*itF).first < limit; ++itF) {
			if ((*itF).second.display_order == f) {
//				_RPT2(0, "display %d -> stream %d\n", f, (*itF).first);
				return (*itF).first;
			}
		}

		return -1;
	}

	// happily, this one is easier

	long StreamToDisplayOrder(long f) {
		if (f<0)
			return -1;

		FrameMap::iterator it = mFrameTable.find(f);

		if (it == mFrameTable.end()) {
			FrameType t;
			ReadPicture(f, NULL, 0, t);

			if (it == mFrameTable.end())
				return -1;
		}

		return (*it).second.display_order;
	}

	long ReadPicture(long stream_number, void *buf, long maxbuffer, FrameType &ftype) {

		if (stream_number < 0)
			return -1;

		// check if we already have the frame.

		FrameMap::iterator	it = mFrameTable.find(stream_number);

		if (it == mFrameTable.end()) {
			// Not found -- attempt scan.  Find nearest two frames
			// and lerp between them.

			FrameMap::iterator itLower = mFrameTable.lower_bound(stream_number);			
			FrameMap::iterator itUpper = mFrameTable.upper_bound(stream_number);

			// We should have received the very first and very last frames
			// in our frame table from the initialization scan and thus be able
			// to bound every frame in between.  If we can't, the frame is out
			// of range.

			if (itUpper == mFrameTable.end())
				return -1;

			// lower_bound returns the first element >=.  boo.

			if (itLower != mFrameTable.begin())
				--itLower;

			// Grab the pack positions and lerp a guess at the seek position.

			long first_frame	= itLower==mFrameTable.begin() ? 0 : itLower->first;
			int64 first_pos		= itLower==mFrameTable.begin() ? 0 : itLower->second.first_packet + itLower->second.packet_offset;
			int64 second_pos	= itUpper->second.first_packet + itUpper->second.packet_offset;
			int64 desired_pos	= first_pos + (second_pos - first_pos) * (stream_number - first_frame) / (itUpper->first - first_frame);

			// Call parent to do a pack scan and let's hope we find it!

			mParseScanDesiredFrame = stream_number;

			VDDEBUG("Looking for %d\n", stream_number);

			if (!mpReader->DoScanByPos(desired_pos, mnStreamID, first_pos, second_pos))
				return -1;

			// Try again.

			it = mFrameTable.find(stream_number);

			VDASSERT(it != mFrameTable.end());
		}

		// Return frame type.

		ftype = it->second.type;

//		_RPT2(0, "Frame %d is of type %c\n", it->first, "0IPBD567"[ftype]);
		
		// I hope it's not too long.

		if (it->second.length > maxbuffer || !buf)
			return it->second.length;

		// Yahoo!  Read the picture in.

//		_RPT2(0, "Reading from %I64x (%d)\n", it->second.first_packet, it->second.length);

		if (!ReadPackets(mnStreamID,
				it->second.first_packet,
				it->second.packet_offset,
				(char *)buf,
				it->second.length))
				return -1;

		return it->second.length;
	}

	/////////////// scanner ///////////////

private:
	bool mbSeenGOP;
	bool mbSeenSeq;
	int mPhase;
	unsigned char mParseBuf[256];
	int mnParseCount, mnParseLimit;
	long		mGOPStartFrame;
	int64		mposGOP;
	long		mParseScanDesiredFrame;

	// This should only be set if we've found a GOP that is in our
	// table.

	bool		mbParseCaptureGOP;

	// Records the relative byte position in the stream of the first
	// byte after the current block being parsed.

	int64		mParseCounter;

	eParseResult (VDMPEGVideoStream::*mpOnSeqHeader)();
	eParseResult (VDMPEGVideoStream::*mpOnGOPHeader)(int hr, int mn, int sc, int pc, bool drop);

private:
	void _ParseResetScan() {
		mFrameLimboTable.erase(mFrameLimboTable.begin(), mFrameLimboTable.end());
		mbSeenGOP = false;
		mbSeenSeq = false;
		mPhase = 0;
		mnParseCount = 0;
		mnParseLimit = 0;
		mParseCounter = 0;
		mbFrameInProgress = false;
		mbParseCaptureGOP = false;
	}

public:
	void ParseBeginInitialScan() {
		_ParseResetScan();

		mpOnSeqHeader = &VDMPEGVideoStream::_ParseInitialSequenceHeader;
		mpOnGOPHeader = &VDMPEGVideoStream::_ParseInitialGOPHeader;
		mParseScanDesiredFrame = -1;

		printf("restarting!!!\n");
	}

	void ParseBeginSeekScan(bool master) {
		_ParseResetScan();

		mpOnSeqHeader = &VDMPEGVideoStream::_ParseSeekSequenceHeader;
		mpOnGOPHeader = &VDMPEGVideoStream::_ParseGOPHeader;

		if (!master)
			mParseScanDesiredFrame = -1;
	}

	void ParseEnd() {
		if (mbSeenGOP) {
			long first, last;

			_LimboFlush(first, last);
		}
	}

	eParseResult ParseEndSeek() {
		if (mbSeenGOP) {
			long first, last;

			if (_LimboFlush(first, last)) {
				if (mParseScanDesiredFrame >= 0) {
					if (mParseScanDesiredFrame >= last)
						return kParseAbort;		// err, off end of file, chief...

					if (mParseScanDesiredFrame < first) {
						return kParseRewind;	// uhh, go back more
					}
				}

				// I guess we found it.

				return kParseComplete;
			}
		}

		return kParseRewind;
	}

	// frameno is derived from the PTS.

	eParseResult Parse(unsigned char *buf0, int length, int64 pts, int64 packet_pos) {
		unsigned char *buf = buf0;
		int i;
		eParseResult r;

		RecordPacket(packet_pos, length);

		// Keep track of where we are.  This is only valid for one scan.

		mParseCounter += length;

		// sequence header:				000001B3
		// group-of-pictures header:	000001B8

		unsigned char *limit = buf+length;

		while(buf < limit) {

			if (mnParseCount < mnParseLimit) {
				mParseBuf[mnParseCount++] = *buf++;
				continue;
			}

			switch(mPhase) {
			case 0:		// nothing
			case 1:		// 00
				while(*buf) {
					mPhase = 0;
					if (++buf >= limit)
						goto reached_end;
				}
				++mPhase;
				if (++buf >= limit)
					goto reached_end;
				break;
			case 2:		// 00 00
				if (*buf == 0x01) {
					mPhase = 3;
				} else if (*buf != 0x00)
					mPhase = 0;

				if (++buf >= limit)
					goto reached_end;;
				break;
			case 3:		// 00 00 01

				// if we have a frame open, close it!

				if (mbFrameInProgress && (!*buf || *buf>=0xB0)) {
					mFrameInProgress.length		= (int)((mParseCounter-length) - mFrameInProgressStart + (buf-buf0));

					mFrameLimboTable.push_back(mFrameInProgress);
					mbFrameInProgress = false;
				}

				// begin processing header

				if (*buf == 0xB3) {			// 000001B3 (sequence header)
					++buf;
					mnParseCount = 0;
					mnParseLimit = 8;
					mPhase = 4;
				} else if (*buf == 0xB8) {	// 000001B8 (GOP header)
					GOPInfo ginfo;
					int64 bytepos = packet_pos+(buf-buf0);
					GOPMap::iterator it = mGOPTable.find(bytepos);

					mbParseCaptureGOP = false;
					if (it == mGOPTable.end()) {
						ginfo.first_frame = -1;
						ginfo.frame_count = -1;

						VDDEBUG("Adding new GOP (bytepos %I64x)\n", bytepos);

						mGOPTable.insert(GOPMap::value_type(bytepos, ginfo));
						mbParseCaptureGOP = true;
					} else if (it->second.first_frame<0) {
						mbParseCaptureGOP = true;
						VDDEBUG("Scanning incomplete GOP %I64x (%I64x %d %d)\n", bytepos, it->first, it->second.first_frame, it->second.frame_count);
					} else if (mParseScanDesiredFrame>=0) {
						long first = it->second.first_frame;
						long last = first + it->second.frame_count;

						VDDEBUG("This GOP range at %I64x is known: %d-%d\n", bytepos, first, last);

						if (mParseScanDesiredFrame < first) {
							_LimboFlush(first, last);
							return kParseRewind;	// uhh, go back more
						}

						if (mParseScanDesiredFrame < last)
							return kParseComplete;	// found it
					}

					long first, last;

					if (_LimboFlush(first, last) && mParseScanDesiredFrame >= 0) {
						if (mParseScanDesiredFrame < first) {
							return kParseRewind;	// uhh, go back more
						}

						if (mParseScanDesiredFrame < last)
							return kParseComplete;	// found it
					}

					mposGOP = bytepos;

					++buf;
					mPhase = 7;
					mnParseCount = 0;
					mnParseLimit = 4;
				} else if (!*buf && mbParseCaptureGOP) {			// 00000100 (picture header)

//					_RPT3(0, "Beginning picture at %I64x+%x (pts=%I64d)\n", packet_pos, buf-buf0, mParsePTS);

					mFrameInProgress.first_packet	= packet_pos;
					mFrameInProgress.packet_offset	= buf-buf0;
					mFrameInProgress.length			= 0;
					mFrameInProgress.frameno		= -1;
					mFrameInProgressStart			= (mParseCounter-length) + (buf-buf0);

					++buf;
					mPhase = 8;
					mnParseCount = 0;
					mnParseLimit = 4;
				} else {
					mPhase = 0;
				}

				break;

			case 4:		// sequence header (8 bytes minimum)
				// woohoo!!  read the next 8 bytes in
				//
				// +-------------------------------+
				// |           width 4-11          | buf[0]
				// +---------------+---------------+
				// |   width 0-3   |  height 8-11  | buf[1]
				// +-------------------------------+
				// |           height 0-7          | buf[2]
				// +---------------+---------------+
				// |     aspect    |   framerate   | buf[3]
				// +---------------+---------------+
				// |         bitrate 10-17         | buf[4]
				// +-------------------------------+
				// |          bitrate 2-9          | buf[5]
				// +-------+---+-------------------+
				// | br0-1 | 1 |   VBV buffer 5-9  | buf[6]
				// +-------+---+-------+---+---+---+
				// |  VBV buffer 0-4   |CPF|CIQ|...| buf[7]
				// +-------------------+---+---+---+

				mParsedHdr.width		= (mParseBuf[0]<<4) + (mParseBuf[1]>>4);
				mParsedHdr.height		= ((mParseBuf[1]<<8)&0xf00) + mParseBuf[2];
				mParsedHdr.aspect		= (mParseBuf[3]&0xf0)>>4;
				mParsedHdr.framerate	= mParseBuf[3]&0x0f;
				mParsedHdr.bitrate		= (mParseBuf[4]<<10) + (mParseBuf[5]<<2) + (mParseBuf[6]>>6);
				mParsedHdr.vbvbuffersize= ((mParseBuf[6]&0x1f)<<5) + (mParseBuf[7]>>3);
				mParsedHdr.constrained	= 0!=(mParseBuf[7]&4);
				mParsedHdr.custom_intra		= false;
				mParsedHdr.custom_nonintra	= false;

				// Return a more sane number for VBR.

				if (mParsedHdr.bitrate == 0x3FFFF)
					mParsedHdr.bitrate = -1;

				// Sanity check -- width, height, aspect, framerate, and bitrate may not be zero.
				// Marker bit must be set.

				if (!mParsedHdr.width || !mParsedHdr.height || !mParsedHdr.aspect
					|| !mParsedHdr.framerate || !mParsedHdr.bitrate
					|| !(mParseBuf[6]&0x20)) {

					mPhase = 0;
					break;
				}

				// Pass.

				if (mParseBuf[7]&2) {		// custom intramatrix?
					++mPhase;
					mnParseLimit += 64;
				} else {
					mPhase = 0;

					if (kParseContinue != (r = (this->*mpOnSeqHeader)()))
						return r;
				}
				break;

			case 5:		// sequence header intramatrix
				mParsedHdr.custom_intra = true;

				for(i=0; i<64; ++i)
					mParsedHdr.intra[i] = ((mParseBuf[7+i]&1)<<7) + ((mParseBuf[8+i]&0xfe)>>1);

				if (mParseBuf[7+64]&1) {
					mnParseLimit += 64;
					++mPhase;
				} else {
					mPhase = 0;

					if (kParseContinue != (r = (this->*mpOnSeqHeader)()))
						return r;
				}

				break;

			case 6:		// sequence header nonintramatrix
				mParsedHdr.custom_nonintra = true;
				memcpy(mParsedHdr.nonintra, mParseBuf+8+64, 64);

				mPhase = 0;

				if (kParseContinue != (r = (this->*mpOnSeqHeader)()))
					return r;

				break;

			case 7:		// GOP header (4 bytes)
				//
				// +---+-------------------+-------+
				// |DFF|       hours       | min4-5| buf[0]
				// +---+-----------+---+---+-------+
				// |  minutes 0-3  | 1 | secs 3-5  | buf[1]
				// +-----------+---+---+-----------+
				// |  secs 0-3 |   pictures 1-5    | buf[2]
				// +---+---+---+-------------------+
				// |pc0|C_G|B_L|xxxxxxxxxxxxxxxxxxx| buf[3] (closed_gop, broken_link)
				// +---+---+---+-------------------+

				{
					int hours, minutes, seconds, pictures;
					bool dropf;

					mPhase = 0;

					// Verify marker bit.

					if (!(mParseBuf[1] & 0x08))
						break;

					// Decode the timestamp.

					hours = (mParseBuf[0]>>2)&31;
					minutes = ((mParseBuf[0]&3)<<4) + (mParseBuf[1]>>4);
					seconds = ((mParseBuf[1]&7)<<3) + (mParseBuf[2]>>5);
					pictures = (mParseBuf[2]&31)*2 + (mParseBuf[3]>>7);
					dropf = 0 != (mParseBuf[0] & 0x80);

					// XingMPEG Encoder kludge

#if 0
					if (hours+minutes+seconds+pictures && !pictures) {
						if (++seconds >= 60) {
							seconds = 0;
							if (++minutes >= 60) {
								minutes = 0;
								if (++hours >= 24)
									hours = 0;
							}
						}
					}
#endif

					// Verify that time components are legal.

					if (hours>=24 || minutes>=60 || seconds>=60 || pictures>=60)
						break;

					if (kParseContinue != (r = (this->*mpOnGOPHeader)(hours, minutes, seconds, pictures, dropf)))
						return r;
					break;
				}

			case 8:		// Picture header (4-5 bytes)
				//
				// +-------------------------------+
				// |     temporal reference 2-9    | buf[0]
				// +-------+-----------+-----------+
				// | tr0-1 |   type    |delay 13-15| buf[1]
				// +-------------------+-----------+
				// |         VBV delay 5-12        | buf[2]
				// +-------------------+---+-------+
				// |     VBV delay 0-4 |FPF|FFC 1-2| buf[3] (full_pel_forward_vector, forward_f_code)
				// +---+---+-----------+---+-------+
				// |FFC|FPB|back f code|xxxxxxxxxxx| buf[4] (full_pel_backward_vector)
				// +---+---+-----------+-----------+

				// Type may not be zero.

				if (!(mParseBuf[1] & 0x38)) {
					mPhase = 0;
					break;
				}

				// If the picture type is P or B (2 or 3), we will need an additional
				// byte.

				++mPhase;
				if ((mParseBuf[1]&0x30) == 0x10) {
					++mnParseLimit;
					break;
				}

			case 9:		// Picture header complete

				mPhase = 0;
				if ((mParseBuf[1] & 0x38) == 0x18) {	// B-frame: backward_f_code may not be zero.
					int bfc = (mParseBuf[4]>>3)&7;

					if (!bfc)
						break;
				}

				if ((mParseBuf[1] & 0x30) == 0x10) {	// P-frame or B-frame: forward_f_code may not be zero.
					int ffc = ((mParseBuf[3]&3)<<1) + ((mParseBuf[4]&0x80)>>7);

					if (!ffc)
						break;
				}

				// Looks okay.  Send it off.

				if (kParseContinue != (r = _ParsePicHeader()))
					return r;

				break;

			}
		}

reached_end:

		return kParseContinue;
	}

private:

	eParseResult _ParseInitialSequenceHeader() {
		mInitialSeqHdr = mParsedHdr;
		mbSeenSeq = true;
		return kParseContinue;
	}

	eParseResult _ParseSeekSequenceHeader() {
		return kParseContinue;
	}

	eParseResult _ParseInitialGOPHeader(int h,int m,int s,int p,bool drop) {
		_ParseGOPHeader(h, m, s, p, drop);
		if (mbSeenGOP && mbSeenSeq)
			return kParseComplete;

		mbSeenGOP = true;

		return kParseContinue;
	}

	eParseResult _ParseGOPHeader(int h, int m, int s, int p, bool bDropFlag) {
		mbSeenGOP = true;

		// Figure out the current frame from the GOP time.

		VDDEBUG("Processing GOP: %d:%02d:%02d.%02d\n", h, m, s, p);

		switch(mInitialSeqHdr.framerate) {
		case 1:	p += (s+60*m + 1440*h)*24; break;		// drop_frame_flag cannot be set for 23.976
		case 2:	p += (s+60*m + 1440*h)*24; break;
		case 3:	p += (s+60*m + 1440*h)*25; break;
		case 4:
			if (bDropFlag) {
				// This is a pain.
				//
				// drop_frame_flag is set, so pictures 0 and 1 at the start of each
				// minute dropped, except for minutes 0, 10, 20, 30, 40, and 50.
				// This gives 54*(60*30 - 2) + 6*60*30 = 107892 pictures per hour.
				//
				// Minutes divisible by 10 have 1800 pictures.
				// Minutes not divisible by 10 have 1798 pictures.
				// Every 10 minutes is 17982 pictures.

				p += 17982 * (m / 10) + h * 107892;

				// Every minute is 1798 pictures except the first, which is 1800.
				// Strip off the two phantom pictures for non-10 minutes.

				m %= 10;

				if (m)
					p += 1798*m;

				// Every second is 30 pictures except the first second, which is 28,
				// except for the first minute of every 10 minutes....

				p += s*30;

				if (s && m)
					p -= 2;

			} else {
				p += (s+60*m + 1440*h)*30;
			}
			break;
		case 5:	p += (s+60*m + 1440*h)*30; break;
		case 6:	p += (s+60*m + 1440*h)*50; break;
		case 7:	p += (s+60*m + 1440*h)*60; break;		// drop_frame_flag cannot be set for 59.94
		case 8:	p += (s+60*m + 1440*h)*60; break;
		}

		VDDEBUG("Frame is %d\n", p);

		mGOPStartFrame = p;

		return kParseContinue;
	}

	eParseResult _ParsePicHeader() {
		if (mbSeenGOP) {
			int tref = (mParseBuf[0]<<2) + ((mParseBuf[1]&0xc0)>>6);
			int type = (mParseBuf[1]>>3)&7;
			
//			_RPT2(0, "picture found: %c-frame (temporal ref: %d)\n", "0IPBD567"[type], tref);
			
			VDASSERT(type && type<=3);
				
			// Document the frame.
			
			mbFrameInProgress = true;

			mFrameInProgress.type			= (FrameType)type;				
		} else
			_RPT0(0,"dropping picture\n");
				
		return kParseContinue;
	}

	bool _LimboFlush(long& firstframe, long& lastframe) {
		FrameList::iterator it = mFrameLimboTable.begin();
		FrameList::iterator itEnd = mFrameLimboTable.end();

		if (it == itEnd) {
			return false;
		}

		FrameList::iterator itLastIP = itEnd;
		int bframeno = 0;

		// Number all pages and try to spot a base PTS.

		for(; it!=itEnd; ++it) {
			TentativeFrameInfo& info = *it;

			if (info.type == kBFrame)
				info.frameno = bframeno++;
			else {
				if (itLastIP != itEnd)
					itLastIP->frameno = bframeno++;

				itLastIP = it;
			}
		}

		if (itLastIP != itEnd)
			itLastIP->frameno = bframeno++;

		// Add GOP.

		GOPInfo ginfo;

		ginfo.first_frame		= mGOPStartFrame;
		ginfo.frame_count		= bframeno;

		pair<GOPMap::iterator,bool> insresult = mGOPTable.insert(GOPMap::value_type(mposGOP, ginfo));

		if (!insresult.second) {
			// Is it incomplete?

			firstframe = (*insresult.first).second.first_frame;

			if (firstframe < 0)
				(*insresult.first).second = ginfo;
			else {
				lastframe = firstframe + (*insresult.first).second.frame_count;

				_RPT1(0,"Warning: LimboFlush called preexisting GOP %I64x!\n", (*insresult.first).first);
				return true;
			}
		}

//		_RPT3(0, "range: %I64x %d %d\n", mposGOP, mGOPTable[mposGOP].first_frame, mGOPTable[mposGOP].frame_count);

		// Dump frames into the frame heap.

		long frame = mGOPStartFrame;

		for(it = mFrameLimboTable.begin(); it != itEnd; ++it) {
			TentativeFrameInfo& tinfo = *it;
			FrameInfo info;

			info.first_packet	= tinfo.first_packet;
			info.packet_offset	= tinfo.packet_offset;
			info.length			= tinfo.length;
			info.type			= tinfo.type;
			info.display_order	= tinfo.frameno + mGOPStartFrame;

#if 0
			VDDEBUG("Adding frame %d: pos(%I64x), length(%ld), type(%c)\n"
					, frame
					, info.first_packet + info.packet_offset
					, info.length
					, "0IPBD567"[tinfo.type]);
#endif

			mFrameTable[frame++] = info;
		}

		// Wipe limbo table.

		mFrameLimboTable.erase(mFrameLimboTable.begin(), mFrameLimboTable.end());

		// Return range.

		firstframe = mGOPStartFrame;
		lastframe = frame;

//		printf("flushing range: [%d,%d)\n", firstframe, lastframe);

		return true;
	}
};

///////////////////////////////////////////////////////////////////////////

class VDMPEGFile : public IVDMPEGFile, public IVDMPEGPacketReader {
private:
	struct MPEGSystemHeader {
		int		header_length;
		int		rate_bound;
		int		audio_bound;
		int		video_bound;
		bool	fixed:1,
				constrained:1,
				audio_lock:1,
				video_lock:1;
	};

	enum { kInitScanLimit = 131072 };
	enum { kBufferSize = 131072 };

	VDAtomicInt refCount;

	unsigned char *pBuffer;
	int64 posBufferBase;
	long nBufferLimit;
	long nBufferOffset;
	long nBufferSize;

	MPEGSystemHeader syshdr;

	VDMPEGAudioStream *pAudioStream[32];
	VDMPEGVideoStream *pVideoStream[16];

	long	nAudioStreamMask;
	long	nVideoStreamMask;

	unsigned char packetbuf[65536];

	VDMPEGCache mPacketCache;

	VDFile		mFile;

public:
	VDMPEGFile()
		: refCount(0)
		, pBuffer(new unsigned char[kBufferSize])
		, posBufferBase(0)
		, nBufferLimit(0)
		, nBufferOffset(0)
		, nBufferSize(kBufferSize)
		, mPacketCache(128, 4096)
	{
		memset(pAudioStream, 0, sizeof pAudioStream);
		memset(pVideoStream, 0, sizeof pVideoStream);
	}

	~VDMPEGFile() {
		Close();
		delete[] pBuffer;
	}

	int AddRef() { return refCount.inc(); }
	int Release() { int rc = refCount.dec(); if (!rc) delete this; return rc; }

	void Open(const wchar_t *pszFile) {
		mFile.open(pszFile, nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting | nsVDFile::kSequential);
		_streamFlush();
		posBufferBase = 0;

		nAudioStreamMask = 0;
		nVideoStreamMask = 0;
	}

	void Close() {
		int i;

		for(i=0; i<32; ++i) {
			delete pAudioStream[i];
			pAudioStream[i] = NULL;
		}

		for(i=0; i<16; ++i) {
			delete pVideoStream[i];
			pVideoStream[i] = NULL;
		}

		mFile.closeNT();		// It's open for read.  We don't care if the close fails.
	}

	bool Init() {
		int64 sysclk;
		long mux;

		// Scan for first packet.

		if (!_streamScanPackStart(sysclk, mux))
			return false;

		int stream;
		int length;
		int64 pts;
		int64 dts;

		if (!_streamScanSystemHeader(syshdr))
			return false;

		// Scan the first 128K for streams.  Once we've found a stream,
		// continue scanning until we get the first PTS for it.

		int nStreamsWithoutFormat = 0;
		long nVideoFormatMask = -1L;
		long nAudioFormatMask = -1L;
		int64 packetpos;

		while(_streamScanPacket(-1, -1, stream, length, pts, dts, packetpos) && (_streamPos() < kInitScanLimit || nStreamsWithoutFormat>0)) {
//		while(_streamScanPacket(-1, -1, stream, length, pts, dts, packetpos)) {
			_streamRead(packetbuf, length);

			mPacketCache.Write(packetbuf, packetpos, length);

			if (stream < 32) {
				if (!pAudioStream[stream]) {
					pAudioStream[stream] = new VDMPEGAudioStream(this, stream);
					pAudioStream[stream]->AddRef();
					pAudioStream[stream]->ParseInit(NULL, 0, -1, 0);
					++nStreamsWithoutFormat;
					nAudioStreamMask |= (1<<stream);
				}

				if (nAudioFormatMask & (1<<stream)) {
					eParseResult r = pAudioStream[stream]->ParseInit(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
					else if (r == kParseComplete) {
						nAudioFormatMask &= ~(1<<stream);
						--nStreamsWithoutFormat;
					}
				}
			} else {
				if (!pVideoStream[stream-32]) {
					pVideoStream[stream-32] = new VDMPEGVideoStream(this, stream);
					pVideoStream[stream-32]->AddRef();
					pVideoStream[stream-32]->ParseBeginInitialScan();
					++nStreamsWithoutFormat;
					nVideoStreamMask |= (1<<(stream-32));
				}

//				if (nVideoFormatMask & (1<<(stream-32))) {
				{
					eParseResult r = pVideoStream[stream-32]->Parse(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
					else if (r == kParseComplete) {
						nVideoFormatMask &= ~(1<<(stream-32));
						--nStreamsWithoutFormat;
					}
				}
			}
		}

		// Try to determine the length of streams.  PTS markers must be placed every
		// 0.7s, so we should hit the final PTSes for all streams if we seek back
		// a full second (~180K at VideoCD rates), assuming streams are the same
		// length.  From there, we can determine the total length of audio streams
		// by looking for packets past the PTS; with video streams we have to
		// count frames as well.
#if 1
		int64 pos = mFile.size() - syshdr.rate_bound*50;
//		int64 pos = 0;

//		printf("seeking to %I64d\n", pos);

		if (pos<0)
			pos = 0;

		_streamSeek(pos);

		// Reinitialize parsing for all audio streams.

		for(stream=0; stream<32; ++stream)
			if (pAudioStream[stream]) {
				pAudioStream[stream]->ParseReset();
			}

		// Reinitialize parsing for all video streams.

		for(stream=0; stream<16; ++stream)
			if (pVideoStream[stream]) {
				pVideoStream[stream]->ParseBeginSeekScan(false);
			}

		// Decode until we hit the end.

		if (!_streamScanPackStart(sysclk, mux))
			return false;

		while(_streamScanPacket(-1, -1, stream, length, pts, dts, packetpos)) {
			_streamRead(packetbuf, length);

			mPacketCache.Write(packetbuf, packetpos, length);

//			printf("%I64d\n", packetpos);

			if (stream >= 32) {
				if (pVideoStream[stream-32]) {
					eParseResult r = pVideoStream[stream-32]->Parse(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
				}
			} else {
				if (pAudioStream[stream]) {
					eParseResult r = pAudioStream[stream]->Parse(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
				}
			}
		}

		for(stream=0; stream<16; ++stream)
			if (pVideoStream[stream])
				pVideoStream[stream]->ParseEndSeek();
#endif

		return true;
	}

	long getAudioStreamMask() { return nAudioStreamMask; }
	long getVideoStreamMask() { return nVideoStreamMask; }

	IVDMPEGAudioStream *getAudioStream(int n) {
		if (!pAudioStream[n])
			return NULL;

		pAudioStream[n]->AddRef();

		return pAudioStream[n];
	}

	IVDMPEGVideoStream *getVideoStream(int n) {
		if (!pVideoStream[n])
			return NULL;

		pVideoStream[n]->AddRef();

		return pVideoStream[n];
	}

	bool PreScan() {
		int64 sysclk;
		long mux;
		int stream;

		_streamSeek(0);

		// Reinitialize parsing for all audio streams.

		for(stream=0; stream<32; ++stream)
			if (pAudioStream[stream]) {
				pAudioStream[stream]->ParseReset();
			}

		// Reinitialize parsing for all video streams.

		for(stream=0; stream<16; ++stream)
			if (pVideoStream[stream]) {
				pVideoStream[stream]->ParseBeginSeekScan(false);
			}

		// Decode until we hit the end.

		if (!_streamScanPackStart(sysclk, mux))
			return false;

		long megabytes = (long)(mFile.size()+1048575)>>20;

		VDProgressAbortable progress(megabytes, "MPEG-1 file parser", "%ldMB of %ldMB", "Prescanning input file");

		int length;
		int64 pts;
		int64 dts;
		int64 packetpos;

		while(_streamScanPacket(-1, -1, stream, length, pts, dts, packetpos)) {
			progress.advance((long)(mFile.tell() >> 20));

			_streamRead(packetbuf, length);

			mPacketCache.Write(packetbuf, packetpos, length);

			if (stream >= 32) {
				if (pVideoStream[stream-32]) {
					eParseResult r = pVideoStream[stream-32]->Parse(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
				}
			} else {
				if (pAudioStream[stream]) {
					eParseResult r = pAudioStream[stream]->Parse(packetbuf, length, pts, packetpos);

					if (r == kParseAbort)
						return false;
				}
			}
		}

		for(stream=0; stream<16; ++stream)
			if (pVideoStream[stream])
				pVideoStream[stream]->ParseEndSeek();

		return true;
	}

	//////////////////
	//
	//	Scan for a desired packet or series of packets.  The scan begins
	//	one-second before the desired packet and continues until the
	//	stream handler discovers:
	//
	//		kParseComplete:		The desired packets have been found.
	//		kParseRewind:		The current read position is too far
	//							forward.  Restart at the original
	//							position minus a defined amount that
	//							doubles each time (binary exp backoff).
	//		kParseAbort:		The desired packet is known to be
	//							missing -- this can occur if the handler
	//							spots timestamps before and after the
	//							desired range during the scan.
	//							

	bool DoScanByPos(int64 pos, int active_stream, int64 lobound, int64 hibound) {
		int64 sysclk;
		long mux;
		int stream;
		int length;
		int64 pts;
		int64 dts;
		int64 packetpos;
		int64 backoff = syshdr.rate_bound * 50;		// start with one-second backoff

		// round backoff up to 64K, and pos down by that amount

		backoff = (backoff + 65535) & ~65535;

		pos = (pos - backoff) & ~65535;
		if (pos<0) pos = 0;

		// Decode until we hit the end.

//		_RPT1(0, "DoScanByPos: beginning scan at %I64x\n", pos);

		for(;;) {
			bool successful;

			if (active_stream >= 32)
				pVideoStream[active_stream-32]->ParseBeginSeekScan(true);
			else
				pAudioStream[active_stream]->ParseReset();

			_streamSeek(pos & ~65535);

			if (!_streamScanPackStart(sysclk, mux))
				return false;

			do {
				eParseResult r;

				successful = _streamScanPacket(-1, -1, stream, length, pts, dts, packetpos);

				if (successful && stream != active_stream) {
					_streamSkip(length);
					continue;
				}

				if (successful) {
					_streamRead(packetbuf, length);
					mPacketCache.Write(packetbuf, packetpos, length);
				} else
					stream = active_stream;

				if (stream >= 32) {		// video
					if (successful)
						r = pVideoStream[stream-32]->Parse(packetbuf, length, pts, packetpos);
					else {
						_RPT0(0, "DoScanByPos: end of file reached\n");
						r = pVideoStream[stream-32]->ParseEndSeek();
					}
				} else {				// audio

					if (successful)
						r = pAudioStream[stream]->Parse(packetbuf, length, pts, packetpos);
					else
						r = kParseRewind;
				}

				if (r == kParseRewind) {
					if (pos <= lobound)
						return false;

					pos -= backoff;
					if (pos < 0)
						pos = 0;

					backoff *= 2;

					if (pos < lobound)
						pos = lobound;

					_RPT1(0, "DoScanByPos: missed, restarting scan at %I64x\n", pos);

					break;
				} else if (r == kParseAbort) {
					_RPT0(0, "DoScanByPos: scan aborted by handler\n");
					return false;
				} else if (r == kParseComplete)
					return true;
			} while(successful);
		}
	}

	// The packet position given is the first byte after the start code.

	bool ReadPacket(int desired_stream, int64 packet, int offset, char *dst, int length) {

		if (mPacketCache.Read(dst, packet, length, offset))
			return true;

		int64 packetpos;
		int64 pts, dts;
		int pktlength;
		int stream;
		long audio_mask = 0, video_mask = 0;

		if (desired_stream<32)
			audio_mask = 1<<desired_stream;
		else
			video_mask = 1<<(desired_stream-32);

		_streamSeek(packet - 4);

		if (_streamScanPacket(audio_mask, video_mask, stream, pktlength, pts, dts, packetpos)) {
			VDASSERT(stream == desired_stream);
			pktlength -= offset;
			if (pktlength > length)
				pktlength = length;

			if (offset)
				_streamSkip(offset);

			if (!_streamRead(dst, pktlength))
				return false;

			length -= pktlength;
			dst += pktlength;
			if (!length)
				return true;

			offset = 0;
		}

		return false;
	}

	//////////////////

private:
	void _streamSeek(int64 pos) {
		if (pos >= posBufferBase && pos <= posBufferBase + nBufferLimit) {
			nBufferOffset = (long)(pos - posBufferBase);
			return;
		}

		_streamFlush();
		if (pos >= posBufferBase && pos < posBufferBase + nBufferSize) {
			long off = (long)(pos - posBufferBase);

			_streamReload();
			nBufferOffset += off;

			if (nBufferOffset >= nBufferLimit)
				_streamFlush();
		} else {
			mFile.seek(pos, nsVDFile::kSeekStart);
			posBufferBase = pos;
		}
	}

	void _streamFlush() {
		posBufferBase += nBufferLimit;
		nBufferLimit = 0;
	}

	bool _streamReload() {
		posBufferBase += nBufferLimit;
		nBufferLimit = mFile.readData(pBuffer, nBufferSize);
		nBufferOffset = 0;

		return nBufferLimit > 0;
	}

	inline int _streamReadNextByte() {
		if (nBufferOffset >= nBufferLimit && !_streamReload())
			return -1;

		return (int)pBuffer[nBufferOffset++];
	}

	inline int _streamReadAndCompareNextLong(long cv) {
		if (nBufferOffset+4 <= nBufferLimit) {
			nBufferOffset += 4;
			return *(long *)&pBuffer[nBufferOffset-4] == cv;
		}

		// nuts.

		long v = 0;
		int i, c;

		for(i=0; i<4; ++i) {
			c = _streamReadNextByte();

			if (c<0)
				return -1;

			v = (v<<8) + c;
		}

		return v == cv;
	}

	bool _streamSkip(int bytes) {
		while(bytes > 0) {
			if (nBufferOffset < nBufferLimit) {
				int tc = nBufferLimit - nBufferOffset;

				if (tc > bytes)
					tc = bytes;

				nBufferOffset += tc;
				bytes -= tc;
			} else
				if (!_streamReload())
					return false;
		}

		return true;
	}

	bool _streamRead(void *dst, int bytes) {
		while(bytes > 0) {
			if (nBufferOffset < nBufferLimit) {
				int tc = nBufferLimit - nBufferOffset;

				if (tc > bytes)
					tc = bytes;

				memcpy(dst, pBuffer + nBufferOffset, tc);

				nBufferOffset += tc;
				dst = (char *)dst + tc;
				bytes -= tc;
			} else
				if (!_streamReload())
					return false;
		}

		return true;
	}

	int64 _streamPos() {
		return posBufferBase + nBufferOffset;
	}

	int _streamScanStartCode() {
		for(;;) {
			int c;

			// Nothing

			do {
				if ((c = _streamReadNextByte()) < 0)
					return -1;
			} while(c);

			// 00

			if ((c = _streamReadNextByte()) < 0)
				return -1;

			if (c)
				continue;
				
			// 00 00

			do {
				if ((c = _streamReadNextByte()) < 0)
					return -1;
			} while(!c);

			if (c != 1)
				continue;

			return _streamReadNextByte();
		}
	}

	bool _streamScanPackStart(int64& sysclk_ref, long& mux_rate) {
		// Pack start codes are 000001BA.

		int c;

		while((c = _streamScanStartCode()) >= 0) {

			if (c == 0xBA) {
				unsigned char buf[8];

				// woohoo!!  read the next 8 bytes in
				//
				// +---+---+---+---+-----------+---+
				// | 0 | 0 | 1 | 0 |sysclk32-30| 1 | buf[0]
				// +---+---+---+---+-----------+---+
				// |          sysclk 29-22         | buf[1]
				// +---------------------------+---+
				// |        sysclk 21-15       | 1 | buf[2]
				// +---------------------------+---+
				// |          sysclk 14-7          | buf[3]
				// +---------------------------+---+
				// |         sysclk 6-0        | 1 | buf[4]
				// +---+-----------------------+---+
				// | 1 |      mux rate 21-15       | buf[5]
				// +---+---------------------------+
				// |          mux rate 14-7        | buf[6]
				// +---------------------------+---+
				// |       mux rate  6-0       | 1 | buf[7]
				// +---------------------------+---+

				if (!_streamRead(buf, 8))
					return false;

				// decode scr and mux rate

				sysclk_ref	= ((int64)(buf[0]&0x0e) << 29)
							+ ((int64) buf[1]       << 22)
							+ ((int64)(buf[2]&0xfe) << 14)
							+ ((int64) buf[3]       <<  7)
							+ (       (buf[4]&0xfe) >>  1);

				mux_rate	=(((long)(buf[5]&0x7f) << 15)
							+ ((long)buf[6] << 7)
							+ ((buf[7]&0xfe) >> 1)) * 400;

				// validate - mux rate cannot be zero

				if (	(buf[0]&0xf1) != 0x21
					||	(buf[2]&buf[4]&buf[7]&1) != 1
					||	(buf[5]&0x80) != 0x80
					||	!mux_rate) {

					// Oops, bad pack.  Back up and continue scanning.

					_streamSeek(_streamPos() - 8);
					continue;
				}

				// looks good to me!

				return true;
			}
		}

		return false;
	}

	// This routine assumes you've already seeked up to a pack header.

	bool _streamScanSystemHeader(MPEGSystemHeader& header) {
		unsigned char buf[8];

		// System headers always follow pack headers, so it's safer
		// to look for a valid system header after a valid pack
		// header.

		for(;;) {
			int c = _streamReadAndCompareNextLong(0xBB010000);

			if (c<0)
				return false;
			else if (!c) {
				int64 sysref;
				long muxrate;

				if (!_streamScanPackStart(sysref, muxrate))
					return false;
			} else {
				// read in system header (8+ bytes)
				//
				// +-------------------------------+
				// |       header length 15-8      | buf[0]
				// +-------------------------------+
				// |       header length  7-0      | buf[1]
				// +---+---------------------------+
				// | 1 |     rate bound 21-15      | buf[2]
				// +---+---------------------------+
				// |        rate bound 14-7        | buf[3]
				// +---------------------------+---+
				// |        rate bound 6-0     | 1 | buf[4]
				// +-----------------------+---+---+
				// |      audio bound      |F_F|C_P| buf[5]  fixed_flag, constrained_parameters
				// +---+---+---+-----------+---+---+
				// |A_L|V_L| 1 |    video bound    | buf[6]  audio_lock, video_lock
				// +---+---+---+-------------------+
				// |      reserved (1111 1111)     | buf[7]
				// +-------------------------------+

				if (!_streamRead(buf, 8))
					return false;

				// decode systems header

				header.header_length	= ((int)buf[0]<<8) + buf[1];
				header.rate_bound		= (((int)buf[2]&0x7f)<<15)
										+ ((int)buf[3]<<7)
										+ ((int)buf[4]>>1);
				header.audio_bound		= (buf[5]&0xfc)>>2;
				header.video_bound		= buf[6]&0x1f;
				header.fixed			= 0!=(buf[5]&2);
				header.constrained		= 0!=(buf[5]&1);
				header.audio_lock		= 0!=(buf[6]&0x80);
				header.video_lock		= 0!=(buf[6]&0x40);

				// check system header for validity

				int64 pos = _streamPos();

				if (	(buf[2]&0x80) != 0x80
					||	(buf[4]&0x01) != 0x01
					||	(buf[6]&0x20) != 0x20
					||	header.header_length < 8
					||	header.audio_bound > 32
					||	header.video_bound > 16
					||	header.header_length < 6
					||	!_streamSkip(header.header_length - 6)
					) {

					// failed -- continue search

					_streamSeek(pos - 8);
				} else
					return true;
			}
		}
	}

	// This should be as robust as possible.  Preferably call after a pack.

	bool _streamScanPacket(long audio_mask, long video_mask, int& stream, int& length, int64& pts, int64& dts, int64 &packet_pos) {
		int c;

		for(;;) {
			while((c = _streamScanStartCode()) >= 0 && c < 0xBC)
				;

			if (c < 0)
				return false;

			stream = c;

			packet_pos = _streamPos();

			// process packet
			//
			// BC		reserved
			// BD		private_stream_1
			// BE		padding
			// BF		private_stream_2
			// C0-DF	audio stream
			// E0-EF	video stream
			// F0-FF	reserved stream

			if ((c = _streamReadNextByte())<0) return false;
			length = c<<8;
			if ((c = _streamReadNextByte())<0) return false;
			length += c;

			if (stream != 0xBF) {	// private_stream_2 lacks stuff
				int64 crap_start = _streamPos();

				// eat stuffing bytes

				do {
					c = _streamReadNextByte();
				} while(c == 0xff);

				// 01 -> STD buffer scale / size

				if ((c & 0xc0) == 0x40) {
					if ((c = _streamReadNextByte())<0)
						return false;
					if ((c = _streamReadNextByte())<0)
						return false;
				}

				// 0010 -> PTS only
				// 0011 -> PTS + DTS
				//
				// +---+---+---+---+-----------+---+
				// | 0 | 0 | 1 |DTS| PTS 32-30 | 1 | buf[0]
				// +---+---+---+---+-----------+---+
				// | Presentation timestamp 29-22  | buf[1]
				// +---------------------------+---+
				// |         PTS 21-15         | 1 | buf[2]
				// +---------------------------+---+
				// |  Presentation timestamp 14-7  | buf[3]
				// +---------------------------+---+
				// |          PTS 6-0          | 1 | buf[4]
				// +---+---+---+---+-----------+---+
				// | 0 | 0 | 0 | 1 | DTS 32-30 | 1 | buf[5]
				// +---+---+---+---+-----------+---+
				// |    Decoding timestamp 29-22   | buf[6]
				// +---------------------------+---+
				// |         DTS 21-15         | 1 | buf[7]
				// +---------------------------+---+
				// |    Decoding timestamp 14-7    | buf[8]
				// +---------------------------+---+
				// |          DTS 6-0          | 1 | buf[9]
				// +---------------------------+---+

				unsigned char buf[10];
				bool pts_exists = !!(c&0x20);
				bool dts_exists = !!(c&0x10);

				buf[0] = c;

				if ((c&0xf0) == 0x20) {			// 0010 -> PTS only
					if (!_streamRead(buf+1, 4))
						return false;
				} else if ((c&0xf0) == 0x30) {	// 0011 -> PTS and DTS
					if (!_streamRead(buf+1, 9))
						return false;
				} else if (c != 0x0f) {			// otherwise must be 00001111
					// Uhh... uh oh.
					//
					// Looks like we were either fooled or something got
					// corrupted.  Assume remainder of packet may be
					// missing and drop immediately to start code scan.

					continue;
				}

				if (pts_exists) {

					// Validate PTS marker bits.  Force resync on failure.

					if (!(buf[0]&buf[2]&buf[4]&1))
						continue;

					pts	= ((int64)(buf[0]&0x0e) << 29)
						+ ((int64) buf[1]       << 22)
						+ ((int64)(buf[2]&0xfe) << 14)
						+ ((int64) buf[3]       <<  7)
						+ (       (buf[4]&0xfe) >>  1);
				} else
					pts = -1;

				if (dts_exists) {

					// Validate DTS marker bits.  Force resync on failure.

					if ((buf[5]&0xf1)!=0x011 || (buf[7]&buf[9]&1)!=1)
						continue;

					dts	= ((int64)(buf[5]&0x0e) << 29)
						+ ((int64) buf[6]       << 22)
						+ ((int64)(buf[7]&0xfe) << 14)
						+ ((int64) buf[8]       <<  7)
						+ (       (buf[9]&0xfe) >>  1);
				} else
					dts = pts;

				// If STD_buffer and PTS/DTS stuff ends up longer
				// than the header, someone fscked up and we should
				// drop to resync.

				length -= (long)(_streamPos() - crap_start);

				if (length < 0)
					continue;
			}

			// if this is a stream we're interested in, return,
			// otherwise skip the data in the stream

			switch(stream & 0xf0) {
			case 0xc0:		// C0-DF: audio
			case 0xd0:
				if (audio_mask & (1<<(stream&0x1f))) {
					stream -= 0xc0;
					return true;
				}
				break;
			case 0xe0:
				if (video_mask & (1<<(stream&0x1f))) {
					stream -= 0xc0;
					return true;
				}
			}

			// Skip packet payload and continue looking for interesting streams

			_streamSkip(length);
		}
	}
};

IVDMPEGFile *CreateVDMPEGFile() {
	return new VDMPEGFile();
}
