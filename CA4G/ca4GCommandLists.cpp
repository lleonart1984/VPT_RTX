#include "ca4G_Private.h"

namespace CA4G {

	void CommandListManager::__All(gObj<ResourceWrapper> & const dst, gObj<ResourceWrapper> & const src) {

		if (EquivalentDesc(dst->internalResource->GetDesc(), src->internalResource->GetDesc()))
		{ // have the same dimensions
			cmdList->CopyResource(dst->internalResource, src->internalResource);
		}
		else
		{
			if (dst->internalResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER && src->internalResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{ // both buffers
				cmdList->CopyBufferRegion(
					dst->internalResource, 0, src->internalResource, 0, src->internalResource->GetDesc().Width);
			}
			else
				if (dst->internalResource->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
				{ // readback buffer
					// Buffer from upload to any resource on the gpu
					auto device = getInternalDevice();

					int numberOfSubresources = src->desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? src->desc.MipLevels :
						src->desc.MipLevels * src->desc.DepthOrArraySize;

					D3D12_PLACED_SUBRESOURCE_FOOTPRINT* Layouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numberOfSubresources];
					UINT* NumRows = new UINT[numberOfSubresources];
					UINT64* BytesForRow = new UINT64[numberOfSubresources];
					UINT64 totalBytes;
					device->GetCopyableFootprints(&dst->desc, 0, numberOfSubresources, 0, Layouts, NumRows, BytesForRow, &totalBytes);

					for (int i = 0; i < numberOfSubresources; i++) {

						D3D12_TEXTURE_COPY_LOCATION Dst{};
						Dst.pResource = src->internalResource;
						Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
						Dst.SubresourceIndex = i;

						D3D12_TEXTURE_COPY_LOCATION Src{};
						Src.pResource = dst->internalResource;
						Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
						Src.PlacedFootprint = Layouts[i];
						cmdList->CopyTextureRegion(&Src, 0, 0, 0, &Dst, nullptr);
					}
					delete[] Layouts;
					delete[] NumRows;
					delete[] BytesForRow;
				}
				else
			{
				// Buffer from upload to any resource on the gpu
				auto device = getInternalDevice();

				int numberOfSubresources = dst->desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? dst->desc.MipLevels :
					dst->desc.MipLevels * dst->desc.DepthOrArraySize;

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT* Layouts = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[numberOfSubresources];
				UINT* NumRows = new UINT[numberOfSubresources];
				UINT64* BytesForRow = new UINT64[numberOfSubresources];
				UINT64 totalBytes;
				device->GetCopyableFootprints(&src->desc, 0, numberOfSubresources, 0, Layouts, NumRows, BytesForRow, &totalBytes);

				for (int i = 0; i < numberOfSubresources; i++) {

					D3D12_TEXTURE_COPY_LOCATION Dst{};
					Dst.pResource = dst->internalResource;
					Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					Dst.SubresourceIndex = i;

					D3D12_TEXTURE_COPY_LOCATION Src{};
					Src.pResource = src->internalResource;
					Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					Src.PlacedFootprint = Layouts[i];
					cmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
				}
				delete[] Layouts;
				delete[] NumRows;
				delete[] BytesForRow;
			}
		}
	}
	
	void CommandListManager::Loading::FromData(gObj<Texture2D> &texture, gObj<TextureData> data) {
		D3D12_RESOURCE_DESC d;
		ZeroMemory(&d, sizeof(D3D12_RESOURCE_DESC));
		d.Width = data->Width;
		d.Height = data->Height;
		d.Dimension = data->Dimension;
		d.DepthOrArraySize = data->ArraySlices;
		d.MipLevels = data->MipMaps;
		d.Flags = D3D12_RESOURCE_FLAG_NONE;
		d.SampleDesc.Count = 1;
		d.SampleDesc.Quality = 0;
		d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		d.Format = data->Format;
		texture = new Texture2D(this->manager->manager->creating->CreateResourceAndWrap(d, D3D12_RESOURCE_STATE_COPY_DEST));
		texture->resource->CreateForUploading();
		texture->resource->uploadingResourceCache->UploadFullData(data->Data, data->DataSize, data->FlipY);
		manager->__All(texture->resource, texture->resource->uploadingResourceCache);
	}

	void CommandListManager::Loading::ToData(gObj<Texture2D> texture, gObj<TextureData>& data) {
		
		data = TextureData::CreateEmpty(texture->Width, texture->Height, 1, 1, texture->resource->desc.Format);

		texture->resource->CreateForDownloading();
		
		texture->ChangeStateTo(manager->cmdList, D3D12_RESOURCE_STATE_GENERIC_READ);

		manager->__All(texture->resource->downloadingResourceCache, texture->resource);
		texture->resource->downloadingResourceCache->DownloadFullData(data->Data, data->DataSize, false);
	}

	inline DX_Device CommandListManager::getInternalDevice() { return manager->device; }


}