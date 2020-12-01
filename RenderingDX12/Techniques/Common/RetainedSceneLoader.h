#pragma once

class RetainedSceneLoader : public Technique, public IHasScene {
public:
	gObj<Texture2D>* Textures;
	int TextureCount;
	gObj<Buffer> MaterialBuffer;
	gObj<Buffer> MaterialIndexBuffer;
	gObj<Buffer> TransformBuffer;
	gObj<Buffer> VertexBuffer;
	gObj<Buffer> ObjectBuffer;

	void UpdateTransforms(gObj<GraphicsManager> manager) {
		manager gCopy PtrData(TransformBuffer, Scene->Transforms());
	}

	void UpdateMaterials(gObj<GraphicsManager> manager) {
		manager gCopy PtrData(MaterialBuffer, &Scene->Materials().first());
	}

protected:
	void Startup() {
		perform(CreatingAssets);
	}

	void CreatingAssets(gObj<GraphicsManager> manager) {
		// loading scene textures
		TextureCount = Scene->Textures().size();
		Textures = new gObj<Texture2D>[TextureCount];
		for (int i = 0; i < Scene->Textures().size(); i++)
			manager gLoad FromData(Textures[i], Scene->Textures()[i]);

		// load full vertex buffer of all scene geometries
		gBind(VertexBuffer) _ gCreate StructuredBuffer<SCENE_VERTEX>(Scene->VerticesCount());
		VertexBuffer->SetDebugName(L"Scene Vertices");
		manager gCopy PtrData(VertexBuffer, Scene->Vertices());
		
		// For each vertex index, object id is the object index.
		gBind(ObjectBuffer) _ gCreate StructuredBuffer<int>(Scene->VerticesCount());
		ObjectBuffer->SetDebugName(L"Object Buffer");
		manager gCopy PtrData(ObjectBuffer, Scene->ObjectIds());

		// Transform for each object index
		gBind(TransformBuffer) _ gCreate StructuredBuffer<float4x4>(Scene->ObjectsCount());
		TransformBuffer->SetDebugName(L"Transform Buffer");
		UpdateTransforms(manager);

		// Material index for each object index
		gBind(MaterialIndexBuffer) _ gCreate StructuredBuffer<int>(Scene->ObjectsCount());
		MaterialIndexBuffer->SetDebugName(L"Material Index Buffer");
		manager gCopy PtrData(MaterialIndexBuffer, Scene->MaterialIndices());

		// Material data for each material index
		gBind(MaterialBuffer) _ gCreate StructuredBuffer<SCENE_MATERIAL>(Scene->Materials().size());
		MaterialBuffer->SetDebugName(L"Material Buffer");
		UpdateMaterials(manager);
	}
};