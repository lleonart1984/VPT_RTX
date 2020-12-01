#include "ca4G_Private.h"


namespace CA4G {

	void RTPipelineManager::__OnSet(DXRManager* manager)
	{
		ID3D12DescriptorHeap* heaps[] = { this->manager->descriptors->gpu_csu->getInnerHeap(), this->manager->descriptors->gpu_smp->getInnerHeap() };

		if (manager->fallbackCmdList)
			manager->fallbackCmdList->SetDescriptorHeaps(2, heaps);
		else
			manager->cmdList->SetDescriptorHeaps(2, heaps);
	}

	void IRTProgram::UpdateRayGenLocals(DXRManager* cmdList, gObj<RayGenerationHandle> shader) {
		// Get shader identifier
		byte* shaderID;
		int shaderIDSize;
		if (manager->fallbackDevice != nullptr)
			shaderIDSize = manager->fallbackDevice->GetShaderIdentifierSize();
		else // DirectX Raytracing
			shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		byte* shaderRecordStart = raygen_shaderTable->GetShaderRecordStartAddress(0);
		memcpy(shaderRecordStart, shader->cachedShaderIdentifier, shaderIDSize);
		if (!raygen_locals.isNull())
			BindLocalsOnShaderTable(raygen_locals, shaderRecordStart + shaderIDSize);
	}

	void IRTProgram::UpdateMissLocals(DXRManager* cmdList, gObj<MissHandle> shader, int index)
	{
		// Get shader identifier
		void* shaderID;
		int shaderIDSize;
		int shaderRecordSize;
		if (manager->fallbackDevice != nullptr)
			shaderIDSize = manager->fallbackDevice->GetShaderIdentifierSize();
		else // DirectX Raytracing
			shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		shaderRecordSize = miss_shaderTable->Stride;// shaderIDSize + (miss_locals.isNull() ? 0 : miss_locals->rootSize);

		byte* shaderRecordStart = miss_shaderTable->GetShaderRecordStartAddress(index);
		memcpy(shaderRecordStart, shader->cachedShaderIdentifier, shaderIDSize);
		if (!miss_locals.isNull())
			BindLocalsOnShaderTable(miss_locals, shaderRecordStart + shaderIDSize);
	}

	void IRTProgram::UpdateHitGroupLocals(DXRManager* cmdList, gObj<HitGroupHandle> shader, int index) {
		// Get shader identifier
		byte* shaderID;
		int shaderIDSize;
		int shaderRecordSize;
		if (manager->fallbackDevice != nullptr)
			shaderIDSize = manager->fallbackDevice->GetShaderIdentifierSize();
		else // DirectX Raytracing
			shaderIDSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		shaderRecordSize = group_shaderTable->Stride;// shaderIDSize + (hitGroup_locals.isNull() ? 0 : hitGroup_locals->rootSize);

		byte* shaderRecordStart = group_shaderTable->GetShaderRecordStartAddress(index);
		memcpy(shaderRecordStart, shader->cachedShaderIdentifier, shaderIDSize);
		if (!hitGroup_locals.isNull())
			BindLocalsOnShaderTable(hitGroup_locals, shaderRecordStart + shaderIDSize);
	}

