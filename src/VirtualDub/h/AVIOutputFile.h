//	VirtualDub - Video processing and capture application
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

#ifndef f_AVIOUTPUTFILE_H
#define f_AVIOUTPUTFILE_H

class IVDMediaOutput;

class IVDMediaOutputAVIFile : public IVDMediaOutput {
public:
	virtual void disable_os_caching() = 0;
	virtual void disable_extended_avi() = 0;
	virtual void set_1Gb_limit() = 0;
	virtual void set_capture_mode(bool b) = 0;
	virtual void setAlignment(int stream, uint32 align) = 0;
	virtual void setInterleaved(bool bInterleaved) = 0;
	virtual void setBuffering(sint32 nBufferSize, sint32 nChunkSize) = 0;
	virtual void setSegmentHintBlock(bool fIsFinal, const char *pszNextPath, int cbBlock) = 0;
	virtual void setHiddenTag(const char *pTag) = 0;
	virtual void setIndexingLimits(sint32 nMaxSuperIndexEntries, sint32 nMaxSubIndexEntries) = 0;

	virtual void setTextInfoEncoding(int codePage, int countryCode, int language, int dialect) = 0;
	virtual void setTextInfo(uint32 ckid, const char *text) = 0;

	virtual uint32 bufferStatus(uint32 *lplBufferSize) = 0;
};

IVDMediaOutputAVIFile *VDCreateMediaOutputAVIFile();

#endif
