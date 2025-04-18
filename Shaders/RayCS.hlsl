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
}

// 스트럭처드 버퍼
// 광선 충돌 검출용
StructuredBuffer<Vertex> gVertexBuffer;
StructuredBuffer<uint> gIndexBuffer;

// 광선
cbuffer Ray : register(b0)
{
    float3 gRayOrigin;
    float3 gRayDir;
}

// 충돌 결과
RWStructuredBuffer<PickingResult> gPickingResult;

[numthreads(64, 1, 1)]
void CS_Picking(uint3 tid : SV_DispatchThreadID)
{
    
    // 폄면 원본 메시 데이터 접근
    StructuredBuffer<Vertex> vertices = gVertexBuffer;
    StructuredBuffer<uint> indices = gIndexBuffer;
    
    // 광선-메시 교차 검사
    for (uint i = 0; i < gNumTriangles; ++i)
    {
        uint3 tri = indices[i].xyz;
        float3 v0 = vertices[tri.x].WorldPos;
        float3 v1 = vertices[tri.y].WorldPos;
        float3 v2 = vertices[tri.z].WorldPos;
        
        // 높이맵 변형 적용 (Domain Shader와 동일 로직)
        v0 += ApplyDisplacement(v0);
        v1 += ApplyDisplacement(v1);
        v2 += ApplyDisplacement(v2);
        
        float t; float2 bary;
        if (RayIntersectTriangle(ray.Origin, ray.Dir, v0, v1, v2, t, bary))
        {
            // 가장 가까운 교차점 저장
            if (t < gPickingResult.Distance)
            {
                gPickingResult.Hit = true;
                gPickingResult.TriangleID = i;
                gPickingResult.BaryCoords = bary;
                gPickingResult.Distance = t;
            }
        }
    }
}
