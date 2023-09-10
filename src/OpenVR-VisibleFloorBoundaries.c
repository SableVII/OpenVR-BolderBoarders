#include <stdio.h>
#include <math.h>
#include <float.h>
//#include <stdbool.h>

//Make it so we don't need to include any other C files in our build.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "../inc/rawdraw_sf.h"

#undef EXCTERN_C
#include "../inc/openvr_capi.h"

// Global entry points ~ Required by openvr_capi
intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
bool VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
bool VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

struct VR_IVRSystem_FnTable* oSystem;
struct VR_IVROverlay_FnTable* oOverlay;
struct VR_IVRChaperone_FnTable* oChaperone;

struct HmdQuad_t chaperoneQuad;

//float boundsWidth = 0.0;
//float boundsHeight = 0.0;

// In meters
float boundsSize = 0.0;

struct VRTextureBounds_t boundsUV;

float currentBoundsWidth = FLT_MAX;
float currentBoundsHeight = FLT_MAX;

GLuint texture;

VROverlayHandle_t overlayHandle;

int createdWindow = 0;

struct HmdMatrix34_t m;

void* CNOVRGetOpenVRFunctionTable( const char * interfacename )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interfacename );
	void* ret = (void *)VR_GetGenericInterface( fnTableName, &e );
	printf( "Getting System FnTable: %s = %p (%d)\n", fnTableName, ret, e );
	if( !ret )
	{
		exit( 1 );
	}
	return ret;
}

//#define WIDTH 690
//#define HEIGHT 660
#define PIXELS_PER_METER 200
#define MAX_OVERLAY_SIZE 1024
#define BORDER_WIDTH 4
#define BORDER_WIDTH_DIV2 BORDER_WIDTH/2
#define BORDER_MARGIN 6

#define INITIALIZE_CHECK_MAX 500
#define INITIALIZE_WAIT_MILISECONDS 2500

int overlayPixelSize = 100;

// ReCalculates Bounds if a change in the Chaperone is dectected
int ReCalculateBounds()
{
	oChaperone->GetPlayAreaRect(&chaperoneQuad);
	float boundsWidth = chaperoneQuad.vCorners[0].v[0] - chaperoneQuad.vCorners[1].v[0];
	float boundsHeight = chaperoneQuad.vCorners[0].v[2] - chaperoneQuad.vCorners[2].v[2];
	if (boundsWidth == currentBoundsWidth && boundsHeight == currentBoundsHeight)
	{
		return 0;
	}

	printf("Boundaries have changed dimentions\n");

	currentBoundsWidth = boundsWidth;
	currentBoundsHeight = boundsHeight;

	//printf("bounds? %f, %f, %f\n", qwad.vCorners[0].v[0], qwad.vCorners[0].v[1], qwad.vCorners[0].v[2]);

	if (boundsWidth < 0.0)
	{
		boundsWidth *= -1;
	}

	if (boundsHeight < 0.0)
	{
		boundsHeight *= -1;
	}

	if (boundsWidth < 0.5)
	{
		boundsWidth = 0.5;
	}

	if (boundsHeight < 0.5)
	{
		boundsHeight = 0.5;
	}

	boundsSize = boundsWidth;
	if (boundsHeight > boundsWidth)
	{
		boundsSize = boundsHeight;
	}

	overlayPixelSize = (int)(boundsSize * PIXELS_PER_METER); 

	//if (overlayPixelSize > MAX_OVERLAY_SIZE)
	//{
	//	overlayPixelSize = MAX_OVERLAY_SIZE;
	//}

	if (overlayPixelSize > MAX_OVERLAY_SIZE - BORDER_MARGIN * 2)
	{
		overlayPixelSize = MAX_OVERLAY_SIZE - BORDER_MARGIN * 2;
	}

	boundsSize += boundsSize * (BORDER_MARGIN / (float)overlayPixelSize);

	if (createdWindow == 0)
	{
		CNFGSetup("OpenVR-VisibleFloorBoundaries", -MAX_OVERLAY_SIZE, -MAX_OVERLAY_SIZE);
		//CNFGSetup("OpenVR-VisibleFloorBoundaries", overlayPixelSize + BOARDER_MARGIN * 2, overlayPixelSize + BOARDER_MARGIN * 2);

		oOverlay->SetOverlayTextureBounds(overlayHandle, &boundsUV);
		createdWindow = 1;
	}

	oOverlay->SetOverlayWidthInMeters(overlayHandle, ((float)MAX_OVERLAY_SIZE - (float)BORDER_MARGIN * 2.0) / (float)PIXELS_PER_METER);
	//oOverlay->SetOverlayWidthInMeters(overlayHandle, boundsSize * (MAX_OVERLAY_SIZE / ((boundsSize * PIXELS_PER_METER) + BOARDER_MARGIN * 2)));
	oOverlay->ShowOverlay(overlayHandle);

	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, overlayPixelSize, overlayPixelSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MAX_OVERLAY_SIZE, MAX_OVERLAY_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	oOverlay->SetOverlayTransformAbsolute(overlayHandle, ETrackingUniverseOrigin_TrackingUniverseStanding, &m);

	return 1;
}