	void IRTProgram::BindOnGPU(DXRManager* manager, gObj<BindingsHandle> globals) {
		DX_CommandList cmdList = manager->cmdList;
		DX_FallbackCommandList fallbackCmdList = manager->fallbackCmdList;
		// Foreach bound slot
		for (int i = 0; i < globals->csuBindings.size(); i++)
		{
			auto binding = globals->csuBindings[i];

			switch (binding.Root_Parameter.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				cmdList->SetComputeRoot32BitConstants(i, binding.Root_Parameter.Constants.Num32BitValues,
					binding.ConstantData.ptrToConstant, 0);
				break;
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{
#pragma region DESCRIPTOR TABLE
				// Gets the range length (if bound an array) or 1 if single.
				int count = binding.DescriptorData.ptrToCount == nullptr ? 1 : *binding.DescriptorData.ptrToCount;

				// Gets the bound resource if single
				gObj<ResourceView> resource = binding.DescriptorData.ptrToCount == nullptr ? *((gObj<ResourceView>*)binding.DescriptorData.ptrToResourceViewArray) : nullptr;

				// Gets the bound resources if array or treat the resource as a single array case
				gObj<ResourceView>* resourceArray = binding.DescriptorData.ptrToCount == nullptr ? &resource
					: *((gObj<ResourceView>**)binding.DescriptorData.ptrToResourceViewArray);

				// foreach resource in bound array (or single resource treated as array)
				for (int j = 0; j < count; j++)
				{
					// reference to the j-th resource (or bind null if array is null)
					gObj<ResourceView> resource = resourceArray == nullptr ? nullptr : *(resourceArray + j);

					if (!resource)
						// Grant a resource view to create null descriptor if missing resource.
						resource = ResourceView::getNullView(this->manager, binding.DescriptorData.Dimension);
					else
					{
						switch (binding.Root_Parameter.DescriptorTable.pDescriptorRanges[0].RangeType)
						{
						case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
							if (resource->resource->LastUsageState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
							{
								resource->BarrierUAV(cmdList);
								resource->ChangeStateFromTo(cmdList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
							}
							else
								resource->ChangeStateTo(cmdList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
							break;
						case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
							//resource->ChangeStateTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
							if (resource->resource->LastUsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
								resource->BarrierUAV(cmdList);
							else
								resource->ChangeStateToUAV(cmdList);
							break;
						}
					}
					// Gets the cpu handle at not visible descriptor heap for the resource
					D3D12_CPU_DESCRIPTOR_HANDLE handle;
					resource->getCPUHandleFor(binding.Root_Parameter.DescriptorTable.pDescriptorRanges[0].RangeType, handle);

					// Adds the handle of the created descriptor into the src list.
					manager->srcDescriptors.add(handle);
				}
				// add the descriptors range length
				manager->dstDescriptorRangeLengths.add(count);
				int startIndex = this->manager->descriptors->gpu_csu->Malloc(count);
				manager->dstDescriptors.add(this->manager->descriptors->gpu_csu->getCPUVersion(startIndex));
				cmdList->SetComputeRootDescriptorTable(i, this->manager->descriptors->gpu_csu->getGPUVersion(startIndex));
				break; // DESCRIPTOR TABLE
#pragma endregion
			}
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			{
#pragma region DESCRIPTOR CBV
				// Gets the range length (if bound an array) or 1 if single.
				gObj<ResourceView> resource = *((gObj<ResourceView>*)binding.DescriptorData.ptrToResourceViewArray);
				cmdList->SetComputeRootConstantBufferView(i, resource->resource->GetGPUVirtualAddress());
				break; // DESCRIPTOR CBV
#pragma endregion
			}
			case D3D12_ROOT_PARAMETER_TYPE_SRV: // this parameter is used only for top level data structures
			{
#pragma region DESCRIPTOR SRV
				// Gets the range length (if bound an array) or 1 if single.
				gObj<SceneOnGPU> scene = *((gObj<SceneOnGPU>*)binding.SceneData.ptrToScene);

				if (manager->manager->fallbackDevice != nullptr)
				{ // Used Fallback device
					fallbackCmdList->SetTopLevelAccelerationStructure(i, scene->topLevelAccFallbackPtr);
				}
				else
					cmdList->SetComputeRootShaderResourceView(i, scene->topLevelAccDS->resource->GetGPUVirtualAddress());
				break; // DESCRIPTOR CBV
#pragma endregion
			}
			}
		}
	}

	void IRTProgram::BindLocalsOnShaderTable(gObj<BindingsHandle> locals, byte* shaderRecordData) {
		for (int i = 0; i < locals->csuBindings.size(); i++)
		{
			auto binding = locals->csuBindings[i];

			switch (binding.Root_Parameter.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				memcpy(shaderRecordData, binding.ConstantData.ptrToConstant, binding.Root_Parameter.Constants.Num32BitValues * 4);
				shaderRecordData += binding.Root_Parameter.Constants.Num32BitValues * 4;
				break;
			default:
				shaderRecordData += 4 * 4;
			}
		}
	}

	void InstanceCollection::Loading::Instance(gObj<GeometriesOnGPU> geometries, UINT mask, int instanceContribution, UINT instanceID, float4x4 transform)
	{
		manager->usedGeometries->add(geometries);

		if (manager->manager->fallbackDevice != nullptr) {
			D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC d{ };
			FillMat4x3(d.Transform, transform);
			d.InstanceMask = mask;
			d.Flags = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			//d.Flags = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
			d.InstanceContributionToHitGroupIndex = instanceContribution;
			d.AccelerationStructure = geometries->emulatedPtr;
			if (manager->isUpdating)
			{
				d.InstanceID = instanceID == INTSAFE_UINT_MAX ? manager->currentInstance : instanceID;
				manager->fallbackInstances[manager->currentInstance++] = d;
			}
			else
			{
				int index = manager->fallbackInstances->size();
				d.InstanceID = instanceID == INTSAFE_UINT_MAX ? index : instanceID;
				manager->fallbackInstances->add(d);
			}
		}
		else {
			D3D12_RAYTRACING_INSTANCE_DESC d{ };
			d.Flags = D3D12_RAYTRACING_INSTANCE_FLAGS::D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			FillMat4x3(d.Transform, transform);
			d.InstanceMask = mask;
			d.InstanceContributionToHitGroupIndex = instanceContribution;
			d.AccelerationStructure = geometries->bottomLevelAccDS->resource->GetGPUVirtualAddress();
			if (manager->isUpdating) {
				d.InstanceID = instanceID == INTSAFE_UINT_MAX ? manager->currentInstance : instanceID;
				manager->instances[manager->currentInstance++] = d;
			}
			else {
				int index = manager->instances->size();
				d.InstanceID = instanceID == INTSAFE_UINT_MAX ? index : instanceID;
				manager->instances->add(d);
			}
		}
	}

	gObj<GeometriesOnGPU> GeometryCollection::Creating::BakedGeometry(bool allowUpdates, bool preferFastTrace) {

		// creates the bottom level acc ds and emulated gpu pointer if necessary
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = 
			(preferFastTrace ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
			| (allowUpdates ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;
		inputs.NumDescs = manager->geometries->size();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.pGeometryDescs = &manager->geometries->first();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
		if (manager->manager->fallbackDevice != nullptr)
		{
			manager->manager->fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		}
		else // DirectX Raytracing
		{
			manager->manager->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		}

		D3D12_RESOURCE_STATES initialResourceState;
		if (manager->manager->fallbackDevice != nullptr)
		{
			initialResourceState = manager->manager->fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Raytracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}

		gObj<Buffer> scratchBuffer = manager->manager->creating->GenericBuffer<byte>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			prebuildInfo.ScratchDataSizeInBytes, CPU_ACCESS_NONE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		gObj<Buffer> buffer = manager->manager->creating->GenericBuffer<byte>(initialResourceState,
			prebuildInfo.ResultDataMaxSizeInBytes, CPU_ACCESS_NONE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.Inputs = inputs;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer->resource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = buffer->resource->GetGPUVirtualAddress();
		}

		if (manager->manager->fallbackDevice != nullptr)
		{
			ID3D12DescriptorHeap *pDescriptorHeaps[] = {
				manager->manager->descriptors->gpu_csu->getInnerHeap(),
				manager->manager->descriptors->gpu_smp->getInnerHeap() };
			manager->cmdList->fallbackCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
			manager->cmdList->fallbackCmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		}
		else
			manager->cmdList->cmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		buffer->BarrierUAV(manager->cmdList->cmdList);

		gObj<GeometriesOnGPU> result = new GeometriesOnGPU();
		result->bottomLevelAccDS = buffer;
		result->scratchBottomLevelAccDS = scratchBuffer;
		result->geometries = this->manager->geometries->clone();

		if (manager->manager->fallbackDevice != nullptr) {
			// store an emulated gpu pointer via UAV
			UINT numBufferElements = static_cast<UINT>(prebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
			result->emulatedPtr = manager->manager->CreateFallbackWrappedPointer(buffer, numBufferElements);
		}

		return result;
	}
	
	gObj<GeometriesOnGPU> GeometryCollection::Creating::RebuiltGeometry(bool allowUpdates, bool preferFastTrace) {
		
		//if (!manager->isUpdating)
		//	throw CA4GException("Can not rebuild a geometry is being created.");

		// creates the bottom level acc ds and emulated gpu pointer if necessary
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags =
			(preferFastTrace ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD)
			| (allowUpdates ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;
		inputs.NumDescs = manager->geometries->size();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.pGeometryDescs = &manager->geometries->first();

		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.Inputs = inputs;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = this->manager->updatingGeometry->scratchBottomLevelAccDS->resource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = this->manager->updatingGeometry->bottomLevelAccDS->resource->GetGPUVirtualAddress();
		}

		if (manager->manager->fallbackDevice != nullptr)
		{
			ID3D12DescriptorHeap *pDescriptorHeaps[] = {
				manager->manager->descriptors->gpu_csu->getInnerHeap(),
				manager->manager->descriptors->gpu_smp->getInnerHeap() };
			manager->cmdList->fallbackCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
			manager->cmdList->fallbackCmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		}
		else
			manager->cmdList->cmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		manager->updatingGeometry->bottomLevelAccDS->BarrierUAV(manager->cmdList->cmdList);

		return manager->updatingGeometry;
	}

	gObj<GeometriesOnGPU> GeometryCollection::Creating::UpdatedGeometry() {
		if (!manager->isUpdating)
			throw CA4GException("Can not update a geometry is being created.");
		// creates the bottom level acc ds and emulated gpu pointer if necessary
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;
		inputs.NumDescs = manager->geometries->size();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		inputs.pGeometryDescs = &manager->geometries->first();

		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.Inputs = inputs;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = manager->updatingGeometry->scratchBottomLevelAccDS->resource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = manager->updatingGeometry->bottomLevelAccDS->resource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.SourceAccelerationStructureData = manager->updatingGeometry->bottomLevelAccDS->resource->GetGPUVirtualAddress();
		}

		if (manager->manager->fallbackDevice != nullptr)
		{
			ID3D12DescriptorHeap *pDescriptorHeaps[] = {
				manager->manager->descriptors->gpu_csu->getInnerHeap(),
				manager->manager->descriptors->gpu_smp->getInnerHeap() };
			manager->cmdList->fallbackCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
			manager->cmdList->fallbackCmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		}
		else
			manager->cmdList->cmdList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);

		manager->updatingGeometry->bottomLevelAccDS->BarrierUAV(manager->cmdList->cmdList);

		return manager->updatingGeometry;
	}

	void GeometryCollection::PrepareBuffer(gObj<Buffer> bufferForGeometry) {
		bufferForGeometry->BarrierUAV(cmdList->cmdList);
		bufferForGeometry->ChangeStateTo(cmdList->cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
	}

	void ProceduralGeometryCollection::Setting::AABBs(gObj<Buffer> aabbs) {
		aabbs->ChangeStateTo(manager->cmdList->cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);
		manager->boundAABBs = aabbs;
	}

	gObj<SceneOnGPU> InstanceCollection::Creating::BakedScene(bool allowUpdate, bool preferFastTrace) {
		// Bake scene using instance buffer and generate the top level DS
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = 
			(allowUpdate ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE) |
			(preferFastTrace ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD);
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;

		if (manager->manager->fallbackDevice != nullptr)
			inputs.NumDescs = manager->fallbackInstances->size();
		else
			inputs.NumDescs = manager->instances->size();

		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		if (manager->manager->fallbackDevice != nullptr)
			manager->manager->fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
		else // DirectX Raytracing
			manager->manager->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

		gObj<Buffer> scratchBuffer = manager->manager->creating->GenericBuffer<byte>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			prebuildInfo.ScratchDataSizeInBytes, CPU_ACCESS_NONE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		D3D12_RESOURCE_STATES initialResourceState;
		if (manager->manager->fallbackDevice != nullptr)
		{
			initialResourceState = manager->manager->fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Raytracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}

		gObj<Buffer> buffer = manager->manager->creating->GenericBuffer<byte>(initialResourceState,
			prebuildInfo.ResultDataMaxSizeInBytes, CPU_ACCESS_NONE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		gObj<Buffer> instanceBuffer;

		if (manager->manager->fallbackDevice != nullptr) {
			instanceBuffer = manager->manager->creating->GenericBuffer<byte>(D3D12_RESOURCE_STATE_GENERIC_READ,
				sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC)*manager->fallbackInstances->size(), CPU_WRITE_GPU_READ, D3D12_RESOURCE_FLAG_NONE);
			instanceBuffer->resource->UpdateMappedData(0, (void*)&manager->fallbackInstances->first(), sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC)*manager->fallbackInstances->size());
		}
		else {
			instanceBuffer = manager->manager->creating->GenericBuffer<byte>(D3D12_RESOURCE_STATE_GENERIC_READ,
				sizeof(D3D12_RAYTRACING_INSTANCE_DESC)*manager->instances->size(), CPU_WRITE_GPU_READ, D3D12_RESOURCE_FLAG_NONE);
			instanceBuffer->resource->UpdateMappedData(0, (void*)&manager->instances->first(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC)*manager->instances->size());
		}

		// Build acc structure

		// Top Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
		{
			inputs.InstanceDescs = instanceBuffer->resource->internalResource->GetGPUVirtualAddress();
			topLevelBuildDesc.Inputs = inputs;
			topLevelBuildDesc.ScratchAccelerationStructureData = scratchBuffer->resource->GetGPUVirtualAddress();
			topLevelBuildDesc.DestAccelerationStructureData = buffer->resource->GetGPUVirtualAddress();
		}

		if (manager->manager->fallbackDevice != nullptr)
		{
			ID3D12DescriptorHeap *pDescriptorHeaps[] = {
				manager->manager->descriptors->gpu_csu->getInnerHeap(),
				manager->manager->descriptors->gpu_smp->getInnerHeap() };
			manager->cmdList->fallbackCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
			manager->cmdList->fallbackCmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
		}
		else
			manager->cmdList->cmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

		// Valid for Top level ds as well?
		//buffer->ChangeStateToUAV(manager->cmdList->cmdList);

		gObj<SceneOnGPU> result = new SceneOnGPU();

		result->scratchBuffer = scratchBuffer;
		result->topLevelAccDS = buffer;
		result->instancesBuffer = instanceBuffer;
		result->usedGeometries = manager->usedGeometries->clone();
		result->instances = manager->instances->clone();
		result->fallbackInstances = manager->fallbackInstances->clone();
		// Create a wrapped pointer to the acceleration structure.
		if (manager->manager->fallbackDevice != nullptr)
		{
			UINT numBufferElements = static_cast<UINT>(prebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
			result->topLevelAccFallbackPtr = manager->manager->CreateFallbackWrappedPointer(buffer, numBufferElements);
		}

		return result;
	}

	gObj<SceneOnGPU> InstanceCollection::Creating::UpdatedScene() {
		// Bake scene using instance buffer and generate the top level DS
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = buildFlags;

		if (manager->manager->fallbackDevice != nullptr)
			inputs.NumDescs = manager->fallbackInstances->size();
		else
			inputs.NumDescs = manager->instances->size();

		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		gObj<Buffer> instanceBuffer = manager->updatingScene->instancesBuffer;
		if (manager->manager->fallbackDevice != nullptr) {
			instanceBuffer->resource->UpdateMappedData(0, (void*)&manager->fallbackInstances->first(), sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC)*manager->fallbackInstances->size());
		}
		else {
			instanceBuffer->resource->UpdateMappedData(0, (void*)&manager->instances->first(), sizeof(D3D12_RAYTRACING_INSTANCE_DESC)*manager->instances->size());
		}

		// Build acc structure

		// Top Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
		{
			inputs.InstanceDescs = instanceBuffer->resource->internalResource->GetGPUVirtualAddress();
			topLevelBuildDesc.Inputs = inputs;
			topLevelBuildDesc.ScratchAccelerationStructureData = manager->updatingScene->scratchBuffer->resource->GetGPUVirtualAddress();
			topLevelBuildDesc.SourceAccelerationStructureData = manager->updatingScene->topLevelAccDS->resource->GetGPUVirtualAddress();
			topLevelBuildDesc.DestAccelerationStructureData = manager->updatingScene->topLevelAccDS->resource->GetGPUVirtualAddress();
		}

		if (manager->manager->fallbackDevice != nullptr)
		{
			ID3D12DescriptorHeap *pDescriptorHeaps[] = {
				manager->manager->descriptors->gpu_csu->getInnerHeap(),
				manager->manager->descriptors->gpu_smp->getInnerHeap() };
			manager->cmdList->fallbackCmdList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
			manager->cmdList->fallbackCmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
		}
		else
			manager->cmdList->cmdList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

		// Valid for Top level ds as well?
		//buffer->ChangeStateToUAV(manager->cmdList->cmdList);

		manager->updatingScene->usedGeometries = manager->usedGeometries->clone();
		return manager->updatingScene;
	}


	void RTPipelineManager::Close() {
		// TODO: Create the so

#pragma region counting states
		int count = 0;

		// 1 x each library (D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY)
		count += loadedLibraries.size();

		int maxAttributes = 2 * 4;
		int maxPayload = 3 * 4;
		int maxStackSize = 1;

		for (int i = 0; i < loadedPrograms.size(); i++)
		{
			gObj<IRTProgram> program = loadedPrograms[i];

			maxAttributes = max(maxAttributes, program->AttributesSize);
			maxPayload = max(maxPayload, program->PayloadSize);
			maxStackSize = max(maxStackSize, program->StackSize);

			// Global root signature
			if (!program->globals.isNull())
				count++;
			// Local raygen root signature
			if (!program->raygen_locals.isNull())
				count++;
			// Local miss root signature
			if (!program->miss_locals.isNull())
				count++;
			// Local hitgroup root signature
			if (!program->hitGroup_locals.isNull())
				count++;

			// Associations to global root signature
			if (program->associationsToGlobal.size() > 0)
				count++;
			// Associations to raygen local root signature
			if (program->associationsToRayGenLocals.size() > 0)
				count++;
			// Associations to miss local root signature
			if (program->associationsToMissLocals.size() > 0)
				count++;
			// Associations to hitgroup local root signature
			if (program->associationsToHitGroupLocals.size() > 0)
				count++;
			// 1 x each hit group
			count += program->hitGroups.size();
		}

		// 1 x shader config
		count++;
		// 1 x pipeline config
		count++;

#pragma endregion

		AllocateStates(count);

#pragma region Fill States

		int index = 0;
		// 1 x each library (D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY)
		for (int i = 0; i < loadedLibraries.size(); i++)
			SetDXIL(index++, loadedLibraries[i]->bytecode, loadedLibraries[i]->exports);

		D3D12_STATE_SUBOBJECT* globalRS = nullptr;
		D3D12_STATE_SUBOBJECT* localRayGenRS = nullptr;
		D3D12_STATE_SUBOBJECT* localMissRS = nullptr;
		D3D12_STATE_SUBOBJECT* localHitGroupRS = nullptr;

		for (int i = 0; i < loadedPrograms.size(); i++)
		{
			gObj<IRTProgram> program = loadedPrograms[i];

			// Global root signature
			if (!program->globals.isNull())
			{
				globalRS = &dynamicStates[index];
				SetGlobalRootSignature(index++, program->globals->rootSignature);
			}
			// Local raygen root signature
			if (!program->raygen_locals.isNull())
			{
				localRayGenRS = &dynamicStates[index];
				SetLocalRootSignature(index++, program->raygen_locals->rootSignature);
			}
			// Local miss root signature
			if (!program->miss_locals.isNull())
			{
				localMissRS = &dynamicStates[index];
				SetLocalRootSignature(index++, program->miss_locals->rootSignature);
			}
			// Local hitgroup root signature
			if (!program->hitGroup_locals.isNull())
			{
				localHitGroupRS = &dynamicStates[index];
				SetLocalRootSignature(index++, program->hitGroup_locals->rootSignature);
			}

			for (int j = 0; j < program->hitGroups.size(); j++)
			{
				auto hg = program->hitGroups[j];
				if (hg->intersection.isNull())
					SetTriangleHitGroup(index++, hg->shaderHandle,
						hg->anyHit.isNull() ? nullptr : hg->anyHit->shaderHandle,
						hg->closestHit.isNull() ? nullptr : hg->closestHit->shaderHandle);
				else
					SetProceduralGeometryHitGroup(index++, hg->shaderHandle,
						hg->anyHit ? hg->anyHit->shaderHandle : nullptr,
						hg->closestHit ? hg->closestHit->shaderHandle : nullptr,
						hg->intersection ? hg->intersection->shaderHandle : nullptr);
			}

			// Associations to global root signature
			if (program->associationsToGlobal.size() > 0)
				SetExportsAssociations(index++, globalRS, program->associationsToGlobal);
			// Associations to raygen local root signature
			if (!program->raygen_locals.isNull() && program->associationsToRayGenLocals.size() > 0)
				SetExportsAssociations(index++, localRayGenRS, program->associationsToRayGenLocals);
			// Associations to miss local root signature
			if (!program->miss_locals.isNull() && program->associationsToMissLocals.size() > 0)
				SetExportsAssociations(index++, localMissRS, program->associationsToMissLocals);
			// Associations to hitgroup local root signature
			if (!program->hitGroup_locals.isNull() && program->associationsToHitGroupLocals.size() > 0)
				SetExportsAssociations(index++, localHitGroupRS, program->associationsToHitGroupLocals);
		}

		// 1 x shader config
		SetRTSizes(index++, maxAttributes, maxPayload);
		SetMaxRTRecursion(index++, maxStackSize);

#pragma endregion

		// Create so
		D3D12_STATE_OBJECT_DESC soDesc = { };
		soDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		soDesc.NumSubobjects = index;
		soDesc.pSubobjects = this->dynamicStates;

		if (manager->fallbackDevice != nullptr) // emulating with fallback device
		{
			auto hr = manager->fallbackDevice->CreateStateObject(&soDesc, IID_PPV_ARGS(&fbso));
			if (FAILED(hr))
				throw CA4GException::FromError(CA4G_Errors_BadPSOConstruction, nullptr, hr);
		}
		else {
			auto hr = manager->device->CreateStateObject(&soDesc, IID_PPV_ARGS(&so));
			if (FAILED(hr))
				throw CA4GException::FromError(CA4G_Errors_BadPSOConstruction, nullptr, hr);
		}

		// Get All shader identifiers
		for (int i = 0; i < loadedPrograms.size(); i++)
		{
			auto prog = loadedPrograms[i];
			for (int j = 0; j < prog->loadedShaderPrograms.size(); j++) {
				auto shaderProgram = prog->loadedShaderPrograms[j];

				if (manager->fallbackDevice != nullptr)
				{
					shaderProgram->cachedShaderIdentifier = fbso->GetShaderIdentifier(shaderProgram->shaderHandle);
				}
				else // DirectX Raytracing
				{
					CComPtr<ID3D12StateObject> __so = so;
					CComPtr<ID3D12StateObjectProperties> __soProp;
					__so->QueryInterface<ID3D12StateObjectProperties>(&__soProp);

					shaderProgram->cachedShaderIdentifier = __soProp->GetShaderIdentifier(shaderProgram->shaderHandle);
				}
			}
		}
	}

	
}