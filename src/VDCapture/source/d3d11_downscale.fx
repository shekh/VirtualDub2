extern Texture2D srctex : register(t0);
extern SamplerState srcsamp : register(s0);
extern Texture2D coefftex : register(t1);
extern SamplerState coeffsamp : register(s1);

extern float2 coeffaxis : register(c0);
extern float4 uvstep : register(c1);

static const float kDeltaScale = 4.0f;
static const float kDeltaOffset = -512.0f / 255.0f;
static const float kCoeffScale = 255.0f / 128.0f;
static const float kCoeffOffset = -64.0f / 255.0f * kCoeffScale;

///////////////////////////////////////////////////////////////////////////

void VS_Downsample(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float4 oT0 : TEXCOORD0,
	out float4 oT1 : TEXCOORD1,
	out float oT2 : TEXCOORD2
)
{
	oPos = float4(pos.xy, 0.5, 1);
	oT0 = float4(uv, (uv + uvstep.xy).yx);
	oT1 = float4(uv + uvstep.xy*2, (uv + uvstep.xy*3).yx);
	oT2 = dot(uv, coeffaxis);
}

///////////////////////////////////////////////////////////////////////////

float4 PS_Downsample4(float4 pos : SV_Position, float4 uv0 : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float2 uv2 = uv0 + (co0.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0 = srctex.Sample(srcsamp, uv2 - uvstep.xy);
	float4 px1 = srctex.Sample(srcsamp, uv2);
	float4 px2 = srctex.Sample(srcsamp, uv2 + uvstep.xy);
	float4 px3 = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	
	coeff0.w = 1 - dot(coeff0.xyz, float3(1,1,1));
	
	return px0 * coeff0.x + px1 * coeff0.y + px2 * coeff0.z + px3 * coeff0.w;
}

float4 PS_Downsample6(float4 pos : SV_Position, float4 uv0 : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float2 uv2 = uv0 + (co1.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0 = srctex.Sample(srcsamp, uv2 - 2*uvstep.xy);
	float4 px1 = srctex.Sample(srcsamp, uv2 - uvstep.xy);
	float4 px2 = srctex.Sample(srcsamp, uv2);
	float4 px3 = srctex.Sample(srcsamp, uv2 + uvstep.xy);
	float4 px4 = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	float4 px5 = srctex.Sample(srcsamp, uv2 + 3*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
		
	return px0 * coeff0.x
		 + px1 * coeff0.y
		 + px2 * coeff0.z
		 + px3 * coeff0.w
		 + px4 * coeff1.x
		 + px5 * coeff1.y
		 ;
}

float4 PS_Downsample8(float4 pos : SV_Position, float4 uv : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float2 uv2 = uv + (co1.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0 = srctex.Sample(srcsamp, uv2 - 3*uvstep.xy);
	float4 px1 = srctex.Sample(srcsamp, uv2 - 2*uvstep.xy);
	float4 px2 = srctex.Sample(srcsamp, uv2 - 1*uvstep.xy);
	float4 px3 = srctex.Sample(srcsamp, uv2);
	float4 px4 = srctex.Sample(srcsamp, uv2 + 1*uvstep.xy);
	float4 px5 = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	float4 px6 = srctex.Sample(srcsamp, uv2 + 3*uvstep.xy);
	float4 px7 = srctex.Sample(srcsamp, uv2 + 4*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
	
	coeff1.w = 0;	
	coeff1.w = 1 - dot(coeff0 + coeff1, float4(1,1,1,1));
	
	return px0 * coeff0.x
		 + px1 * coeff0.y
		 + px2 * coeff0.z
		 + px3 * coeff0.w
		 + px4 * coeff1.x
		 + px5 * coeff1.y
		 + px6 * coeff1.z
		 + px7 * coeff1.w
		 ;
}

float4 PS_Downsample10(float4 pos : SV_Position, float4 uv : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float4 co2 = coefftex.Sample(coeffsamp, float2(coeffu, 0.625f));
	float2 uv2 = uv.xy + (co2.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0 = srctex.Sample(srcsamp, uv2 - 4*uvstep.xy);
	float4 px1 = srctex.Sample(srcsamp, uv2 - 3*uvstep.xy);
	float4 px2 = srctex.Sample(srcsamp, uv2 - 2*uvstep.xy);
	float4 px3 = srctex.Sample(srcsamp, uv2 - 1*uvstep.xy);
	float4 px4 = srctex.Sample(srcsamp, uv2);
	float4 px5 = srctex.Sample(srcsamp, uv2 + 1*uvstep.xy);
	float4 px6 = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	float4 px7 = srctex.Sample(srcsamp, uv2 + 3*uvstep.xy);
	float4 px8 = srctex.Sample(srcsamp, uv2 + 4*uvstep.xy);
	float4 px9 = srctex.Sample(srcsamp, uv2 + 5*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
	float4 coeff2 = co2 * kCoeffScale + kCoeffOffset;
		
	return px0 * coeff0.x
		 + px1 * coeff0.y
		 + px2 * coeff0.z
		 + px3 * coeff0.w
		 + px4 * coeff1.x
		 + px5 * coeff1.y
		 + px6 * coeff1.z
		 + px7 * coeff1.w
		 + px8 * coeff2.x
		 + px9 * coeff2.y
		 ;
}

float4 PS_Downsample12(float4 pos : SV_Position, float4 uv : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float4 co2 = coefftex.Sample(coeffsamp, float2(coeffu, 0.625f));
	float2 uv2 = uv + (co2.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0  = srctex.Sample(srcsamp, uv2 - 5*uvstep.xy);
	float4 px1  = srctex.Sample(srcsamp, uv2 - 4*uvstep.xy);
	float4 px2  = srctex.Sample(srcsamp, uv2 - 3*uvstep.xy);
	float4 px3  = srctex.Sample(srcsamp, uv2 - 2*uvstep.xy);
	float4 px4  = srctex.Sample(srcsamp, uv2 - 1*uvstep.xy);
	float4 px5  = srctex.Sample(srcsamp, uv2);
	float4 px6  = srctex.Sample(srcsamp, uv2 + 1*uvstep.xy);
	float4 px7  = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	float4 px8  = srctex.Sample(srcsamp, uv2 + 3*uvstep.xy);
	float4 px9  = srctex.Sample(srcsamp, uv2 + 4*uvstep.xy);
	float4 px10 = srctex.Sample(srcsamp, uv2 + 5*uvstep.xy);
	float4 px11 = srctex.Sample(srcsamp, uv2 + 6*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
	float4 coeff2 = co2 * kCoeffScale + kCoeffOffset;
	
	coeff2.w = 0;	
	coeff2.w = 1 - dot(coeff0 + coeff1 + coeff2, float4(1,1,1,1));
	
	return px0  * coeff0.x
		 + px1  * coeff0.y
		 + px2  * coeff0.z
		 + px3  * coeff0.w
		 + px4  * coeff1.x
		 + px5  * coeff1.y
		 + px6  * coeff1.z
		 + px7  * coeff1.w
		 + px8  * coeff2.x
		 + px9  * coeff2.y
		 + px10 * coeff2.z
		 + px11 * coeff2.w
		 ;
}

float4 PS_Downsample14(float4 pos : SV_Position, float4 uv : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float4 co2 = coefftex.Sample(coeffsamp, float2(coeffu, 0.625f));
	float4 co3 = coefftex.Sample(coeffsamp, float2(coeffu, 0.875f));
	float2 uv2 = uv.xy + (co3.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	
	float4 px0  = srctex.Sample(srcsamp, uv2 - 6*uvstep.xy);
	float4 px1  = srctex.Sample(srcsamp, uv2 - 5*uvstep.xy);
	float4 px2  = srctex.Sample(srcsamp, uv2 - 4*uvstep.xy);
	float4 px3  = srctex.Sample(srcsamp, uv2 - 3*uvstep.xy);
	float4 px4  = srctex.Sample(srcsamp, uv2 - 2*uvstep.xy);
	float4 px5  = srctex.Sample(srcsamp, uv2 - 1*uvstep.xy);
	float4 px6  = srctex.Sample(srcsamp, uv2);
	float4 px7  = srctex.Sample(srcsamp, uv2 + 1*uvstep.xy);
	float4 px8  = srctex.Sample(srcsamp, uv2 + 2*uvstep.xy);
	float4 px9  = srctex.Sample(srcsamp, uv2 + 3*uvstep.xy);
	float4 px10 = srctex.Sample(srcsamp, uv2 + 4*uvstep.xy);
	float4 px11 = srctex.Sample(srcsamp, uv2 + 5*uvstep.xy);
	float4 px12 = srctex.Sample(srcsamp, uv2 + 6*uvstep.xy);
	float4 px13 = srctex.Sample(srcsamp, uv2 + 7*uvstep.xy);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
	float4 coeff2 = co2 * kCoeffScale + kCoeffOffset;
	float4 coeff3 = co3 * kCoeffScale + kCoeffOffset;
		
	return px0  * coeff0.x
		 + px1  * coeff0.y
		 + px2  * coeff0.z
		 + px3  * coeff0.w
		 + px4  * coeff1.x
		 + px5  * coeff1.y
		 + px6  * coeff1.z
		 + px7  * coeff1.w
		 + px8  * coeff2.x
		 + px9  * coeff2.y
		 + px10 * coeff2.z
		 + px11 * coeff2.w
		 + px12 * coeff3.x
		 + px13 * coeff3.y
		 ;
}

float4 PS_Downsample16(float4 pos : SV_Position, float4 uv0 : TEXCOORD0, float4 uv1 : TEXCOORD1, float coeffu : TEXCOORD2) : SV_Target0 {
	float4 co0 = coefftex.Sample(coeffsamp, float2(coeffu, 0.125f));
	float4 co1 = coefftex.Sample(coeffsamp, float2(coeffu, 0.375f));
	float4 co2 = coefftex.Sample(coeffsamp, float2(coeffu, 0.625f));
	float4 co3 = coefftex.Sample(coeffsamp, float2(coeffu, 0.875f));
	float2 duv = (co3.w * kDeltaScale + kDeltaOffset)*uvstep.xy;
	float4 duvuv = float4(duv, duv.yx);
	float4 uvAB = uv0 + duvuv;
	float4 uvCD = uv1 + duvuv;
	
	float4 uvABm2 = uvAB - uvstep.xyyx*8;
	float4 uvCDm2 = uvCD - uvstep.xyyx*8;
	float4 uvABm1 = uvAB - uvstep.xyyx*4;
	float4 uvCDm1 = uvCD - uvstep.xyyx*4;
	float4 uvABp1 = uvAB + uvstep.xyyx*4;
	float4 uvCDp1 = uvCD + uvstep.xyyx*4;
	
	float4 px0 = srctex.Sample(srcsamp, uvABm2.xy);
	float4 px1 = srctex.Sample(srcsamp, uvABm2.wz);
	float4 px2 = srctex.Sample(srcsamp, uvCDm2.xy);
	float4 px3 = srctex.Sample(srcsamp, uvCDm2.wz);
	float4 px4 = srctex.Sample(srcsamp, uvABm1.xy);
	float4 px5 = srctex.Sample(srcsamp, uvABm1.wz);
	float4 px6 = srctex.Sample(srcsamp, uvCDm1.xy);
	float4 px7 = srctex.Sample(srcsamp, uvCDm1.wz);
	float4 px8 = srctex.Sample(srcsamp, uvAB.xy);
	float4 px9 = srctex.Sample(srcsamp, uvAB.wz);
	float4 px10 = srctex.Sample(srcsamp, uvCD.xy);
	float4 px11 = srctex.Sample(srcsamp, uvCD.wz);
	float4 px12 = srctex.Sample(srcsamp, uvABp1.xy);
	float4 px13 = srctex.Sample(srcsamp, uvABp1.wz);
	float4 px14 = srctex.Sample(srcsamp, uvCDp1.xy);
	float4 px15 = srctex.Sample(srcsamp, uvCDp1.wz);
	
	float4 coeff0 = co0 * kCoeffScale + kCoeffOffset;
	float4 coeff1 = co1 * kCoeffScale + kCoeffOffset;
	float4 coeff2 = co2 * kCoeffScale + kCoeffOffset;
	float4 coeff3 = co3 * kCoeffScale + kCoeffOffset;
	
	coeff3.w = 0;	
	coeff3.w = 1 - dot(coeff0 + coeff1 + coeff2 + coeff3, float4(1,1,1,1));
	
	return px0 * coeff0.x
		 + px1 * coeff0.y
		 + px2 * coeff0.z
		 + px3 * coeff0.w
		 + px4 * coeff1.x
		 + px5 * coeff1.y
		 + px6 * coeff1.z
		 + px7 * coeff1.w
		 + px8 * coeff2.x
		 + px9 * coeff2.y
		 + px10 * coeff2.z
		 + px11 * coeff2.w
		 + px12 * coeff3.x
		 + px13 * coeff3.y
		 + px14 * coeff3.z
		 + px15 * coeff3.w
		 ;
}

//$$export_shader vs_4_0_level_9_3 VS_Downsample g_VDCapDXGI12_VS_Downsample
//$$export_shader ps_4_0_level_9_3 PS_Downsample4 g_VDCapDXGI12_PS_Downsample4
//$$export_shader ps_4_0_level_9_3 PS_Downsample6 g_VDCapDXGI12_PS_Downsample6
//$$export_shader ps_4_0_level_9_3 PS_Downsample8 g_VDCapDXGI12_PS_Downsample8
//$$export_shader ps_4_0_level_9_3 PS_Downsample10 g_VDCapDXGI12_PS_Downsample10
//$$export_shader ps_4_0_level_9_3 PS_Downsample12 g_VDCapDXGI12_PS_Downsample12
//$$export_shader ps_4_0_level_9_3 PS_Downsample14 g_VDCapDXGI12_PS_Downsample14
//$$export_shader ps_4_0_level_9_3 PS_Downsample16 g_VDCapDXGI12_PS_Downsample16
