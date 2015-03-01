//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2008 Avery Lee
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

#ifndef DISPLAYDX9_PS14_FXH
#define DISPLAYDX9_PS14_FXH

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 1.4 bicubic path - 5 texture stages, 2 passes (NVIDIA GeForceFX+, ATI RADEON 8500+)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubic1_4 {
	float4	pos		: POSITION;
	float2	uvfilt	: TEXCOORD0;
	float2	uvsrc0	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
	float2	uvsrc2	: TEXCOORD3;
	float2	uvsrc3	: TEXCOORD4;
};

VertexOutputBicubic1_4 VertexShaderBicubic1_4A(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	float step = IN.uv.x * vd_srcsize.w;
	IN.uv2.x += step;
	
	OUT.pos = IN.pos;
	OUT.pos.x += 2.0f * step;
	OUT.uvfilt.x = IN.uv2.x * vd_vpsize.x * vd_interphtexsize.w;
	OUT.uvfilt.y = 0;

	float2 uv = IN.uv2 * float2(vd_srcsize.x, vd_srcsize.y) * vd_texsize.wz;
	OUT.uvsrc0 = uv + float2(-1.5f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc1 = uv + float2( 0.0f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc2 = uv + float2( 0.0f, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc3 = uv + float2(+1.5f, vd_fieldinfo.y)*vd_texsize.wz;

	float ulo = 0.5f * vd_texsize.w;
	float uhi = (vd_srcsize.x - 0.5f) * vd_texsize.w;
	
	OUT.uvsrc0.x = clamp(OUT.uvsrc0.x, ulo, uhi);
	OUT.uvsrc1.x = clamp(OUT.uvsrc1.x, ulo, uhi);
	OUT.uvsrc2.x = clamp(OUT.uvsrc2.x, ulo, uhi);
	OUT.uvsrc3.x = clamp(OUT.uvsrc3.x, ulo, uhi);

	return OUT;
}

VertexOutputBicubic1_4 VertexShaderBicubic1_4B(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	float step = IN.uv.y * vd_srcsize.z;
	IN.uv2.y += step;
	
	OUT.pos = IN.pos;
	OUT.pos.y -= 2.0f * step;
	OUT.uvfilt.x = IN.uv2.y * vd_vpsize.y * vd_interpvtexsize.w;
	OUT.uvfilt.y = 0;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0,  0.0f)*vd_tempsize.wz;
	OUT.uvsrc2 = uv + float2(0,  0.0f)*vd_tempsize.wz;
	OUT.uvsrc3 = uv + float2(0, +1.5f)*vd_tempsize.wz;

	float vlo = 0.5f * vd_tempsize.z;
	float vhi = (vd_srcsize.y - 0.5f) * vd_tempsize.z;
	
	OUT.uvsrc0.y = clamp(OUT.uvsrc0.y, vlo, vhi);
	OUT.uvsrc1.y = clamp(OUT.uvsrc1.y, vlo, vhi);
	OUT.uvsrc2.y = clamp(OUT.uvsrc2.y, vlo, vhi);
	OUT.uvsrc3.y = clamp(OUT.uvsrc3.y, vlo, vhi);
	
	return OUT;
}

pixelshader bicubic1_4_ps = asm {
	ps_1_4
	texld r0, t0
	texld r1, t1
	texld r2, t2
	texld r3, t3
	texld r4, t4

	lrp r1, r0.b, r1, r4
	lrp r2, r0.g, r2, r3
	add r1, r2, -r1
	mad r0, r1, r0.r, r2
};

technique bicubic1_4 {
	pass horiz <
		string vd_target="temp";
		string vd_viewport="out, src";
		int vd_tilemode = 1;
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4A();
		PixelShader = <bicubic1_4_ps>;
		
		Texture[0] = <vd_interphtexture>;
		AddressU[0] = Wrap;
		AddressV[0] = Clamp;
		MipFilter[0] = None;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MipFilter[1] = None;
		MinFilter[1] = Point;
		MagFilter[1] = Point;
		
		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MipFilter[2] = None;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
		
		Texture[3] = <vd_srctexture>;
		AddressU[3] = Clamp;
		AddressV[3] = Clamp;		
		MipFilter[3] = None;
		MinFilter[3] = Point;
		MagFilter[3] = Point;
		
		Texture[4] = <vd_srctexture>;
		AddressU[4] = Clamp;
		AddressV[4] = Clamp;		
		MipFilter[4] = None;
		MinFilter[4] = Point;
		MagFilter[4] = Point;
	}
	
	pass vert <
		string vd_target="";
		string vd_viewport="out,out";
		int vd_tilemode = 2;
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4B();
		PixelShader = <bicubic1_4_ps>;
		Texture[0] = <vd_interpvtexture>;
		Texture[1] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		Texture[3] = <vd_temptexture>;
		Texture[4] = <vd_temptexture>;
	}
}

#endif