void DrawBoundaryTexture()
{
	CNFGBGColor = 0x00000000; //Force transparent background

	CNFGClearFrame();

	//Change color to white.
	CNFGColor( 0xffffffcf ); 	

	if (boundsSize != 0)
	{
		int horizontalOffset = MAX_OVERLAY_SIZE/2 - (boundsSize * PIXELS_PER_METER)/2.0 - BORDER_WIDTH_DIV2 - 1;
		int verticalOffset = MAX_OVERLAY_SIZE/2 - (boundsSize * PIXELS_PER_METER)/2.0 - BORDER_WIDTH_DIV2 - 1;
		float pixelRatio = (float)(overlayPixelSize + BORDER_MARGIN * 2) / 2.0;
		float boundsRatio = (float)(overlayPixelSize + BORDER_MARGIN * 2) / boundsSize;

		// Top Rectangle
		CNFGTackRectangle(
			horizontalOffset + (int)(chaperoneQuad.vCorners[0].v[0] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 1,
			verticalOffset + (int)(chaperoneQuad.vCorners[0].v[2] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2,
			horizontalOffset + (int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2,
			verticalOffset + (int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2
		);

		// Left Rectangle
		CNFGTackRectangle(
			horizontalOffset + (int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2,
			verticalOffset + (int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2,
			horizontalOffset + (int)(chaperoneQuad.vCorners[2].v[0] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2,
			verticalOffset + (int)(chaperoneQuad.vCorners[2].v[2] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2 +1 
		);

		// Bottom Rectangle
		CNFGTackRectangle(
			horizontalOffset + (int)(chaperoneQuad.vCorners[2].v[0] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2,
			verticalOffset + (int)(chaperoneQuad.vCorners[2].v[2] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2 + 1,
			horizontalOffset + (int)(chaperoneQuad.vCorners[3].v[0] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 1,
			verticalOffset + (int)(chaperoneQuad.vCorners[3].v[2] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 1
		);	
	
		// Right Rectangle
		CNFGTackRectangle(
			horizontalOffset + (int)(chaperoneQuad.vCorners[3].v[0] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 1,
			verticalOffset + (int)(chaperoneQuad.vCorners[3].v[2] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2 + 1,
			horizontalOffset + (int)(chaperoneQuad.vCorners[0].v[0] * boundsRatio + pixelRatio) + BORDER_WIDTH_DIV2 + 1,
			verticalOffset + (int)(chaperoneQuad.vCorners[0].v[2] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 
		);	

		CNFGPenX = horizontalOffset + (int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 15;
		CNFGPenY = verticalOffset + (int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) - BORDER_WIDTH_DIV2 + 15;
		CNFGDrawText( "~Sable7 <3", 2 );

	}

	//Display the image and wait for time to display next frame.
	CNFGSwapBuffers();

	//CNFGFlushRender();

	glBindTexture(GL_TEXTURE_2D, texture);

	//printf("BoundsSize: %f\n", boundsSize);

	printf("%d Max %d PixelSize\n", MAX_OVERLAY_SIZE, overlayPixelSize);

	//glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, overlayPixelSize + BOARDER_MARGIN * 2, overlayPixelSize + BOARDER_MARGIN * 2, 0);
	//glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, MAX_OVERLAY_SIZE / ((float)MAX_OVERLAY_SIZE/(float)overlayPixelSize), MAX_OVERLAY_SIZE / ((float)MAX_OVERLAY_SIZE/(float)overlayPixelSize), 0);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, MAX_OVERLAY_SIZE, MAX_OVERLAY_SIZE, 0);

	struct Texture_t overlayTex;
	overlayTex.eType = ETextureType_TextureType_OpenGL;
	overlayTex.eColorSpace = EColorSpace_ColorSpace_Auto;
	overlayTex.handle = (void*)(intptr_t)texture;

	oOverlay->SetOverlayTexture(overlayHandle, &overlayTex);
}

int main()
{
    printf("Initializing OpenVR-VisibleFloorBoundaries application\n");

	int initializeCheckCount = 0;
	while (1)
	{
		EVRInitError e;	
		
		VR_InitInternal(&e, EVRApplicationType_VRApplication_Overlay);
		if (e != EVRInitError_VRInitError_None)
		{
			printf("Failed to initialize OpenVR API %d\n", e);
			Sleep(INITIALIZE_WAIT_MILISECONDS);
		}
		else
		{
			printf("Initialized successfully with OpenVR\n");
			break;
		}

		initializeCheckCount += 1;
		if (initializeCheckCount >= INITIALIZE_CHECK_MAX)
		{
			printf("Continually failed to initialize OpenVR API. Ending OpenVR-VisibleFloorBoundaries application\n");
			return -1;
		}
	}

	oSystem = (struct VR_IVRSystem_FnTable*)CNOVRGetOpenVRFunctionTable(IVRSystem_Version);
	oOverlay = (struct VR_IVROverlay_FnTable*)CNOVRGetOpenVRFunctionTable(IVROverlay_Version);
	oChaperone = (struct VR_IVRChaperone_FnTable*)CNOVRGetOpenVRFunctionTable(IVRChaperone_Version);

	if (!oSystem || !oOverlay || !oChaperone)
	{ 
		printf("Error getting function tables from OpenVR System and/or OpenVR Overlay and/or OpenVR Chaperone");
		return -2;
	}

	oOverlay->CreateOverlay("OpenVR-VisibleFloorBoundaries-overlay", "OpenVR-VisibleFloorBoundaries", &overlayHandle);
	oOverlay->SetOverlayColor(overlayHandle, 1, 1, 1);

	boundsUV.uMin = 0;
	boundsUV.uMax = 1;
	boundsUV.vMin = 0;
	boundsUV.vMax = 1;

	//struct HmdQuad_t qwad;
	oChaperone->GetPlayAreaRect(&chaperoneQuad);

	//printf("Playspace Size: %f, %f  | Size: %f | Size in Pixels: %d\n", boundsWidth, boundsHeight, boundsSize, overlayPixelSize, (int)(boundsSize * PIXELS_PER_METER));

	// Trying to set transform matrix? x.x
	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			//printf("m[%d][%d]: %f\n", i, j, m.m[i][j]);
			m.m[i][j] = 0.0;
		}
	}

	m.m[0][0] = 1;
	m.m[1][2] = 1;
	m.m[2][1] = -1;

	while(CNFGHandleInput())
	{
		// Draw frame if needing to ReCalculate bounds.
		if (ReCalculateBounds() == 1)
		{
			//DrawBoundaryTexture();
		}

		DrawBoundaryTexture();

		Sleep(50);
	}

	return 0;
}