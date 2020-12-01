# CA4G Project

![Tutorial1](Images/ca4g.jpg)

![Tutorial1](Images/PhotonMap.png)

The CA4G project allows to abstract to developers from the specific details for driving DX12-based applications. This programming can be bothersome for simple tasks such as resources handling, synchronization, and other graphic-related tasks. This header-only library serves as a façade for accessing to such functionalities and at the same time, uses good practices for DX managing guided using most of the Do’s and Don’ts NVidia advices.

Some of the highlighted features added so far includes:

**Triple buffering support**. CA4G uses a tree Backbuffer swap chain and uses synchronization and allocators to exploit CPU and GPU workloads efficiently.

**Basic synchronization tools**: CA4G hides all proposed DX12 synchronization mechanisms and just exposes a Signal concept. Signals can be trigger by a graphic process and can be waitable for CPU programs to wait for GPU execution. Also, expose functions to flush pending CPU work to the GPU. All resource access barrier are managed by the engine.

**Multi-engine support**: CA4G encapsulates the graphics pipeline usage through Technique concept and execution of process. Processes are methods receiving a command list manager. Depending on the type of the command list (Copying, Compute, Graphics) a different engine is used (i.e. Command Queue).

**Multi-threading support**: CA4G device manager has up to 8 threads for command population. All asynchronous processes are queued in producer-consumer queue that deploys process execution across all threads. CA4G exposes functions to flush all pending work to the GPU and wait (on the CPU) for it.

**Typed Resource View**: DX12 unifies resource concept in just one type (`ID3D12Resource`). Nevertheless, CA4G wraps this concept internally and only exposes resource views. Resource views are typed versions of a resource that has the information of the “view” (Descriptors) and other external functionalities for sub-resource managing, slicing, among others. Sub-resources can be treated the same way of resources.

Typed resource view helps in other aspects such as binding, root descriptor construction and static type-check of resource handling.

**Customizable Pipeline state objects**: In CA4G there is a concept Pipeline Bindings with two roles. Representing bindings to the pipeline (resources, constants, samplers, render targets, depth buffers) used to construct the Root Signature object and, in the other hand, manage all settable states to the pipeline exposing functionalities to setup, such as Input Layout, Depth Tests, Rasterizer state, shader stages, etc., used to create the `ID3D12PipelineStateObject`.

**DXR support**: CA4G can access to DXR engine in DX12. If there is not hardware support for this interface the CA4G will use the fallback device instead. DXR can be used through CA4G in a friendly manner hiding bothersome tasks and settings.

## Presenter Class

Presenter object is the "factory" class proposed for rendering through DX12. This class allows to load Techniques, manage triple buffering and other specific tasks for render output presenting.

Use `Load` method to load and initialize a technique, and the use `Present` method to execute the graphic process represented in the technique. Most of the objects defined in CA4G needs to be managed using smart pointers. Pointers to CA4G graphic objects are managed using `gObj<T>` wrapper.

Next code shows a presenter object creation using a handle to the rendered window. Additional parameters may be included for full screen, number of buffers in swap chain and a to indicate if a warp device should be used instead of hardware.

```c++
static gObj<Presenter> presenter = new Presenter(hWnd);
```

​	The graphic loop presenting a specific technique can be as follow:

```c++
static gObj<MyTechnique> technique;

presenter->Load(technique);

MSG msg; // Windows message
ZeroMemory(&msg, sizeof(msg));

while(msg.message != WM_QUIT){
	if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		continue;
	}

	presenter->Present(technique);
}
```

## Technique class

A technique is a class representing the developer's usage intention of the DX12 API. The technique has three special methods:

**Constructor**: Use the constructor of a technique to initialize the object with required parameters. Mostly will be used with constants for the technique (will never change). Creational parameters can be passed to the Technique with the additional variadic parameters of `Presenter::Load` method.

**Startup**: This method will be called once the first time the technique is presented. You may use of any DX12 functionality here (even drawing!), but normally will be used to load assets, preprocess scene, populate bundles, and load resource data. Create pipeline objects or other subtechniques objects.

**Frame**: This method will be called every time the technique is presented in a Presenter object. You may use this method to draw in the render target.

Both methods (Startup and Frame) can access to the `DeviceManager` object used to execute graphics processes. Any method receiving a `CommandListManager` subclass can be considered as a graphics process. The main role of `Startup` and `Frame` method is to call to execute other methods as graphic process. This is the common usage of `PopulateCommandList` method in other DX12 engines.

Graphics process indicates with the parameter which kind of engine will process the command list. This is the way CA4G supports multiple engines usages. Next code shows a Startup method of a technique using a Copy engine to load some assets and a Compute engine to perform some preprocess computation.

```c++
void LoadAssets (gObj<CopyingManager> manager) { ... }

void ComputeSomePreprocessedData(gObj<ComputeManager> manager) { ... }

void Startup() {
	perform(LoadAssets);
    perform(ComputeSomePreprocessedData);
}
```
Notice perform macro hides internally the usage of the internal `DeviceManager` accessible from a technique used to enqueue all tasks. To perform this loading stage asynchronously you just need to enqueue the graphic process asynchronously.

