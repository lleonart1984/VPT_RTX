#pragma once
#include "stdafx.h"

#define BUNNY_OBJ 0
#define CORNELL_OBJ 1
#define RING_OBJ 2
#define BUDDHA_OBJ 3
#define SPONZA_OBJ 4
#define SIBENIK_OBJ 5
#define SANMIGUEL_OBJ 6

#define USE_SCENE BUDDHA_OBJ

#define USE_VOLUME 0

#define MOVE_LIGHT false

// Change this to force every frame camera dirty
#define PERMANENT_CAMERA_DIRTY false

// Uncomment this to use warp device for unsupported DX12 functionalities in your device
//#define WARP

// Uncomment this to Force fallback device
//#define FORCE_FALLBACK

//class NoTechnique : public Technique {}; gObj<NoTechnique> technique;

//gObj<SphereScatteringTestTechnique> technique;

//gObj<RetainedBasicRenderer> technique;
//gObj<VPT_Technique> technique;
gObj<VST_Technique> technique;

//#define TEST_WSAPIT
//#define TEST_WSMRAPIT

// APIT
//gObj<DebugAPIT> technique;
//gObj<RaymarchRT<APITConstruction, APITDescription>> technique;
#ifdef TEST_WSAPIT
//gObj<DebugWSAPIT> technique;
gObj<RaymarchRT<WorldSpaceAPIT, APITDescription>> technique;
#endif

#ifdef TEST_WSMRAPIT
//gObj<DebugWSMRAPIT> technique;
gObj<RaymarchRT<WorldSpaceMRAPIT, MRAPITDescription>> technique;
#endif


//#define GENERATE_IMAGES 1000