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
		manager _copy PtrData(TransformBuffer, Scene->Transforms());
	}

	void UpdateMaterials(gObj<GraphicsManager> manager) {
		manager _copy PtrData(MaterialBuffer, &Scene->Materials().first());
	}

protected:
	void Startup() {
		VertexBuffer = __create StructuredBuffer<SCENE_VERTEX>(Scene->VerticesCount());
		VertexBuffer->SetDebugName(L"Scene Vertices");
		ObjectBuffer = __create StructuredBuffer<int>(Scene->VerticesCount());
		ObjectBuffer->SetDebugName(L"Object Buffer");
		TransformBuffer = __create StructuredBuffer<float4x4>(Scene->ObjectsCount());
		TransformBuffer->SetDebugName(L"Transform Buffer");
		MaterialIndexBuffer = __create StructuredBuffer<int>(Scene->ObjectsCount());
		MaterialIndexBuffer->SetDebugName(L"Material Index Buffer");
		MaterialBuffer = __create StructuredBuffer<SCENE_MATERIAL>(Scene->Materials().size());
		MaterialBuffer->SetDebugName(L"Material Buffer");
		TextureCount = Scene->Textures().size();
		Textures = new gObj<Texture2D>[TextureCount];
		perform(CreatingAssets);
	}

	void CreatingAssets(gObj<GraphicsManager> manager) {
		// loading scene textures
		for (int i = 0; i < Scene->Textures().size(); i++)
			manager _load FromData(Textures[i], Scene->Textures()[i]);

		// load full vertex buffer of all scene geometries
		manager _copy PtrData(VertexBuffer, Scene->Vertices());
		
		// For each vertex index, object id is the object index.
		manager _copy PtrData(ObjectBuffer, Scene->ObjectIds());

		// Transform for each object index
		UpdateTransforms(manager);

		// Material index for each object index
		manager _copy PtrData(MaterialIndexBuffer, Scene->MaterialIndices());

		// Material data for each material index
		UpdateMaterials(manager);
	}
};