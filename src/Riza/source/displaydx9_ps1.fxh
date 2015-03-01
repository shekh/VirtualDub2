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

#ifndef DISPLAYDX9_PS1_FXH
#define DISPLAYDX9_PS1_FXH

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 1.1 bicubic path - 4 texture stages, 2 passes (NVIDIA GeForce3/4)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

static const float offset = 1.0f / 128.0f;

struct VertexOutputBicubic1_1 {
	float4	pos		: POSITION;
	float2	uvfilt	: TEXCOORD0;
	float2	uvsrc0	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
	float2	uvsrc2	: TEXCOORD3;
};

VertexOutputBicubic1_1 VertexShaderBicubic1_1A(VertexInput IN) {
	VertexOutputBicubic1_1 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.x * vd_vpsize.x * vd_interphtexsize.w;
	OUT.uvfilt.y = 0;
	OUT.uvsrc0 = IN.uv + float2(-1.0f + offset, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2( 0.0f + offset, vd_fieldinfo.y)*vd_texsize.wz;
	OUT.uvsrc2 = IN.uv + float2(+1.0f + offset, vd_fieldinfo.y)*vd_texsize.wz;
	return OUT;
}

VertexOutputBicubic1_1 VertexShaderBicubic1_1B(VertexInput IN) {
	VertexOutputBicubic1_1 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = IN.uv2.y * vd_vpsize.y * vd_interpvtexsize.w;
	OUT.uvfilt.y = 0;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.0f + offset)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0,  0.0f + offset)*vd_tempsize.wz;
	OUT.uvsrc2 = uv + float2(0, +1.0f + offset)*vd_tempsize.wz;
	return OUT;
}

pixelshader PixelShaderBicubic1_1 = asm {
	ps_1_1
	tex t0				;displacement texture
	texbeml t1, t0		;p0/p1 weighted
	texbeml t2, t0		;p1/p2 weighted
	texbeml t3, t0		;p2/p3 weighted
	add_d2 r0, t1, t3
	add_x2 r0, t2, -r0
};

technique bicubic1_1 {
	pass horiz <
		string vd_target="temp";
		string vd_viewport="out, src";
		string vd_bumpenvscale="vd_texsize";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1A();
		PixelShader = <PixelShaderBicubic1_1>;
		
		Texture[0] = <vd_interphtexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MipFilter[0] = None;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MipFilter[1] = None;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		BumpEnvMat00[1] = 0.0f;
		BumpEnvMat01[1] = 0.0f;
		BumpEnvMat10[1] = 0.0f;
		BumpEnvMat11[1] = 0.0f;
		BumpEnvLScale[1] = <0.25f*0.75f>;
		BumpEnvLOffset[1] = 0.0f;
		
		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MipFilter[2] = None;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
		BumpEnvMat00[2] = 0.0f;
		BumpEnvMat01[2] = 0.0f;
		BumpEnvMat10[2] = 0.25f;
		BumpEnvMat11[2] = 0.0f;
		BumpEnvLScale[2] = <0.25f*0.75f>;
		BumpEnvLOffset[2] = 0.5f;

		Texture[3] = <vd_srctexture>;
		AddressU[3] = Clamp;
		AddressV[3] = Clamp;
		MipFilter[3] = None;
		MinFilter[3] = Linear;
		MagFilter[3] = Linear;
		BumpEnvMat00[3] = 0.0f;
		BumpEnvMat01[3] = 0.0f;
		BumpEnvMat10[3] = 0.0f;
		BumpEnvMat11[3] = 0.0f;
		BumpEnvLScale[3] = <0.25f*0.75f>;
		BumpEnvLOffset[3] = 0.0f;
	}
	
	pass vert <
		string vd_target="";
		string vd_viewport="out,out";
		string vd_bumpenvscale="vd_tempsize";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1B();
		PixelShader = <PixelShaderBicubic1_1>;
		Texture[0] = <vd_interpvtexture>;
		Texture[1] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		BumpEnvMat10[2] = 0.0f;
		BumpEnvMat11[2] = 0.25f;
		Texture[3] = <vd_temptexture>;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	UYVY/YUY2 to RGB -- pixel shader 1.1
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_UYVY_to_RGB_1_1(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2)
{
	oPos = pos;
	oT0 = uv;
	oT1 = oT0 + vd_texsize.wz * float2(0.25, 0);
	oT2.x = vd_srcsize.x * uv2.x / 16.0f;
	oT2.y = 0;
}

