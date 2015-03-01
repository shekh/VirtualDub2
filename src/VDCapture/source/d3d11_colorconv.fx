extern Texture2D srctex : register(t0);
extern SamplerState srcsamp : register(s0);

extern float4 axis : register(c0);
extern float4 uvplacement : register(c1);

///////////////////////////////////////////////////////////////////////////

void VS_ColorConv1(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2,
	out float2 oT3 : TEXCOORD3
)
{
	oPos = float4(pos.xy, 0.5, 1);
	oT0 = uv + uvplacement.xy * -3.0f;
	oT1 = uv + uvplacement.xy * -1.0f;
	oT2 = uv + uvplacement.xy * +1.0f;
	oT3 = uv + uvplacement.xy * +3.0f;
}

void VS_ColorConv2(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2,
	out float2 oT3 : TEXCOORD3,
	out float2 oT4 : TEXCOORD4,
	out float2 oT5 : TEXCOORD5,
	out float2 oT6 : TEXCOORD6,
	out float2 oT7 : TEXCOORD7
)
{
	oPos = float4(pos.xy, 0.5, 1);
	
	float2 uv0 = uv + uvplacement.xy * -3.0f;
	float2 uv1 = uv + uvplacement.xy * -1.0f;
	float2 uv2 = uv + uvplacement.xy * +1.0f;
	float2 uv3 = uv + uvplacement.xy * +3.0f;
	
	oT0 = uv0 - uvplacement.zw;
	oT1 = uv0 + uvplacement.zw;
	oT2 = uv1 - uvplacement.zw;
	oT3 = uv1 + uvplacement.zw;
	oT4 = uv2 - uvplacement.zw;
	oT5 = uv2 + uvplacement.zw;
	oT6 = uv3 - uvplacement.zw;
	oT7 = uv3 + uvplacement.zw;
}

///////////////////////////////////////////////////////////////////////////

float4 PS_ColorConv1(float4 pos : SV_Position,
	float2 uv0 : TEXCOORD0,
	float2 uv1 : TEXCOORD1,
	float2 uv2 : TEXCOORD2,
	float2 uv3 : TEXCOORD3
) : SV_Target0 {
	float4 px0 = float4(srctex.Sample(srcsamp, uv0).rgb, 1);
	float4 px1 = float4(srctex.Sample(srcsamp, uv1).rgb, 1);
	float4 px2 = float4(srctex.Sample(srcsamp, uv2).rgb, 1);
	float4 px3 = float4(srctex.Sample(srcsamp, uv3).rgb, 1);
	
	float4 r;
	
	r.b = dot(px0, axis);
	r.g = dot(px1, axis);
	r.r = dot(px2, axis);
	r.a = dot(px3, axis);
	
	return r;
}

float4 PS_ColorConv2(float4 pos : SV_Position,
	float2 uv0 : TEXCOORD0,
	float2 uv1 : TEXCOORD1,
	float2 uv2 : TEXCOORD2,
	float2 uv3 : TEXCOORD3,
	float2 uv4 : TEXCOORD4,
	float2 uv5 : TEXCOORD5,
	float2 uv6 : TEXCOORD6,
	float2 uv7 : TEXCOORD7
) : SV_Target0 {
	float4 px0 = float4(srctex.Sample(srcsamp, uv0).rgb + srctex.Sample(srcsamp, uv1).rgb, 1);
	float4 px1 = float4(srctex.Sample(srcsamp, uv2).rgb + srctex.Sample(srcsamp, uv3).rgb, 1);
	float4 px2 = float4(srctex.Sample(srcsamp, uv4).rgb + srctex.Sample(srcsamp, uv5).rgb, 1);
	float4 px3 = float4(srctex.Sample(srcsamp, uv6).rgb + srctex.Sample(srcsamp, uv7).rgb, 1);
	
	float4 r;
	
	r.b = dot(px0, axis);
	r.g = dot(px1, axis);
	r.r = dot(px2, axis);
	r.a = dot(px3, axis);
	
	return r;
}

///////////////////////////////////////////////////////////////////////////

void VS_ColorConvYUY2(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float2 oT0 : TEXCOORD0,
	out float2 oT1 : TEXCOORD1,
	out float2 oT2 : TEXCOORD2
)
{
	oPos = float4(pos.xy, 0.5, 1);
	oT0 = uv + uvplacement.xy * -1.5f;
	oT1 = uv + uvplacement.xy * -0.5f;
	oT2 = uv + uvplacement.xy * +0.5f;
}

float4 PS_ColorConvYUY2(float4 pos : SV_Position,
	float2 uv0 : TEXCOORD0,
	float2 uv1 : TEXCOORD1,
	float2 uv2 : TEXCOORD2
) : SV_Target0 {
	float4 px0 = float4(srctex.Sample(srcsamp, uv0).rgb, 1);
	float4 px1 = float4(srctex.Sample(srcsamp, uv1).rgb, 1);
	float4 px2 = float4(srctex.Sample(srcsamp, uv2).rgb, 1);
	
	float4 r;
	
	const float4 yaxis = { 0.2567882f, 0.5041294f, 0.0979059f, 16.0f/255.0f };
	const float4 cbaxis = {	-0.1482229f, -0.2909928f, 0.4392157f, 128.0f/255.0f };
	const float4 craxis = {	0.4392157f, -0.3677883f, -0.0714274f, 128.0f/255.0f };
	
	float4 pxc = (px0 + px2)*0.5f + px1;
	
	r.b = dot(px1, yaxis);
	r.g = dot(pxc, cbaxis*0.5f);
	r.r = dot(px2, yaxis);
	r.a = dot(pxc, craxis*0.5f);
	
	return r;
}

//$$export_shader vs_4_0_level_9_3 VS_ColorConv1 g_VDCapDXGI12_VS_ColorConv1
//$$export_shader vs_4_0_level_9_3 VS_ColorConv2 g_VDCapDXGI12_VS_ColorConv2
//$$export_shader ps_4_0_level_9_3 PS_ColorConv1 g_VDCapDXGI12_PS_ColorConv1
//$$export_shader ps_4_0_level_9_3 PS_ColorConv2 g_VDCapDXGI12_PS_ColorConv2
//$$export_shader vs_4_0_level_9_3 VS_ColorConvYUY2 g_VDCapDXGI12_VS_ColorConvYUY2
//$$export_shader ps_4_0_level_9_3 PS_ColorConvYUY2 g_VDCapDXGI12_PS_ColorConvYUY2
