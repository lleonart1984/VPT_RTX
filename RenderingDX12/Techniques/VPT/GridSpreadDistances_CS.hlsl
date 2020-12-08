/// GridSpreadDistances_CS

#include "../CommonGI/Definitions.h"
#include "../CommonGI/Parameters.h"

/// Grid with source values
Texture3D<float>		GridSrc : register(t0);
/// Grid constructed with the new values for each cell
RWTexture3D<float>		GridDst : register(u0);

cbuffer GridInfo : register(b0) {
	int Size;
	float3 Min;
	float3 Max;
}

cbuffer LevelInfo : register(b1) {
	int Level;
}

[numthreads(CS_1D_GROUPSIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	/// for each cell consider the distances in adjacent cells at specific distance (depending on the level)
	/// If all distances are greater than the required distance to spread, the new distance is updated.

	int3 currentCell = int3(DTid.x % Size, DTid.x / Size % Size, DTid.x / (Size * Size));

	int Radius = (int)round(pow(3, Level));
	float RequiredDistance = (Radius - 1) * 0.5;

	float minDistance = 10000;
	for (int bz = -1; bz <= 1; bz++)
		for (int by = -1; by <= 1; by++)
			for (int bx = -1; bx <= 1; bx++)
			{
				int3 adjCell = clamp(int3(bx, by, bz) * (Radius)+currentCell, 0, Size - 1);
				minDistance = min(minDistance, GridSrc[adjCell]);
			}

	if (minDistance >= RequiredDistance)
	{ // can spread
		GridDst[currentCell] = 2 * RequiredDistance + 1 + minDistance;
	}
	else
	{ // can not enlarge with current info
		GridDst[currentCell] = GridSrc[currentCell];
	}
}
