#define XNUM 64

#pragma enable_d3d12_debug_symbols

struct Vertex
{
	float3 Pos;
	float3 Normal;
	float2 Tex;
};

struct PickingResult
{
	bool Hit;
	float3 IntersectPos;
    float Distance;
};

// 스트럭처드 버퍼
// 광선 충돌 검출용
StructuredBuffer<Vertex> gVertexBuffer : register(t1);
StructuredBuffer<uint> gIndexBuffer : register(t2);

// 광선
cbuffer Ray : register(b0)
{
    float3 gRayOrigin;
    float3 gRayDir;
};

cbuffer PlaneInfo : register(b1)
{
    uint gWidth;
    uint gHeight;
	uint gNumTriangles;
};

Texture2D gHeightMap           : register(t0);


// 충돌 결과
RWStructuredBuffer<PickingResult> gPickingResult : register(u0);



// 광선-삼각형 교차 검사
bool RayIntersectTriangle(
    float3 rayOrigin, float3 rayDir,
    float3 v0, float3 v1, float3 v2,
    out float t, out float2 bary)
{
   	float3 e1 = v1 - v0;
    float3 e2 = v2 - v0; // 수정: 방향 변경
    float3 p = cross(rayDir, e2);
    float det = dot(e1, p);
    
    if (det < 1e-8 && det > -1e-8) return false; // 수정: 부호 고려
    
    float invDet = 1.0f / det;
    float3 T = rayOrigin - v0;
    
    bary.x = dot(T, p) * invDet;
    // bary.y = dot(rayDir, cross(T, e1)) * invDet; // 수정: e1 순서
    bary.y = dot(cross(T, e1),rayDir) * invDet;

    if (bary.x < 0.0f || bary.y < 0.0f || (bary.x + bary.y) > 1.0f) // 수정: 조건 강화
        return false;
    
    t = dot(e2, cross(T, e1)) * invDet;
	
	

    return t > 0.0f;

	// bary.x for v1, bary.y for v2, (1 - bary.x - bary.y) for v0
}

bool RayIntersectQuad(
	float3 rayOrigin, float3 rayDir,
	float3 v0, float3 v1, float3 v2, float3 v3,
	out float t, out float2 uv)
{
	/*
	float3 dv1 = v1 - v0;
	float3 dv2 = v2 - v0;

	float3 n = cross(dv2, dv1);

	float nDot = dot(n,rayDir);
	
	if(nDot < 1e-8 && nDot > -1e-8) return false;

	float d = - dot(n,v0);

	t = -(dot(n,rayOrigin) + d)/nDot;

	float3 intersectPos = rayOrigin + t*rayDir;
	
	float3 vp = intersectPos - v0;


	

	if(intersectPos.x<v0.x || intersectPos.x>v1.x || intersectPos.z<v2.z || intersectPos.z>v0.z)
		return false;
	

	float u = dot(vp, normalize(dv1)) / length(dv1);
	float v = dot(vp, normalize(dv2)) / length(dv2);
	
	uv = float2(u,v);
	
	return true;
	*/
	bool hit1 = RayIntersectTriangle(rayOrigin,rayDir,v0,v1,v2, t, uv);
	
	if(hit1)
		return hit1;
	
	bool hit2 = RayIntersectTriangle(rayOrigin,rayDir,v1,v2,v3,t,uv);
	
	return hit2;
}

float ApplyDisplacement(float3 VertexPos, float2 TexC)
{
	// int u = clamp(VertexPos.x + gWidth/2,0,gWidth-1);
	// int v = clamp(-VertexPos.z + gHeight/2,0,gHeight-1);
	// int2 uv = int2(u,v);
	// return gHeightMap[uv].r * 100.0f;
	// return gHeightMap[TexC].r * 100.0f;
	
	 int u = TexC.x * gWidth;
	 int v = TexC.y * gHeight;
	 int2 uv = int2(u,v);
	 return gHeightMap[uv].r * 50.0f;


	// return float3(0.0f,sampledHeightMap,0.0f);
}




[numthreads(1, 1, 1)]
void IntersectCS(uint3 tid : SV_DispatchThreadID)
{
    // 폄면 원본 메시 데이터 접근
    StructuredBuffer<Vertex> vertices = gVertexBuffer;
    StructuredBuffer<uint> indices = gIndexBuffer;
    
	bool bIsIntersected = false;
	
	PickingResult result = {false,float3(0,0,0),1000.0f};
    // 광선-메시 교차 검사
	// [unroll(64)]
    for (uint i = 0; i < gNumTriangles; i++)
	// for (uint i = tid.x; i<gNumTriangles; i+=64)
    {

		uint idx0 = indices[4*i];
		uint idx1 = indices[4*i+1];
		uint idx2 = indices[4*i+2];
		uint idx3 = indices[4*i+3];

        float3 v0 = vertices[idx0].Pos;
        float3 v1 = vertices[idx1].Pos;
        float3 v2 = vertices[idx2].Pos;
		float3 v3 = vertices[idx3].Pos;
        
		float2 v0UV = vertices[idx0].Tex;
        float2 v1UV = vertices[idx1].Tex;
        float2 v2UV = vertices[idx2].Tex;
		float2 v3UV = vertices[idx3].Tex;

        // 높이맵 변형 적용 (Domain Shader와 동일 로직)
		// 높이맵 텍스처 필요
        v0.y += ApplyDisplacement(v0,v0UV);
        v1.y += ApplyDisplacement(v1,v1UV);
        v2.y += ApplyDisplacement(v2,v2UV);
        v3.y += ApplyDisplacement(v3,v3UV);
		
        float t; float2 bary;
		/*
        if (RayIntersectTriangle(gRayOrigin, gRayDir, v0, v1, v2, t, bary))
        {
            // 가장 가까운 교차점 저장
            if (t < result.Distance)
            {
                result.Hit = true;
				result.IntersectPos = (1-bary.x-bary.y)*v0 + bary.x*v1 + bary.y*v2;
                result.Distance = t;
            }
        }
		 if (RayIntersectTriangle(gRayOrigin, gRayDir, v1, v2, v3, t, bary))
        {
            // 가장 가까운 교차점 저장
            if (t < result.Distance)
            {
                result.Hit = true;
				result.IntersectPos = (1-bary.x-bary.y)*v0 + bary.x*v1 + bary.y*v2;
                result.Distance = t;
            }
        }
		*/
		if(RayIntersectQuad(gRayOrigin, gRayDir, v0,v1,v2,v3, t, bary))
		{
			if(t<result.Distance)
			{
				result.Hit = true;
				result.IntersectPos = gRayOrigin + t * gRayDir; 
				result.Distance = t;
			}
		}
		
    }
	
	GroupMemoryBarrierWithGroupSync();
	
	gPickingResult[0] = result;

	// gPickingResult[0].Hit = true;
	// gPickingResult[0].IntersectPos = desPos;
	// gPickingResult[0].Distance = desDist;
}