technique uyvy_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_UYVY_to_RGB_1_1();

		PixelShader = asm {
			ps_1_1
			def c0, 0.4065, 0, 0.1955, 0.582		// -Cr->G/2, 0, -Cb->G/2, Y_coeff/2
			def c1, 0.399, 0, 0.5045, -0.0627451	// Cr->R/4, 0, Cb->B/4, -Y_bias
			def c2, 0, 1, 0, 0

			tex t0							// Y
			tex t1							// C
			tex t2							// select
			
			dp3 r0, t0, c2					// select Y1 from green
			
			dp3 r1.rgb, t1_bias, c0			// compute chroma green / 2
			+ lrp r0.a, t2.a, t0.a, r0.a	// select Y1/Y2 based on even/odd
			
			mul r1.rgb, r1, c2				// restrict chroma green to green channel
			
			mad r1.rgb, t1_bx2, c1, -r1		// compute chroma red/blue / 2 and merge chroma green
			+ add r0.a, r0.a, c1.a			// add luma bias (-16/255)
			
			mad_x2 r0.rgb, r0.a, c0.a, r1	// scale luma and merge chroma
			+ mov r0.a, c2.a
		};
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

technique hdyc_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_UYVY_to_RGB_1_1();

		PixelShader = asm {
			ps_1_1
			def c0, 0.2665, 0, 0.1065, 0.582		// -Cr->G/2, 0, -Cb->G/2, Y_coeff/2
			def c1, 0.44825, 0, 0.528, -0.0627451	// Cr->R/4, 0, Cb->B/4, -Y_bias
			def c2, 0, 1, 0, 0

			tex t0							// Y
			tex t1							// C
			tex t2							// select
			
			dp3 r0, t0, c2					// select Y1 from green
			
			dp3 r1.rgb, t1_bias, c0			// compute chroma green / 2
			+ lrp r0.a, t2.a, t0.a, r0.a	// select Y1/Y2 based on even/odd
			
			mul r1.rgb, r1, c2				// restrict chroma green to green channel
			
			mad r1.rgb, t1_bx2, c1, -r1		// compute chroma red/blue / 2 and merge chroma green
			+ add r0.a, r0.a, c1.a			// add luma bias (-16/255)
			
			mad_x2 r0.rgb, r0.a, c0.a, r1	// scale luma and merge chroma
			+ mov r0.a, c2.a
		};
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

technique yuy2_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_UYVY_to_RGB_1_1();

		PixelShader = asm {
			ps_1_1
			def c0, 0, 0.1955, 0, 0.582		// 0, -Cb->G/2, 0, Y_coeff/2
			def c1, 0, 0.5045, 0, -0.0627451// 0, Cb->B/4, 0, -Y_bias
			def c2, 0, 1, 0, 0.4065			// [green], -Cr->G/2
			def c3, 1, 0, 0, 0.798			// [red], Cr->R/2

			tex t0							// Y
			tex t1							// C
			tex t2							// select
			
			dp3 r1.rgb, t1_bias, c0			// compute chroma green / 2 (Cb half)
			
			dp3 t2.rgb, t0, c3				// extract Y1 (red)
			+ mad r1.a, t1_bias, c2.a, r1.b	// compute chroma green / 2
						
			dp3 r0.rgb, t1_bx2, c1			// compute Cb (green) -> chroma blue
			+ mul r0.a, t1_bias, c3.a		// compute Cr (alpha) -> chroma red

			lrp r1.rgb, c2, -r1.a, r0		// merge chroma green and chroma blue
			+ lrp t0.a, t2.a, t2.b, t0.b	// select Y from Y1 (red) and Y2 (blue)
			
			lrp r1.rgb, c3, r0.a, r1		// merge chroma red
			+ add t0.a, t0.a, c1.a			// add luma bias (-16/255)
			
			mad_x2 r0.rgb, t0.a, c0.a, r1	// scale luma and merge chroma
			+ mov r0.a, c2.a
		};
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_hevenoddtexture>;
		AddressU[2] = Wrap;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	UYVY/YUY2 to RGB -- pixel shader 1.1
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_NV12_to_RGB_1_1(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1)
{
	oPos = pos;
	oT0 = uv;
	oT1 = oT0 + vd_texsize.wz * float2(0.25, 0);
}

