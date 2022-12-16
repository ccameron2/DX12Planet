
#define MaxLights 16

struct Light
{
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
	
	// Pass vertex color into the pixel shader.
	vout.Colour = vin.Colour;
    
	return vout;
}

float4 PS(VOut pin) : SV_Target
{
	
	// Interpolating normal can unnormalize it, so renormalize it.
	pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = normalize(EyePosW - pin.PosW);

	// Indirect lighting.
	float3 ambient = AmbientLight.xyz;
	
	float3 lightDir = normalize(Lights[0].Position - pin.PosW.xyz);
	
	//float3 directionalLightPos = {0.0f,1.0f,0.0f};
	//lightDir = normalize(directionalLightPos);
	
	float3 lightColour = { 0.9f, 0.9f, 1.0f };
	float3 diffuseLight = ambient + lightColour * max(dot(pin.NormalW, lightDir), 0);
	
	float3 halfway = normalize(lightDir + toEyeW);
	float specularPower = 64.0f;
	float3 specularLight = lightColour * pow(max(dot(pin.NormalW, halfway), 0), specularPower);
	
	float3 diffuseColour = pin.Colour.xyz;
	float specularColour = pin.Colour.a;

	float3 finalColour = (diffuseLight * diffuseColour) + (specularLight * specularColour);

	return float4(finalColour, 1.0f);
}

//// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
//// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
//float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
//{
//	float cosIncidentAngle = saturate(dot(normal, lightVec));

//	float f0 = 1.0f - cosIncidentAngle;
//	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

//	return reflectPercent;
//}

//float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, float4 colour)
//{
//	// Shinyness to roughness
//	const float m = 0.125f * 256.0f;
//	float3 halfVec = normalize(toEye + lightVec);
	
//	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
//	float3 fresnelR0 = { 0.01f, 0.01f, 0.01f };
//	float3 fresnelFactor = SchlickFresnel(fresnelR0, halfVec, lightVec); //FresnelR0

//	float3 specAlbedo = fresnelFactor * roughnessFactor;

//    // Our spec formula goes outside [0,1] range, but we are 
//    // doing LDR rendering.  So scale it down a bit.
//	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

//	return (colour.rgb + specAlbedo) * lightStrength;
//}

//float4 PS(VOut pin) : SV_Target
//{
	
//	// Interpolating normal can unnormalize it, so renormalize it.
//	pin.NormalW = normalize(pin.NormalW);

//    // Vector from point being lit to eye. 
//	float3 toEyeW = normalize(EyePosW - pin.PosW);

//	// Indirect lighting.
//	float4 ambient = AmbientLight * pin.Colour;
	
//	// The light vector aims opposite the direction the light rays travel.
//	float3 lightVec = -Lights[0].Direction;

//    // Scale light down by Lambert's cosine law.
//	float ndotl = max(dot(lightVec, pin.NormalW), 0.0f);
//	float3 lightStrength = Lights[0].Strength * ndotl;
//	float3 shadowFactor = 1.0f;

//	float4 directLight = 0;
//	float3 blinnPhong = BlinnPhong(lightStrength, lightVec, pin.NormalW, toEyeW, pin.Colour);
//	directLight.xyz += shadowFactor * blinnPhong;
//	float4 litColour = ambient * directLight;
//	litColour.a = pin.Colour.a;
	
//	//return litColour;
	
//	return pin.Colour;
//}


