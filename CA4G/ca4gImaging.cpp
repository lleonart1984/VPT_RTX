#include "ca4G_Private.h"
#include "DirectXTex.h"

namespace CA4G {

	gObj<TextureData> CreateTextureDataFromScratchImage(DirectX::TexMetadata &metadata, DirectX::ScratchImage &image, bool flipY) {
		byte* buffer = new byte[image.GetPixelsSize()];
		memcpy(buffer, image.GetPixels(), image.GetPixelsSize());

		return metadata.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ?
			new TextureData(metadata.width, metadata.height, metadata.mipLevels, metadata.arraySize, metadata.format, buffer, image.GetPixelsSize(), flipY)
			: new TextureData(metadata.width, metadata.height, metadata.depth, metadata.format, buffer, image.GetPixelsSize(), flipY);
	}

	TextureData::TextureData(int width, int height, int mipMaps, int slices, DXGI_FORMAT format, byte* data, long long dataSize, bool flipY)
		:Width(width), Height(height),
		MipMaps(mipMaps),
		ArraySlices(slices),
		Format(format),
		Data(data),
		DataSize(dataSize),
		FlipY(flipY),
		pixelStride(DirectX::BitsPerPixel(format) / 8),
		Dimension(D3D12_RESOURCE_DIMENSION_TEXTURE2D)
	{
		footprints = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[mipMaps * slices];
		int index = 0;
		UINT64 offset = 0;
		for (int s = 0; s < slices; s++)
		{
			int w = Width;
			int h = Height;
			for (int m = 0; m < mipMaps; m++)
			{
				footprints[index].Footprint.Width = w;
				footprints[index].Footprint.Height = h;
				footprints[index].Footprint.Format = format;
				footprints[index].Footprint.RowPitch = pixelStride * w;
				footprints[index].Footprint.Depth = 1;
				footprints[index].Offset = offset;

				offset += w * h * pixelStride;

				w /= 2; w = max(1, w);
				h /= 2; h = max(1, h);

				index++;
			}
		}
	}

	TextureData::TextureData(int width, int height, int depth, DXGI_FORMAT format, byte* data, long long dataSize, bool flipY)
		:Width(width), Height(height),
		MipMaps(1),
		Depth(depth),
		Format(format),
		Data(data),
		DataSize(dataSize),
		FlipY(flipY),
		pixelStride(DirectX::BitsPerPixel(format) / 8),
		Dimension(D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		footprints = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[1];

		footprints[0].Footprint.Width = width;
		footprints[0].Footprint.Height = height;
		footprints[0].Footprint.Depth = depth;
		footprints[0].Footprint.Format = format;
		footprints[0].Footprint.RowPitch = pixelStride * width;
		footprints[0].Offset = 0;
	}

	TextureData::~TextureData() {
		delete[] Data;
		delete[] footprints;
	}

	gObj<TextureData> TextureData::CreateEmpty(int width, int height, int mipMaps, int slices, DXGI_FORMAT format) {

		int pixelStride = DirectX::BitsPerPixel(format) / 8;
		UINT64 total = 0;
		int w = width, h = height;
		for (int i = 0; i < mipMaps; i++)
		{
			total += w * h* pixelStride;
			w /= 2;
			h /= 2;
			w = max(1, w);
			h = max(1, h);
		}
		total *= slices;
		byte* data = new byte[total];
		ZeroMemory(data, total);
		return new TextureData(width, height, mipMaps, slices, format, data, total, false);
	}

	gObj<TextureData> TextureData::CreateEmpty(int width, int height, int depth, DXGI_FORMAT format) {
		int pixelStride = DirectX::BitsPerPixel(format) / 8;
		UINT64 total = width * height* depth * pixelStride;
		byte* data = new byte[total];
		ZeroMemory(data, total);
		return new TextureData(width, height, depth, format, data, total, false);
	}

	gObj<TextureData> TextureData::LoadFromFile(const char * filename) {

		static bool InitializedWic = false;

		if (!InitializedWic) {
			void* res = nullptr;
			CoInitialize(res);
			InitializedWic = true;
		}

		wchar_t* file = new wchar_t[strlen(filename) + 1];
		OemToCharW(filename, file);
		DirectX::TexMetadata metadata;
		DirectX::ScratchImage scratchImage;

		bool notDDS;
		bool notWIC;
		bool notTGA;

		if (
			(notDDS = FAILED(DirectX::LoadFromDDSFile(file, 0, &metadata, scratchImage)))
			&& (notWIC = FAILED(DirectX::LoadFromWICFile(file, 0, &metadata, scratchImage)))
			&& (notTGA = FAILED(DirectX::LoadFromTGAFile(file, &metadata, scratchImage))))
			return nullptr;

		return CreateTextureDataFromScratchImage(metadata, scratchImage, true);
	}

	void TextureData::SaveToFile(gObj<TextureData> data, const char* filename) {
		static bool InitializedWic = false;

		if (!InitializedWic) {
			void* res = nullptr;
			CoInitialize(res);
			InitializedWic = true;
		}

		DirectX::Image img;
		img.format = data->Format;
		img.width = data->Width;
		img.height = data->Height;
		img.rowPitch = data->pixelStride * data->Width;
		img.slicePitch = data->pixelStride * data->Width * data->Height;
		img.pixels = data->Data;

		wchar_t* file = new wchar_t[strlen(filename) + 1];
		OemToCharW(filename, file);

		DirectX::SaveToWICFile(img, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_JPEG), file);
	}

}