#pragma once

class RetainedSceneLoader : public Technique, public IHasScene {
public:
	gObj<Texture2D>* Textures;
	int TextureCount;
	gObj<Buffer> MaterialBuffer;
	gObj<Buffer> VolMaterialBuffer;
	gObj<Buffer> MaterialIndexBuffer;
	gObj<Buffer> TransformBuffer;
	gObj<Buffer> VertexBuffer;
	gObj<Buffer> ObjectBuffer;

	void UpdateTransforms(gObj<GraphicsManager> manager) {
		manager _copy PtrData(TransformBuffer, Scene->getTransforms());
	}

	void UpdateMaterials(gObj<GraphicsManager> manager) {
		manager _copy PtrData(MaterialBuffer, Scene->getMaterialBuffer());
	}

	void UpdateVolMaterials(gObj<GraphicsManager> manager) {
		manager _copy PtrData(VolMaterialBuffer, Scene->getVolumeMaterialBuffer());
	}

protected:
	void Startup() {
		VertexBuffer = __create StructuredBuffer<SCENE_VERTEX>(Scene->getVertexBufferSize());
		VertexBuffer->SetDebugName(L"Scene Vertices");
		ObjectBuffer = __create StructuredBuffer<int>(Scene->getVertexBufferSize());
		ObjectBuffer->SetDebugName(L"Object Buffer");
		TransformBuffer = __create StructuredBuffer<float3x4>(Scene->getObjectCount());
		TransformBuffer->SetDebugName(L"Transform Buffer");
		MaterialIndexBuffer = __create StructuredBuffer<int>(Scene->getObjectCount());
		MaterialIndexBuffer->SetDebugName(L"Material Index Buffer");
		MaterialBuffer = __create StructuredBuffer<SCENE_MATERIAL>(Scene->getMaterialCount());
		MaterialBuffer->SetDebugName(L"Material Buffer");
		VolMaterialBuffer = __create StructuredBuffer<SCENE_VOLMATERIAL>(Scene->getMaterialCount());
		VolMaterialBuffer->SetDebugName(L"Volume Material Buffer");
		TextureCount = Scene->getTextureCount();
		Textures = new gObj<Texture2D>[TextureCount];
		perform(CreatingAssets);
	}

	void CreatingAssets(gObj<GraphicsManager> manager) {
		// loading scene textures
		for (int i = 0; i < Scene->getTextureCount(); i++)
			manager _load FromData(Textures[i], Scene->getTexture(i));

		// load full vertex buffer of all scene geometries
		manager _copy PtrData(VertexBuffer, Scene->getVertexBuffer());
		
		// For each vertex index, object id is the object index.
		manager _copy PtrData(ObjectBuffer, Scene->getObjectsIds());

		// Transform for each object index
		UpdateTransforms(manager);

		// Material index for each object index
		manager _copy PtrData(MaterialIndexBuffer, Scene->getMaterialIndicesBuffer());

		// Material data for each material index
		UpdateMaterials(manager);

		// Volume material used
		UpdateVolMaterials(manager);
	}

	void Frame() {
		if (this->IsMaterialDirty)
		{
			perform(UpdateMaterials);
			this->IsMaterialDirty = false;
		}
		if (this->IsVolMaterialDirty)
		{
			perform(UpdateVolMaterials);
			this->IsVolMaterialDirty = false;
		}
		if (this->IsTransformsDirty)
		{
			perform(UpdateTransforms);
			this->IsTransformsDirty = false;
		}
	}
};