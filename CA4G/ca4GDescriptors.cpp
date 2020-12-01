#include "ca4G_Private.h"


namespace CA4G {
	CPUDescriptorHeapManager::CPUDescriptorHeapManager(DX_Device device, D3D12_DESCRIPTOR_HEAP_TYPE type, int capacity) {
		size = device->GetDescriptorHandleIncrementSize(type);
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NodeMask = 0;
		desc.NumDescriptors = capacity;
		desc.Type = type;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
		startCPU = heap->GetCPUDescriptorHandleForHeapStart().ptr;
	}

	int CPUDescriptorHeapManager::AllocateNewHandle() {
		int index;
		mutex.Acquire();
		if (free != nullptr) {
			index = free->index;
			Node *toDelete = free;
			free = free->next;
			delete toDelete;
		}
		else
			index = allocated++;
		mutex.Release();
		return index;
	}
	
	GPUDescriptorHeapManager::GPUDescriptorHeapManager(DX_Device device, D3D12_DESCRIPTOR_HEAP_TYPE type, int capacity) :capacity(capacity) {
		size = device->GetDescriptorHandleIncrementSize(type);
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NodeMask = 0;
		desc.NumDescriptors = capacity;
		desc.Type = type;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		auto hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
		if (FAILED(hr))
			throw CA4GException::FromError(CA4G_Errors_RunOutOfMemory, "Creating descriptor heaps.");
		startCPU = heap->GetCPUDescriptorHandleForHeapStart().ptr;
		startGPU = heap->GetGPUDescriptorHandleForHeapStart().ptr;
		mallocOffset = 0;
	}
}