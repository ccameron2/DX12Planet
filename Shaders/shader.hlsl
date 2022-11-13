
cbuffer cbPerObject : register(b0)
{
	float4x4 gWorldViewProj;
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
	vout.Pos = mul(float4(vin.Pos, 1.0f), gWorldViewProj);
	
	// Pass vertex color into the pixel shader.
	vout.Colour = vin.Colour;
    
	return vout;
}

float4 PS(VOut pin) : SV_Target
{
	return pin.Colour;
}


