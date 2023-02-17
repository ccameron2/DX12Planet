
#define MaxLights 16

struct Light
{
	float3 Colour;
	float padding1;
	float3 Strength;
	float FalloffStart; // point/spot light only
	float3 Direction; // directional/spot light only
	float FalloffEnd; // point/spot light only
	float3 Position; // point light only
	float SpotPower; // spot light only
};


cbuffer cbPerObjectConstants : register(b0)
{
	float4x4 World;
};

cbuffer cbPerPassConstants : register(b1)
{
	float4x4 View;
	float4x4 Proj;
	float4x4 ViewProj;
	float3 EyePosW;
	float padding1;
	float2 RenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;
	float padding2;
	float padding3;
	float4 AmbientLight;
	Light Lights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
};

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
	
	float3 worldNormal = normalize(pIn.NormalW);
	
	// Blinn-Phong lighting (Point)
	//float3 lightVector = Lights[0].Position - pIn.PosW;
	//float lightDistance = length(lightVector);
	//float3 lightNormal = lightVector / lightDistance;
	//float3 cameraNormal = normalize(EyePosW - pIn.PosW);
	//float3 halfwayNormal = normalize(cameraNormal + lightNormal);
	
	// Attenuate light colour
	//float3 attLightColour = Lights[0].Colour / lightDistance;
	
	//// Diffuse
	//float lightDiffuseLevel = saturate(dot(worldNormal, lightNormal));
	//float3 lightDiffuseColour = attLightColour * lightDiffuseLevel;
	
	//// Specular
	//float lightSpecularLevel = saturate(dot(worldNormal, halfwayNormal));
	//float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
	// Blinn-Phong lighting (Direction)
	float3 lightVector = -Lights[0].Direction;
	float3 cameraNormal = normalize(EyePosW - pIn.PosW);
	float3 halfwayNormal = normalize(cameraNormal + lightVector);
	
	// Diffuse
	float lightDiffuseLevel = saturate(dot(worldNormal, lightVector));
	float3 lightDiffuseColour = Lights[0].Colour * lightDiffuseLevel;
	
	// Specular
	float lightSpecularLevel = saturate(dot(worldNormal, halfwayNormal));
	float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
	float4 diffuseColour = pIn.Colour;
	diffuseColour = gDiffuseAlbedo;
	
	float3 specularColour = { 1.0f, 1.0f, 1.0f };
	float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
	return float4(finalColour, diffuseColour.a);
}