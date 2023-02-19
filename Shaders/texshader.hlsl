
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

static const float gParallaxDepth = 0.04f;

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
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Roughness;
	float4x4 MatTransform;
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

Texture2D Textures[6] : register(t0);

//Texture2D AlbedoMap	
//Texture2D RoughnessMap
//Texture2D NormalMap
//Texture2D MetalnessMap
//Texture2D DisplacementMap	
//Texture2D AOMap

SamplerState Sampler : register(s4);

static const float PI = 3.1415f;
static const float GAMMA = 2.2f;

float4 CalculateLighting(float3 albedo, float roughness, float metalness, float2 uv, float ao, float3 n, float3 v, bool direction = true, float3 posW = {0,0,0})
{
	// Diffuse colour from material/mesh
	float4 baseDiffuse = DiffuseAlbedo;

	// Sum lighting from each light
	float3 colour = albedo * AmbientLight.xyz * ao;
	
	float3 lightVector = 0;
	float3 l = 0;
	float3 lc = 0;
	
	// Calculate Physically based Rendering BRDF
	if (direction)
	{
		// Set to direction light
		lightVector = normalize(-Lights[0].Direction);
		l = lightVector;
		
		lc = Lights[0].Colour.rgb;
	}
	else
	{		
		lightVector = Lights[0].Position - posW; // Vector to light
		float lightDistance = length(lightVector); // Distance to light
		l = lightVector / lightDistance; // Normal to light
		
		// Attenuate light colour
		lc = Lights[0].Colour.rgb / lightDistance;
	}

	float3 h = normalize(l + v); // Halfway normal is halfway between camera and light normals	
	// Various dot products used throughout
	float nDotL = max(dot(n, l), 0.001f);
	float nDotH = max(dot(n, h), 0.001f);
	float vDotH = max(dot(v, h), 0.001f);

	// Lambert diffuse
	float3 lambert = albedo / PI;

	// Surface normal dot view (camera) direction - don't allow it to become 0 or less
	float nDotV = max(dot(n, v), 0.001f);

	// Select specular color based on metalness
	float3 specularColour = lerp(0.04f, albedo, metalness);
	
	// Microfacet specular - fresnel term
	float3 F = specularColour + (1 - specularColour) * pow(max(1.0f - vDotH, 0.0f), 5.0f);

	// Microfacet specular - normal distribution term
	float alpha = max(roughness * roughness, 2.0e-3f);
	float alpha2 = alpha * alpha;
	float nDotH2 = nDotH * nDotH;
	float dn = nDotH2 * (alpha2 - 1) + 1;
	float D = alpha2 / (PI * dn * dn);

	// Microfacet specular - geometry term
	float k = (roughness + 1);
	k = k * k / 8;
	float gV = nDotV / (nDotV * (1 - k) + k);
	float gL = nDotL / (nDotL * (1 - k) + k);
	float G = gV * gL;

	// Full brdf, diffuse + specular
	float3 brdf = baseDiffuse.rgb * lambert + F * G * D / (4 * nDotL * nDotV);

	// Accumulate punctual light equation for this light
	colour += PI * nDotL * lc * brdf;
	colour = pow(colour, 1 / GAMMA);
	
	return float4(colour, baseDiffuse.a);
}

float4 AOPBRPS(VOut pIn) : SV_Target
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
	float3 parallaxOffset = mul(tangentMatrix, v); // Transform camera normal into tangent space (so it is local to texture)
	float2 uv = pIn.UV + (gParallaxDepth * displacement * parallaxOffset.xy); // Offset texture uv in camera's view direction based on displacement. You can divide 
	                                                                          // this by parallaxOffset.z to remove limiting (stronger effect, prone to artefacts)
	
	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
	float3 tangentNormal = normalize(Textures[2].Sample(Sampler, uv).xyz * 2.0f - 1.0f);

	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
	float3 n = mul(tangentNormal, tangentMatrix);

	// Sample PBR textures

	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Textures[1].Sample(Sampler, uv).r;
	float metalness = Textures[3].Sample(Sampler, uv).r;
	float ao = Textures[5].Sample(Sampler, uv).r;
	
	return CalculateLighting(albedo,roughness,metalness,uv,ao,n,v);	
}

