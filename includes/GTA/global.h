#include "stdafx.h"

uint32_t ResX, ResY;
RsGlobalType* RsGlobal;
float fWideScreenWidthScaleDown, fWideScreenHeightScaleDown;
float fHudWidthScale, fHudHeightScale;
float fSubtitlesScale;
float fCustomWideScreenWidthScaleDown;
float fCustomWideScreenHeightScaleDown;
uint32_t AspectRatioWidth, AspectRatioHeight;
uint32_t FrontendAspectRatioWidth, FrontendAspectRatioHeight;
float fCustomAspectRatioHor, fCustomAspectRatioVer;
float fRadarWidthScale, fCustomRadarWidthScale, fRadarHeightScale, fCustomRadarHeightScale;
float fPlayerMarkerPos;
std::string szForceAspectRatio;
std::string szFrontendAspectRatio;
bool bSmallerVehicleCorona;
bool bNoLightSquare;
bool bFOVControl;
float fEmergencyVehiclesFix;
float fCrosshairPosFactor;
uint32_t nHideAABug;
float fCrosshairHeightScaleDown;
bool bIVRadarScaling;
uint32_t ReplaceTextShadowWithOutline;
bool DisableWhiteCrosshairDot;
float fCustomRadarWidthIV;
float fCustomRadarHeightIV;
float fCustomRadarRingWidthIV;
float fCustomRadarRingHeightIV;
std::string szSelectedMultisamplingLevels;
uint32_t SelectedMultisamplingLevels;
uint32_t* BordersVar1;
uint32_t* BordersVar2;
bool* bWideScreen;
bool* bIsInCutscene;
bool bRestoreCutsceneFOV;
bool bDontTouchFOV;
bool bSmartCutsceneBorders;
bool bAllowAltTabbingWithoutPausing;
int(__cdecl* CSprite2dDrawRect)(class CRect const &, class CRGBA const &);
int(__cdecl* CSprite2dDrawRect2)(class CRect const &, class CRGBA const &, class CRGBA const &, class CRGBA const &, class CRGBA const &);
void(__thiscall *funcCCameraAvoidTheGeometry)(void*, RwV3d const&, RwV3d const&, RwV3d&, float);
std::map<void*, float> FOVMods;