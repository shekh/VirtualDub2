//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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

#ifndef BACKFACE_H
#define BACKFACE_H

#include <vd2/system/VDString.h>

#ifdef _DEBUG
	#define VD_BACKFACE_ENABLED 1
#endif

#if VD_BACKFACE_ENABLED
	class VDBackfaceService;
	class VDBackfaceClass;
	class VDBackfaceObjectBase;

	///////////////////////////////////////////////////////////////////////////
	class IVDBackfaceOutput {
	public:
		virtual void operator<<(const char *s) = 0;
		virtual void operator()(const char *format, ...) = 0;
		virtual VDStringA GetTag(VDBackfaceObjectBase *p) = 0;
		virtual VDStringA GetBlurb(VDBackfaceObjectBase *p) = 0;
	};

	///////////////////////////////////////////////////////////////////////////
	class VDBackfaceObjectNode {
	public:
		VDBackfaceObjectNode *mpObjPrev, *mpObjNext;
	};

	class VDBackfaceObjectBase : private VDBackfaceObjectNode {
		friend VDBackfaceService;
	public:
		VDBackfaceObjectBase() {}
		VDBackfaceObjectBase(const VDBackfaceObjectBase&);
		~VDBackfaceObjectBase();

	protected:
		VDBackfaceClass *BackfaceInitClass(const char *shortname, const char *longname);
		void BackfaceInitObject(VDBackfaceClass *);

	private:
		virtual void BackfaceDumpObject(IVDBackfaceOutput&);
		virtual void BackfaceDumpBlurb(IVDBackfaceOutput&);

		VDBackfaceClass *mpClass;
		uint32	mInstance;
	};

	template<class T>
	class VDBackfaceObject : public VDBackfaceObjectBase {
	public:
		VDBackfaceObject() {
			static VDBackfaceClass *spClass = BackfaceInitClass(T::BackfaceGetShortName(), T::BackfaceGetLongName());

			BackfaceInitObject(spClass);
		}
	};

	///////////////////////////////////////////////////////////////////////////

	void VDBackfaceOpenConsole();
#else
	class IVDBackfaceStream {};
	struct VDBackfaceObjectBase {};
	template<class T> class VDBackfaceObject : public VDBackfaceObjectBase{};

	void VDBackfaceOpenConsole();
#endif

#endif
