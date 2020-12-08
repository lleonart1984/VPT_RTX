#pragma once

#include "../../Techniques/GUI_Traits.h"
#include "../CommonGI/Parameters.h"

struct VST_Technique : public Technique, public IHasScene, public IHasLight, public IHasCamera, public IHasScatteringEvents, public IHasAccumulative {

	~VST_Technique() {}

#pragma region Grid Construction Compute Shaders

	struct Voxelizer : public ComputePipelineBindings {
		void Setup() {
			__set ComputeShader(ShaderLoader::FromFile(".\\Techniques\\VPT\\GridVoxelization_CS.cso"));
		}

		gObj<Buffer> Vertices;
		gObj<Buffer> OB;
		gObj<Buffer> Transforms;

		gObj<Buffer> Triangles;
		gObj<Texture3D> Head;
		gObj<Buffer> NextBuffer;
		gObj<Buffer> Malloc;

		gObj<Buffer> GridInfo;

		void Globals() {
			SRV(0, Vertices, ShaderType_Any);
			SRV(1, OB, ShaderType_Any);
			SRV(2, Transforms, ShaderType_Any);

			UAV(0, Triangles, ShaderType_Any);
			UAV(1, Head, ShaderType_Any);
			UAV(2, NextBuffer, ShaderType_Any);
			UAV(3, Malloc, ShaderType_Any);

			CBV(0, GridInfo, ShaderType_Any);
		}
	};

	struct InitialDistances : public ComputePipelineBindings {
		void Setup() {
			__set ComputeShader(ShaderLoader::FromFile(".\\Techniques\\VPT\\GridInitialDistances_CS.cso"));
		}

		gObj<Buffer> Vertices;
		gObj<Buffer> OB;
		gObj<Buffer> Transforms;
		gObj<Buffer> Triangles;
		gObj<Texture3D> Head;
		gObj<Buffer> NextBuffer;

		gObj<Texture3D> DistanceField;

		gObj<Buffer> GridInfo;

		void Globals() {
			SRV(0, Vertices, ShaderType_Any);
			SRV(1, OB, ShaderType_Any);
			SRV(2, Transforms, ShaderType_Any);
			SRV(3, Triangles, ShaderType_Any);
			SRV(4, Head, ShaderType_Any);
			SRV(5, NextBuffer, ShaderType_Any);

			UAV(0, DistanceField, ShaderType_Any);

			CBV(0, GridInfo, ShaderType_Any);
		}
	};

	struct Spreading : public ComputePipelineBindings {
		void Setup() {
			__set ComputeShader(ShaderLoader::FromFile(".\\Techniques\\VPT\\GridSpreadDistances_CS.cso"));
		}

		gObj<Buffer> GridSrc;
		gObj<Buffer> GridDst;
		gObj<Buffer> GridInfo;
		int LevelInfo;

		void Globals() {
			UAV(0, GridDst, ShaderType_Any);
			SRV(0, GridSrc, ShaderType_Any);
			CBV(0, GridInfo, ShaderType_Any);
		}

		void Locals() {
			CBV(1, LevelInfo, ShaderType_Any);
		}
	};

#pragma endregion

	// DXR pipeline for pathtracing stage
	struct DXR_PT_Pipeline : public RTPipelineManager {
		gObj<RayGenerationHandle> PTMainRays;
		gObj<MissHandle> EnvironmentMap;
		gObj<ClosestHitHandle> PTScattering;
		gObj<HitGroupHandle> PTMaterial;

		class DXR_RT_IL : public DXIL_Library<DXR_PT_Pipeline> {
			void Setup() {
				__load DXIL(ShaderLoader::FromFile(".\\Techniques\\VPT\\VST_RT.cso"));

				__load Shader(Context()->PTMainRays, L"PTMainRays");
				__load Shader(Context()->EnvironmentMap, L"EnvironmentMap");
				__load Shader(Context()->PTScattering, L"PTScattering");
			}
		};
		gObj<DXR_RT_IL> _Library;