```c++
void LoadAssets (gObj<CopyingManager> manager) { ... }

void ComputeSomePreprocessedData(gObj<ComputeManager> manager) { ... }

void Startup() {
    perform_async(LoadAssets);
    perform_async(ComputeSomePreprocessedData);
}
```

CA4G manages internally a fixed number (defined normally 8) of workers. The main thread host the synchronous worker, and the remain will wait for async tasks to do. This is implemented using a producer consumer queue internally and threads are waiting for new task to be enqueue. That means that every perform_async doesn't represent a new thread is created but that the async process is enqueue for future execution in some engine.

### Synchronizing CPU-GPU work

The proposed mechanism for synchronization in CA4G is the Signal concept. Every time the user use perform_async to enqueue some graphics process asynchronously, this process is enqueued. Using flush method will produce all pending work to be completed (finish populating all pending works in queue) and executed by their respective command queue (engines).​		

```c++
perform_async(LoadAssets);
perform_async(ComputeSomePreprocessedData);
getManager()->Flush(ENGINE_MASK_ALL);
```

There is a macro for such command that can be more easy to read.

```c++
flush_all_to_gpu;
```

Notice that this code means: "I want every pending work for every engine to be sent to the GPU and will wait until that occurs", but that doesn't mean the work was really finished on the GPU. Nevertheless, this command serves as a barrier for different graphic process you want to synchronize on the GPU.

For CPU-GPU synchronization you will need to send a signal through the GPU that will "trigger" an event object in CPU.

For this purpose you may use the `SendSignal` method.

```c++
auto signal = getManager()->SendSignal(ENGINE_MASK_ALL);
```

This method will send a signal through the specific engines and return an object that has all needed values for fencing strategy. Notice you may signal through just one engine and not the others, or through all engines.

Then the CPU can wait for a specific signal object to trigger the event using the `WaitFor` method.

```c++
getManager()->WaitFor(signal);
```

The proposed DSL via macros allows to write this commands in a more readable way. Next code shows a simple combination of previous methods, that waits for the GPU completion of some CPU commanding processes.

```c++
wait_for(signal(flush_all_to_gpu));
```

> All device manager methods for work submission and synchronization must be used only by `Setup` and `Frame` method execution. Never use these methods inside a graphics process method because they are executed by demand and only represents command list populating processes.

## Graphics Processes

All GPU work is considered as a graphics process in CA4G. DX12 unified all work submission to the GPU using Command lists of different engines. The `CommandListManager` class in CA4G is designed for such purpose. This class has several subclasses, one for each different command list type. When a technique calls to perform a graphics process, is really saying "populate a command list with the commands in that method".

The related commands in a graphics command list are grouped depending on the purpose. For instance, all clearing functions are grouped together in a clearing inner object exposed in a command list manager. This is intended for the DSL usage. Next code shows a sample of graphics process that only clears the render target with a specific color.

```c++
void Frame() {
	// Every frame perform a single process using a Graphics Engine
	perform(GraphicProcess);
}

// Graphic Process to clear the render target with a backcolor
void GraphicProcess(GraphicsManager *manager) {
	// render_target macro expands to getManager()->getBackBuffer()
    // returing the current frame render target being rendered.
	manager	gClear RT(render_target, Backcolor);
}
```
Grouping inner objects used are intended for:

**setting**: The function represents a set or binding to the object of some feature. Macro `gSet`. Used for setting states to the pipeline in a `CommandListManager` or settings states to the `PipelineBindings` object.

**clearing**: The function represents a clear action over other objects. Macro `gClear`. Used in a `CommandListManager` object. 

**loading**: The function represents an action of loading an existing object (and initialize). Macro `gLoad`. Used for loading `Technique` objects and `PipelineBindings` objects.

**dispatcher**: The function represents an action of submitting some draw call or dispatch. Macro `gDispatch`. Used to perform draw calls in a `GraphicsManager` object.

**copying**: The function is intended to copy a buffer from one resource or memory to another. Macro `gCopy`. Used in all `CommandListManager` objects to copy from CPU to GPU and vice versa.

**creating**: The function is intended to create an empty object of a specific type. Macro `gCreate`. Used to create resource objects.

## Resource Management

Despite DX12 proposes an unified resource handling through `ID3D12Resource` interface, in CA4G is used a typed version again. That means that there are several types for each kind of representation a resource may have. For instance, a Buffer, a Texture2D or even a Texture2DMS.

The reason for this is to hide resource object from developers and export all the time the object managed as a `ResourceView`. Resource views hides internally the real resource and exposes functions for querying description of the resource, slicing (gets a range of sub-resources), among others functionalities.

Resources can be created by a `DeviceManager` object. Since technique manage internally a device manager in both, startup and frame methods, a resource can be created any time. Every resource usage is protected via smart pointers using the `gObj<T>` wrapper.

