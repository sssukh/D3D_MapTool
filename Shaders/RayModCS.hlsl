
#pragma enable_d3d12_debug_symbols


cbuffer ModHeightInfo                : register(b0)
{
    uint IntersectRange;

    uint MaxStrengthRange;

    float ModifyingStrength;

    float3 IntersectPos;
};


Texture2D gHeightMap                 : register(t0);

RWTexture2D<float4> gModHeightMap    : register(u0);

[numthreads(8,8,1)]
void ModCS(uint3 tid : SV_DispatchThreadID)
{
    uint width = gHeightMap.Length.x;
    uint height = gHeightMap.Length.y;

    // int u = clamp(VertexPos.x + gWidth/2,0,gWidth-1);
    // int v = clamp(-VertexPos.z + gHeight/2,0,gHeight-1);

    uint u = clamp(IntersectPos.x + width/2,0,width-1);
    uint v = clamp(-IntersectPos.z + height/2,0,height-1);
    
    int2 uv = int2(u,v);

    int2 delta = uv - int2(tid.x,tid.y);

    float deltaSq = dot(float2(delta),float2(delta));

    float denom = max(IntersectRange*IntersectRange - MaxStrengthRange*MaxStrengthRange, 1.0);

    float heightModR = saturate((IntersectRange*IntersectRange - deltaSq)/denom);
    
    float4 texMod = gHeightMap[tid.xy];

    texMod.r = min(texMod.r + heightModR * ModifyingStrength, 1.0f);

    gModHeightMap[tid.xy] = texMod;
}