		struct DXR_RT_Program : public RTProgram<DXR_PT_Pipeline> {
			void Setup() {
				__set Payload(24); // a float3 + 3 ints
				__set StackSize(1); // No recursion needed!
				__load Shader(Context()->PTMainRays);
				__load Shader(Context()->EnvironmentMap);
				__create HitGroup(Context()->PTMaterial, Context()->PTScattering, nullptr, nullptr);
			}

			gObj<SceneOnGPU> Scene;
			gObj<Buffer> Vertices;
			gObj<Buffer> Transforms;
			gObj<Buffer> Materials;
			gObj<Buffer> VolMaterials;

			gObj<Texture2D>* Textures;
			int TextureCount;

			gObj<Buffer> LightingCB;
			int2 Frame;
			gObj<Buffer> ProjToWorld;
			gObj<Buffer> GridInfo;

			gObj<Texture3D> Grid;

			gObj<Texture2D> Output;
			gObj<Texture2D> Accum;

			struct ObjInfo {
				int TriangleOffset;
				int TransformIndex;
				int MaterialIndex;
			} CurrentObjectInfo;

			void Globals() {
				UAV(0, Output, 1);
				UAV(1, Accum, 1);

				ADS(0, Scene, 0);
				SRV(1, Grid);
				
				SRV(0, Vertices, 1);
				SRV(1, Transforms, 1);
				SRV(2, Materials, 1);
				SRV(3, VolMaterials, 1);

				SRV_Array(4, Textures, TextureCount, 1);

				Static_SMP(0, Sampler::Linear(), 1);

				CBV(0, Frame, 1); // Accumulation info (current frame and accIsComplexity)

				CBV(0, LightingCB);
				CBV(1, ProjToWorld);
				CBV(2, GridInfo);
			}

			void HitGroup_Locals() {
				CBV(3, CurrentObjectInfo);
			}
		};
		gObj<DXR_RT_Program> _Program;

		void Setup() override
		{
			__load Library(_Library);
			__load Program(_Program);
		}
	};

	// Scene loading process to retain scene on the GPU
	gObj<RetainedSceneLoader> sceneLoader;

	gObj<Voxelizer> voxelizer;
	gObj<InitialDistances> initializing;
	gObj<Spreading> spreading;
	gObj<DXR_PT_Pipeline> dxrPTPipeline;

	gObj<Texture3D> Grid;
	gObj<Texture3D> GridTmp;

	int gridSize = 512;

	struct GridInfo {
		int Size;
		float3 Min;
		float3 Max;
	};

	void Startup() {

		// Load and setup scene loading process
		sceneLoader = new RetainedSceneLoader();
		sceneLoader->SetScene(this->Scene);
		__load Subprocess(sceneLoader);

		wait_for(signal(flush_all_to_gpu));

		__load Pipeline(voxelizer);
		__load Pipeline(initializing);
		__load Pipeline(spreading);
		__load Pipeline(dxrPTPipeline);

		/// Loads a static scene for further ray-tracing
		VB = __create GenericBuffer<SCENE_VERTEX>(D3D12_RESOURCE_STATE_GENERIC_READ, Scene->getVertexBufferSize(), CPU_WRITE_GPU_READ);

		// Load assets
		perform(CreatingAssets);

		perform(CreateSceneOnGPU);
	}

	void SetScene(gObj<CA4G::Scene> scene) {
		IHasScene::SetScene(scene);
		if (sceneLoader != nullptr)
			sceneLoader->SetScene(scene);
	}

