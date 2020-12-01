#include "ca4G_Private.h"


namespace CA4G {
	D3D12_SHADER_BYTECODE ShaderLoader::FromFile(const char* bytecodeFilePath) {
		D3D12_SHADER_BYTECODE code;
		FILE* file;
		if (fopen_s(&file, bytecodeFilePath, "rb") != 0)
		{
			throw CA4GException::FromError(CA4G_Errors_ShaderNotFound);
		}
		fseek(file, 0, SEEK_END);
		long long count;
		count = ftell(file);
		fseek(file, 0, SEEK_SET);

		byte* bytecode = new byte[count];
		int offset = 0;
		while (offset < count) {
			offset += fread_s(&bytecode[offset], min(1024, count - offset), sizeof(byte), 1024, file);
		}
		fclose(file);

		code.BytecodeLength = count;
		code.pShaderBytecode = (void*)bytecode;
		return code;
	}

	D3D12_SHADER_BYTECODE ShaderLoader::FromMemory(const byte* bytecodeData, int count) {
		D3D12_SHADER_BYTECODE code;
		code.BytecodeLength = count;
		code.pShaderBytecode = (void*)bytecodeData;
		return code;
	}

	
}