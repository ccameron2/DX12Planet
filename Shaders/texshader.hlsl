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

Texture2D Textures[6] : register(t0);

//Texture2D AlbedoMap	
//Texture2D RoughnessMap
//Texture2D NormalMap
//Texture2D MetalnessMap
//Texture2D DisplacementMap	
//Texture2D AOMap

SamplerState Sampler : register(s4);

float4 PS(VOut pIn) : SV_Target
{
	// Parallax mapping

	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
	float3 worldNormal = normalize(pIn.NormalW);
	float3 worldTangent = normalize(pIn.Tangent);

	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
	float3 worldBitangent = cross(worldNormal, worldTangent);
	float3x3 tangentMatrix = float3x3(worldTangent, worldBitangent, worldNormal);

	// Get displacement at current pixel, convert 0->1 range to -0.5 -> 0.5 range
	float displacement = Textures[4].Sample(Sampler, pIn.UV).r - 0.5f;

	// Parallax mapping
	float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
	float2 uv = 0;
	
	if (parallax)
	{
		float3 parallaxOffset = mul(tangentMatrix, v); // Transform camera normal into tangent space (so it is local to texture)
		uv = pIn.UV + (gParallaxDepth * displacement * parallaxOffset.xy); // Offset texture uv in camera's view direction based on displacement. You can divide 	                                                                          // this by parallaxOffset.z to remove limiting (stronger effect, prone to artefacts)
	}
	else{ uv = pIn.UV; }
		
	
	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
	float3 tangentNormal = normalize(Textures[2].Sample(Sampler, uv).xyz * 2.0f - 1.0f);

	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
	float3 n = mul(tangentNormal, tangentMatrix);

	// Sample PBR textures

	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Textures[1].Sample(Sampler, uv).r;
	float metalness = Textures[3].Sample(Sampler, uv).r;
	float ao = Textures[5].Sample(Sampler, uv).r;
	
	return CalculateLighting(albedo, roughness, metalness, ao, n, v);
}