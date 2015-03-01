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

#ifndef f_AF_BASE_H
#define f_AF_BASE_H

#include <vector>
#include <vd2/system/VDString.h>

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>

namespace nsVDAudioFilterBase {
	typedef uint32				Type_U32;
	typedef uint64				Type_U64;
	typedef sint32				Type_S32;
	typedef sint64				Type_S64;
	typedef double				Type_Double;
	typedef VDStringA			Type_AStr;
	typedef	VDStringW			Type_WStr;
	typedef	std::vector<char>	Type_Block;

	struct ConfigEntryExt {
		VDXPluginConfigEntry info;
		ptrdiff_t objoffset;
	};
}

#define VDAFBASE_BEGIN_CONFIG(filtername_)					\
	namespace nsVDAFConfigInfo_##filtername_ {				\
		template<int n> struct ConfigInfo { static const nsVDAudioFilterBase::ConfigEntryExt members; };	\
		template<int n> const nsVDAudioFilterBase::ConfigEntryExt ConfigInfo<n>::members = {0}

#define VDAFBASE_STRUCT_ENTRY(filtername_, idx_, type_, name_, sdesc_, ldesc_)						\

#define VDAFBASE_CONFIG_ENTRY(filtername_, idx_, type_, name_, sdesc_, ldesc_)						\
		template<> struct ConfigInfo<idx_>;										\
		template<> struct ConfigInfo<idx_> : public ConfigInfo<idx_-1> {		\
			nsVDAudioFilterBase::Type_##type_ name_;							\
			static const nsVDAudioFilterBase::ConfigEntryExt members;			\
		};																		\
																				\
		const nsVDAudioFilterBase::ConfigEntryExt ConfigInfo<idx_>::members={ &ConfigInfo<idx_-1>::members.info, idx_, VDXPluginConfigEntry::kType##type_, L#name_, sdesc_, ldesc_, offsetof(ConfigInfo<idx_>, name_) }
		
#define VDAFBASE_END_CONFIG(filtername_, idx_)		\
		}											\
		typedef nsVDAFConfigInfo_##filtername_::ConfigInfo<idx_> VDAudioFilterData_##filtername_


class VDAudioFilterBase {
public:
	VDAudioFilterBase() {}
	virtual ~VDAudioFilterBase() {}

	virtual uint32 Run();
	virtual sint64 Seek(sint64 microsecs);
	virtual uint32 Prepare();
	virtual void Start();
	virtual void Stop();
	virtual unsigned Suspend(void *dst, unsigned size);
	virtual void Resume(const void *src, unsigned size);
	virtual unsigned GetParam(unsigned idx, void *dst, unsigned size);
	virtual void SetParam(unsigned idx, const void *src, unsigned size);
	virtual bool Config(HWND hwnd);
	virtual uint32 Read(unsigned pin, void *dst, uint32 samples);

	//////////////////////

	virtual void *GetConfigPtr() { return 0; }

	//////////////////////

	static void		__cdecl DestroyProc				(const VDAudioFilterContext *pContext);
	static uint32	__cdecl RunProc					(const VDAudioFilterContext *pContext);
	static sint64	__cdecl SeekProc				(const VDAudioFilterContext *pContext, sint64 microsecs);
	static uint32	__cdecl PrepareProc				(const VDAudioFilterContext *pContext);
	static void		__cdecl StartProc				(const VDAudioFilterContext *pContext);
	static void		__cdecl StopProc				(const VDAudioFilterContext *pContext);
	static unsigned	__cdecl SuspendProc				(const VDAudioFilterContext *pContext, void *dst, unsigned size);
	static void		__cdecl ResumeProc				(const VDAudioFilterContext *pContext, const void *src, unsigned size);
	static unsigned	__cdecl GetParamProc			(const VDAudioFilterContext *pContext, unsigned idx, void *dst, unsigned size);
	static void		__cdecl SetParamProc			(const VDAudioFilterContext *pContext, unsigned idx, const void *src, unsigned size);
	static bool		__cdecl ConfigProc				(const VDAudioFilterContext *pContext, HWND hwnd);
	static uint32	__cdecl ReadProc				(const VDAudioFilterContext *pContext, unsigned pin, void *dst, uint32 samples);

	static const VDAudioFilterVtbl sVtbl;

protected:
	const nsVDAudioFilterBase::ConfigEntryExt *GetParamEntry(const unsigned idx);

	const VDAudioFilterContext		*mpContext;
};

#endif
