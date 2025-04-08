#pragma enable_d3d12_debug_symbols


Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void NormalCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{
	int minX = max(0,dispatchThreadID.x-1);
	int minY = max(0,dispatchThreadID.y-1);
	int maxX = min(gInput.Length.x-1, dispatchThreadID.x+1);
	int maxY = min(gInput.Length.y-1, dispatchThreadID.y+1);
	
	float tmpX = (float)dispatchThreadID.x/gInput.Length.x;
	float tmpY = (float)dispatchThreadID.y/gInput.Length.y;
	float2 xyPos = float2(tmpX, tmpY);
	
	float dx = (float)1/gInput.Length.x;
	float dy = (float)1/gInput.Length.y;

	float3 posCen = float3(xyPos.xy,gInput[dispatchThreadID.xy].r);
	float3 posRight = float3(xyPos.x+dx,gInput[int2(maxX,dispatchThreadID.y)].r,xyPos.y);
	float3 posLeft = float3(xyPos.x-dx,gInput[int2(minX,dispatchThreadID.y)].r,xyPos.y);
	float3 posUp = float3(xyPos.x,gInput[int2(dispatchThreadID.x,minY)].r,xyPos.y-dy);
	float3 posDown = float3(xyPos.x,gInput[int2(dispatchThreadID.x,maxY)].r,xyPos.y+dy);

	float3 gradX = posRight - posLeft;
	float3 gradY = posUp - posDown;

	float3 normal = normalize(cross(gradX,gradY));

	
	float4 normalValue = float4(normal,0.0f);

	gOutput[dispatchThreadID.xy] = normalValue;

}

