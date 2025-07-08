//***************************************************************************************
// DrawNormals.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

// Include common HLSL code.
#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float3 PosL		: POSITION;
    float3 NormalL  : NORMAL;
	float2 TexC     : TEXCOORD;
	InstanceData InstData : INSTDATA;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut)0.0f;

	InstanceData InstData = gInstanceData[instanceID];
	
	vout.InstData = InstData; 

    vout.PosL = vin.PosL;
	
	vout.NormalL = vin.NormalL;

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
	
	float4x4 world = patch[0].InstData.World;

	PatchTess pt;
	
	float3 centerL = 0.25f*(patch[0].PosL + patch[1].PosL + patch[2].PosL + patch[3].PosL);
	float4 centerW = mul(float4(centerL, 1.0f), world);
	
	
	// Uniformly tessellate the patch.

	pt.EdgeTess[0] = CalcTessFactor(0.5f * mul(float4(patch[0].PosL + patch[2].PosL,1.0f),world));
	pt.EdgeTess[1] = CalcTessFactor(0.5f * mul(float4(patch[0].PosL + patch[1].PosL,1.0f),world));
	pt.EdgeTess[2] = CalcTessFactor(0.5f * mul(float4(patch[1].PosL + patch[3].PosL,1.0f),world));
	pt.EdgeTess[3] = CalcTessFactor(0.5f * mul(float4(patch[2].PosL + patch[3].PosL,1.0f),world));
	
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
	InstanceData InstData : INSTDATA;
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
	hout.InstData = p[i].InstData;
	
	return hout;
}

struct DomainOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	InstanceData InstData : INSTDATA;
};


// The domain shader is called for every vertex created by the tessellator.  
// It is like the vertex shader after tessellation.
[domain("quad")]
DomainOut DS(PatchTess patchTess, 
			 float2 uv : SV_DomainLocation, 
			 const OutputPatch<HullOut, 4> quad)
{

	InstanceData instData = quad[0].InstData;
	float4x4 world = instData.World;
	float4x4 texTransform = instData.TexTransform;

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

	float4 texC = mul(float4(t, 0.0f, 1.0f), texTransform);

	float4 posW = mul(float4(p, 1.0f), world);
	dout.PosH = mul(posW, gViewProj);
	dout.PosW = (float3)posW;
	dout.NormalW = mul(normal, (float3x3)world);
	dout.TexC = texC.xy;


	dout.InstData = quad[0].InstData;

	return dout;
}

float4 PS(DomainOut pin) : SV_Target
{
	// Fetch the material data.
	/*
	MaterialData matData = gMaterialData[InstData.MaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseMapIndex = matData.DiffuseMapIndex;
	uint normalMapIndex = matData.NormalMapIndex;
	
    // Dynamically look up the texture in the array.
    diffuseAlbedo *= gTextureMaps[diffuseMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	*/
#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif

	// Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
	
    // NOTE: We use interpolated vertex normal for SSAO.

    // Write normal in view space coordinates
    float3 normalV = mul(pin.NormalW, (float3x3)gView);
    return float4(normalV, 0.0f);
}