float4 ParallaxPBRPS(VOut pIn) : SV_Target
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
	float3 parallaxOffset = mul(tangentMatrix, v); // Transform camera normal into tangent space (so it is local to texture)
	float2 uv = pIn.UV + (gParallaxDepth * displacement * parallaxOffset.xy); // Offset texture uv in camera's view direction based on displacement. You can divide 
	                                                                          // this by parallaxOffset.z to remove limiting (stronger effect, prone to artefacts)
	
	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
	float3 tangentNormal = normalize(Textures[2].Sample(Sampler, uv).xyz * 2.0f - 1.0f);

	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
	float3 n = mul(tangentNormal, tangentMatrix);

	// Sample PBR textures

	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Textures[1].Sample(Sampler, uv).r;
	float metalness = Textures[3].Sample(Sampler, uv).r;
	float ao = 1.0f;

	return CalculateLighting(albedo, roughness, metalness, uv, ao, n,v);
}

float4 NormalPBRPS(VOut pIn) : SV_Target
{
	// Parallax mapping

	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
	float3 worldNormal = normalize(pIn.NormalW);
	float3 worldTangent = normalize(pIn.Tangent);

	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
	float3 worldBitangent = cross(worldNormal, worldTangent);
	float3x3 tangentMatrix = float3x3(worldTangent, worldBitangent, worldNormal);
		
	float3 v = normalize(EyePosW - pIn.PosW); // Get normal to camera, called v for view vector in PBR equations
	
	// Use normal UVs
	float2 uv = pIn.UV;
	
	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
	float3 tangentNormal = normalize(Textures[2].Sample(Sampler, uv).xyz * 2.0f - 1.0f);

	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
	float3 n = mul(tangentNormal, tangentMatrix);

	// Sample PBR textures

	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
	float roughness = Textures[1].Sample(Sampler, uv).r;
	float metalness = Textures[3].Sample(Sampler, uv).r;
	float ao = 1.0f;

	return CalculateLighting(albedo, roughness, metalness, uv, ao, n, v);
}

//float4 NormalPBRPS(VOut pIn) : SV_Target
//{
//	// Parallax mapping

//	// Renormalise pixel normal and tangent because interpolation from the vertex shader can introduce scaling (refer to Graphics module lecture notes)
//	float3 worldNormal = normalize(pIn.NormalW);
//	float3 worldTangent = normalize(pIn.Tangent);

//	// Create the bitangent - at right angles to normal and tangent. Then with these three axes, form the tangent matrix - the local axes for the current pixel
//	float3 worldBitangent = cross(worldNormal, worldTangent);
//	float3x3 tangentMatrix = float3x3(worldTangent, worldBitangent, worldNormal);
	
//	// Get normal to camera, called v for view vector in PBR equations
//	float3 v = normalize(EyePosW - pIn.PosW); 
	
//	// Use normal UVs
//	float2 uv = pIn.UV;
	
//	// Sample the normal map - including conversion from 0->1 RGB values to -1 -> 1 XYZ values
//	float3 tangentNormal = normalize(Textures[2].Sample(Sampler, uv).xyz * 2.0f - 1.0f);

//	// Convert the sampled normal from tangent space (as it was stored) into world space. This becomes the n, the world normal for the PBR equations below
//	float3 n = mul(tangentNormal, tangentMatrix);

//	// Sample PBR textures
//	float3 albedo = Textures[0].Sample(Sampler, uv).rgb;
//	float roughness = Textures[1].Sample(Sampler, uv).r;
//	float metalness = 1.0f;
//	float ao = 1.0f;
	
//	return CalculateLighting(albedo, roughness, metalness, uv, ao, n, v);
//}