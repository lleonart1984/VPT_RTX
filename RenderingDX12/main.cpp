#include "ImGui\imgui.h"
#include "ImGui\imgui_impl_win32.h"
#include "ImGui\imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <shlobj.h>

#include "stdafx.h"


extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

template<typename T>
void RenderGUI(gObj<Technique> t) {
	gObj<T> h = t.Dynamic_Cast<T>();
	if (h)
		GuiFor(h);
}
float lastFrameTimeInMS = 0;

void GuiFor(gObj<IHasBackcolor> t) {
	ImGui::ColorEdit3("clear color", (float*)&t->Backcolor); // Edit 3 floats representing a color
}
void GuiFor(gObj<IHasTriangleNumberParameter> t) {
	ImGui::SliderInt("Number of tris", &t->NumberOfTriangles, t->MinimumOfTriangles, t->MaximumOfTriangles);
}
void GuiFor(gObj<IHasRaymarchDebugInfo> t) {
	ImGui::Checkbox("Show Count Steps", &t->CountSteps);
	ImGui::Checkbox("Show Count Hits", &t->CountHits);
}
void GuiFor(gObj<IHasParalellism> t) {
#ifdef MAX_NUMBER_OF_ASYNC_PROCESSES
	ImGui::SliderInt("Number of workers", &t->NumberOfWorkers, 1, MAX_NUMBER_OF_ASYNC_PROCESSES);
#endif
}
void GuiFor(gObj<IHasLight> t) {
	float3 light = t->Light->Intensity;
	float maxCmp = max(1, max(light.x, max(light.y, light.z)));
	float3 lightColor = light / maxCmp;
	bool changedColor = ImGui::ColorEdit3("Light color", (float*)&lightColor);
	bool changedIntensity = ImGui::SliderFloat("Light Intensity", &maxCmp, 0, 1000, "%.3f", 2);
	float3 newLight = lightColor * maxCmp;
	if (changedColor || changedIntensity)
	{
		t->Light->Intensity = newLight;
		t->LightSourceIsDirty = true;
	}
	float lightY = t->Light->Position.y;
	bool changedPosition = ImGui::SliderFloat("Light position", &lightY, 0.2, 2);
	if (changedPosition) {
		t->Light->Position.y = lightY;
		t->LightSourceIsDirty = true;
	}

	float alpha = atan2f(t->Light->Direction.z, t->Light->Direction.x);
	float beta = asinf(t->Light->Direction.y);
	bool changeDirection = ImGui::SliderFloat("Light Direction Alpha", &alpha, 0, 3.141596 * 2);
	changeDirection |= ImGui::SliderFloat("Light Direction Beta", &beta, -3.141596 / 2, 3.141596 / 2);
	if (changeDirection) {
		t->Light->Direction = float3(cos(alpha) * cos(beta), sin(beta), sin(alpha) * cos(beta));
		t->LightSourceIsDirty = true;
	}
}

void GuiFor(gObj<IHasVolume> t) {
	if (
		ImGui::SliderFloat("Density", &t->densityScale, 0.001, 1000) |
		ImGui::SliderFloat("Absortion", &t->globalAbsortion, 0, 1)) {
		auto asLight = t.Dynamic_Cast<IHasLight>();
		if (asLight)
			asLight->LightSourceIsDirty = true;
	}
}

int selectedMaterial = 0;

void GuiFor(gObj<IHasScene> t) {
	
	ImGui::DragInt("Material Index", &selectedMaterial, 1, 0, t->Scene->getMaterialCount());

	SCENE_VOLMATERIAL &vol = t->Scene->getVolumeMaterialBuffer()[selectedMaterial];

	float3 extinction = vol.Extinction;
	float size = max(extinction.x, max(extinction.y, extinction.z));
	extinction = extinction * 1.0 / max(0.0000001, size);
	float3 absorption = 1 - vol.ScatteringAlbedo;

	if (
		ImGui::SliderFloat("Size", (float*)&size, 0.01, 1000, "%.3f", 2) |
		ImGui::SliderFloat3("Extinction", (float*)&extinction, 0.0, 1.0) |
		ImGui::SliderFloat3("Absorption", (float*)&absorption, 0.0, 1.0, "%.5f", 4) |
		ImGui::SliderFloat3("G", (float*)&vol.G, -0.99, 0.99)
		) {
		t->Scene->getVolumeMaterialBuffer()[selectedMaterial].Extinction = extinction * size;
		t->Scene->getVolumeMaterialBuffer()[selectedMaterial].ScatteringAlbedo = 1 - absorption;
		t->IsVolMaterialDirty = true;
		auto asLight = t.Dynamic_Cast<IHasLight>();
		if (asLight)
			asLight->LightSourceIsDirty = true;
	}
}

