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
	float3 PosW : POSITION;
	float4 Colour : COLOUR;
	float3 NormalW : NORMAL;
	float2 UV : UV;
    float3 Tangent : TANGENT;
};

VOut VS(VIn vin)
{
	VOut vout;
	
	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), World);
	vout.PosW = posW.xyz;
	
	vout.NormalW = mul(vin.NormalL, (float3x3)World);
	
	vout.PosH = mul(posW, ViewProj);
    
	vout.UV = vin.UV;
	
    vout.Tangent = vin.Tangent;
	
	return vout;
}

Texture2D Textures[1] : register(t0);

float4 PS(VOut pIn) : SV_Target
{
	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
	float3 worldNormal = normalize(pIn.NormalW);
	float3 worldTangent = normalize(pIn.Tangent);
	
	float2 uv = pIn.UV;
		
	// Sample textures
	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Roughness;
	float metalness = Metallic;
	float ao = 1.0f;
	
	float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
	float3 n = worldNormal;
	
	return CalculateLighting(albedo, roughness, metalness, ao, n, v);
}