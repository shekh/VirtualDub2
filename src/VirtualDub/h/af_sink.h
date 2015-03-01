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

#ifndef f_AF_SINK_H
#define f_AF_SINK_H

class IVDAudioFilterSink {
public:
	virtual uint32 ReadSamples(void *dst, uint32 samples) = 0;
	virtual const void *GetFormat()=0;
	virtual int GetFormatLen()=0;
	virtual sint64 GetLength()=0;
	virtual bool IsEnded() = 0;
};

IVDAudioFilterSink *VDGetAudioFilterSinkInterface(void *);

#endif
