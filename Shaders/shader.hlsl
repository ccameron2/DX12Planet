
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
	
	float3 specularColour = { 1.0f, 1.0f, 1.0f };
	float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
	return float4(finalColour, diffuseColour.a);
}

//float4 PS(VOut pin) : SV_Target
//{
	
//	 //Interpolating normal can unnormalize it, so renormalize it.
//	pin.NormalW = normalize(pin.NormalW);

//     //Vector from point being lit to eye. 
//	float3 toEyeW = normalize(EyePosW - pin.PosW);

//	 //Indirect lighting.
//	float3 ambient = AmbientLight.xyz;
	
//	float3 lightDir = normalize(Lights[0].Position - pin.PosW.xyz);
	
//	//float3 directionalLightPos = { 0.0f, 1.0f, 0.0f };
//	//lightDir = normalize(directionalLightPos);
	
//	float3 lightColour = { 0.9f, 0.9f, 1.0f };
//	float3 diffuseLight = ambient + lightColour * max(dot(pin.NormalW, lightDir), 0);
	
//	float3 halfway = normalize(lightDir + toEyeW);
//	float specularPower = 64.0f;
//	float3 specularLight = lightColour * pow(max(dot(pin.NormalW, halfway), 0), specularPower);
	
//	float3 diffuseColour = pin.Colour.xyz;
//	float specularColour = pin.Colour.a;

//	float3 finalColour = (diffuseLight * diffuseColour) + (specularLight * specularColour);

//	return float4(finalColour, 1.0f);
//}

//float CalcAttenuation(float d, float falloffStart, float falloffEnd)
//{
//    // Linear falloff.
//	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
//}

//// Schlick gives an approximation to Fresnel reflectance (see pg. 233 "Real-Time Rendering 3rd Ed.").
//// R0 = ( (n-1)/(n+1) )^2, where n is the index of refraction.
//float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
//{
//	float cosIncidentAngle = saturate(dot(normal, lightVec));

//	float f0 = 1.0f - cosIncidentAngle;
//	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

//	return reflectPercent;
//}

//float3 BlinnPhong(float3 colour, float3 lightStrength, float3 lightVec, float3 normal, float3 toEye)
//{
//	const float m = 0.7f * 256.0f; // Shinyness
//	float3 halfVec = normalize(toEye + lightVec);

//	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
//	float3 fresnelFactor = SchlickFresnel(0.05f, halfVec, lightVec); // Fresnel

//	float3 specAlbedo = fresnelFactor * roughnessFactor;

//    // Our spec formula goes outside [0,1] range, but we are 
//    // doing LDR rendering.  So scale it down a bit.
//	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

//	return (colour + specAlbedo) * lightStrength;
//}

//float3 ComputePointLight(float3 colour, Light L, float3 pos, float3 normal, float3 toEye)
//{
//    // The vector from the surface to the light.
//	float3 lightVec = L.Position - pos;

//    // The distance from surface to light.
//	float d = length(lightVec);

//    // Range test.
//	if (d > L.FalloffEnd)
//		return 0.0f;

//    // Normalize the light vector.
//	lightVec /= d;

//    // Scale light down by Lambert's cosine law.
//	float ndotl = max(dot(lightVec, normal), 0.0f);
//	float3 lightStrength = L.Strength * ndotl;

//    // Attenuate light by distance.
//	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
//	lightStrength *= att;

//	return BlinnPhong(colour, lightStrength, lightVec, normal, toEye);
//}

//float3 ComputeDirectionalLight(float3 colour, Light L, float3 normal, float3 toEye)
//{
//    // The light vector aims opposite the direction the light rays travel.
//	float3 lightVec = -L.Direction;

//    // Scale light down by Lambert's cosine law.
//	float ndotl = max(dot(lightVec, normal), 0.0f);
//	float3 lightStrength = L.Strength * ndotl;

//	return BlinnPhong(colour, lightStrength, lightVec, normal, toEye);
//}

//float4 ComputeLighting(float3 colour, Light gLights[MaxLights],
//                       float3 pos, float3 normal, float3 toEye,
//                       float3 shadowFactor)
//{
//	float3 result = 0.0f;
//	for (int i = 0; i < 1; i++)
//	{
//		//result += ComputePointLight(colour, gLights[i], pos, normal, toEye);
//		result += shadowFactor[i] * ComputeDirectionalLight(colour, gLights[i], normal, toEye);
//	}
//	return float4(result, 0.0f);
//}

//float4 PS(VOut pin) : SV_Target
//{
//    // Interpolating normal can unnormalize it, so renormalize it.
//	pin.NormalW = normalize(pin.NormalW);

//    // Vector from point being lit to eye. 
//	float3 toEyeW = normalize(EyePosW - pin.PosW);

//	// Indirect lighting.
//	float4 ambient = AmbientLight * pin.Colour;

//	const float shininess = 1.0f - 0.3f; //Roughness
//	float3 shadowFactor = 1.0f;
//	float4 directLight = ComputeLighting(pin.Colour.xyz, Lights, pin.PosW,
//        pin.NormalW, toEyeW, shadowFactor);

//	float4 litColour = ambient + directLight;

//    // Common convention to take alpha from diffuse material.
//	litColour.a = 1.0f;
	
//	//litColour.rgb = 0.5 * (pin.NormalW + 1);
//	//litColour.rgb = pin.NormalW;
	
//	//float3 normalEndpoint = pin.PosW + 0.1 * pin.NormalW;
	
//	return litColour;
//}