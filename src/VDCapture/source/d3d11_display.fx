extern Texture2D srctex : register(t0);
extern SamplerState srcsamp : register(s0);

///////////////////////////////////////////////////////////////////////////

void VS_Display(
	float2 pos : POSITION,
	float2 uv : TEXCOORD0,
	out float4 oPos : SV_Position,
	out float2 oT0 : TEXCOORD0
)
{
	oPos = float4(pos.xy, 0.5, 1);
	oT0 = uv;
}

///////////////////////////////////////////////////////////////////////////

float4 PS_Display(float4 pos : SV_Position,
	float2 uv0 : TEXCOORD0
) : SV_Target0 {
	float4 px0 = float4(srctex.Sample(srcsamp, uv0).rgb, 1);	
	return px0;
}

//$$export_shader vs_4_0_level_9_3 VS_Display g_VDCapDXGI12_VS_Display
//$$export_shader ps_4_0_level_9_3 PS_Display g_VDCapDXGI12_PS_Display
