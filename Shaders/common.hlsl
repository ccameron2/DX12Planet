
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
static const float PI = 3.1415f;
static const float GAMMA = 2.2f;

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
	float Metallic;
	float3 padding4;
	float4x4 MatTransform;
};

float4 BlinnPhong(float4 colour, float3 n, float3 v)
{
	// Blinn-Phong lighting (Direction)
	float3 lightVector = -Lights[0].Direction;
	float3 halfwayNormal = normalize(v + lightVector);
	
		// Diffuse
	float lightDiffuseLevel = saturate(dot(n, lightVector));
	float3 lightDiffuseColour = Lights[0].Colour * lightDiffuseLevel;
	
		// Specular
	float lightSpecularLevel = saturate(dot(n, halfwayNormal));
	float3 lightSpecularColour = lightDiffuseColour * lightSpecularLevel;
	
	float4 diffuseColour = colour;
		
	float3 specularColour = { 1.0f, 1.0f, 1.0f };
	float3 finalColour = diffuseColour.rgb * (AmbientLight.rgb + lightDiffuseColour) + (specularColour * lightSpecularColour);
	
	return float4(finalColour, diffuseColour.a);
}

float4 CalculateLighting(float3 albedo, float roughness, float metalness, float ao, float3 n, float3 v, bool direction = true, float3 posW = { 0, 0, 0 })
{
	// Diffuse colour from material/mesh
	float4 baseDiffuse = DiffuseAlbedo;

	// Sum lighting from each light
	float3 colour = albedo * AmbientLight.xyz * ao;
	
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
	colour += PI * nDotL * lc * li * brdf;
	
	colour = pow(colour, 1 / GAMMA);
	
	return float4(colour, baseDiffuse.a);
}