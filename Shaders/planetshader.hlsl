#include "common.hlsl"

static bool BlinnPhong = true;

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
	
	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
	float3 worldNormal = normalize(pIn.NormalW);
		
    float3 dx = ddx(pIn.PosW);
    float3 dy = ddy(pIn.PosW);
	
    float3 n = normalize(cross(dx, dy));
		
    if (BlinnPhong)
    {
		// Blinn-Phong lighting (Direction)
        float3 lightVector = -Lights[0].Direction;
        float3 cameraNormal = normalize(EyePosW - pIn.PosW);
        float3 halfwayNormal = normalize(cameraNormal + lightVector);
	
		// Diffuse
        float lightDiffuseLevel = saturate(dot(n, lightVector));
        float3 lightDiffuseColour = Lights[0].Colour * lightDiffuseLevel;
	
		// Specular
        float lightSpecularLevel = saturate(dot(n, halfwayNormal));
        float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
        float4 diffuseColour = pIn.Colour;
		
        float3 specularColour = { 1.0f, 1.0f, 1.0f };
        float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
        return float4(finalColour, diffuseColour.a);	
    }
	
	// Sample PBR
    float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
	
	float3 albedo = pIn.Colour;
	float roughness = 0.95f;
	float metalness = 1.0f;
	float ao = 1.0f;
	
	return CalculateLighting(albedo, roughness, metalness, ao, n, v);
}