void GuiFor(gObj<IHasHomogeneousVolume> t) {
	if (
		ImGui::SliderFloat("Density", &t->densityScale, 0.001, 20) |
		ImGui::SliderFloat("G", &t->gFactor, -0.99, 0.99) |
		ImGui::SliderFloat("Absortion", &t->globalAbsortion, 0, 1)) {
		auto asLight = t.Dynamic_Cast<IHasLight>();
		if (asLight)
			asLight->LightSourceIsDirty = true;
	}
}

void GuiFor(gObj<IHasScatteringEvents> t) {
	if (
		ImGui::SliderFloat("Size", &t->size, 0.01, 1000, "%.3f", 2) |
		ImGui::SliderFloat3("Scattering", (float*)&t->scattering, 0.1, 5.0) |
		ImGui::SliderFloat3("Absorption", (float*)&t->absorption, 0.0, 1.0, "%.5f", 4) |
		ImGui::SliderFloat3("G", (float*)&t->gFactor, -0.99, 0.99) |
		ImGui::SliderFloat("Pathtracing", &t->pathtracing, -.01, 1.01) |
		ImGui::Checkbox("Debug", &t->CountSteps) | 
		ImGui::SliderFloat("Dbg Threshold", (float*)&t->DebugTreshold, 0.0, 10.0, "%.5f", 4)
		) {
		auto asLight = t.Dynamic_Cast<IHasLight>();
		if (asLight)
			asLight->LightSourceIsDirty = true;
	}
}

void GuiFor(gObj<IHasAccumulative> t) {
	ImGui::Text("Current Frame %i", t->CurrentFrame);
	if (t->StopFrame != 0)
		ImGui::Text("ETA %.3f s", (t->StopFrame - t->CurrentFrame - 1) * lastFrameTimeInMS / 1000);

	bool changeStop = ImGui::InputInt("Stop Frame", &t->StopFrame);

	if (changeStop && t->StopFrame <= t->CurrentFrame && t->StopFrame != 0) {
		auto asLight = t.Dynamic_Cast<IHasLight>();
		if (asLight)
			asLight->LightSourceIsDirty = true;
	}
}


LPSTR desktop_directory()
{
	static char path[MAX_PATH + 1];
	if (SHGetSpecialFolderPathA(HWND_DESKTOP, path, CSIDL_DESKTOP, FALSE))
		return path;
	else
		return "ERROR";
}

void MixGlassMaterial(SCENE_MATERIAL* material, float alpha, float eta = 1.6) {
	material->RefractionIndex = eta;
	material->Specular = CA4G::lerp(material->Specular, float3(1, 1, 1), alpha);
	material->Roulette = CA4G::lerp(material->Roulette, float4(0, 0, 0, 1), alpha);
}

void MixEmissiveMaterial(SCENE_MATERIAL* material, float3 emissive) {
	material->Emissive = emissive;
}

void MixMirrorMaterial(SCENE_MATERIAL* material, float alpha) {
	material->Specular = CA4G::lerp(material->Specular, float3(1, 1, 1), alpha);
	material->Roulette = CA4G::lerp(material->Roulette, float4(0, 0, 1, 0), alpha);
}

void MixGlossyMaterial(SCENE_MATERIAL* material, float alpha) {
	material->Specular = CA4G::lerp(material->Specular, float3(1, 1, 1), alpha);
	material->SpecularSharpness = material->SpecularSharpness * (1 - alpha) + 100 * alpha;
	material->Roulette = CA4G::lerp(material->Roulette, float4(0, 1, 0, 0), alpha);
}

