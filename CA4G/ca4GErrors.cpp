#include "ca4G_Private.h"

namespace CA4G {

	CA4GException CA4GException::FromError(CA4G_Errors error, const char * arg, HRESULT hr) {

		static char fullMessage[1000];

		ZeroMemory(&fullMessage, sizeof(fullMessage));

		const char * errorMessage;

		switch (error) {
		case CA4G_Errors_BadPSOConstruction:
			errorMessage = "Error in PSO construction. Check all registers are bound and the input layout is compatible.";
			break;
		case CA4G_Errors_UnsupportedFormat:
			errorMessage = "Error in Format. Check resource format is supported.";
			break;
		case CA4G_Errors_ShaderNotFound:
			errorMessage = "Shader file was not found. Check file path in pipeline binding.";
			break;
		case CA4G_Errors_RunOutOfMemory:
			errorMessage = "Run out of memory during object creation.";
			break;
		case CA4G_Errors_Invalid_Operation:
			errorMessage = "Invalid operation.";
			break;
		case CA4G_Errors_Unsupported_Fallback:
			errorMessage = "Fallback raytracing device was not supported. Check OS is in Developer Mode.";
			break;
		default:
			errorMessage = "Unknown error in CA4G";
			break;
		}

		if (errorMessage != nullptr)
			strcat_s(fullMessage, errorMessage);
		
		if (arg != nullptr)
			strcat_s(fullMessage, arg);

		if (FAILED(hr)) // Some HRESULT to show
		{
			_com_error err(hr);

			char hrErrorMessage[1000];
			ZeroMemory(hrErrorMessage, sizeof(hrErrorMessage));
			CharToOem(err.ErrorMessage(), hrErrorMessage);

			strcat_s(fullMessage, hrErrorMessage);
		}

		return CA4GException(fullMessage);
	}

}