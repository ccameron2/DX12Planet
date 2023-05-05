#include "common.hlsl"

struct VIn
{
	float3 PosL : POSITION;
	float4 Colour : COLOUR;
	float3 NormalL : NORMAL;
};

struct VOut
{
	float4 PosH : SV_POSITION;
	float3 PosW : POSITION;
	float4 Colour : COLOUR;
	float3 NormalW : NORMAL;
};

VOut VS(VIn vin)
{
	VOut vout;
	
	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), World);
	vout.PosW = posW.xyz;
	
	vout.NormalW = mul(vin.NormalL, (float3x3)World);
	
	vout.PosH = mul(posW, ViewProj);
	
	// Pass vertex colour into the pixel shader.
	vout.Colour = vin.Colour;
    
	return vout;
}

float4 PS(VOut pIn) : SV_Target
{
	VOut vOut;
	
	float3 dx = ddx(pIn.PosW);
	float3 dy = ddy(pIn.PosW);
	
	float3 n = normalize(cross(dx, dy));
	float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
	
	// Calculate steepness relative to the up vector
	float3 pos = normalize(pIn.PosW.xyz);
	float steepness = dot(n, pos);
	
	float3 center = float3(0.0f, 0.0f, 0.0f); // center of the sphere
	pos = normalize(pIn.PosW.xyz - center.xyz);
	
	// Assign colors based on elevation and steepness
	float elevation = length(pIn.PosW.xyz - center.xyz) - 0.5f; // sphere radius is 0.5
	float3 albedo;
	if (elevation > 0.6f) // high elevation, use snow
		albedo = float3(1.0f, 1.0f, 1.0f); // white
	else if (elevation > 0.59f) // high elevation, use dirt
		albedo = float3(0.7f, 0.4f, 0.1f); 
	else if (elevation < 0.49f) // low elevation, use dark sand
		albedo = float3(0.6f, 0.45f, 0.3f); 
	else if (elevation < 0.51f) // low elevation, use sand
		albedo = float3(1.0f, 0.9f, 0.7f); 
	else if (steepness > 0.85f) // steep enough for grass
		albedo = float3(0.2f, 0.6f, 0.2f); 
	else if (steepness < 0.77f) // steep enough for stone
		albedo = float3(0.5f, 0.5f, 0.5f); 
	else // in between, use dirt
		albedo = float3(0.7f, 0.4f, 0.1f); 
		
	// Set roughness, metalness, and AO to constant values
	float roughness = 0.95f;
	float metalness = 1.0f;
	float ao = 1.0f;
	
	return CalculateLighting(albedo, roughness, metalness, ao, n, v);
}