class ImGUIPresenter : public Presenter {
public:
	ImGUIPresenter(HWND hWnd, bool fullScreen = false, int buffers = 2, bool useFrameBuffering = false, bool warpDevice = false):Presenter(hWnd, fullScreen, buffers, useFrameBuffering, warpDevice)
	{
	}

	void OnPresentGUI(D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle,
		DX_CommandList commandList) {
		commandList->OMSetRenderTargets(1, &renderTargetHandle, false, nullptr);
		ID3D12DescriptorHeap* dh[1] = { this->getDescriptorsHeapForGUI() };
		commandList->SetDescriptorHeaps(1, dh);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
};

bool DeviceManager::FORCE_FALLBACK_DEVICE = false;

// Good cameras for bunny
//float3 cameraPositions[] = { float3(0.4,0.3,0), float3(0.24, 0.37, 0.54), float3(-0.24, 0.57, 0.44), float3(-0.14, 0.17, -0.35), float3(0.2, 0.02, 0.3) };

// Good cameras for clouds
//float3 cameraPositions[] = { float3(1.2,0.1,0), float3(0.84, -0.37, 0.64), float3(-1.4, -0.17, 0.4), float3(-0.54, -0.17, -0.75), float3(0.2, 0.02, 0.3) };

// Good cameras for sponza
//float3 cameraPositions[] = { float3(0.1,0.1,0), float3(0.14, 0.17, 0.14), float3(-0.14, 0.17, 0.14), float3(-0.14, 0.17, -0.15), float3(0.2, 0.02, 0.1) };
float3 cameraPositions[] = { float3(0,0.6,0), float3(0.14, 0.17, 0.14), float3(-0.14, 0.17, 0.14), float3(-0.14, 0.17, -0.15), float3(0.2, 0.02, 0.1) };
float3 cameraTargets[] = { float3(0,0,-0.1), float3(-0.44, 0.1, 0.14 - 1),float3(0.44, 0.20, 0.14 - 1), float3(1 + 0.14, 0.3, 0.2 + 0.14),float3(0.2 - 1, 0.02, 0.1) };
//float3 cameraTargets[] = { float3(0,0,0), float3(-0, 0, 0),float3(0, 0, 0), float3(0, 0, 0),float3(0, 0, 0) };
int movingCamera = -1;
bool ScreenShotDesired;


float randomf() {
	return rand() / (float)RAND_MAX;
}

float3 generateInBox() {
	return float3(randomf(), randomf(), randomf());
}

int main(int, char**)
{
    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("CA4G_Samples_Window"), NULL };
    RegisterClassEx(&wc);
	
	int windowWidth= 1440;
	//int windowWidth = 1080;
	//int windowWidth = 600 * 7/5;
	//int windowWidth = 600 * 7/5;


	//int windowWidth = 1920; // Full HD
	//int windowWidth = 1280;
	//int windowWidth = 600*7/5; // Lucys
	//int windowWidth = 1024;
	//int windowWidth = 600 * 7 / 5;
	//int windowHeight = 300;
	//int windowHeight = 800 * 7 / 5;
	//int windowHeight = 768;
	int windowHeight = 1080;

	//int windowHeight = 800*7/5; // Lucys
	HWND hWnd = CreateWindow(_T("CA4G_Samples_Window"), _T("CA4G Samples"), WS_OVERLAPPEDWINDOW, 100, 100, windowWidth+1024-1008, windowHeight+768-729, NULL, NULL, wc.hInstance, NULL);
	//HWND hWnd = CreateWindow(_T("CA4G_Samples_Window"), _T("CA4G Samples"), WS_OVERLAPPEDWINDOW, 100, 100, 1280+1024-1008, 800+768-729, NULL, NULL, wc.hInstance, NULL);

	// Show the window
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	gObj<ScreenShotTechnique> screenshot;

#ifdef FORCE_FALLBACK
	DeviceManager::FORCE_FALLBACK_DEVICE = true;
#else
	DeviceManager::FORCE_FALLBACK_DEVICE = false;
#endif

#ifdef WARP
	static Presenter* presenter = new Presenter(hWnd, false, 2, false, true);
#else
	static Presenter* presenter = new ImGUIPresenter(hWnd);
#endif

#ifdef TEST_WSAPIT
    presenter->Load(technique, APITDescription{ 8 });
#else
#ifdef TEST_WSMRAPIT
	presenter->Load(technique, MRAPITDescription{ 8, 1, 8 });
#else
    presenter->Load(technique);
#endif
#endif