	void CreatingAssets(gObj<CopyingManager> manager) {

#pragma region DXR Pathtracing Pipeline Objects
		dxrPTPipeline->_Program->TextureCount = sceneLoader->TextureCount;
		dxrPTPipeline->_Program->Textures = sceneLoader->Textures;
		dxrPTPipeline->_Program->Materials = sceneLoader->MaterialBuffer;
		dxrPTPipeline->_Program->VolMaterials = sceneLoader->VolMaterialBuffer;
		dxrPTPipeline->_Program->Vertices = sceneLoader->VertexBuffer;
		dxrPTPipeline->_Program->Transforms = sceneLoader->TransformBuffer;
		dxrPTPipeline->_Program->LightingCB = __create ConstantBuffer<Lighting>();
		dxrPTPipeline->_Program->ProjToWorld = __create ConstantBuffer<float4x4>();

		dxrPTPipeline->_Program->Output = __create DrawableTexture2D<RGBA>(render_target->Width, render_target->Height);
		dxrPTPipeline->_Program->Accum = __create DrawableTexture2D<float4>(render_target->Width, render_target->Height);
#pragma endregion

		voxelizer->Vertices = sceneLoader->VertexBuffer;
		voxelizer->Transforms = sceneLoader->TransformBuffer;
		voxelizer->OB = sceneLoader->ObjectBuffer;

		voxelizer->Triangles = __create RWStructuredBuffer<int>(10000000);
		voxelizer->NextBuffer = __create RWStructuredBuffer<int>(10000000);
		voxelizer->Malloc = __create RWStructuredBuffer<int>(1);
		voxelizer->Head = __create DrawableTexture3D<int>(gridSize, gridSize, gridSize);

		Grid = __create DrawableTexture3D<float>(gridSize, gridSize, gridSize);
		GridTmp = __create DrawableTexture3D<float>(gridSize, gridSize, gridSize);

		voxelizer->GridInfo = __create ConstantBuffer<GridInfo>();

		initializing->GridInfo = voxelizer->GridInfo;
		initializing->Triangles = voxelizer->Triangles;
		initializing->NextBuffer = voxelizer->NextBuffer;
		initializing->Head = voxelizer->Head;

		initializing->Vertices = sceneLoader->VertexBuffer;
		initializing->Transforms = sceneLoader->TransformBuffer;
		initializing->OB = sceneLoader->ObjectBuffer;

		dxrPTPipeline->_Program->GridInfo = voxelizer->GridInfo;
		spreading->GridInfo = voxelizer->GridInfo;

		float3 minim, maxim;
		sceneLoader->Scene->computeSceneAABB(minim, maxim);

		float3 dim = maxim - minim;
		float maxDim = max(dim.x, max(dim.y, dim.z));

		manager _copy ValueData(voxelizer->GridInfo, GridInfo{
			gridSize,
			minim - float3(0.01, 0.01, 0.01),
			minim + float3(maxDim, maxDim, maxDim) + float3(0.02, 0.02, 0.02)
			});
	}

	gObj<Buffer> VB;
	void CreateSceneOnGPU(gObj<DXRManager> manager) {

		// load full vertex buffer of all scene geometries
		manager _copy PtrData(VB, Scene->getVertexBuffer());

		auto instances = manager _create Instances();

		for (int i = 0; i < Scene->getObjectCount(); i++)
		{
			auto geometries = manager _create TriangleGeometries();
			geometries _set VertexBuffer(VB, SCENE_VERTEX::Layout());

			// Create a geometry for each obj loaded group
			auto sceneObj = Scene->getObject(i);
			geometries _load Geometry(sceneObj.startVertex, sceneObj.vertexesCount);

			gObj<GeometriesOnGPU> geometriesOnGPU;
			geometriesOnGPU = geometries _create BakedGeometry();

			// load current object as an instance.
			instances _load Instance(geometriesOnGPU, 255U, i, i, *sceneObj.Transform);
		}
		dxrPTPipeline->_Program->Scene = instances _create BakedScene();
	}