```c++
gObj<Buffer> vertices = nullptr;
...
void Startup(){
	vertices = this->creating->VertexBuffer<VERTEX>(3);
}
```

With the proposed DSL the code below looks like next.

```c++
gObj<Buffer> vertices = nullptr;
...
void Startup(){
	vertices = _ gCreate VertexBuffer<VERTEX>(3);
}
```

In order to fill a resource with data, it may be used a `CopyingManager` graphics process. This is the most basic engine proposed in DX12 and in some architectures, submitting work to different engines might represent real parallel work submission to the GPU.

```c++
void Startup() {
	...	
	// Create a simple vertex buffer for a triangle
	vertices = _ gCreate VertexBuffer<VERTEX>(3);
	// Performs a copying commanding execution for uploading data to resources
	perform(UploadData);
}

// A copy engine can be used to populate buffers using GPU commands.
void UploadData(gObj<CopyingManager> manager) {
	// Copies a buffer written using an initializer_list
	manager	gCopy ListData(vertices, {
			VERTEX { float3(0.5, 0, 0), float3(1, 0, 0)},
			VERTEX { float3(-0.5, 0, 0), float3(0, 1, 0)},
			VERTEX { float3(0, 0.5, 0), float3(0, 0, 1)},
		});
}
```
Every resource view manage internally the different descriptors needed for each kind of view. For instance, a `Texture2D` used as a shader resource and then as a render target will cache both descriptors internally.

> This process is hidden from the developer, so you will never need to deal with descriptors, bindings, copying descriptors, descriptor heaps, null descriptors, visible and non-visible descriptor heaps, heaps ring-buffer allocations, resource barriers, resources uploading and readback versions, sub-resources footprints and other DX12 overwhelming stuff. You are very welcome.

### Textures

Texture objects can be created by different ways. First, a normal texture to represent an image.

```c++
manager gLoad FromFile(myTexture, "c:\\Users\\...\\Desktop\\Models\\skybox.dds");
```

This method creates the texture object using the data loaded from an image path. Imaging functionalities of CA4G uses a project published in GitHub. DirectXTex. (link here).

If you need an empty texture you may use:

```c++
auto myTexture = _ gCreate DrawableTexture2D<float4>(512, 512);
```

This texture object is ready for UAV and Render Target purposes. You may indicate how many mipmaps to allocate an the initial state for the internal resource.

Depending on the resource kind, you may access to sub-resources and treat them as resources. Updating data to a resource and sub-resources, binding resources and sub-resources, are treated the same in CA4G.

Imaging you have a texture array created representing a `CubeTexture`. You can use each different cube face texture as a render target in some process.

```c++
for (int i=0; i<6; i++)
{
	gObj<Texture2D> face = myCubeTexture->CreateArraySlice(i);
	RenderFace(i, face, manager);
}			
```

Next sample shows how to populate an array with all sub-resources of a Texture2D.

```c++
// Compute number of subresources
subresourcesCount = Texture->getMipsCount()*Texture->getSlicesCount();
// Array of subresources views
Subresources = new gObj<Texture2D>[subresourcesCount];

int index = 0;
for (int j = 0; j < Texture->getSlicesCount(); j++)
	for (int i = 0; i < Texture->getMipsCount(); i++)
		// Creates a subresource view for each posible array slice and mip slice.
        Subresources[index] = Texture->CreateSubresource(j, i);
```

Texture pixel format can be specified using regular types, vector types (`float2`, `float3`,`float4`, ...) and other types representing color components in an unsigned integer using different distributions, `RGBA` and `ARGB` structs. Scalar types such as `int`, `unsigned int`, `short`, `byte`, ..., are supported as well.

## Pipeline objects and resource bindings

DX proposes a new concept to setup most of the render states required during draw calls and dispatching, the `ID3D12PipelineObjectState`. This object is constructed with all data regarded to states that will be set on the GPU to setup the graphics pipeline. These states include shaders, rasterizer states, depth test, stencil tests, input layout description, information about bound render targets, depth buffers, etc.

Other states needs to be setup outside pipeline state object, such as viewport, scissor rectangle, vertex buffer and index buffer bindings.

In the other hand, bindings of resources to the GPU for shaders accessibility is done creating descriptor heaps, and copying needed descriptors to that memory, manage a root signature to know how pipeline shaders can access to such resources, and setup the offset of each entry in a root signature to the descriptor heap.

In CA4G there is a class designed to manage everything related to the pipeline object and also includes the binding purpose. So, all the work described below is hidden for the developer.

For CA4G, a `PipelineBindings` object is a generic class that builds a setting object as a mixin of all render state manager object can be set (Notice that depending on the pipeline usage you will require some states or others...). Nevertheless, a common usable version of `PipelineBindings` will be a `GraphicsPipelineBindings` and we will start explaining from it.

### Pipeline Bindings for Graphics purposes

Every draw call needs a pipeline set before with the information about what shaders to set, rasterization options, etc.. You may define such settings in a single typed object inheriting from `GraphicsPipelineBindings`.

This subclass has three methods to override:

