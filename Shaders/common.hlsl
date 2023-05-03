
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

static const float gParallaxDepth = 0.03f;
static const float PI = 3.1415f;
static const float GAMMA = 2.2f;

TextureCube IBLCubeMap : register(t0,space1);

cbuffer cbPerObjectConstants : register(b0)
{
	float4x4 World;
	bool parallax;
	float3 padding;
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
	float TexDebugIndex;
	float padding3;
	float4 AmbientLight;
	Light Lights[MaxLights];
};

cbuffer cbMaterial : register(b2)
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Roughness;
	float Metallic;
	float3 padding4;
	float4x4 MatTransform;
};

SamplerState Sampler : register(s4);

float4 CalculateLighting(float3 albedo, float roughness, float metalness, float ao, float3 n, float3 v, 
						float3 emissive = {0,0,0}, bool direction = true, float3 posW = { 0, 0, 0 })
{	
	float3 lightVector = 0;
	float3 l = 0;
	float3 lc = 0;
	float li = Lights[0].Strength;
	
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
	
	float3 reflectionVector = reflect(-v, n);
	
	float3 IBLDiffuse = IBLCubeMap.SampleLevel(Sampler, n, 9.0f).rgb;

	float specularMipLevel = 8 * log2(roughness + 1);
	
	float3 IBLSpecular = IBLCubeMap.SampleLevel(Sampler, reflectionVector, specularMipLevel);
	
	float nDotv = max(dot(n, v), 0.001f);
	float3 IBLFresnel = specularColour + (1 - specularColour) * pow(max(1.0f - nDotv, 0.0f), 5.0f);
	
	float3 colour = ao * (albedo * IBLDiffuse + (1 - roughness) * IBLFresnel * IBLSpecular);
	
	// Microfacet specular - fresnel term
	float3 F = specularColour + (1 - specularColour) * pow(max(1.0f - vDotH, 0.0f), 5.0f);

	// Microfacet specular - normal distribution term
	float alpha1 = max(roughness * roughness, 2.0e-3f);
	float alpha2 = alpha1 * alpha1;
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
	float3 brdf = albedo.rgb * lambert + F * G * D / (4 * nDotL * nDotV);

	// Accumulate punctual light equation for this light
	colour += PI * nDotL * lc * li * brdf;
	colour += emissive;
	
	colour = pow(colour, 1 / GAMMA);
	
	return float4(colour, 1.0f);
}