	void Frame() {
		sceneLoader->IsMaterialDirty = this->IsMaterialDirty;
		sceneLoader->IsTransformsDirty = this->IsTransformsDirty;
		sceneLoader->IsVolMaterialDirty = this->IsVolMaterialDirty;
		ExecuteFrame(sceneLoader); // Needed to update transform, materials or volume materials if needed.

		static bool first = true;

		if (first) { // if dynamic scene this need to be done everyframe
			perform(BuildGrid);
			first = false;
		}

		perform(Pathtracing);
	}

	void BuildGrid(gObj<DXRManager> manager) {
		auto compute = manager.Dynamic_Cast<ComputeManager>();

		compute _clear UAV(voxelizer->Head, uint4(-1));
		compute _set Pipeline(voxelizer);
		compute _dispatch Threads((int)ceil(sceneLoader->VertexBuffer->ElementCount / 3.0 / CS_1D_GROUPSIZE));

		initializing->DistanceField = Grid;
		compute _set Pipeline(initializing);
		compute _dispatch Threads(gridSize * gridSize * gridSize / CS_1D_GROUPSIZE);

		for (int level = 0; level < ceil(log(gridSize) / log(2)); level++)
		{
			spreading->GridDst = GridTmp;
			spreading->GridSrc = Grid;
			compute _set Pipeline(spreading);

			spreading->LevelInfo = level;
			compute _dispatch Threads(gridSize * gridSize * gridSize / CS_1D_GROUPSIZE);

			Grid = spreading->GridDst;
			GridTmp = spreading->GridSrc;
		}

		dxrPTPipeline->_Program->Grid = Grid;
	}

	float4x4 view, proj;

	void Pathtracing(gObj<DXRManager> manager) {

		static int FrameIndex = 0;

		auto rtProgram = dxrPTPipeline->_Program;

		if (CameraIsDirty || LightSourceIsDirty)
		{
			Camera->GetMatrices(render_target->Width, render_target->Height, view, proj);

			// Update Light intensity and position
			manager _copy ValueData(rtProgram->LightingCB, Lighting{
					Light->Position, 0,
					Light->Intensity, 0,
					Light->Direction, 0
				});

			FrameIndex = 0;
			manager _clear UAV(rtProgram->Output, float4(0, 0, 0, 0));
			manager _clear UAV(rtProgram->Accum, float4(0, 0, 0, 0));

			manager _copy ValueData(rtProgram->ProjToWorld, mul(view, proj).getInverse());
		}
		rtProgram->Frame = int2(FrameIndex, this->CountSteps ? 1 : 0);

		// Set DXR Pipeline
		manager _set Pipeline(dxrPTPipeline);
		// Activate program with main shaders
		manager _set Program(rtProgram);

		int startTriangle;

#pragma region Single Pathtrace Stage

		static bool firstTime = true;

		if (firstTime) {
			// Set Miss in slot 0
			manager _set Miss(dxrPTPipeline->EnvironmentMap, 0);

			// Setup a simple hitgroup per object
			// each object knows the offset in triangle buffer
			// and the material index for further light scattering
			startTriangle = 0;
			for (int i = 0; i < Scene->getObjectCount(); i++)
			{
				auto sceneObject = Scene->getObject(i);

				rtProgram->CurrentObjectInfo.TriangleOffset = startTriangle;
				rtProgram->CurrentObjectInfo.TransformIndex = i;
				rtProgram->CurrentObjectInfo.MaterialIndex = Scene->getMaterialIndicesBuffer()[i];

				manager _set HitGroup(dxrPTPipeline->PTMaterial, i);

				startTriangle += sceneObject.vertexesCount / 3;
			}

			firstTime = false;
		}

		// Setup a raygen shader
		manager _set RayGeneration(dxrPTPipeline->PTMainRays);

		rtProgram->Frame = FrameIndex;

		if (FrameIndex < StopFrame || StopFrame == 0) {

			CurrentFrame = FrameIndex;

			// Dispatch primary rays
			manager _dispatch Rays(render_target->Width, render_target->Height);

			FrameIndex++;
		}

		manager _copy All(render_target, rtProgram->Output);

#pragma endregion
	}
};