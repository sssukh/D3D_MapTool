#define XNUM 64

struct Vertex
{
	float3 Pos;
	float3 Normal;
	float3 Tex;
};

struct PickingResult
{
	bool Hit;
    // uint TriangleID;
    // float2 BaryCoords = bary;
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
    float3 e2 = v2 - v0;
    float3 p = cross(rayDir, e2);
    float det = dot(e1, p);
    
    if (abs(det) < 1e-8) return false;
    
    float invDet = 1.0 / det;
    float3 T = rayOrigin - v0;
    
    bary.x = dot(T, p) * invDet;
    if (bary.x < 0 || bary.x > 1) return false;
    
    float3 q = cross(T, e1);
    bary.y = dot(rayDir, q) * invDet;
    if (bary.y < 0 || bary.x + bary.y > 1) return false;
    
    t = dot(e2, q) * invDet;
    return t > 0;

	// bary.x for v1, bary.y for v2, (1 - bary.x - bary.y) for v0
}

float ApplyDisplacement(float3 VertexPos)
{
	int u = clamp(VertexPos.x + gWidth/2,0,gWidth-1);
	int v = clamp(VertexPos.z + gHeight/2,0,gHeight-1);
	int2 uv = int2(u,v);
	return gHeightMap[uv].r * 100.0f;

	// return float3(0.0f,sampledHeightMap,0.0f);
}




[numthreads(1, 1, 1)]
void IntersectCS(uint3 tid : SV_DispatchThreadID)
{
    // 폄면 원본 메시 데이터 접근
    StructuredBuffer<Vertex> vertices = gVertexBuffer;
    StructuredBuffer<uint> indices = gIndexBuffer;
    
	bool bIsIntersected = false;
	
	float desDist; float3 desPos;
	
    // 광선-메시 교차 검사
	// [unroll(64)]
    for (uint i = 0; i < gNumTriangles; i++)
	// for (uint i = tid.x; i<gNumTriangles; i+=64)
    {

		uint idx0 = indices[3*i];
		uint idx1 = indices[3*i+1];
		uint idx2 = indices[3*i+2];
		
        float3 v0 = vertices[idx0].Pos;
        float3 v1 = vertices[idx1].Pos;
        float3 v2 = vertices[idx2].Pos;
        
        // 높이맵 변형 적용 (Domain Shader와 동일 로직)
		// 높이맵 텍스처 필요
        v0.y += ApplyDisplacement(v0);
        v1.y += ApplyDisplacement(v1);
        v2.y += ApplyDisplacement(v2);
        
		
        float t; float2 bary;
        if (RayIntersectTriangle(gRayOrigin, gRayDir, v0, v1, v2, t, bary))
        {
			//desDist = t;
			// desPos = (1-bary.x-bary.y)*v0 + bary.x*v1 + bary.y*v2;
            // 가장 가까운 교차점 저장
            // if (t < gPickingResult[0].Distance)
            // {
                gPickingResult[0].Hit = true;
            //    // gPickingResult.TriangleID = i;
            //    // gPickingResult.BaryCoords = bary;
				gPickingResult[0].IntersectPos = (1-bary.x-bary.y)*v0 + bary.x*v1 + bary.y*v2;
                gPickingResult[0].Distance = t;
            // }
			
			

        	// 원자적 연산으로 Distance 업데이트
        	// uint oldDist;
        	// InterlockedMin(gPickingResult[0].Distance, asuint(t), oldDist);
            /*
        	if (t < asfloat(oldDist))
        	{
        		gPickingResult[0].IntersectPos = (1-bary.x-bary.y)*v0 + bary.x*v1 + bary.y*v2;
        		gPickingResult[0].Hit = true;
        		gPickingResult[0].Distance = t;
        	}
			*/
			
        }
		
    }
	
	// GroupMemoryBarrierWithGroupSync();
	

	// gPickingResult[0].Hit = true;
	// gPickingResult[0].IntersectPos = desPos;
	// gPickingResult[0].Distance = desDist;
}
