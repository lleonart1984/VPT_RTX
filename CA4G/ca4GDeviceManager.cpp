#include "ca4G_Private.h"

namespace CA4G {

#pragma region Device Manager

	DeviceManager::DeviceManager(DX_Device device, int buffers, bool useFrameBuffering, bool isWarpDevice)
		:
		device(device),
		counting(new CountEvent()),
		descriptors(new DescriptorsManager(device)),
		creating(new Creating(this)),
		loading(new Loading(this))
	{
		auto hr = D3D12CreateRaytracingFallbackDevice(device, CreateRaytracingFallbackDeviceFlags::ForceComputeFallback, 0, IID_PPV_ARGS(&fallbackDevice));

		if (FAILED(hr)) {
			throw CA4GException::FromError(CA4G_Errors_Unsupported_Fallback, nullptr, hr);
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS5 options;
		if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)))
			&& options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			if (!FORCE_FALLBACK_DEVICE)
				fallbackDevice = nullptr; // device supports DXR! no necessary fallback device
		}

		Scheduler = new GPUScheduler(this, useFrameBuffering, CA4G_MAX_NUMBER_OF_WORKERS, buffers);
	}

	DeviceManager::~DeviceManager() {
		if (!__NullBuffer.isNull())
			__NullBuffer = nullptr;
		if (!__NullTexture2D.isNull())
			__NullTexture2D = nullptr;

		delete creating;
		delete loading;
	}

	// Create a wrapped pointer for the Fallback Layer path.
	WRAPPED_GPU_POINTER DeviceManager::CreateFallbackWrappedPointer(gObj<Buffer> resource, UINT bufferNumElements)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
		rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
		rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

		D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;

		// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
		UINT descriptorHeapIndex = 0;
		if (!fallbackDevice->UsingRaytracingDriver())
		{
			descriptorHeapIndex = this->descriptors->gpu_csu->MallocPersistent();
			bottomLevelDescriptor = descriptors->gpu_csu->getCPUVersion(descriptorHeapIndex);
			device->CreateUnorderedAccessView(resource->resource->internalResource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
		}
		return fallbackDevice->GetWrappedPointerSimple(descriptorHeapIndex, resource->resource->internalResource->GetGPUVirtualAddress());
	}


#pragma endregion

}