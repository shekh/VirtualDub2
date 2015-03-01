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

#include "stdafx.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>
#include <vd2/system/memory.h>
#include "InputFileTestPhysics.h"

VDTestVidPhysSimulator::VDTestVidPhysSimulator()
	: mParticleCount(0)
	, mTriAngle(0.0f)
	, mTriAngVel(0.0f)
	, mFirePhase(0.0f)
{
}

VDTestVidPhysSimulator::~VDTestVidPhysSimulator() {
}

void VDTestVidPhysSimulator::Step(float dt) {
	int n = mParticleCount;

	mFirePhase += dt;
	float firedx = sinf(mFirePhase) * 50.0f;

	float tx[3];
	float ty[3];
	float td[3];
	float tnx[3];
	float tny[3];
	for(int i=0; i<3; ++i) {
		float t = (float)i * 6.28318f / 3.0f + mTriAngle;
		tx[i] = 320.0f + 105.0f*cosf(t);
		ty[i] = 240.0f - 105.0f*sinf(t);
	}

	tnx[0] = ty[0] - ty[1];
	tnx[1] = ty[1] - ty[2];
	tnx[2] = ty[2] - ty[0];

	tny[0] = tx[1] - tx[0];
	tny[1] = tx[2] - tx[1];
	tny[2] = tx[0] - tx[2];

	for(int i=0; i<3; ++i) {
		float s = 1.0f / sqrtf(tnx[i]*tnx[i] + tny[i]*tny[i]);
		tnx[i] *= s;
		tny[i] *= s;
	}

	td[0] = tx[0]*tnx[0] + ty[0]*tny[0];
	td[1] = tx[1]*tnx[1] + ty[1]*tny[1];
	td[2] = tx[2]*tnx[2] + ty[2]*tny[2];

	for(int i=0; i<n;) {
		Particle& par = mParticles[i];
		float ox = par.x;
		float oy = par.y;
		float nx = ox + par.vx*dt;
		float ny = oy + par.vy*dt;

		// check for triangle collision
		float od[3]={
			nx*tnx[0] + ny*tny[0] - td[0],
			nx*tnx[1] + ny*tny[1] - td[1],
			nx*tnx[2] + ny*tny[2] - td[2]
		};

		if (od[0] < 0 && od[1] < 0 && od[2] < 0) {
			int plane;

			if (od[0] > od[1]) {	// 0 > 1
				if (od[2] > od[0])	// 2 > 0 > 1
					plane = 2;
				else				// 0 > 1,2
					plane = 0;
			} else {				// 1 > 0
				if (od[2] > od[1])	// 2 > 1 > 0
					plane = 2;
				else				// 1 > 0,2
					plane = 1;
			}

			nx -= tnx[plane]*od[plane];
			ny -= tny[plane]*od[plane];

			float vdot = par.vx*tnx[plane] + par.vy*tny[plane];

			if (vdot < 0) {
				float fx = vdot*tnx[plane];
				float fy = vdot*tny[plane];
				par.vx -= fx;
				par.vy -= fy;

				float armx = nx - 320.0f;
				float army = ny - 240.0f;
				mTriAngVel += (fx*-army + fy*armx) * -0.00001f;
			}
		}

		par.x = nx;
		par.y = ny;

		if (par.y < -10.0f) {
			par = mParticles[n-1];
			--n;
			continue;
		}

		par.vy += -130.0f*dt;

		++i;
	}

	if (n < kMaxParticles) {
		Particle& par = mParticles[n++];

		float fx = (float)rand() / (float)RAND_MAX;
		par.y = 490.0f;
		par.x = fx * 640.0f;
		par.vy = 0.0f;
		par.vx = ((float)rand() / (float)RAND_MAX) * 0.1f - (fx - 0.5f) * 400.0f + firedx;
	}

	mParticleCount = n;
	mTriAngle += mTriAngVel * dt;
}

void VDTestVidPhysSimulator::EncodeFrame(VDTestVidPhysVideo& video) {
	VDTestVidPhysFrame& frame = video.mFrames.push_back();

	frame.mTriRotation = mTriAngle;
	frame.mParticleCount = mParticleCount;
	frame.mFirstParticle = (int)video.mParticles.size();

	video.mParticles.resize(frame.mFirstParticle + mParticleCount);
	VDTestVidPhysPartPos *partpos = video.mParticles.data() + frame.mFirstParticle;
	for(int i=0; i<mParticleCount; ++i) {
		const Particle& par = mParticles[i];

		partpos[i].mX = (sint16)VDRoundToInt(par.x * 16.0f);
		partpos[i].mY = (sint16)VDRoundToInt(par.y * 16.0f);
	}
}
