#pragma enable_d3d12_debug_symbols


Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void NormalCS(int3 groupThreadID : SV_GroupThreadID,
				int3 dispatchThreadID : SV_DispatchThreadID)
{


	// [] [] []
	// [] [] []
	// [] [] []
	int2 xyValues[3][3];
	
	xyValues[1][1] = dispatchThreadID.xy;	

	{
		int xValue = max(0,dispatchThreadID.x-1);
		for(int i=0;i<3;++i)
		{
			xyValues[i][0] = int2(xValue,dispatchThreadID.y -1 + i);
		}
	}
	{
		int xValue = min(gInput.Length.x-1,dispatchThreadID.x+1);
		for(int i=0;i<3;++i)
		{
			xyValues[i][2] = int2(xValue,dispatchThreadID.y -1 +i);
		}
	}
	
	{
		int yValue = max(0,dispatchThreadID.y-1);
		for(int i=0;i<3;++i)
		{
			xyValues[0][i].y =  yValue;
		}
		xyValues[0][1].x = dispatchThreadID.x;
	}
	{
		int yValue = min(gInput.Length.y-1,dispatchThreadID.y+1);
		for(int i=0;i<3;++i)
		{
			xyValues[2][i].y =  yValue;
		}
		xyValues[2][1].x = dispatchThreadID.x;
	}

	float3 vec = (0,0,0);

	for(int i=0;i<9;++i)
	{
		int2 tmpXY = xyValues[i/3][i%3];
		vec += float3(tmpXY,gInput[tmpXY].r) - float3(xyValues[1][1],gInput[xyValues[1][1]].r);
	}

	vec = normalize(vec);
	
	float4 normalValue = float4(vec,0.0f);

	gOutput[dispatchThreadID.xy] = normalValue;

}