**Setup**: This method will be invoked to set all values proposed for each pipeline state. I.e. rasterizer states (cull mode, fill mode). In this method, shaders must be set as well.

**Globals**: This method will be used to collect all global resource bindings proposed for this pipeline. All resources and samplers fields declared as bound in this method will be updated once the pipeline object is set to the pipeline.

**Locals**: This method will be used to collect all local resource bindings proposed for this pipeline. All resources and samplers fields declared as bound in this method will be updated just before any draw call to the pipeline.

Next code shows a sample of pipeline bindings implementation.

```c++
struct MyBasicPipeline : public GraphicsPipelineBindings {
    // Render target this pipeline will use
    gObj<Texture2D> renderTarget;
    // Setup method is overriden to load shader codes and set other default settings
    void Setup() {
        // Loads a compiled shader object and set as vertex shader
        auto vsBytecode = LoadByteCode("NoTransforms_VS.cso");
        _ gSet VertexShader(vsBytecode);
        // Loads a compiled shader object and set as pixel shader
        auto psBytecode = LoadByteCode("SimpleColor_PS.cso");
        _ gSet PixelShader(psBytecode);
        // Setting the input layout of the pipeline
        _ gSet InputLayout({
            VertexElement { VertexElementType_Float, 3, "POSITION"},
            VertexElement { VertexElementType_Float, 3, "COLOR"}
        });
    }
    void Globals() {
        // Only one binding when this pipeline state object is set.
        RTV(0, renderTarget);
    }
}
```

> `Globals` and `Locals` are used when the `PipelineBindings` object is initialized internally (loaded in a technique) for collecting field references. Those methods will be only called during initialization and used to create root signature for the pipeline object state. Every time the pipeline object needs to be set or draw call requires for locals, the fields of the object are requested (accessed using a store reference pointer) and bind the real resource.
>
> Therefore, it is an error to implement such methods using conditionals or loops that can be referring to a value will change after initialization.

For binding use the following methods:

`CBV(slot, &resource, shaderType, space)`: Specifies a constant buffer must be bound to the slot of a specific shader stage.

`SRV(slot, &resource, shaderType, space)`: Specifies a shader resource view must be bound to the slot of a specific shader stage.

`SRV_Array(slot, &resources, &count, shaderType, space)`: Specifies a range of shader resource views of certain length (count) must be bound to the slot of a specific shader stage.

`UAV(...)`: Bounds a resource as an UAV.

`RTV(...)`: Bounds a texture as a render target.

`DSV(...)`: Bounds a texture as depth stencil view.

`Static_SMP(...)`: Bounds a sampler object as a static sampler in the root signature. The sampler object will be queried at initialization so it must be a constant value.

### Building Custom Pipeline State Object

Pipeline state objects are built from small pieces named pipeline subobject states. Each subobject type has a flag indicating the type and the specific value to set after it. DX12 supports a flexible mechanism for building customs pipeline state object designing a struct to describe the stream with all pipeline object setup.

In CA4G this feature is supported using a mixin strategy using inheritance and templates. In fact, the `GraphicsPipelineBindings` object used in the sample below is defined as follows.

> ```c++
> struct GraphicsPipelineBindings : public PipelineBindings <
> 	DebugStateManager,
> 	VertexShaderStageStateManager,
> 	PixelShaderStageStateManager,
> 	DomainShaderStageStateManager,
> 	HullShaderStageStateManager,
> 	GeometryShaderStageStateManager,
> 	StreamOutputStateManager,
> 	BlendingStateManager,
> 	BlendSampleMaskStateManager,
> 	RasterizerStateManager,
> 	DepthStencilStateManager,
> 	InputLayoutStateManager,
> 	IndexBufferStripStateManager,
> 	PrimitiveTopologyStateManager,
> 	RenderTargetFormatsStateManager,
> 	DepthStencilFormatStateManager,
> 	MultisamplingStateManager,
> 	NodeMaskStateManager,
> 	RootSignatureStateManager
> > {
> };
> ```

Each state manager struct used here has two roles. First the expanded memory required by the DX12 pipeline object construction method. Second, each manager has specific tool methods to easily set/modify values for that specific state "slot". For instance, the `DepthStencilStateManager` has the functions shown next:

```c++
void NoDepthTest() { ... }
void DepthTest (enable, write, comparison) { ... }
void StencilTest(enable, readMask, writeMask) { ... }
void StencilOperationAtFront (...) { ... }
void StencilOperationAtBack (...) { ... }
```

Once the `PipelineBindings` subtype is defined a setting inner type is built with all the mixin and all function to read/set/modify states are accessible from it. Thus, the way to setup a pipeline binding object is via:

```c++
this->setting->DepthTest(true, false, D3D12_COMPARISON_FUNC_GREATER);
```

or using the DSL version:

```c++
_ gSet DepthTest(true, false, D3D12_COMPARISON_FUNC_GREATER);
```

