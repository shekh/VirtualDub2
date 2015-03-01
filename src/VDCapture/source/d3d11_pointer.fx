extern Texture2D srctex : register(t0);
extern SamplerState srcsamp : register(s0);

extern float4 position_mapping : register(c0);

///////////////////////////////////////////////////////////////////////////

void VS_Pointer(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float2 oT0 : TEXCOORD0
)
{
	oPos = float4(uv.xy * position_mapping.xy + position_mapping.zw, 0.5, 1);
	oT0 = uv;
}

///////////////////////////////////////////////////////////////////////////

float4 PS_Pointer_Blend(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
	return srctex.Sample(srcsamp, uv);
}

float4 PS_Pointer_MaskA0(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
	float4 c = srctex.Sample(srcsamp, uv);
	
	clip(0.5f - c.a);	
	return c;
}

float4 PS_Pointer_MaskA1(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
	float4 c = srctex.Sample(srcsamp, uv);
	
	clip(c.a - 0.5f);
	return c;
}

//$$export_shader vs_4_0_level_9_3 VS_Pointer g_VDCapDXGI12_VS_Pointer
//$$export_shader ps_4_0_level_9_3 PS_Pointer_Blend g_VDCapDXGI12_PS_Pointer_Blend
//$$export_shader ps_4_0_level_9_3 PS_Pointer_MaskA0 g_VDCapDXGI12_PS_Pointer_MaskA0
//$$export_shader ps_4_0_level_9_3 PS_Pointer_MaskA1 g_VDCapDXGI12_PS_Pointer_MaskA1