technique nv12_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_NV12_to_RGB_1_1();

		PixelShader = asm {
			ps_1_1
			def c0, 0.582192, 0.582192, 0.582192, -0.0627451		// Y/2, Y_bias
			def c1, 0, -0.0979406, 0.504308, 0		// Cb/4
			def c2, 0.798013, -0.406484, 0, 0		// Cr/2

			tex t0							// Y
			tex t1							// C

			mul_x2 r0.rgb, t1_bias, c1
			+ add t0.a, t0.b, c0.a
			mad r0.rgb, t1_bias.a, c2, r0
			mad_x2 r0.rgb, t0.a, c0, r0
			+ mov r0.a, c1.a
		};
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	YCbCr to RGB -- pixel shader 1.1
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_YCbCr_to_RGB_1_1(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2)
{
	oPos = pos;
	oT0 = uv;
	oT1 = (uv2 * vd_chromauvscale * vd_srcsize.xy + vd_chromauvoffset) * vd_tex2size.wz;
	oT2 = (uv2 * vd_chromauvscale * vd_srcsize.xy + vd_chromauvoffset) * vd_tex2size.wz;
}

pixelshader PS_YCbCr_to_RGB_1_1 = asm {
	ps_1_1

	def c0, 0, -0.09775, 0.5045, -0.0365176
	def c1, 0.798, -0.4065, 0, 0.582

	tex t0							// Y
	tex t1							// Cb
	tex t2							// Cr
	
	mad r0, t1_bx2, c0, c0.a
	mad r0, t2_bias, c1, r0
	mad_x2 r0, t0, c1.a, r0
};

technique ycbcr_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_YCbCr_to_RGB_1_1();
		PixelShader = <PS_YCbCr_to_RGB_1_1>;
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_src2atexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MinFilter[1] = Linear;
		MagFilter[1] = Linear;
		
		Texture[2] = <vd_src2btexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Linear;
		MagFilter[2] = Linear;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pal8 to RGB -- pixel shader 1.1
//
//	Note: Intel 965 Express chipsets are reported to cut corners on precision here, which prevents
//	this shader from working properly (colors 0 and 1 are indistinguishable). As a workaround, we
//	use a PS2.0 shader if available.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void VS_Pal8_to_RGB_1_1(
	float4 pos : POSITION,
	float2 uv : TEXCOORD0,
	float2 uv2 : TEXCOORD1,
	out float4 oPos : POSITION,
	out float2 oT0 : TEXCOORD0,
	out float3 oT1 : TEXCOORD1,
	out float3 oT2 : TEXCOORD2)
{
	oPos = pos;
	oT0 = uv;
	oT1 = float3(0, 255.75f / 256.0f, 0);
	oT2 = float3(0, 0, 0);
}
	

technique pal8_to_rgb_1_1 {
	pass < string vd_viewport = "unclipped,unclipped"; > {
		VertexShader = compile vs_1_1 VS_Pal8_to_RGB_1_1();
		PixelShader = asm {
			ps_1_1
			tex t0
			texm3x2pad t1, t0
			texm3x2tex t2, t0
			mov r0, t2
		};
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MinFilter[0] = Point;
		MagFilter[0] = Point;
		
		Texture[2] = <vd_srcpaltexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
	}
}

#endif
