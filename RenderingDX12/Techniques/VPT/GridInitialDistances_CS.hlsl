/// GridInitialDistances_CS

#include "../CommonGI/Definitions.h"
#include "../CommonGI/Parameters.h"
#include "../Common/Distances.h"

StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<int> OB : register(t1); // Object buffers
StructuredBuffer<float4x3> Transforms : register(t2); // World transforms
StructuredBuffer<int> TriangleIndices : register(t3); // Linked list values (references to the triangles)
Texture3D<int> Head : register(t4); // Per cell linked list head
StructuredBuffer<int> Next : register(t5); // Per linked lists next references....


/// Initial distance field grid with only distances to adjacent cells.
/// If all adjacent cells are empty, then the distance will be 0.9999, representing a coverage of almost all adjacent cell.
/// The distance field is representing the external distance for the current cell... 
/// i.e. distance 0 means the cell is empty but some adjacent cell has a triangle close to some of the borders of the current cell.
RWTexture3D<float> DistanceField : register(u0);

cbuffer GridInfo : register(b0) {
	int Size;
	float3 Min;
	float3 Max;
}

cbuffer Filter : register(b1) {
	int TriangleStart;
	int TriangleCount;
	bool UseTransform;
}

/// Gets the triangle in Grid space (0,0,0)-(Size, Size, Size)
void GetTriangle(int triangleIndex, inout float3 t[3]) {
	float4x3 world = UseTransform ? Transforms[OB[triangleIndex * 3]] : float4x3(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);

	t[0] = (mul(float4(vertices[triangleIndex * 3 + 0].P, 1), world) - Min) * Size / (Max - Min);
	t[2] = (mul(float4(vertices[triangleIndex * 3 + 2].P, 1), world) - Min) * Size / (Max - Min);
	t[1] = (mul(float4(vertices[triangleIndex * 3 + 1].P, 1), world) - Min) * Size / (Max - Min);
}

[numthreads(CS_1D_GROUPSIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	int3 currentCell = int3(DTid.x % Size, DTid.x / Size % Size, DTid.x / (Size * Size));

	if (Head[currentCell] != -1) // not empty cell
	{
		DistanceField[currentCell] = -1; // Negative distance values for occupied cells.
		return;
	}

	float3 corners[2][2][2];
	for (int cz = 0; cz < 2; cz++)
		for (int cy = 0; cy < 2; cy++)
			for (int cx = 0; cx < 2; cx++)
				corners[cx][cy][cz] = currentCell + float3(cx, cy, cz);


	float dist = 0.99999;

	for (int bz = -1; bz <= 1; bz++)
		for (int by = -1; by <= 1; by++)
			for (int bx = -1; bx <= 1; bx++)
			{
				int3 b = int3(bx, by, bz);

				int3 adjCell = clamp(currentCell + b, 0, Size - 1);

				int type = abs(bz) + abs(by) + abs(bx);

				if (type == 3) // corners
				{
					int currentTriangle = Head[adjCell];
					while (currentTriangle != -1) {

						float3 t[3];
						GetTriangle(TriangleIndices[currentTriangle], t);

						dist = min(dist, distanceP2T(corners[(bx + 1) / 2][(by + 1) / 2][(bz + 1) / 2], t[0], t[1], t[2]));

						currentTriangle = Next[currentTriangle];
					}
				}
				if (type == 2) // edges (bx == 0 || by == 0 || bz == 0)
				{
					int3 planeAxis = 1 - abs(b);
					int3 mask = abs(b);

					int3 coord0 = (b + 1) / 2;
					int3 coord1 = coord0 * mask + planeAxis;

					float3 edge[2] = { corners[coord0.x][coord0.y][coord0.z], corners[coord1.x][coord1.y][coord1.z] };

					int currentTriangle = Head[adjCell];
					while (currentTriangle != -1) {

						float3 t[3];
						GetTriangle(TriangleIndices[currentTriangle], t);

						dist = min(dist, distanceS2T(edge[0], edge[1], t[0], t[1], t[2]));

						currentTriangle = Next[currentTriangle];
					}
				}
				if (type == 1)
				{
					float3 N = b;
					float3 C = currentCell + ((b + 1) / 2); // intentionally int division
					float3 B = abs(bz) == 1 ? float3(1, 0, 0) : float3(0, 0, 1);
					float3 T = abs(cross(B, N)); // TODO: improve this!

					int currentTriangle = Head[adjCell];
					while (currentTriangle != -1) {

						float3 t[3];
						GetTriangle(TriangleIndices[currentTriangle], t);

						dist = min(dist, distanceQ2T(C, B, T, N, t[0], t[1], t[2]));

						currentTriangle = Next[currentTriangle];
					}
				}
			}

	DistanceField[currentCell] = dist; // Value between 0..0.99999 indicating the safe distance in a cell regarding the adjacents.
}
