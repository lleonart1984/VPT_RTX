#pragma once

#include "..\Common\RetainedSceneLoader.h"
#include "..\Common\ConstantBuffers.h"

class RetainedPipeline : public GraphicsPipelineBindings {
public:
	gObj<Texture2D> renderTarget;
	gObj<Texture2D> depthBuffer;

	// CBs
	gObj<Buffer> globals;
	gObj<Buffer> lighting;

	// SRVs
	gObj<Buffer> vertices;
	gObj<Buffer> objectIds;

	gObj<Buffer> transforms;
	gObj<Buffer> materialIndices;

	gObj<Buffer> materials;
	
	gObj<Texture2D>* Textures = nullptr;
	int TextureCount;

protected:
	void Setup() {
		__set VertexShader(ShaderLoader::FromFile(".\\Techniques\\RetainedBasic\\RetainedRendering_VS.cso"));
		__set PixelShader(ShaderLoader::FromFile(".\\Techniques\\RetainedBasic\\RetainedRendering_PS.cso"));
		__set InputLayout({});
		__set DepthTest();
	}

	void Globals()
	{
		RTV(0, renderTarget);
		DBV(depthBuffer);

		Static_SMP(0, Sampler::Linear(), ShaderType_Pixel);

		CBV(0, lighting, ShaderType_Pixel);
		SRV(0, materialIndices, ShaderType_Pixel);
		SRV(1, materials, ShaderType_Pixel);
		CBV(0, globals, ShaderType_Vertex);
		SRV(0, vertices, ShaderType_Vertex);
		SRV(1, objectIds, ShaderType_Vertex);
		SRV(2, transforms, ShaderType_Vertex);
		SRV_Array(2, Textures, TextureCount, ShaderType_Pixel);
	}
};


class RetainedBasicRenderer : public Technique, public IHasScene, public IHasCamera, public IHasLight, public IHasBackcolor {
public:
	gObj<RetainedSceneLoader> sceneLoader;

	gObj<Texture2D> depthBuffer;
	gObj<RetainedPipeline> pipeline;
	gObj<Buffer> globalsCB;
	gObj<Buffer> lightingCB;

protected:
	void SetScene(gObj<CA4G::Scene> scene)
	{
		IHasScene::SetScene(scene);
		if (sceneLoader != nullptr)
			sceneLoader->SetScene(scene);
	}

	void Startup() {
		if (sceneLoader == nullptr) // no loader has be bound
		{
			sceneLoader = new RetainedSceneLoader();
			sceneLoader->SetScene(this->Scene);
			__load Subprocess(sceneLoader);
		}

		// Load and setup pipeline resource
		__load Pipeline(pipeline);

		// Create depth buffer resource
		depthBuffer = __create DepthBuffer(render_target->Width, render_target->Height);

		// Create globals VS constant buffer
		globalsCB = __create ConstantBuffer<Globals>();
		lightingCB = __create ConstantBuffer<Lighting>();

		pipeline->depthBuffer = depthBuffer;
		pipeline->globals = globalsCB;
		pipeline->lighting = lightingCB;

		pipeline->TextureCount = sceneLoader->TextureCount;
		pipeline->Textures = sceneLoader->Textures;
		pipeline->vertices = sceneLoader->VertexBuffer;
		pipeline->objectIds = sceneLoader->ObjectBuffer;
		pipeline->materialIndices = sceneLoader->MaterialIndexBuffer;
		pipeline->materials = sceneLoader->MaterialBuffer;
		pipeline->transforms = sceneLoader->TransformBuffer;
	}

	void Graphics(gObj<GraphicsManager> manager) {
		
		pipeline->renderTarget = render_target;

		float4x4 view, proj;
		Camera->GetMatrices(render_target->Width, render_target->Height, view, proj);
		manager _copy ValueData(globalsCB, Globals{ proj, view });
		manager _copy ValueData(lightingCB, Lighting{
					mul(float4(Light->Position, 1), view).getXYZHomogenized(), 0,
					Light->Intensity
				});

		manager _clear RT(render_target, float4(Backcolor, 1));
		manager _clear Depth(depthBuffer, 1);

		manager _set Viewport(render_target->Width, render_target->Height);
		manager _set Pipeline(pipeline);

		manager _dispatch Triangles(sceneLoader->VertexBuffer->ElementCount);
	}

	void Frame() {
		perform(Graphics);
	}
};