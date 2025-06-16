//***************************************************************************************
// Shadows.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct HullOut
{
	float3 PosL    : POSITION;
	float2 TexC    : TEXCOORD;
};

struct DomainOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};


VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;

	// MaterialData matData = gMaterialData[gMaterialIndex];
	
	/*
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);

    // Transform to homogeneous clip space.
    vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;
	*/	

	vout.PosL = vin.PosL;
	vout.TexC = vin.TexC;

    return vout;
}

struct PatchTess
{
	float EdgeTess[4] : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHS(InputPatch<VertexOut, 4> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	/*
	pt.EdgeTess[0] = 2.0f;
	pt.EdgeTess[1] =  2.0f;
	pt.EdgeTess[2] =  2.0f;
	pt.EdgeTess[3] =  2.0f;
	

	pt.InsideTess[0] =  2.0f;
	pt.InsideTess[1] =  2.0f;
	*/

	
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

[domain("quad")]  
[partitioning("integer")] // 정수 분할 방식
[outputtopology("triangle_cw")]  // 시계방향 삼각형
[outputcontrolpoints(4)]  
[patchconstantfunc("ConstantHS")]  // 패치 상수 함수 지정
[maxtessfactor(64.0f)]
HullOut HS(InputPatch<VertexOut,4> inputPatch,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	HullOut hout;
	hout.PosL = inputPatch[i].PosL;
	hout.TexC = inputPatch[i].TexC;

	return hout;


}

[domain("quad")]
DomainOut DS(PatchTess patchTess, 
			 float2 uv : SV_DomainLocation, 
			 const OutputPatch<HullOut, 4> quad)
{
	DomainOut dout;
	
	// 사각형 패치 보간 (Bilinear)
	float3 posTop = lerp(quad[0].PosL, quad[1].PosL, uv.x);
	float3 posBottom = lerp(quad[3].PosL, quad[2].PosL, uv.x);
	float3 p = lerp(posTop, posBottom, uv.y);

	float2 texTop = lerp(quad[0].TexC, quad[1].TexC, uv.x);
	float2 texBottom = lerp(quad[3].TexC, quad[2].TexC, uv.x);
	float2 t = lerp(texTop, texBottom, uv.y);


	float height = gHeightMap.SampleLevel(gsamAnisotropicWrap, t, 0).r;

	p.y = p.y + height * 50;

	

	float4 texC = mul(float4(t, 0.0f, 1.0f), gTexTransform);

	float4 posW = mul(float4(p, 1.0f), gWorld);
	dout.PosH = mul(posW, gViewProj);
	dout.TexC = texC.xy;



	return dout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(DomainOut pin)  
{
	// Fetch the material data.
	// MaterialData matData = gMaterialData[gMaterialIndex];
	float4 diffuseAlbedo = gDiffuseAlbedo;
    uint diffuseMapIndex = gTexIndex;
	
	// Dynamically look up the texture in the array.
	diffuseAlbedo *= gDiffuseMap[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif

	
}


