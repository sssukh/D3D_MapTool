#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	nointerpolation uint MatIndex  : MATINDEX;
};



VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID) 
{
	VertexOut vout = (VertexOut)0.0f;

	InstanceData instData = gInstanceData[instanceID];
	float4x4 world = instData.World;
	float4x4 texTransform = instData.TexTransform;
	uint matIndex = instData.MaterialIndex;

	MaterialData matData = gMaterialData[matIndex];

	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), world);
	vout.PosW = posW.xyz;


	// Transform to homogeneous clip space.
	vout.PosH = mul(posW, gViewProj);
	
	// Output vertex attributes for interpolation across triangle.
	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;
	
	vout.NormalW = mul(float4(vin.NormalL,1.0f), world).xyz;
	
	vout.MatIndex = instData.MaterialIndex;
	
    return vout;
}


/*
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

*/

float4 PS(VertexOut pin) : SV_Target
{
	MaterialData matData = gMaterialData[pin.MatIndex];
	float4 mDiffuseAlbedo = matData.DiffuseAlbedo;
	float3 mFresnelR0 = matData.FresnelR0;
	float  mRoughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;
    
	float4 diffuseAlbedo = gDiffuseMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC) * mDiffuseAlbedo;

    // Vector from point being lit to eye. 
	float3 toEyeW = gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye; // normalize

    // Light terms.
    float4 ambient = gAmbientLight*diffuseAlbedo;

	// float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
	// shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

    const float shininess = 1.0f - mRoughness;
    Material mat = { diffuseAlbedo, mFresnelR0, shininess };
    float3 shadowFactor = 0.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;
	
	if(gMouseMode==1)
	{	
		

	}


    // Common convention to take alpha from diffuse albedo.
    litColor.a = diffuseAlbedo.a;

    return litColor;
}