You may use a custom pipeline object for reducing the complexity (memory) of the pipeline state object in your technique or simply to select subobject types with new functionalities available in DX12 such as the `DepthStencilWithDepthBoundsStateManager` object.

### Using a `PipelineBindings` object in a sample

Once you have defined a specialization of a PipelineBindings according to your needs (normally in Startup method of your technique via loading->Pipeline method), you need to bind all required fields with the resources.

Some fields can be left `nullptr` if you are intending to bind a null descriptor to such slot. 

Global fields must be set before pipeline binding object is set on the pipeline. Locals fields must be set before each draw call.

```c++
void GraphicProcess(GraphicsManager *manager) {
    float4x4 view, proj;
    Camera->GetMatrices(render_target->Width, render_target->Height, view, proj);

    // Updates camera buffer each frame
    manager gCopy ValueData(cameraCB, Globals{ proj, view });

    // Setting up per frame bindings
    pipeline->cameraCB = cameraCB;
   	// this is necessary every frame because 3 different render targets 
    // might be used for triple-buffering support.
    pipeline->renderTarget = render_target; 

    manager gClear RT(render_target, Backcolor);

    // Set the pipeline state object
    manager gSet Pipeline(pipeline);
    // Set the viewport to the dimensions of the Backbuffer
    manager gSet Viewport(BackBufferWidth, BackBufferHeight);
    // Set the vertex buffer object to the pipeline
    manager gSet VertexBuffer(vertices);
    // Set the index buffer to the pipeline
    manager gSet IndexBuffer(indices);

    for (int i = 0; i < subresourcesCount; i++)
    {
        // Locals bindings
        pipeline->transformCB = transforms[i];
        pipeline->Texture = Subresources[i];

        // Draw a quad with 4 vertices and 6 indices
        manager gDraw IndexedTriangles(6);
    }
}
```



# Raytracing support

One of the main ideas behind CA4G is to fully support all capabilities of DirectX12 API. Every technique that might be implemented over DirectX12 should be possible to implement using this facade. DXR is not an exception.

In CA4G is presented a new command list manager named `DXRManager`. This command list manager works internally over a DIRECT-type command list (can be cast to `GraphicsManager` if necessary). Similarly to a `GraphicsManager` setup (setting a pipeline object), DXR works setting a `RTPipelineManager` object into the pipeline. Then, developer can decide what ray-tracing program to activate and how to setup shaders (ray-generations, miss and hit groups) to obtain the desired ray-trace process.

For devices that doesn't have DXR hardware support, internally in DXRManager will be used a fallback device object. Developers do not need to handle this.

To explain step-by-step proposed DXR support in CA4G will be used the first example from the DirectX Samples repository. In this example a basic library is defined with three shaders: a shader to generate parallel rays in a normalized viewport (`MyRaygenShader`), a miss shader to paint a solid color when ray misses the geometry (`MyMissShader`), and a closest hit shader (`MyClosestHitShader`) to color ray payload with the barycentric coordinates of the intersection.

On the other hand, the applications needs to setup a single bottom level acceleration data structure with a simple triangle geometry, and a top level acceleration data structure with a single instance to this bottom level geometry. Then a global signature is used to refer to the Scene object (a shader resource view), and the output target (an UAV Texture2D). Only for illustration, a local signature is used to refer to a constant with some other application parameter (the viewports).

The shader was lightly modified from DirectX Samples and is shown next.

```c
struct Viewport
{
	float left;
	float top;
	float right;
	float bottom;
};

struct RayGenConstantBuffer {
	Viewport viewport;
	Viewport stencil;
};

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);

bool IsInsideViewport(float2 p, Viewport viewport)
{
	return (p.x >= viewport.left && p.x <= viewport.right)
		&& (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
	float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

// Orthographic projection since we're raytracing in screen space.
float3 rayDir = float3(0, 0, 1);
float3 origin = float3(lerpValues * 2 - 1, -1);

if (IsInsideViewport(origin.xy, g_rayGenCB.stencil))
{
	// Trace the ray.
	// Set the ray's extents.
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
	// Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	// TMin should be kept small to prevent missing geometry at close contact areas.
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RayPayload payload = { float4(0, 0, 1, 1) };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex().xy] = payload.color;
}
else
{
	// Render interpolated DispatchRaysIndex outside the stencil window
	RenderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
}

}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(1, 0, 1, 1);
}
```

Next, we will explain how this example can be implemented using CA4G concepts to support DXR caps.

## Ray-tracing Pipeline Manager

In CA4G the `RTPipelineManager` class is meant to be inherited specializing some methods to describe what DXIL libraries should loaded and which ray-tracing programs can be loaded from them. That means that it can be loaded 3 DXIL libraries and use all exported shaders to setup one single program.

The program's role is to stablish several shaders will be used in a dispatch rays call, therefore, shares the same global signature and local signatures by shader type.

Lets define for this example a new `RTPipelineManager` subclass object named `DXRBasic`.

```c++
struct DXRBasic : public RTPipelineManager {
...
}
```

This type must expose all shader objects (represented in CA4G with a specific handle object), libraries and programs loaded.

