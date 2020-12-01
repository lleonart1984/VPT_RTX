#include "ca4G_Private.h"


namespace CA4G {

	ResourceWrapper::ResourceWrapper(gObj<DeviceManager> manager, D3D12_RESOURCE_DESC description, DX_Resource resource, D3D12_RESOURCE_STATES initialState) : manager(manager), internalResource(resource), desc(description) {
		LastUsageState = initialState; // state at creation
		D3D12_HEAP_PROPERTIES heapProp;
		D3D12_HEAP_FLAGS flags;
		resource->GetHeapProperties(&heapProp, &flags);
		if (heapProp.Type == D3D12_HEAP_TYPE_DEFAULT)
			cpu_access = CPU_ACCESS_NONE;
		else
			if (heapProp.Type == D3D12_HEAP_TYPE_UPLOAD)
				cpu_access = CPU_WRITE_GPU_READ;
			else
				cpu_access = CPU_READ_GPU_WRITE;

		subresources = description.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.MipLevels : desc.MipLevels * desc.DepthOrArraySize;

		pLayouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[subresources];
		pNumRows = new unsigned int[subresources];
		pRowSizesInBytes = new UINT64[subresources];
		manager->device->GetCopyableFootprints(&desc, 0, subresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &pTotalSizes);
	}

	void ResourceWrapper::CreateForUploading() {
		if (!uploadingResourceCache) {
			mutex.Acquire();

			if (!uploadingResourceCache) {
				auto size = GetRequiredIntermediateSize(manager->device, desc);

				D3D12_RESOURCE_DESC finalDesc = { };
				finalDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				finalDesc.Format = DXGI_FORMAT_UNKNOWN;
				finalDesc.Width = size;
				finalDesc.Height = 1;
				finalDesc.DepthOrArraySize = 1;
				finalDesc.MipLevels = 1;
				finalDesc.SampleDesc.Count = 1;
				finalDesc.SampleDesc.Quality = 0;
				finalDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				finalDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

				D3D12_HEAP_PROPERTIES uploadProp;
				uploadProp.Type = D3D12_HEAP_TYPE_UPLOAD;
				uploadProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				uploadProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				uploadProp.VisibleNodeMask = 1;
				uploadProp.CreationNodeMask = 1;
				DX_Resource res;
				manager->device->CreateCommittedResource(&uploadProp, D3D12_HEAP_FLAG_NONE, &finalDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
					IID_PPV_ARGS(&res));

				uploadingResourceCache = new ResourceWrapper(manager, this->desc, res, D3D12_RESOURCE_STATE_GENERIC_READ);
			}

			mutex.Release();
		}
	}

	void ResourceWrapper::CreateForDownloading() {
		if (!this->downloadingResourceCache) {
			mutex.Acquire();

			if (!downloadingResourceCache) {
				auto size = GetRequiredIntermediateSize(manager->device, desc);

				D3D12_RESOURCE_DESC finalDesc = { };
				finalDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				finalDesc.Format = DXGI_FORMAT_UNKNOWN;
				finalDesc.Width = size;
				finalDesc.Height = 1;
				finalDesc.DepthOrArraySize = 1;
				finalDesc.MipLevels = 1;
				finalDesc.SampleDesc.Count = 1;
				finalDesc.SampleDesc.Quality = 0;
				finalDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				finalDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

				D3D12_HEAP_PROPERTIES downloadProp;
				downloadProp.Type = D3D12_HEAP_TYPE_READBACK;
				downloadProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				downloadProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				downloadProp.VisibleNodeMask = 1;
				downloadProp.CreationNodeMask = 1;
				DX_Resource res;
				manager->device->CreateCommittedResource(&downloadProp, D3D12_HEAP_FLAG_NONE, &finalDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
					IID_PPV_ARGS(&res));

				downloadingResourceCache = new ResourceWrapper(manager, this->desc, res, D3D12_RESOURCE_STATE_COPY_DEST);
			}

			mutex.Release();
		}
	}


