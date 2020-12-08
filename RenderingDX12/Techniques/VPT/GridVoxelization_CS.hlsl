/// GRID VOXELIZATION

// Creates a grid of linked list. Each cell contains a linked list to all triangles intersecting

#include "../CommonGI/Definitions.h"
#include "../CommonGI/Parameters.h"

// Scene description (Only geometric information needed)
StructuredBuffer<Vertex> vertices : register(t0);
StructuredBuffer<int> OB : register(t1); // Object buffers
StructuredBuffer<float4x3> Transforms : register(t2); // World transforms

// Triangles indices for each node of the linked lists
RWStructuredBuffer<int> TriangleIndices : register(u0); // Linked list values (references to the triangles)
// Grid of all linked lists' head
// Per cell linked list head. This buffer should be filled with -1 (Null references) before starting.
RWTexture3D<int> Head : register(u1);
// For each node represents the next node
RWStructuredBuffer<int> Next : register(u2); // Per linked lists next references....
// Buffer used to allocate node references with interlocked operation
RWStructuredBuffer<int> Malloc : register(u3); // incrementer buffer

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

// Converts a world position into a grid space (0,0,0) - (Size, Size, Size)
float3 FromPositionToCell(float3 P, float4x3 world) {
	return (mul(float4(P, 1), world) - Min) * Size / (Max - Min);
}

[numthreads(CS_1D_GROUPSIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	uint NumberOfVertices, stride;
	vertices.GetDimensions(NumberOfVertices, stride);

	if (DTid.x >= TriangleCount)
		return;

	int triangleIndex = TriangleStart + DTid.x;

	float4x3 world = UseTransform ? Transforms[OB[triangleIndex * 3]] : float4x3(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0);

	float3 c1 = FromPositionToCell(vertices[triangleIndex * 3 + 0].P, world);
	float3 c2 = FromPositionToCell(vertices[triangleIndex * 3 + 1].P, world);
	float3 c3 = FromPositionToCell(vertices[triangleIndex * 3 + 2].P, world);

	float3 P = c1;
	float3 N = cross(c3 - c1, c2 - c1);

	// Determining range of cells that cover the triangle.
	int3 maxCell = max(c1, max(c2, c3));// +0.00001;
	int3 minCell = min(c1, min(c2, c3));// -0.00001;

	// 8 evals for each current cell corners with the plane.
	float2x4 evals = float2x4(
		dot(minCell + float3(0, 0, 0) - P, N), dot(minCell + float3(1, 0, 0) - P, N), dot(minCell + float3(0, 1, 0) - P, N), dot(minCell + float3(1, 1, 0) - P, N),
		dot(minCell + float3(0, 0, 1) - P, N), dot(minCell + float3(1, 0, 1) - P, N), dot(minCell + float3(0, 1, 1) - P, N), dot(minCell + float3(1, 1, 1) - P, N)
		);

	for (int cz = minCell.z; cz <= maxCell.z; cz++)
		for (int cy = minCell.y; cy <= maxCell.y; cy++)
			for (int cx = minCell.x; cx <= maxCell.x; cx++)
			{
				int3 currentCell = int3(cx, cy, cz);
				float2x4 currentEvals = evals + dot(N, currentCell - minCell);
				// Intersection occurs if there is a case of positive evaluation and negative evaluation.
				if (!all(currentEvals <= 0) && !all(currentEvals >= 0)) // cell intersects triangle
				{
					int currentReference;
					InterlockedAdd(Malloc[0], 1, currentReference);
					TriangleIndices[currentReference] = triangleIndex;
					InterlockedExchange(Head[currentCell], currentReference, Next[currentReference]);
				}
			}
}