### Wrapping a DXIL library

First, lets use an object to refer a single DXIL library and all its exports.

```c++
class DX_RTX_Sample_RT : public DXIL_Library<DXRBasic> {
	void Setup() {
		_ gLoad DXIL (ShaderLoader::FromFile(".\\Techniques\\Tutorials\\Shaders\\DX_RTX_Basic_RT.cso"));
		_ gLoad Shader(Context()->MyRaygenShader, L"MyRaygenShader");
		_ gLoad Shader(Context()->MyClosestHitShader, L"MyClosestHitShader");
		_ gLoad Shader(Context()->MyMissShader, L"MyMissShader");
	}
};
gObj<DX_RTX_Sample_RT> _Library;


```

Notice shaders are loaded (declared as exported by this library) and fill the handle present in the Context() object. The `Context()` method references to the RT pipeline manager that loaded this library. This is convenient to have only one object with the shader handles. Imagine there are three different libraries with shaders definitions required by a program. Instead of having the same shaders handles replicated in the RT Pipeline Manager object, the library object and the program object, we propose to have them only as fields of the `RTPipelineManager` subclass and the refers to them from other objects using the `Context()` method. The template is used to specify the `RTPipelineManager` type used to load this library, this way, the Context method will be statically typed to the context where this library is being used.

### Defining a Ray Tracing Program

Once a library was loaded, a ray-tracing program can be conceived because required shaders will be available.

The Setup method of a program will load all required shaders and will create hit groups using those shaders. Also, all settings such as `StackSize`, `PayloadSize` and `AttributeSize`, must be fulfilled here if necessary.

```c++
class MyProgram : public RTProgram<DXRBasic>
{
protected:
	void Setup() {
		_ gSet Payload(16);
		_ gLoad Shader(Context()->MyRaygenShader);
		_ gLoad Shader(Context()->MyMissShader);
		_ gCreate HitGroup(ClosestHit, Context()->MyClosestHitShader, {}, {});
	}

	void Globals() override {
		UAV(0, Output);
		ADS(0, Scene); // Acceleration Data Structure
	}

	void RayGeneration_Locals() override {
		CBV(0, RayGenConstantBuffer);
	}
public:
	// Generated by this program
	gObj<HitGroupHandle> ClosestHit;
	
	// Input
	gObj<SceneOnGPU> Scene;
	gObj<Texture2D> Output;
	struct Viewport { float l, t, r, b; };
	struct RayGenConstantBuffer {
		Viewport viewport;
		Viewport stencil;
	} RayGenConstantBuffer;

};
gObj<MyProgram> _Program;
```

The `Globals` method will define all bindings required when this program is activated on the pipeline. This method is used to declare the global signature and all loaded shaders (and hit groups created) in this program are automatically associated to this root signature.

The method RayGeneration_Locals needs to be overridden in order to specify this ray generation shader will use a local data store in the shader table (this is not necessary because only one ray-generation shader can be used in a dispatch ray but was used to illustrate how local signatures are created and associated to shaders).

This program creates a hit group with a closesthit shader and no intersection, neither anyhit shaders.

In next sections will be explained in detail the role of the `SceneOnGPU` object. Now, consider this object as a reference (by any means necessary) to the top level acceleration data structure. 

Once we defined the library wrappers and ray-tracing programs, the RTPipelineManager subobject can be completed.

```c++
struct DXRBasic : public RTPipelineManager {

	gObj<RayGenerationHandle> MyRaygenShader;
	gObj<ClosestHitHandle> MyClosestHitShader;
	gObj<MissHandle> MyMissShader;

	class DX_RTX_Sample_RT : public DXIL_Library<DXRBasic> {...};
	gObj<DX_RTX_Sample_RT> _Library;

	class MyProgram : public RTProgram<DXRBasic> {...};
	gObj<MyProgram> _Program;

	void Setup() override {
		_ gLoad Library(_Library);
		_ gLoad Program(_Program);
	}
}
```

The CA4G façade hides internally in this object the construction of the DX12 state object. The other functionality of a program object is to handle internally shader tables and resource bindings (to the GPU or to the shader table entries).

## Building the Acceleration Structures

In CA4G, the process of building the bottom level acceleration DS is handled by a type named `GeometryCollection`. This object can be created by a `DXRManager` and stores geometries (using load commands) and then creates a buffer with all the bottom level structure using the method in creating module `BakedGeometries()`.

```c++
void BuildScene(gObj<DXRManager> manager) {
	auto geometries = manager gCreate TriangleGeometries();
	geometries gSet VertexBuffer(vertices, VERTEX::Layout());
	geometries gLoad Geometry(0, 3);
	auto geometriesOnGPU = geometries gCreate BakedGeometry();
    
	auto instances = manager gCreate Instances();
	instances gLoad Instance(geometriesOnGPU);
	this->Scene = instances gCreate BakedScene();
}
```

After this, the top level data structure can be created (because will reference the instances to the bottom level objects).

For this, the `InstanceCollection` object is used. This object can be created with the `Intances` method in the `DXRManager::Creating` module.

