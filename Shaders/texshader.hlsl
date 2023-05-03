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

Texture2D Textures[7] : register(t0);

//Texture2D AlbedoMap	
//Texture2D RoughnessMap
//Texture2D NormalMap
//Texture2D MetalnessMap
//Texture2D DisplacementMap	
//Texture2D AOMap

//SamplerState Sampler : register(s4);

float4 PS(VOut pIn) : SV_Target
{
	// Parallax mapping

	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
	float3 worldNormal = normalize(pIn.NormalW);
	float3 worldTangent = normalize(pIn.Tangent);

	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
	// Calculate inverse tangent matrix
	float3 biTangent = cross(worldNormal, worldTangent);
	float3x3 invTangentMatrix = float3x3(worldTangent, biTangent, pIn.NormalW);
	
	// Get normal to camera, called v for view vector in PBR equations
	float3 v = normalize(EyePosW - pIn.PosW);
	
	// Parallax mapping
	float2 uv = pIn.UV;
		
	// Parallax mapping. Comment out for plain normal mapping
	if (parallax)
	{
		// // Get camera direction in model space
		//float3x3 invWorldMatrix = transpose((float3x3) World);
		//float3 cameraModelDir = normalize(mul(v, invWorldMatrix));

		// // Calculate direction to offset UVs (x and y of camera direction in tangent space)
		//float3x3 tangentMatrix = transpose(invTangentMatrix);
		//float2 textureOffsetDir = mul(cameraModelDir, tangentMatrix).xy;

		//// Offset UVs in that direction to account for depth (using height map and some geometry)
		//float texDepth = gParallaxDepth * (Textures[4].Sample(Sampler, uv).r - 0.5f);
		//uv += texDepth * textureOffsetDir;
		
		float displacement = Textures[4].Sample(Sampler, uv).r - 0.5f;
		float3 parallaxOffset = mul(invTangentMatrix, v); // Transform camera normal into tangent space (so it is local to texture)
		float2 uv = pIn.UV + gParallaxDepth * displacement * parallaxOffset.xy;
	}
		
	// Extract normal from map and shift to -1 to 1 range
	float3 textureNormal = 2.0f * Textures[2].Sample(Sampler, uv).rgb - 1.0f;
	textureNormal.y = -textureNormal.y;

	// Convert normal from tangent space to world space
	float3 n = normalize(mul(mul(textureNormal, invTangentMatrix), (float3x3) World));
	
	// Sample PBR textures

	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Textures[1].Sample(Sampler, uv).r;
	float metalness = Textures[3].Sample(Sampler, uv).r;
	float ao = Textures[5].Sample(Sampler, uv).r;
	float3 emissive = Textures[6].Sample(Sampler, uv);
	
	// Return lighting or debug texture
	if (TexDebugIndex == 0) return CalculateLighting(albedo, roughness, metalness, ao, n, v, emissive);
	else if (TexDebugIndex == 1) return Textures[0].Sample(Sampler, uv);
	else if (TexDebugIndex == 2) return Textures[1].Sample(Sampler, uv);
	else if (TexDebugIndex == 3) return Textures[2].Sample(Sampler, uv);
	else if (TexDebugIndex == 4) return Textures[3].Sample(Sampler, uv);
	else if (TexDebugIndex == 5) return Textures[4].Sample(Sampler, uv);
	else if (TexDebugIndex == 6) return Textures[5].Sample(Sampler, uv);
	else if (TexDebugIndex == 7) return Textures[6].Sample(Sampler, uv);

	return CalculateLighting(albedo, roughness, metalness, ao, n, v, emissive);
}