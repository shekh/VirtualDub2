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

#ifndef f_INPUTFILE_H
#define f_INPUTFILE_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vector>
#include <list>
#include <utility>
#include <vd2/system/list.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>

class AVIStripeSystem;
class IAVIReadHandler;
class IAVIReadStream;
class AudioSource;
class IVDVideoSource;
class IFilterModFileTool;
struct VDXMediaInfo;

class VDStringA;

class MyFileError : public MyError {
public:
	enum {
		file_type_unknown=1,
	};
	int error;

	MyFileError(int error, const char *f, ...);
};

class InputFileOptions {
public:
	virtual ~InputFileOptions();
	virtual int write(char *buf, int buflen) const = 0;
};

class InputFilenameNode : public ListNode2<InputFilenameNode> {
public:
	const wchar_t *name;
	int flags;

	InputFilenameNode(const wchar_t *_n, int flags);
	~InputFilenameNode();
};

class InputFile : public vdrefcounted<IVDRefCount> {
protected:
	virtual ~InputFile();

public:
	List2<InputFilenameNode> listFiles;

	virtual void Init(const wchar_t *szFile) = 0;
	virtual bool Append(const wchar_t *szFile, uint32 flags);
	virtual void getAppendFilters(wchar_t *filters, int filters_max);

	virtual void setOptions(InputFileOptions *);
	virtual InputFileOptions *promptForOptions(VDGUIHandle hwndParent);
	virtual InputFileOptions *createOptions(const void *buf, uint32 len);
	virtual void InfoDialog(VDGUIHandle hwndParent);

	typedef std::list<std::pair<uint32, VDStringA> > tFileTextInfo;
	virtual void GetTextInfo(tFileTextInfo& info);

	virtual bool isOptimizedForRealtime();
	virtual bool isStreaming();

	virtual bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	virtual bool GetAudioSource(int index, AudioSource **ppSrc);

	virtual void GetFileTool(IFilterModFileTool **pp){ *pp=0; } 
	virtual int GetInputDriverApiVersion(){ return -1; }
	virtual int GetFileFlags(){ return -1; }

protected:
	void AddFilename(const wchar_t *lpszFile, int flags=0);
};

class VDINTERFACE IVDInputDriver : public IVDRefCount {
public:
	enum Flags {
		kF_None				= 0,
		kF_Video			= 1,
		kF_Audio			= 2,
		kF_PromptForOpts	= 4,
		kF_SupportsOpts		= 8,
		kF_ForceByName		= 16,
		kF_Duplicate		= 32,
		KF_Max				= 0xFFFFFFFFUL
	};

	enum OpenFlags {
		kOF_None			= 0,
		kOF_Quiet			= 1,
		kOF_AutoSegmentScan	= 2,
		kOF_SingleFile	= 4,
		kOF_Sequence	= 8,
		kOF_Max				= 0xFFFFFFFFUL
	};

	enum FileFlags {
		kFF_Sequence = 1,
		kFF_AppendSequence = 2,
	};

	enum DetectionConfidence {
		kDC_None,
		kDC_VeryLow,
		kDC_Low,
		kDC_Moderate,
		kDC_High,

		kDC_Error_NotImpl = -1,
		kDC_Error_MoreData = -2,
	};

	virtual int				GetDefaultPriority() = 0;
	virtual const wchar_t *	GetSignatureName() = 0;
	virtual uint32			GetFlags() = 0;
	virtual const wchar_t *	GetFilenamePattern() = 0;
	virtual bool			DetectByFilename(const wchar_t *pszFilename) = 0;
	virtual DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) = 0;
	virtual InputFile *		CreateInputFile(uint32 flags) = 0;
	virtual DetectionConfidence DetectBySignature3(VDXMediaInfo& info, const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize, const wchar_t* fileName) {
		return DetectBySignature(pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
	}
};

typedef std::vector<vdrefptr<IVDInputDriver> > tVDInputDrivers;

void VDInitInputDrivers();
void VDShutdownInputDrivers();
void VDGetInputDrivers(tVDInputDrivers& l, uint32 flags);
IVDInputDriver *VDGetInputDriverByName(const wchar_t *name);
IVDInputDriver *VDGetInputDriverForLegacyIndex(int idx);
void VDGetInputDriverFilePatterns(uint32 flags, vdvector<VDStringW>& patterns);
VDStringW VDMakeInputDriverFileFilter(const tVDInputDrivers& l, std::vector<int>& xlat);
void VDGetInputDriverFileFilters(const tVDInputDrivers& l, vdvector<VDStringW>& list);

IVDInputDriver::DetectionConfidence VDTestInputDriverForFile(VDXMediaInfo& info, const wchar_t *fn, IVDInputDriver *pDriver);
int VDAutoselectInputDriverForFile(const wchar_t *fn, uint32 flags, tVDInputDrivers& list);
void VDGetInputDriverForFile(uint32 flags, tVDInputDrivers& list);
IVDInputDriver *VDAutoselectInputDriverForFile(const wchar_t *fn, uint32 flags);
void VDOpenMediaFile(const wchar_t *filename, uint32 flags, InputFile **pFile);

#endif