	presenter->Load(screenshot);
	screenshot->FileName = "screenshot.jpg";

	gObj<IHasBackcolor> asBackcolorRenderer = technique.Dynamic_Cast<IHasBackcolor>();
	gObj<IHasScene> asSceneRenderer = technique.Dynamic_Cast<IHasScene>();
	gObj<IHasCamera> asCameraRenderer = technique.Dynamic_Cast<IHasCamera>();
	gObj<IHasLight> asLightRenderer = technique.Dynamic_Cast<IHasLight>();
	gObj<IHasVolume> asVolumeRenderer = technique.Dynamic_Cast<IHasVolume>();


	static Scene* scene = nullptr;
	static Volume* volume = nullptr;
	static Camera* camera = new Camera { float3(0,0,4.0f), float3(0,0,0), float3(0,1,0), PI / 4, 0.001f, 1000.0f };
	static LightSource* lightSource = new LightSource{ float3(2,2,0), normalize(float3(0,1,1)), 0, float3(2, 2, 2) * 3 };
	//static LightSource *lightSource = new LightSource{ float3(2,2,0), normalize(float3(1,1,1)), 0, float3(2, 2, 2)*3 };

	if (asSceneRenderer)
	{
		scene = new Scene();
		
		char* filePath = desktop_directory();
		strcat(filePath, "\\Models\\newLucy.obj");
		scene->loadModelsFrom(filePath, true, Translate(0.4,0.44,0));

		filePath = desktop_directory();
		strcat(filePath, "\\Models\\newDragon.obj");
		scene->loadModelsFrom(filePath, true, Translate(-0.25, 0.14, 0));

		filePath = desktop_directory();
		strcat(filePath, "\\Models\\plate.obj");
		scene->loadModelsFrom(filePath, true, mul(Scale(25, 1, 25), Translate(0, 0, 0)));

		MixGlassMaterial(&scene->getMaterialBuffer()[0], 1, 1.5);
		scene->getVolumeMaterialBuffer()[0] = SCENE_VOLMATERIAL{
				float3(500, 500, 500), // sigma
				float3(0.999, 0.99995, 0.999),
				float3(0.1, 0.1, 0.1)
		};

		MixGlassMaterial(&scene->getMaterialBuffer()[1], 1, 1.5);
		scene->getVolumeMaterialBuffer()[1] = SCENE_VOLMATERIAL{
				float3(500, 500, 500), // sigma
				float3(0.999, 0.99995, 0.999),
				float3(0.9, 0.9, 0.9)
		};

		MixMirrorMaterial(&scene->getMaterialBuffer()[2], 0.3);

		camera->Position = float3(0, 0.5, 1.7);
		camera->Target = float3(0,0.4,0);

		//camera->Position = float3(.43f, .133f, -1.3f);
		//camera->Target = float3(.43f, 0.133f, 0);

		lightSource->Position = float3(0.0, 0.2, -4);
		lightSource->Direction = normalize(float3(0, 1, 1));

		asSceneRenderer->Scene = scene;
	}

	if (asVolumeRenderer) {
		char* volumePath;
		switch (USE_VOLUME) {
		case 0:
			volumePath = desktop_directory();
			//strcat(volumePath, "\\clouds\\cloud-1196.xyz");
			//strcat(volumePath, "\\clouds\\cloud-1191.xyz");
			//strcat(volumePath, "\\clouds\\cloud-1940.xyz");
			//strcat(volumePath, "\\clouds\\cloud-190.xyz");
			//strcat(volumePath, "\\clouds\\cloud-1090.xyz");
			strcat(volumePath, "\\clouds\\cloud-1196.xyz");
			volume = new Volume(volumePath);
			lightSource->Position = float3(0.4, 0.5, 0.3);
			lightSource->Direction = normalize(float3(1, 1, -1));
			lightSource->Intensity = float3(6);
			camera->Position = float3(0, 0, 1);
			camera->Target = float3(0, 0, 0);

			break;
		}

		asVolumeRenderer->SetVolume(volume);
	}

	if (asCameraRenderer) {
		asCameraRenderer->Camera = camera;
	}

