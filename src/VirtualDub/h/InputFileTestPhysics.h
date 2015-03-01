//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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

#ifndef f_VD2_INPUTFILETESTPHYSICS_H
#define f_VD2_INPUTFILETESTPHYSICS_H

#include <vd2/system/vdstl.h>

#ifdef _MSC_VER
	#pragma once
#endif

struct VDTestVidPhysPartPos {
	sint16 mX;
	sint16 mY;
};

struct VDTestVidPhysFrame {
	float mTriRotation;
	int mFirstParticle;
	int mParticleCount;
};

class VDTestVidPhysVideo {
public:
	vdfastvector<VDTestVidPhysFrame> mFrames;
	vdfastvector<VDTestVidPhysPartPos> mParticles;
};

///////////////////////////////////////////////////////////////////////////////

class VDTestVidPhysSimulator {
public:
	VDTestVidPhysSimulator();
	~VDTestVidPhysSimulator();

	void Step(float dt);

	void EncodeFrame(VDTestVidPhysVideo& video);

protected:
	struct Particle {
		float x;
		float y;
		float vx;
		float vy;
	};

	enum {
		kMaxParticles = 100
	};

	int			mParticleCount;
	float		mTriAngle;
	float		mTriAngVel;
	float		mFirePhase;

	Particle	mParticles[kMaxParticles];
};

#endif