Finally the `SceneOnGPU` object (mostly top level acceleration data-structure) can be created using the `BakeScene` method.

## Dispatching rays

```c++
void Raytracing(gObj<DXRManager> manager) {
	auto rtProgram = pipeline->_Program;
	rtProgram->Output = rtRenderTarget;
	rtProgram->Scene = this->Scene;
	rtProgram->RayGenConstantBuffer = {
		{ -0.9, -0.9, 0.9, 0.9},
		{-0.9, -0.9, 0.9, 0.9}
	};
	manager gSet Pipeline(pipeline);
	manager gSet Program(rtProgram);
	manager gSet Miss(pipeline->MyMissShader, 0);
	manager gSet HitGroup(rtProgram->ClosestHit, 0);
	manager gSet RayGeneration(pipeline->MyRaygenShader);
	manager gDispatch Rays(render_target->Width, render_target->Height);
	manager gCopy All(render_target, rtRenderTarget);
}
```

As shown in this code, the "rendering" process of the ray-tracing program can be as follows.

First, set all necessary resources to the program before bindings. Then set the pipeline object and after this activate (with gSet) the program to bind. Notice a program is not forced to have a single ray-generation shader, but it must be defined (set) later.

Set all necessary shaders into the pipeline (this will update necessary shader tables with the shader identifiers and bind local resources). Miss and HitGroup methods receives a way of indexing the current bindings. That means, if you have two different miss shaders you will need to assign for each all local bindings before calling to `Miss (..., index)`.

The dispatch rays command will use all shader tables of the active program and perform the ray-tracing process. Then a copy command can be used to save from the generated UAV object to the render target.

![DXRSample1](Images/DXRSample1.jpg)



# Tutorials

Next sections will illustrate step by step the implementation of several tutorial samples. These tutorials are hosted in the same application. This application will create a technique (different for every sample) and populate the technique with some application data (backcolor, camera, light, other parameters).

The GUI is based in the ImGui project support for DX12 (link here).

The main loop of this application will call the presenter constantly with the created technique. Present method will execute an automatic flush of pending graphics to the GPU.

## Tutorial 1. Creating a basic technique to clear the render target.

 This tutorial shows how to define a technique and the main functions involved.

First, every technique inherit from Technique. A mixin strategy is used to add some application data such as camera description, backcolor, scene to render, lighting and other parameters. Next code shows a technique named `Tutorial1` that will receive a `float3` value from app named `Backcolor`.

```c++
class Tutorial1 : 
	public Technique, 
	public IHasBackcolor {
	// Implement technique methods here...
};
```

Every time a technique is presented, the `Frame` method will be invoked. Overriding this method is the way to express any per-frame logic for our graphics. In this case, we will clear the render target with a simple color.

The Frame method (and discussed later `Startup` method)  in a `Technique` has access to a `DeviceManager` object using `getManager` method. This object allows to create objects, load other techniques, or perform graphics process in order to populate command lists. This last action is simplified with the `perform` macro. Next code shows how to perform a graphics process using a specific Direct Engine (`GraphicsManager`).

```c++
void Frame() {
	perform(GraphicProcess);
}

void GraphicProcess(GraphicsManager *manager) {
	manager	gClear RT(render_target, Backcolor);
}
```

Notice `Backcolor` field was added by the `IHasBackcolor` trait.

![Tutorial1](Images/Tutorial1.jpg)

## Tutorial 2. Populating commands asynchronously.

The purpose of `Frame` and `Startup` methods is to use a `DeviceManager` to perform some graphics commanding and flush to the GPU for proper execution. A simple way to populate asynchronously two command list in CA4G is shown next.

```c++
void Frame() {
	perform_async(GraphicProcess1);
	perform_async(GraphicProcess2);
}

void GraphicProcess1(GraphicsManager *manager) {
	manager gClear RT(render_target, Backcolor); 
}
void GraphicProcess2(GraphicsManager *manager) {
	manager gClear RT(render_target, float3(1,1,1) - Backcolor);
}
```

When this sample is run, two colors will be used randomly to clear the render target depending on race condition showing the effect of the asynchronous execution behind.

## Tutorial 3. A basic pipeline setup.

This tutorial shows the definition of a pipeline bindings object in CA4G. This object plays several roles: Constructs the root signature, defines the pipeline state object and performs bindings of necessary resources to the visible descriptor heap when necessary.

First, define a new class to express your specific pipeline bindings extending `GraphicsPipelineBindings` class. Our bindings object needs at least the render target will be used to render at. A `Texture2D` field will be used to access to the necessary resource during binding.

```c++
struct BasicPipeline : public GraphicsPipelineBindings {
    gObj<Texture2D> renderTarget;
	// Implement pipeline bindings methods here...
}
```

Next, will define the `Setup` method. This method is in charge of loading shaders and setting up the pipeline states.

Assuming there are two compiled shaders objects in respective files with next hlsl codes...

> NoTransforms_VS.hlsl

