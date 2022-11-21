
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
};

struct VIn
{
	float3 Pos : POSITION;
	float4 Colour : COLOUR;
};

struct VOut
{
	float4 Pos : SV_POSITION;
	float4 Colour : COLOUR;
};

VOut VS(VIn vin)
{
	VOut vout;
	
	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.Pos, 1.0f), World);
	vout.Pos = mul(posW, ViewProj);
	
	// Pass vertex color into the pixel shader.
	vout.Colour = vin.Colour;
    
	return vout;
}

float4 PS(VOut pin) : SV_Target
{
	return pin.Colour;
}