	if (asLightRenderer) {
		asLightRenderer->Light = lightSource;
	}

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Platform/Renderer bindings
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX12_Init(presenter->getInnerD3D12Device(), 3, 
        DXGI_FORMAT_R8G8B8A8_UNORM,
        presenter->getDescriptorsHeapForGUI()->GetCPUDescriptorHandleForHeapStart(), 
		presenter->getDescriptorsHeapForGUI()->GetGPUDescriptorHandleForHeapStart());

    ImGui::StyleColorsClassic();

    io.Fonts->AddFontDefault();

	// Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

	static float lastTime = ImGui::GetTime();
	static bool firstFrame = true;

	static int CurrentFrame = 0;

	static int GeneratedImages = 0;

	static bool ShowGUI = true;

	srand(GetTickCount());

    while (msg.message != WM_QUIT)
    {
		float deltaTime = ImGui::GetTime() - lastTime;
		lastTime = ImGui::GetTime();

		if (asLightRenderer)
		{
			// Light Updates here
			if (MOVE_LIGHT)
				asLightRenderer->Light->Position.y = 0.3+sin(ImGui::GetTime()*0.5f)*0.1f;
		}

        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application. 
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

		static int magnifierSize = 30;
		static int magnifierPos[2] = { 300, 300 };
		static bool magnifierOpen = true;

		if (magnifierOpen && ShowGUI)
		{
			ImGui::Begin("Magnifier frame", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
			ImGui::SetWindowPos(ImVec2(magnifierPos[0] - magnifierSize - 9, magnifierPos[1] - magnifierSize - 9));
			ImGui::SetWindowSize(ImVec2(magnifierSize * 2 + 17, magnifierSize*2 + 17));
			ImGui::Image((void*)presenter->getCurrentRTInGUI().ptr, ImVec2(magnifierSize*2+1, magnifierSize*2+1),
				ImVec2(0,0),
				ImVec2(1,1), ImVec4(0,0,0,0), ImVec4(1,1,1, 1));
			ImGui::End();

			ImGui::Begin("Magnifier", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration );

			//static ImVec2 mousePos = ImVec2(presenter->getWindowWidth() / 2, presenter->getWindowHeight() / 2);
			//ImGui::InputFloat2("Mag pos", (float*)&mousePos);

			ImVec2 mousePos = ImVec2(magnifierPos[0], magnifierPos[1]);// ImGui::GetMousePos();

			float pixelWidth = 1.0 / presenter->getWindowWidth();
			float pixelHeight = 1.0 / presenter->getWindowHeight();
			
			ImGui::Image((void*)presenter->getCurrentRTInGUI().ptr, ImVec2(300, 300),
				ImVec2(mousePos.x* pixelWidth - pixelWidth * magnifierSize, mousePos.y* pixelHeight - pixelHeight * magnifierSize),
				ImVec2(mousePos.x* pixelWidth + pixelWidth * magnifierSize, mousePos.y* pixelHeight + pixelHeight * magnifierSize), ImVec4(2, 2, 2, 1), ImVec4(0, 0, 0, 1));

			ImGui::End();
		}

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
		{
			ShowGUI = !ShowGUI;
		}

		if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
			ScreenShotDesired = true;

		if (ShowGUI)
        {
            ImGui::Begin("Rendering over DX12");                          // Create a window called "Hello, world!" and append into it.

			lastFrameTimeInMS = 1000.0f / ImGui::GetIO().Framerate;

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", lastFrameTimeInMS, ImGui::GetIO().Framerate);

			RenderGUI<IHasBackcolor>(technique);
			RenderGUI<IHasTriangleNumberParameter>(technique);
			RenderGUI<IHasParalellism>(technique);
			RenderGUI<IHasLight>(technique);
			RenderGUI<IHasScene>(technique);
			RenderGUI<IHasRaymarchDebugInfo>(technique);
			RenderGUI<IHasHomogeneousVolume>(technique);
			RenderGUI<IHasVolume>(technique);
			RenderGUI<IHasAccumulative>(technique);
			RenderGUI<IHasScatteringEvents>(technique);

			if (magnifierOpen) {
				ImGui::SliderInt("Magnifier size", &magnifierSize, 10, 100);
				ImGui::InputInt2("Magnifier pos", magnifierPos);
			}

            ImGui::End();

			{
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
				{
					movingCamera++;
					if (movingCamera == ARRAYSIZE(cameraPositions))
						movingCamera = 0;
				}

				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V)))
				{
					magnifierOpen = !magnifierOpen;
				}

				
				bool cameraChanged = CurrentFrame == 1;

				
				if (movingCamera >= 0) {
					float d = length(camera->Position - cameraPositions[movingCamera]);
					cameraChanged = d > 0;
					float alpha = d == 0 ? 1 : min(d, 0.01) / d;
					camera->Position = lerp(camera->Position, cameraPositions[movingCamera], alpha);
					camera->Target = camera->Position + normalize(lerp(camera->Target, cameraTargets[movingCamera], alpha) - camera->Position) * 0.03;
				}

				auto delta = ImGui::GetMouseDragDelta(1);
				if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Space)))
					camera->RotateAround(delta.x*0.01f, -delta.y*0.01f);
				else
					camera->Rotate(delta.x*0.01f, -delta.y*0.01f);

				auto guiDelta = ImGui::GetMouseDragDelta(0);
				if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_RootAndChildWindows) &&
					!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
				{
					magnifierPos[0] += guiDelta.x;
					magnifierPos[1] += guiDelta.y;
				}
				ImGui::ResetMouseDragDelta(0);

				if (delta.x != 0 || delta.y != 0)
				{
					movingCamera = -1;
					cameraChanged = true;
					ImGui::ResetMouseDragDelta(1);
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
				{
					movingCamera = -1;
					camera->MoveForward(deltaTime*10);
					cameraChanged = true;
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
				{
					movingCamera = -1;
					camera->MoveBackward(deltaTime * 10);
					cameraChanged = true;
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
				{
					movingCamera = -1;
					camera->MoveLeft(deltaTime * 10);
					cameraChanged = true;
				}
				if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
				{
					movingCamera = -1;
					camera->MoveRight(deltaTime * 10);
					cameraChanged = true;
				}

				if (asCameraRenderer != nullptr)
					asCameraRenderer->CameraIsDirty = cameraChanged || firstFrame || PERMANENT_CAMERA_DIRTY;

				if (asLightRenderer != nullptr)
					asLightRenderer->LightSourceIsDirty |= firstFrame || MOVE_LIGHT;
			}
        }

        // Rendering
		presenter->Present(technique);
		CurrentFrame++;

#ifdef GENERATE_IMAGES
		if (CurrentFrame > 1000 && GeneratedImages < GENERATE_IMAGES) {

			char screenShotName[100];
			ZeroMemory(screenShotName, 100);
			char screenShotNumber[100];
			ZeroMemory(screenShotNumber, 100);
			itoa(GeneratedImages, screenShotNumber, 10);
			strcat(screenShotName, "screenshot");
			strcat(screenShotName, screenShotNumber);
			strcat(screenShotName, ".jpg");

			screenshot->FileName = screenShotName;
			presenter->Present(screenshot);
			presenter->Present(screenshot);
			GeneratedImages++;

			CurrentFrame = 0;

			if (asCameraRenderer != nullptr) // generate a new camera position
			{
				movingCamera = -1;
				int referenceCameraIndex = rand() % ARRAYSIZE(cameraPositions);
				asCameraRenderer->Camera->Position = cameraPositions[referenceCameraIndex] + (generateInBox() * 2 - 1) * 0.3;
				asCameraRenderer->Camera->Target = (generateInBox() * 2 - 1) * 0.01;
				//asCameraRenderer->Camera->Target = (generateInBox() * 2 - 1) * 0.14;
			}
		}


#endif
		// Screen shot
		if (ScreenShotDesired) {
			presenter->Present(screenshot);
			presenter->Present(screenshot);
			ScreenShotDesired = false;
		}

		if (firstFrame)
			ImGui::GetIO().Framerate;

		firstFrame = false;

		if (asLightRenderer)
			asLightRenderer->LightSourceIsDirty = false;

		//Sleep(500);
    }

	presenter->Close();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(hWnd);
    UnregisterClass(_T("CA4G_Samples_Window"), wc.hInstance);

    return 0;
}
