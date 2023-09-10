#include <stdio.h>
#include <math.h>
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

VROverlayHandle_t overlayHandle;

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
#define BOARDER_WIDTH 4
#define BOARDER_WIDTH_DIV2 BOARDER_WIDTH/2
#define BOARDER_MARGIN 6

int overlayPixelSize = 100;

int main()
{
	printf("Woof?\n");

    printf("Initializing OpenVR-VisibleFloorBoundaries\n");

	{
		EVRInitError e;	
		
		VR_InitInternal(&e, EVRApplicationType_VRApplication_Overlay);
		if (e != EVRInitError_VRInitError_None)
		{
			printf("Failed to initialize OpenVR API %d\n", e);
			return -1;
		}
		else
		{
			printf("Everything Dandy!\n");
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

	oOverlay->CreateOverlay("OpenVR-VisibleFloorBoundaries", "OpenVR-VisibleFloorBoundaries", &overlayHandle);
	oOverlay->SetOverlayColor(overlayHandle, 1, 1, 1);

	struct VRTextureBounds_t boundsUV;
	boundsUV.uMin = 0;
	boundsUV.uMax = 1;
	boundsUV.vMin = 0;
	boundsUV.vMax = 1;

	//struct HmdQuad_t qwad;
	oChaperone->GetPlayAreaRect(&chaperoneQuad);

	float boundsWidth = chaperoneQuad.vCorners[0].v[0] - chaperoneQuad.vCorners[1].v[0];
	float boundsHeight = chaperoneQuad.vCorners[0].v[2] - chaperoneQuad.vCorners[2].v[2];

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

	if (overlayPixelSize > MAX_OVERLAY_SIZE)
	{
		overlayPixelSize = MAX_OVERLAY_SIZE;
	}

	boundsSize += boundsSize * (BOARDER_MARGIN / (float)overlayPixelSize);

	//printf("Playspace Size: %f, %f  | Size: %f | Size in Pixels: %d\n", boundsWidth, boundsHeight, boundsSize, overlayPixelSize, (int)(boundsSize * PIXELS_PER_METER));

	CNFGSetup("OpenVR-VisibleFloorBoundaries", overlayPixelSize + BOARDER_MARGIN * 2, overlayPixelSize + BOARDER_MARGIN * 2);
	//CNFGSetup( "Example App", WIDTH, HEIGHT);

	//oOverlay->SetOverlayWidthInMeters(overlayHandle, boundsSize);
	oOverlay->SetOverlayTextureBounds(overlayHandle, &boundsUV);
	oOverlay->SetOverlayWidthInMeters(overlayHandle, boundsSize);
	oOverlay->ShowOverlay(overlayHandle);

	GLuint texture;
	glGenTextures(1, &texture);

	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, overlayPixelSize, overlayPixelSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// Trying to set transform matrix? x.x
	struct HmdMatrix34_t m;

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
	

	oOverlay->SetOverlayTransformAbsolute(overlayHandle, ETrackingUniverseOrigin_TrackingUniverseStanding, &m);
	//oOverlay->SetOverlayTransformProjection(overlayHandle, ETrackingUniverseOrigin_TrackingUniverseStanding, &m);
	

	while(CNFGHandleInput())
	{
		//printf("Looping?\n");

		CNFGBGColor = 0x00000000; //Force transparent background

		CNFGClearFrame();
		//Change color to white.

		CNFGColor( 0xffffffcf ); 	



		// Top Rectangle
		if (boundsSize != 0)
		{
			float pixelRatio = (float)(overlayPixelSize + BOARDER_MARGIN * 2) / 2.0;
			float boundsRatio = (float)(overlayPixelSize + BOARDER_MARGIN * 2) / boundsSize;

			CNFGTackRectangle(
				(int)(chaperoneQuad.vCorners[0].v[0] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[0].v[2] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2
			);

			/*printf("x1: %f  z1: %f  x2: %f  z2: %f\n",
				chaperoneQuad.vCorners[0].v[0],
				chaperoneQuad.vCorners[0].v[2],
				chaperoneQuad.vCorners[1].v[0],
				chaperoneQuad.vCorners[1].v[2] 
			);

			printf("x1: %d  z1: %d  x2: %d  z2: %d\n",
				(int)(chaperoneQuad.vCorners[0].v[0] * (float)overlayPixelSize / boundsSize) + (overlayPixelSize / 2) - BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[0].v[2] * (float)overlayPixelSize / boundsSize) + (overlayPixelSize / 2) - BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[1].v[0] * (float)overlayPixelSize / boundsSize) + (overlayPixelSize / 2) + BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[1].v[2] * (float)overlayPixelSize / boundsSize) + (overlayPixelSize / 2) + BOARDER_WIDTH_DIV2 
			);*/

			// Left Rectangle
			CNFGTackRectangle(
				(int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[2].v[0] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[2].v[2] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2 +1 
			);

			// Bottom Rectangle
			CNFGTackRectangle(
				(int)(chaperoneQuad.vCorners[2].v[0] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2,
				(int)(chaperoneQuad.vCorners[2].v[2] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[3].v[0] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[3].v[2] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 1
			);	
		
			// Right Rectangle
			CNFGTackRectangle(
				(int)(chaperoneQuad.vCorners[3].v[0] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[3].v[2] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[0].v[0] * boundsRatio + pixelRatio) + BOARDER_WIDTH_DIV2 + 1,
				(int)(chaperoneQuad.vCorners[0].v[2] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 
			);	

			CNFGPenX = (int)(chaperoneQuad.vCorners[1].v[0] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 15;
			CNFGPenY = (int)(chaperoneQuad.vCorners[1].v[2] * boundsRatio + pixelRatio) - BOARDER_WIDTH_DIV2 + 15;
			CNFGDrawText( "Sable7 <3", 2 );

		}

		//Display the image and wait for time to display next frame.
		CNFGSwapBuffers();

		//CNFGFlushRender();

		glBindTexture(GL_TEXTURE_2D, texture);

		//printf("BoundsSize: %f\n", boundsSize);

		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, overlayPixelSize + BOARDER_MARGIN * 2, overlayPixelSize + BOARDER_MARGIN * 2, 0);
		//glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, WIDTH, HEIGHT, 0);

		struct Texture_t overlayTex;
		overlayTex.eType = ETextureType_TextureType_OpenGL;
		overlayTex.eColorSpace = EColorSpace_ColorSpace_Auto;
		overlayTex.handle = (void*)(intptr_t)texture;

		oOverlay->SetOverlayTexture(overlayHandle, &overlayTex);

		Sleep(50);
	}

	return 0;
}