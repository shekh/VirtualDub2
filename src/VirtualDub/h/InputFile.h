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

class VDStringA;

class InputFileOptions {
public:
	virtual ~InputFileOptions();
	virtual int write(char *buf, int buflen) const = 0;
};

class InputFilenameNode : public ListNode2<InputFilenameNode> {
public:
	const wchar_t *name;

	InputFilenameNode(const wchar_t *_n);
	~InputFilenameNode();
};

class InputFile : public vdrefcounted<IVDRefCount> {
protected:
	virtual ~InputFile();

public:
	List2<InputFilenameNode> listFiles;

	virtual void Init(const wchar_t *szFile) = 0;
	virtual bool Append(const wchar_t *szFile);

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

protected:
	void AddFilename(const wchar_t *lpszFile);
};

class VDINTERFACE IVDInputDriver : public IVDRefCount {
public:
	enum Flags {
		kF_None				= 0,
		kF_Video			= 1,
		kF_Audio			= 2,
		kF_PromptForOpts	= 4,
		kF_SupportsOpts		= 8,
		KF_Max				= 0xFFFFFFFFUL
	};

	enum OpenFlags {
		kOF_None			= 0,
		kOF_Quiet			= 1,
		kOF_AutoSegmentScan	= 2,
		kOF_Max				= 0xFFFFFFFFUL
	};

	enum DetectionConfidence {
		kDC_None,
		kDC_VeryLow,
		kDC_Low,
		kDC_Moderate,
		kDC_High
	};

	virtual int				GetDefaultPriority() = 0;
	virtual const wchar_t *	GetSignatureName() = 0;
	virtual uint32			GetFlags() = 0;
	virtual const wchar_t *	GetFilenamePattern() = 0;
	virtual bool			DetectByFilename(const wchar_t *pszFilename) = 0;
	virtual DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) = 0;
	virtual InputFile *		CreateInputFile(uint32 flags) = 0;
};

typedef std::vector<vdrefptr<IVDInputDriver> > tVDInputDrivers;

void VDInitInputDrivers();
void VDGetInputDrivers(tVDInputDrivers& l, uint32 flags);
IVDInputDriver *VDGetInputDriverByName(const wchar_t *name);
IVDInputDriver *VDGetInputDriverForLegacyIndex(int idx);
void VDGetInputDriverFilePatterns(uint32 flags, vdvector<VDStringW>& patterns);
VDStringW VDMakeInputDriverFileFilter(const tVDInputDrivers& l, std::vector<int>& xlat);

IVDInputDriver *VDAutoselectInputDriverForFile(const wchar_t *fn, uint32 flags);
void VDOpenMediaFile(const wchar_t *filename, uint32 flags, InputFile **pFile);

#endif
