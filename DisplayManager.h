#pragma once
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <fstream> 
#include <vector>
#include "nvapi\nvapi.h"
#include "..\includes\defines.h"

public enum class DisplayManager_State : int
{
	DM_OK,
	DM_ERROR,
	DM_WINDOWS_ERROR,
	DM_SURROUND_ACTIVE,
	DM_SURROUND_NOT_ACTIVE,
	DM_LIB_NOT_FOUND,
	DM_NOT_INITIALIZED,
	DM_GET_GPU_ERROR,
	DM_GET_DISPLAY_ERROR,
	DM_GET_PATHS_ERROR,
	DM_GET_GRID_TOPO_ERROR,
	DM_GET_WINDOW_POS_ERROR,
	DM_NO_PATH_INFO,
	DM_NO_GRID_TOPOS,
	DM_INCORRECT_FILE_TYPE,
	DM_OUT_OF_MEMORY,
	DM_DISPLAY_CONFIG_NOT_SET,	
	DM_SAVE_FILE_ERROR,
	DM_READ_FILE_ERROR,
	DM_INVALID_ARGUMENT,
	DM_GRID_TOPO_INVALID,
};

class DisplayManager
{
public:

	bool nvapiLibLoaded = false;
	//if window error is returned as state then the error will be populated here
	DWORD windowsError = 0;

	DisplayManager();
	~DisplayManager();	

	//Load the NVAPI library and initialize it
	DisplayManager_State LoadNvapi();
	//UnLoad the NVAPI library 
	DisplayManager_State UnLoadNvapi();
	//ReLoad the NVAPI library and initialize it
	DisplayManager_State ReLoadNvapi();
	
	//Save current setup to memory
	DisplayManager_State SaveSetupToMemory(bool surround);
	//Save current setup to file
	DisplayManager_State SaveSetupToFile(const char* filePath);
	//Save current setup to file
	DisplayManager_State SaveSetupToFile(const char* filePath, bool surround);
	//Load setup from file
	DisplayManager_State LoadSetup(const char* filePath, bool surround);
	//Apply setup in memory to hw
	DisplayManager_State ApplySetup(bool surround);
	//Apply setup in file to hw
	DisplayManager_State ApplySetup(const char* filePath);
	//Windows info for re-application later
	DisplayManager_State SaveWindowPositions();
	//Reposition windows
	DisplayManager_State ApplyWindowPositions();
	//Minimize Windows
	DisplayManager_State MinimizeAllWindows();
	//Check whether surround is already active
	DisplayManager_State IsSurroundActive();
	//Check whether surround from file is already active
	DisplayManager_State IsSurroundActive(const char* filePath);
	
private:
	std::vector<WindowPos> windowPositions;

	NV_DISPLAYCONFIG_FLAGS flags = (NV_DISPLAYCONFIG_FLAGS)(NV_DISPLAYCONFIG_SAVE_TO_PERSISTENCE | NV_DISPLAYCONFIG_DRIVER_RELOAD_ALLOWED | NV_DISPLAYCONFIG_FORCE_MODE_ENUMERATION);

	NvU32 nm_pathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO *nm_pathInfo = NULL;

	NvU32 nm_gridCount;
	NV_MOSAIC_GRID_TOPO *nm_gridTopologies = NULL;

	NvU32 sr_pathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO *sr_pathInfo = NULL;		

	NvU32 sr_gridCount;
	NV_MOSAIC_GRID_TOPO *sr_gridTopologies = NULL;

	NvU32 physicalGpuCount = 0;
	NvPhysicalGpuHandle hPhysicalGpu[NVAPI_MAX_PHYSICAL_GPUS];

	NvU32 nDisplayIds = 0;
	NV_GPU_DISPLAYIDS* pDisplayIds = NULL;
		
	void FreePathInfo(NvU32 pPathCount, NV_DISPLAYCONFIG_PATH_INFO *pPathInfo);

	//Save current setup to memory
	DisplayManager_State SaveCurrentNormalSetup();
	//Save current setup to memory
	DisplayManager_State SaveCurrentSurroundSetup();
	//Save current setup to file
	DisplayManager_State SaveNormalSetup(const char* filePath);
	//Save current setup to file
	DisplayManager_State SaveSurroundSetup(const char* filePath);
	//Load normal setup from file
	DisplayManager_State LoadNormalSetup(const char* filePath);
	//Load surround setup from file
	DisplayManager_State LoadSurroundSetup(const char* filePath);
	//Apply normal setup in memory to hw
	DisplayManager_State ApplyNormalSetup();
	//Apply surround setup in memory to hw
	DisplayManager_State ApplySurroundSetup();

	//Compare functions return true if equal
	bool CompareDisplayPaths(NvU32 nPathInfoCount1, NV_DISPLAYCONFIG_PATH_INFO* pPathInfo1, NvU32 nPathInfoCount2, NV_DISPLAYCONFIG_PATH_INFO* pPathInfo2);
	bool CompareGridTopologies(NvU32 nGridCount1, NV_MOSAIC_GRID_TOPO* pGridTopo1, NvU32 nGridCount2, NV_MOSAIC_GRID_TOPO* pGridTopo2);
	//Compare current setup with file setup
	//DisplayManager_State CompareDisplaySetups();

	//Get functions for setup
	DisplayManager_State GetPhysicalGpus();
	DisplayManager_State GetConnectedDisplays(NvU32* pDisplayIdCount, NV_GPU_DISPLAYIDS** pDisplayIds);
	DisplayManager_State GetDisplayPaths(NvU32* pPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo);
	DisplayManager_State GetGridTopos(NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo);
	DisplayManager_State GetWindows(std::vector<WindowPos> *pGetWindowList);

	//Set functions for setup
	DisplayManager_State SetDisplayPaths(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo);
	DisplayManager_State SetGridTopos(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo);
	DisplayManager_State SetWindows(std::vector<WindowPos> *pSetWindowList);

	//Setup to File Handlers
	/*
	Combined file struct:
		FileHeader
		GridTopoFileHeader
		GridTopoData
		PathInfoFileHeader
		PathInfoData
	*/
	DisplayManager_State ReadFileToSetup(const char* pFilePath, NvU32* pPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo);
	DisplayManager_State SaveSetupToFile(const char* pFilePath, NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo, NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo);

	//Data handlers to type
	DisplayManager_State DataToPathInfo(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, unsigned char* pData);
	DisplayManager_State DataToGridTopo(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo, unsigned char* pData);
	DisplayManager_State PathInfoToData(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo, unsigned int *pDataLength, unsigned char** pOutData);
	DisplayManager_State GridTopoToData(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo, unsigned int *pDataLength, unsigned char** pOutData);

	//File handlers for Path info
	DisplayManager_State PathInfoToDataFile(NvU32 setPathCount, NV_DISPLAYCONFIG_PATH_INFO *pSetPathInfo, const char* pFilePath);	
	DisplayManager_State DataFileToPathInfo(NvU32* pPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, const char* pFilePath);
	
	//File handlers for Grid Topologies
	DisplayManager_State GridTopoToDataFile(NvU32 pSetGridCount, NV_MOSAIC_GRID_TOPO *pSetGridTopo, const char* pFilePath);
	DisplayManager_State DataFileToGridTopo(NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo, const char* pFilePath);
	

	//File Handlers
	bool SaveToFile(const char* filePath, unsigned char* data, unsigned int dataLength);
	bool ReadFromFile(const char* filePath, unsigned char** data);
};

