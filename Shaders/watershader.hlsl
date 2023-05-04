#include "common.hlsl"

struct VIn
{
	float3 PosL : POSITION;
	float4 Colour : COLOUR;
	float3 NormalL : NORMAL;
	float2 UV : UV;
	float3 Tangent : TANGENT;
};

struct VOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : UV;
};

VOut VS(VIn vin)
{
	VOut vout;
	
	// Use local vertex position as cubemap lookup vector.
	vout.PosL = vin.PosL;
	
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), World);
	
	// Always center sky about camera.
	posW.xyz += EyePosW;
	
	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, ViewProj).xyww;
	
	return vout;
}

float4 PS(VOut pin) : SV_Target
{
	return IBLCubeMap.Sample(Sampler, pin.PosL);
}
