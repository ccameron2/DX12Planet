
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

Texture2D TextureMaps[4] : register(t0);

//Texture2D AlbedoMap : register(t0);
//Texture2D NormalMap : register(t1);
//Texture2D RoughnessMap : register(t2);
//Texture2D MetalnessMap : register(t3);
SamplerState Sampler : register(s4);

float4 PS(VOut pIn) : SV_Target
{

//	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
//        float3 worldNormal = normalize(pIn.NormalW);
//        float3 worldTangent = normalize(pIn.Tangent);

//	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
//        float3 worldBitangent = cross(worldNormal, worldTangent);
//        float3x3 tangentMatrix = float3x3(worldTangent, worldBitangent, worldNormal);

//	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
//        float3 tangentNormal = normalize(TextureMaps[1].Sample(Sampler, pIn.UV).xyz * 2.0f - 1.0f);
	
//	// Convert the sampled normal from tangent space (as it was stored) into world space. This now replaces the world normal for lighting
//        worldNormal = mul(tangentNormal, tangentMatrix);

//// Blinn-Phong lighting (Point)
//	//float3 lightVector = Lights[0].Position - pIn.PosW;
//	//float lightDistance = length(lightVector);
//	//float3 lightNormal = lightVector / lightDistance;
//	//float3 cameraNormal = normalize(EyePosW - pIn.PosW);
//	//float3 halfwayNormal = normalize(cameraNormal + lightNormal);
	
//	// Attenuate light colour
//	//float3 attLightColour = Lights[0].Colour / lightDistance;
	
//	//// Diffuse
//	//float lightDiffuseLevel = saturate(dot(worldNormal, lightNormal));
//	//float3 lightDiffuseColour = attLightColour * lightDiffuseLevel;
	
//	//// Specular
//	//float lightSpecularLevel = saturate(dot(worldNormal, halfwayNormal));
//	//float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
//	// Blinn-Phong lighting (Direction)
//    float3 lightVector = -Lights[0].Direction;
//    float3 cameraNormal = normalize(EyePosW - pIn.PosW);
//    float3 halfwayNormal = normalize(cameraNormal + lightVector);
	
//	// Diffuse
//    float lightDiffuseLevel = saturate(dot(worldNormal, lightVector));
//    float3 lightDiffuseColour = Lights[0].Colour * lightDiffuseLevel;
	
//	// Specular
//    float lightSpecularLevel = saturate(dot(worldNormal, halfwayNormal));
//    float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
//	//float4 diffuseColour = pIn.Colour;
//    float4 diffuseColour = TextureMaps[0].Sample(Sampler, pIn.UV);
	
//    float3 specularColour = { 1.0f, 1.0f, 1.0f };
//    float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
//    return float4(finalColour, diffuseColour.a);
	
    VOut vOut;
	
	/////////////////////////////
	// Parallax mapping

	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
    float3 worldNormal = normalize(pIn.NormalW);
    float3 worldTangent = normalize(pIn.Tangent);

	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
    float3 worldBitangent = cross(worldNormal, worldTangent);
    float3x3 tangentMatrix = float3x3(worldTangent, worldBitangent, worldNormal);

	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
    float3 tangentNormal = normalize(TextureMaps[1].Sample(Sampler, pIn.UV).xyz * 2.0f - 1.0f);

	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
    float3 n = mul(tangentNormal, tangentMatrix);
	
	///////////////////////
	// Sample PBR textures

    float3 albedo = TextureMaps[0].Sample(Sampler, pIn.UV).rgb;
    float roughness = TextureMaps[2].Sample(Sampler, pIn.UV).r;
    float metalness = TextureMaps[3].Sample(Sampler, pIn.UV).r;
    roughness = 0.5f;
    metalness = 0.5f;
	// Diffuse colour from material/mesh
    // float4 baseDiffuse = gMaterialDiffuseColour * gMeshColour;

	// Surface normal dot view (camera) direction - don't allow it to become 0 or less
    float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
    float nDotV = max(dot(n, v), 0.001f);

	// Select specular color based on metalness
    float3 specularColour = lerp(0.04f, albedo, metalness);

    float roughnessMip = 8 * log2(roughness + 1); // Heuristic to convert roughness to mip-map. Rougher surfaces will use smaller (blurrier) mip-maps

	// Overall global illumination - rough approximation
    float3 colour = /*baseDiffuse.rgb * */albedo + (1 - roughness);
	
	///////////////////////
	// Calculate Physically based Rendering BRDF

    float3 lightVector = -Lights[0].Direction;
    float3 cameraNormal = normalize(EyePosW - pIn.PosW);
    float3 h = normalize(v + lightVector);

	// Attenuate light colour (reduce strength based on its distance)
    float3 lc = Lights[0].Colour.rgb / length(lightVector);

	// Various dot products used throughout
    float nDotL = max(dot(n, lightVector), 0.001f);
    float nDotH = max(dot(n, h), 0.001f);
    float vDotH = max(dot(v, h), 0.001f);

	// Lambert diffuse
    float3 lambert = albedo / 3.1415f;

	// Microfacet specular - fresnel term
    float3 F = specularColour + (1 - specularColour) * pow(max(1.0f - vDotH, 0.0f), 5.0f);

	// Microfacet specular - normal distribution term
    float alpha = max(roughness * roughness, 2.0e-3f);
    float alpha2 = alpha * alpha;
    float nDotH2 = nDotH * nDotH;
    float dn = nDotH2 * (alpha2 - 1) + 1;
    float D = alpha2 / (3.1415f * dn * dn);

	// Microfacet specular - geometry term
    float k = (roughness + 1);
    k = k * k / 8;
    float gV = nDotV / (nDotV * (1 - k) + k);
    float gL = nDotL / (nDotL * (1 - k) + k);
    float G = gV * gL;

	// Full brdf, diffuse + specular
    float3 brdf = /*baseDiffuse.rgb * */lambert + F * G * D / (4 * nDotL * nDotV);

	// Accumulate punctual light equation for this light
    colour += 3.1415f * nDotL * lc * brdf;

    return float4(colour, /*baseDiffuse.a*/1.0f);
	
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
	
//	// Blinn-Phong lighting (Direction)
//    float3 lightVector = -Lights[0].Direction;
//    float3 cameraNormal = normalize(EyePosW - pIn.PosW);
//    float3 halfwayNormal = normalize(cameraNormal + lightVector);
	
//	// Diffuse
//    float lightDiffuseLevel = saturate(dot(worldNormal, lightVector));
//    float3 lightDiffuseColour = Lights[0].Colour * lightDiffuseLevel;
	
//	// Specular
//    float lightSpecularLevel = saturate(dot(worldNormal, halfwayNormal));
//    float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
//	//float4 diffuseColour = pIn.Colour;
//    float4 diffuseColour = DiffuseMap.Sample(DiffuseFilter, pIn.UV);
	
//    float3 specularColour = { 1.0f, 1.0f, 1.0f };
//    float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
//    return float4(finalColour, diffuseColour.a);
}