```hlsl
struct VS_In {
	float3 P : POSITION;
	float3 C : COLOR;
};

struct PS_In {
	float4 P : SV_POSITION;
	float3 C : COLOR;
};

PS_In main(VS_In input)
{
	PS_In Out = (PS_In)0;
    Out.P = float4(input.P, 1);
    Out.C = input.C;
    return Out;
}
```

> SimpleColor_PS.hlsl

```hlsl
struct PS_In {
	float4 P : SV_POSITION;
	float3 C : COLOR;
};

float4 main(PS_In input) : SV_TARGET
{
	return float4(input.C, 1);
}
```

Loading the shaders and setting in the pipeline states can be as follows.

```c++
void Setup(){
    _ gSet VertexShader(LoadByteCode(".\\Techniques\\Tutorials\\Shaders\\NoTransforms_VS.cso"));
    _ gSet PixelShader(LoadByteCode(".\\Techniques\\Tutorials\\Shaders\\SimpleColor_PS.cso"));
    _ gSet InputLayout({
        VertexElement { VertexElementType_Float, 3, "POSITION"},//float3 P : POSITION
        VertexElement { VertexElementType_Float, 3, "COLOR"}    //float3 C : COLOR
    });
}
```

Notice the importance of setting the input layout into the pipeline state object. This input layout should be sufficient for all demanding vertex shader attributes input for bound vertex program.

Next part of this pipeline bindings object construction is to setup the necessary bindings for every per-set and per-draw bindings.

Methods `Globals` and `Locals` are overridden to "collect" the bindings of necessary resources to the pipeline. These methods will be called only during pipeline bindings object initialization, populating both list of bindings per-set and bindings per-draw. That implies that any kind of loop or conditional logic written in those methods should be based on constants values during initialization because the root signature is built using calls to binding methods (`SRV`, `UAV`, `CBV`) in that moment.

For this sample, the only resource needs to be bound to the pipeline when set is the render target will used to render graphics.

```c++
void Globals() {
	// Only one bind when this pipeline state object is set.
	RTV(0, renderTarget);
}
```
Other tutorials will show more complex bindings, for instance, samplers, static samplers, textures, UAV and resource arrays.

Once the binding object is defined, the technique will use it to setup the pipeline before drawing.

Next technique (Tutorial3) implementation render a simple triangle with position and color. This technique requires the vertex buffer, hence it is necessary to implement an startup logic to create and load the vertex data.

The technique necessary fields and other definitions.

```c++
struct VERTEX {
	float3 Position;
	float3 Color;
};

// Object for pipeline bindings 
gObj<BasicPipeline> pipeline;
// Buffer object created for storing vertexes
gObj<Buffer> vertices;
```

Now defining the method will populate commands for data uploading to the GPU. This method can use a `CopyingManager` engine because will be used only for copy functions.

```c++
void Startup() {
    // Creates the pipeline and close it to be used by the rendering process
    _ gLoad Pipeline(pipeline);

    // Create a simple vertex buffer for the triangle
    vertices = _ gCreate VertexBuffer<VERTEX>(3);

    // Performs a copying commanding execution for uploading data to resources
    perform(UploadData);

}

// A copy engine can be used to populate buffers using GPU commands.
void UploadData(CopyingManager *manager) {
	// Copies a buffer written using an initializer_list
    manager	gCopy ListData(vertices, {
    	VERTEX { float3(0.5, 0, 0), float3(1, 0, 0)},
        VERTEX { float3(-0.5, 0, 0), float3(0, 1, 0)},
        VERTEX { float3(0, 0.5, 0), float3(0, 0, 1)},
	});
}
```

The first time the technique is presented, `Presenter` class will call to startup method and then the first frame  via `Frame` method.

```c++
void Frame() {
	// Every frame perform a single process using a Graphics Engine
	perform(GraphicProcess);
}
```

Method to collect graphics process command list might be implemented as follows.

First, receive a `GraphicsManager` object since we will use it for drawing.

```c++
void GraphicProcess(GraphicsManager *manager) {
	//...
}
```

Before setting the pipeline object, every necessary resource must be attached to it.

```c++
// this is necessary every frame because 3 different render targets 
// can be used for triple-buffering support.
pipeline->renderTarget = render_target; 
```

Next, ask manager to clear the render target with the back color.

```c++
manager gClear RT(render_target, Backcolor);
```

For this sample, three states are necessary to setup. The pipeline bindings object, the viewport (includes the scissor rectangle) and the vertex buffer to use.

```c++
// Set the pipeline state object
manager gSet Pipeline(pipeline);
// Set the viewport to the dimensions of the Backbuffer
manager gSet Viewport(BackBufferWidth, BackBufferHeight);
// Set the vertex buffer object to the pipeline
manager gSet VertexBuffer(vertices);
```

Finally we can draw the triangle primitive using 3 vertices from the bound vertex buffer as next:

```c++
// Draw a triangle with 3 vertices
manager gDraw Triangles(3);
```

![Tutorial3](Images/Tutorial3.jpg)