	void ResourceWrapper::UploadFullData(byte* data, long long dataSize, bool flipRows) {
		D3D12_RANGE range{ };

		if (mappedData == nullptr)
		{
			mutex.Acquire();
			if (mappedData == nullptr)
				internalResource->Map(0, &range, &mappedData);
			mutex.Release();
		}

		int srcOffset = 0;
		for (UINT i = 0; i < subresources; ++i)
		{
			D3D12_MEMCPY_DEST DestData = { (byte*)mappedData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i]) };
			D3D12_SUBRESOURCE_DATA subData;
			subData.pData = data + srcOffset;
			subData.RowPitch = pRowSizesInBytes[i];
			subData.SlicePitch = pRowSizesInBytes[i] * pNumRows[i];
			MemcpySubresource(&DestData, &subData, static_cast<SIZE_T>(pRowSizesInBytes[i]), pNumRows[i], pLayouts[i].Footprint.Depth, flipRows);
			srcOffset += pRowSizesInBytes[i] * pNumRows[i];
		}
	}

	void ResourceWrapper::DownloadFullData(byte* data, long long dataSize, bool flipRows) {
		D3D12_RANGE range{ 0, dataSize };

		mutex.Acquire();
		auto hr = internalResource->Map(0, &range, &mappedData);

		int srcOffset = 0;
		for (UINT i = 0; i < 1; ++i)
		{
			D3D12_SUBRESOURCE_DATA DestData = {
				(byte*)mappedData + pLayouts[i].Offset,
				pLayouts[i].Footprint.RowPitch,
				SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i])
			};
			D3D12_MEMCPY_DEST subData;
			subData.pData = data + srcOffset;
			subData.RowPitch = pRowSizesInBytes[i];
			subData.SlicePitch = pRowSizesInBytes[i] * pNumRows[i];

			MemcpySubresource(&subData, &DestData, static_cast<SIZE_T>(pRowSizesInBytes[i]), pNumRows[i], pLayouts[i].Footprint.Depth, flipRows);
			srcOffset += pRowSizesInBytes[i] * pNumRows[i];
		}
		D3D12_RANGE emptyRange{ 0, 0 };
		internalResource->Unmap(0, &emptyRange);
		mutex.Release();
	}

	
	byte* ResourceWrapper::GetMappedDataAddress() {
		D3D12_RANGE range{ };
		if (mappedData == nullptr)
		{
			mutex.Acquire();
			if (mappedData == nullptr)
				internalResource->Map(0, &range, &mappedData);
			mutex.Release();
		}
		return (byte*)mappedData;
	}

	void ResourceWrapper::UpdateMappedData(int position, void* data, int size) {
		D3D12_RANGE range{ };

		if (mappedData == nullptr)
		{
			mutex.Acquire();
			if (mappedData == nullptr)
				internalResource->Map(0, &range, &mappedData);
			mutex.Release();
		}

		memcpy((UINT8*)mappedData + position, (UINT8*)data, size);
	}

	int ResourceView::getSRV() {
		if ((handleMask & 1) != 0)
			return srv;

		mutex.Acquire();
		if ((handleMask & 1) == 0)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC d;
			ZeroMemory(&d, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
			CreateSRVDesc(d);
			srv = manager->descriptors->cpu_csu->AllocateNewHandle();

			manager->device->CreateShaderResourceView(!resource ? nullptr : resource->internalResource, &d, manager->descriptors->cpu_csu->getCPUVersion(srv));
			handleMask |= 1;
		}
		mutex.Release();
		return srv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ResourceView::getSRVHandle() {
		return manager->descriptors->cpu_csu->getCPUVersion(getSRV());
	}

	int ResourceView::getUAV() {
		if ((handleMask & 2) != 0)
			return uav;

		mutex.Acquire();

		if ((handleMask & 2) == 0)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC d;
			ZeroMemory(&d, sizeof(D3D12_UNORDERED_ACCESS_VIEW_DESC));
			CreateUAVDesc(d);
			uav = manager->descriptors->cpu_csu->AllocateNewHandle();
			manager->device->CreateUnorderedAccessView(!resource ? nullptr : resource->internalResource, NULL, &d, manager->descriptors->cpu_csu->getCPUVersion(uav));
			handleMask |= 2;
		}
		mutex.Release();
		return uav;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ResourceView::getUAVHandle() {
		return manager->descriptors->cpu_csu->getCPUVersion(getUAV());
	}

	int ResourceView::getCBV() {
		if ((handleMask & 4) != 0)
			return cbv;

		mutex.Acquire();

		if ((handleMask & 4) == 0) {
			D3D12_CONSTANT_BUFFER_VIEW_DESC d;
			ZeroMemory(&d, sizeof(D3D12_CONSTANT_BUFFER_VIEW_DESC));
			CreateCBVDesc(d);
			cbv = manager->descriptors->cpu_csu->AllocateNewHandle();
			manager->device->CreateConstantBufferView(&d, manager->descriptors->cpu_csu->getCPUVersion(cbv));
			handleMask |= 4;
		}

		mutex.Release();
		return cbv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ResourceView::getCBVHandle() {
		return manager->descriptors->cpu_csu->getCPUVersion(getCBV());
	}

	int ResourceView::getRTV() {
		if ((handleMask & 8) != 0)
			return rtv;

		mutex.Acquire();

		if ((handleMask & 8) == 0) {
			D3D12_RENDER_TARGET_VIEW_DESC d;
			ZeroMemory(&d, sizeof(D3D12_RENDER_TARGET_VIEW_DESC));
			CreateRTVDesc(d);
			rtv = manager->descriptors->cpu_rt->AllocateNewHandle();
			manager->device->CreateRenderTargetView(!resource ? nullptr : resource->internalResource, &d, manager->descriptors->cpu_rt->getCPUVersion(rtv));
			handleMask |= 8;
		}

		mutex.Release();
		return rtv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ResourceView::getRTVHandle() {
		return manager->descriptors->cpu_rt->getCPUVersion(getRTV());
	}

	int ResourceView::getDSV() {

		if ((handleMask & 16) != 0)
			return dsv;

		mutex.Acquire();

		if ((handleMask & 16) == 0) {
			D3D12_DEPTH_STENCIL_VIEW_DESC d;
			ZeroMemory(&d, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
			CreateDSVDesc(d);
			dsv = manager->descriptors->cpu_ds->AllocateNewHandle();
			manager->device->CreateDepthStencilView(!resource ? nullptr : resource->internalResource, &d, manager->descriptors->cpu_ds->getCPUVersion(dsv));
			handleMask |= 16;
		}

		mutex.Release();
		return dsv;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE ResourceView::getDSVHandle() {
		return manager->descriptors->cpu_ds->getCPUVersion(getDSV());
	}

	ResourceView::~ResourceView() {
		if ((handleMask & 1) != 0)
			manager->descriptors->cpu_csu->Release(srv);
		if ((handleMask & 2) != 0)
			manager->descriptors->cpu_csu->Release(uav);
		if ((handleMask & 4) != 0)
			manager->descriptors->cpu_csu->Release(cbv);
		if ((handleMask & 8) != 0)
			manager->descriptors->cpu_rt->Release(rtv);
		if ((handleMask & 16) != 0)
			manager->descriptors->cpu_ds->Release(dsv);

		/*if (resource != nullptr) {
			resource->references--;
			if (resource->references == 0)
			{
				delete resource;
				resource = nullptr;
			}
		}*/
	}

	gObj<ResourceView> ResourceView::getNullView(gObj<DeviceManager> manager, D3D12_RESOURCE_DIMENSION dimension)
	{
		switch (dimension)
		{
		case D3D12_RESOURCE_DIMENSION_BUFFER:
			return manager->__NullBuffer ? manager->__NullBuffer : manager->__NullBuffer = new Buffer(manager);
		case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			return manager->__NullTexture2D ? manager->__NullTexture2D : manager->__NullTexture2D = new Texture2D(manager);
		}
		throw "not supported yet";
	}

}