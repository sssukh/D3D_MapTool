#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};



VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

    vout.PosL = vin.PosL;
	
	vout.NormalL = vin.NormalL;
	
	vout.TexC = vin.TexC;
	
    return vout;
}

float CalcTessFactor(float4 centerPos)
{
	float d = distance(centerPos,float4(gEyePosW,1.0f));

	const float d0 = 100.0f;
	const float d1 = 500.0f;
	return clamp(20.0f*saturate( (d1-d)/(d1-d0) ),3.0f,64.0f);
}

struct PatchTess
{
	float EdgeTess[4] : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;
	
	float3 centerL = 0.25f*(patch[0].PosL + patch[1].PosL + patch[2].PosL + patch[3].PosL);
	float4 centerW = mul(float4(centerL, 1.0f), gWorld);
	
	
	// Uniformly tessellate the patch.

	pt.EdgeTess[0] = CalcTessFactor(0.5f * mul(float4(patch[0].PosL + patch[2].PosL,1.0f),gWorld));
	pt.EdgeTess[1] = CalcTessFactor(0.5f * mul(float4(patch[0].PosL + patch[1].PosL,1.0f),gWorld));
	pt.EdgeTess[2] = CalcTessFactor(0.5f * mul(float4(patch[1].PosL + patch[3].PosL,1.0f),gWorld));
	pt.EdgeTess[3] = CalcTessFactor(0.5f * mul(float4(patch[2].PosL + patch[3].PosL,1.0f),gWorld));
	
	float tess = CalcTessFactor(centerW);

	pt.InsideTess[0] = tess;
	pt.InsideTess[1] = tess;
	
	return pt;
}

struct HullOut
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut, 4> p, 
		   uint i : SV_OutputControlPointID,
		   uint patchId : SV_PrimitiveID)
{
	HullOut hout;
	
	hout.PosL = p[i].PosL;
	hout.NormalL = p[i].NormalL;
	hout.TexC = p[i].TexC;
	
	return hout;
}

struct DomainOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};


// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("quad")]
DomainOut DS(PatchTess patchTess, 
			 float2 uv : SV_DomainLocation, 
			 const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;
	
	// For quad patch
	// Bilinear interpolation.
	// Get position
	float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x); 
	float3 v2 = lerp(quad[2].PosL, quad[3].PosL, uv.x); 
	float3 p  = lerp(v1, v2, uv.y);
 		
	// For quad patch
	// Bilinear interpolation.
	// Get uv value
	float2 t1 = lerp(quad[0].TexC,quad[1].TexC, uv.x);
	float2 t2 = lerp(quad[2].TexC,quad[3].TexC, uv.x);
	float2 t = lerp(t1,t2,uv.y);

	// For tri patch
	// barycentric interpolation
	// Get Position
	// float3 p = tri[0].PosL * baryCoords.x
	// 			+ tri[1].PosL * baryCoords.y
	// 			+ tri[2].PosL * baryCoords.z;
	
	// For tri patch
	// barycentric interpolation
	// Get uv value
	// float2 t = tri[0].TexC * baryCoords.x
	// 			+ tri[1].TexC * baryCoords.y
	// 			+ tri[2].TexC * baryCoords.z;
	
	// float height = gHeightMap.SampleLevel(gsamAnisotropicWrap, t, 0).r * 2.0f - 1.0f;
	float height = gHeightMap.SampleLevel(gsamAnisotropicWrap, t, 0).r;

	p.y = p.y + height * 50;

	float4 normalMap = gNormalMap.SampleLevel(gsamAnisotropicWrap, t, 0);
	float3 normal = float3(normalMap.rgb);

	float4 texC = mul(float4(t, 0.0f, 1.0f), gTexTransform);

	float4 posW = mul(float4(p, 1.0f), gWorld);
	dout.PosH = mul(posW, gViewProj);
	dout.PosW = (float3)posW;
	dout.NormalW = mul(normal, (float3x3)gWorld);
	dout.TexC = texC.xy;

	return dout;
}



float4 PS(DomainOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseMap[gTexIndex].Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
	
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;
	
	float3 PosOnPlane = pin.PosW;
	
	float2 delta = pin.PosW.xz - gMousePosOnPlane.xz;

	// 범위 내의 픽셀들 강조
	int MaxRange = gIntersectRange*gIntersectRange;
	int MidRange = gMaxStrengthRange*gMaxStrengthRange;
	float range = dot(delta,delta);
	if(range < MaxRange)
		litColor.r = 255.0f;
	if(abs(range - (float)MidRange) < 1.5f)
	{
		litColor.r = 0.0f;
		litColor.b = 255.0f;
	}

	// temporary code for check
	// if(length(PosOnPlane - gMousePosOnPlane)<15)
	//	litColor.r = 255.0f;

    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}



