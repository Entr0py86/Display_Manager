#include "stdafx.h"
#include "DisplayManager.h"

BOOL CALLBACK EnumWindowProc(_In_ HWND   hwnd, _In_ LPARAM lParam)
{
	std::vector<WindowPos>& windows = *reinterpret_cast<std::vector<WindowPos>*>(lParam);
	WindowPos temp;

	temp.hWnd = hwnd;
	temp.winPlacement.length = sizeof(WINDOWPLACEMENT);

	GetWindowTextW(hwnd, temp.windowTitle, 1024);

	int length = ::GetWindowTextLength(hwnd);
	std::wstring title(&temp.windowTitle[0]);
	if (!IsWindowVisible(hwnd) || length == 0 || title == L"Program Manager")
	{
		return TRUE;
	}

	if (GetWindowPlacement(hwnd, &temp.winPlacement))
		GetWindowRect(hwnd, &temp.winPlacement.rcNormalPosition);

	windows.push_back(temp);

	return TRUE;
}

DisplayManager::DisplayManager()
{
	memset(hPhysicalGpu, 0, sizeof(NvPhysicalGpuHandle) * NVAPI_MAX_PHYSICAL_GPUS);
}

DisplayManager::~DisplayManager()
{
	UnLoadNvapi();

	if (pDisplayIds != NULL)
	{
		delete(pDisplayIds);
	}
	//Delete normal setup variables
	if (nm_pathInfo != NULL)
	{
		FreePathInfo(nm_pathCount, nm_pathInfo);
	}
	if (nm_gridTopologies != NULL)
	{
		delete nm_gridTopologies;
	}
	//Delete surround setup variables
	if (sr_pathInfo != NULL)
	{
		FreePathInfo(sr_pathCount, sr_pathInfo);
	}
	if (sr_gridTopologies != NULL)
	{
		delete sr_gridTopologies;
	}
}

DisplayManager_State DisplayManager::LoadNvapi()
{
	NV_MOSAIC_SUPPORTED_TOPO_INFO topoInfo;
	DisplayManager_State ret;
	NvAPI_Status result = NVAPI_OK;

	memset(&topoInfo, 0, sizeof(NV_MOSAIC_SUPPORTED_TOPO_INFO));
    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }   
	
	if (nvapiLibLoaded)
	{
		return DisplayManager_State::DM_OK;
	}

    nvapiInUse = true;
	try
	{
		result = NvAPI_Initialize();
		switch (result)
		{
		case NVAPI_OK:
			ret = DisplayManager_State::DM_OK;
			nvapiLibLoaded = true;
			break;
		case NVAPI_ERROR:
			ret = DisplayManager_State::DM_ERROR;
			nvapiLibLoaded = false;
			break;
		case NVAPI_LIBRARY_NOT_FOUND:
			ret = DisplayManager_State::DM_LIB_NOT_FOUND;
			nvapiLibLoaded = false;
			break;
		}
	}
	catch (...)
	{
	}

	if (nvapiLibLoaded)
	{
		//Force a check that there is a NVidia gpu in the system
		ret = GetPhysicalGpus();
		if (ret == DisplayManager_State::DM_OK)
		{
			//Check that the GPU is supported
			topoInfo.version = NVAPI_MOSAIC_SUPPORTED_TOPO_INFO_VER;
			result = NvAPI_Mosaic_GetSupportedTopoInfo(&topoInfo, NV_MOSAIC_TOPO_TYPE_ALL);
            switch(result)
            {
            case NVAPI_OK:
                ret = DisplayManager_State::DM_OK;
                break;
            case NVAPI_NOT_SUPPORTED:
                ret = DisplayManager_State::DM_NVIDIA_GPU_NOT_SUPPORTED;
                break;
            case NVAPI_ERROR:
                ret = DisplayManager_State::DM_ERROR;
                if (topoInfo.topoBriefsCount > 0)
                {
                    for (NvU32 i = 0; i < topoInfo.topoBriefsCount; i++)
                    {
                        if (topoInfo.topoBriefs[i].isPossible)
                        {
                            ret = DisplayManager_State::DM_OK;
                        }
                    }
                }                
                break;
            default:
                break;
			}
		}
	}
    nvapiInUse = false;
	return ret;
}

DisplayManager_State DisplayManager::UnLoadNvapi()
{
	DisplayManager_State ret;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    
	if (!nvapiLibLoaded)
		return DisplayManager_State::DM_OK;
    nvapiInUse = true;
	try
	{
		switch (NvAPI_Unload())
		{
		case NVAPI_OK:
			nvapiLibLoaded = false;
			ret = DisplayManager_State::DM_OK;
			break;
		case NVAPI_ERROR:
			ret = DisplayManager_State::DM_ERROR;
			break;
		case NVAPI_API_IN_USE:
			nvapiInUse = true;
			ret = DisplayManager_State::DM_ERROR;
			break;
		}
	}
	catch (...)
	{
	}
    nvapiInUse = false;
	return ret;
}

DisplayManager_State DisplayManager::ReLoadNvapi()
{
    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }

	DisplayManager_State ret = UnLoadNvapi();
	if (ret != DisplayManager_State::DM_OK)
		return ret;
	return LoadNvapi();
}


DisplayManager_State DisplayManager::SaveSetupToMemory(bool surround)
{
    DisplayManager_State ret = DisplayManager_State::DM_OK;
    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	if (surround)
        ret = SaveCurrentSurroundSetup();
	else
        ret = SaveCurrentNormalSetup();

    nvapiInUse = false;
    return ret;
}

DisplayManager_State DisplayManager::SaveSetupToData(unsigned int* dataSize, unsigned char** pData)
{
	NvU32 pathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo = NULL;

	NvU32 gridCount;
	NV_MOSAIC_GRID_TOPO *gridTopologies = NULL;
	DisplayManager_State result = DisplayManager_State::DM_OK;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	if (!nvapiLibLoaded)
	{
        if (LoadNvapi() != DisplayManager_State::DM_OK)
        {
            nvapiInUse = false;
            return DisplayManager_State::DM_NOT_INITIALIZED;
        }
	}

	if (GetPhysicalGpus() != DisplayManager_State::DM_OK)
	{
        nvapiInUse = false;
		return DisplayManager_State::DM_GET_GPU_ERROR;
	}

	if ((result = GetDisplayPaths(&pathCount, &pathInfo)) != DisplayManager_State::DM_OK)
	{
		FreePathInfo(pathCount, pathInfo);
        nvapiInUse = false;
		return result;
	}

	if ((result = GetGridTopos(&gridCount, &gridTopologies)) != DisplayManager_State::DM_OK)
	{
		if (nm_gridTopologies != NULL)
			delete nm_gridTopologies;
        nvapiInUse = false;
		return result;
	}
    nvapiInUse = false;
	return SaveSetupToData(dataSize, pData, pathCount, pathInfo, gridCount, gridTopologies);
}

DisplayManager_State DisplayManager::LoadSetup(bool surround, unsigned char* pData)
{
    DisplayManager_State ret = DisplayManager_State::DM_OK;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	if (surround)
        ret = LoadSurroundSetup(pData);
	else
        ret  = LoadNormalSetup(pData);

    nvapiInUse = false;
    return ret;
}

DisplayManager_State DisplayManager::ApplySetup(bool surround)
{
    DisplayManager_State ret = DisplayManager_State::DM_OK;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;

	if (surround)
        ret = ApplySurroundSetup();
	else
        ret = ApplyNormalSetup();
    nvapiInUse = false;
    return ret;
}

DisplayManager_State DisplayManager::ApplySetup(unsigned char* pData)
{
	NvU32 pathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO *pathInfo = NULL;
	NvU32 gridCount;
	NV_MOSAIC_GRID_TOPO *gridTopologies = NULL;
	DisplayManager_State result = DisplayManager_State::DM_OK;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	//Initialize NVAPI
	if (!nvapiLibLoaded)
	{
        if (LoadNvapi() != DisplayManager_State::DM_OK)
        {
            nvapiInUse = false;
            return DisplayManager_State::DM_NOT_INITIALIZED;
        }
	}


	if ((result = ReadDataToSetup(pData, &pathCount, &pathInfo, &gridCount, &gridTopologies)) != DisplayManager_State::DM_OK)
	{
		FreePathInfo(pathCount, pathInfo);
        nvapiInUse = false;
		return result;
	}

	result = SetGridTopos(gridCount, gridTopologies);
    if (result != DisplayManager_State::DM_OK)
    {
        nvapiInUse = false;
        return result;
    }

	result = SetDisplayPaths(pathCount, pathInfo);
    if (result != DisplayManager_State::DM_OK)
    {
        nvapiInUse = false;
        return result;
    }
    nvapiInUse = false;
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SaveCurrentNormalSetup()
{
	DisplayManager_State result = DisplayManager_State::DM_OK;

	if (!nvapiLibLoaded)
	{
		if (LoadNvapi() != DisplayManager_State::DM_OK)
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}

	if (GetPhysicalGpus() != DisplayManager_State::DM_OK)
	{
		return DisplayManager_State::DM_GET_GPU_ERROR;
	}

	if ((result = GetDisplayPaths(&nm_pathCount, &nm_pathInfo)) != DisplayManager_State::DM_OK)
	{
		FreePathInfo(nm_pathCount, nm_pathInfo);
		return result;
	}

	if ((result = GetGridTopos(&nm_gridCount, &nm_gridTopologies)) != DisplayManager_State::DM_OK)
	{
		if (nm_gridTopologies != NULL)
			delete nm_gridTopologies;
		return result;
	}

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SaveCurrentSurroundSetup()
{
	DisplayManager_State result = DisplayManager_State::DM_OK;

	if (!nvapiLibLoaded)
	{
		if (LoadNvapi() != DisplayManager_State::DM_OK)
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}

	if (GetPhysicalGpus() != DisplayManager_State::DM_OK)
	{
		return DisplayManager_State::DM_GET_GPU_ERROR;
	}

	if ((result = GetDisplayPaths(&sr_pathCount, &sr_pathInfo)) != DisplayManager_State::DM_OK)
	{
		FreePathInfo(sr_pathCount, sr_pathInfo);
		return result;
	}

	if ((result = GetGridTopos(&sr_gridCount, &sr_gridTopologies)) != DisplayManager_State::DM_OK)
	{
		if (sr_gridTopologies != NULL)
			delete sr_gridTopologies;
		return result;
	}

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SaveNormalSetup(const char* pFilePath)
{
	return SaveSetupToFile(pFilePath, nm_pathCount, nm_pathInfo, nm_gridCount, nm_gridTopologies);
}

DisplayManager_State DisplayManager::SaveSurroundSetup(const char* pFilePath)
{
	return SaveSetupToFile(pFilePath, sr_pathCount, sr_pathInfo, sr_gridCount, sr_gridTopologies);
}

DisplayManager_State DisplayManager::SaveNormalSetup(unsigned int* dataSize, unsigned char* pData)
{
	return SaveSetupToData(dataSize, &pData, nm_pathCount, nm_pathInfo, nm_gridCount, nm_gridTopologies);
}

DisplayManager_State DisplayManager::SaveSurroundSetup(unsigned int* dataSize, unsigned char* pData)
{
	return SaveSetupToData(dataSize, &pData, sr_pathCount, sr_pathInfo, sr_gridCount, sr_gridTopologies);
}

DisplayManager_State DisplayManager::LoadNormalSetup(const char* pFilePath)
{
	return ReadFileToSetup(pFilePath, &nm_pathCount, &nm_pathInfo, &nm_gridCount, &nm_gridTopologies);
}

DisplayManager_State DisplayManager::LoadSurroundSetup(const char* pFilePath)
{
	return ReadFileToSetup(pFilePath, &sr_pathCount, &sr_pathInfo, &sr_gridCount, &sr_gridTopologies);
}

DisplayManager_State DisplayManager::LoadNormalSetup(unsigned char* pData)
{
	return ReadDataToSetup(pData, &nm_pathCount, &nm_pathInfo, &nm_gridCount, &nm_gridTopologies);
}

DisplayManager_State DisplayManager::LoadSurroundSetup(unsigned char* pData)
{
	return ReadDataToSetup(pData, &sr_pathCount, &sr_pathInfo, &sr_gridCount, &sr_gridTopologies);
}

DisplayManager_State DisplayManager::ApplyNormalSetup()
{
	DisplayManager_State result = DisplayManager_State::DM_OK;

	result = SetGridTopos(nm_gridCount, nm_gridTopologies);
	if (result != DisplayManager_State::DM_OK)
		return result;

	result = SetDisplayPaths(nm_pathCount, nm_pathInfo);
	if (result != DisplayManager_State::DM_OK)
		return result;

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::ApplySurroundSetup()
{
	DisplayManager_State result = DisplayManager_State::DM_OK;

	result = SetGridTopos(sr_gridCount, sr_gridTopologies);
	if (result != DisplayManager_State::DM_OK)
		return result;

	result = SetDisplayPaths(sr_pathCount, sr_pathInfo);
	if (result != DisplayManager_State::DM_OK)
		return result;

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SaveWindowPositions()
{
	return GetWindows(&windowPositions);
}

DisplayManager_State DisplayManager::ApplyWindowPositions()
{
	return SetWindows(&windowPositions);
}

DisplayManager_State DisplayManager::MinimizeAllWindows()
{
	HWND lHwnd = FindWindow(L"Shell_TrayWnd", NULL);
	SendMessage(lHwnd, WM_COMMAND, MIN_ALL, 0);
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::IsSurroundActive()
{
	NvU32 gridCount = 0;
	DisplayManager_State result = DisplayManager_State::DM_OK;
	NV_MOSAIC_GRID_TOPO *pGridTopos = NULL;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	if (!nvapiLibLoaded)
	{
        if (LoadNvapi() != DisplayManager_State::DM_OK)
        {
            nvapiInUse = false;
            return DisplayManager_State::DM_NOT_INITIALIZED;
        }
	}

	result = GetGridTopos(&gridCount, &pGridTopos);
	if (result != DisplayManager_State::DM_OK)
	{
        nvapiInUse = false;
		return result;
	}
	
	for (NvU32 i = 0; i < gridCount; i++)
	{
		//If either is larger than 1 then two or more screens have been set into a surround setup
        if (pGridTopos[i].columns > 1 || pGridTopos[i].rows > 1)
        {
            nvapiInUse = false;
            return DisplayManager_State::DM_SURROUND_ACTIVE; //In surround mode
        }
	}
    nvapiInUse = false;
	//Not in surround
	return DisplayManager_State::DM_SURROUND_NOT_ACTIVE;
}

DisplayManager_State DisplayManager::IsSurroundActive(unsigned char* pData)
{
	NvU32 gridCount = 0;
	NV_MOSAIC_GRID_TOPO *pGridTopos = NULL;

	NvU32 filePathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO *filePathInfo = NULL;
	NvU32 fileGridCount = 0;
	NV_MOSAIC_GRID_TOPO *pFileGridTopos = NULL;

	DisplayManager_State result = DisplayManager_State::DM_OK;

    if (nvapiInUse)
    {
        return DisplayManager_State::DM_BUSY;
    }
    nvapiInUse = true;
	if (!nvapiLibLoaded)
	{
        if (LoadNvapi() != DisplayManager_State::DM_OK)
        {
            nvapiInUse = false;
            return DisplayManager_State::DM_NOT_INITIALIZED;
        }
	}

	result = ReadDataToSetup(pData, &filePathCount, &filePathInfo, &fileGridCount, &pFileGridTopos);
	if (result != DisplayManager_State::DM_OK)
	{
        nvapiInUse = false;
		return result;
	}

	result = GetGridTopos(&gridCount, &pGridTopos);
	if (result != DisplayManager_State::DM_OK)
	{
        nvapiInUse = false;
		return result;
	}

	if (CompareGridTopologies(gridCount, pGridTopos, fileGridCount, pFileGridTopos))
	{
		//In surround mode
		delete pFileGridTopos;
		FreePathInfo(filePathCount, filePathInfo);
        nvapiInUse = false;
		return DisplayManager_State::DM_SURROUND_ACTIVE;
	}
    nvapiInUse = false;
	//Not in surround
	return DisplayManager_State::DM_SURROUND_NOT_ACTIVE;
}

void DisplayManager::FreePathInfo(NvU32 freePathCount, NV_DISPLAYCONFIG_PATH_INFO *freePathInfo)
{
	if (freePathInfo == NULL)
		return;
	for (NvU32 i = 0; i < freePathCount; i++)
	{
		for (NvU32 u = 0; u < freePathInfo[i].targetInfoCount; u++)
		{
			if (freePathInfo[i].targetInfo != NULL)
			{
				if (freePathInfo[i].targetInfo[u].details != NULL)
					delete (freePathInfo[i].targetInfo[u].details);
				delete (&freePathInfo[i].targetInfo[u]);
			}
		}
		if (freePathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || freePathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
		{
			delete(freePathInfo[i].sourceModeInfo);
		}
		else
		{
#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
			for (NvU32 u = 0; u < freePathInfo[i].sourceModeInfoCount; u++)
			{
				if (freePathInfo[i].sourceModeInfo[u] != NULL)
					delete(&freePathInfo[i].sourceModeInfo[u]);
			}
#endif
		}
	}
	freePathInfo = NULL;
}

bool DisplayManager::CompareDisplayPaths(NvU32 nPathInfoCount1, NV_DISPLAYCONFIG_PATH_INFO* pPathInfo1, NvU32 nPathInfoCount2, NV_DISPLAYCONFIG_PATH_INFO* pPathInfo2)
{
	unsigned int length1 = 0;
	unsigned int length2 = 0;
	unsigned char* pData1 = NULL;
	unsigned char* pData2 = NULL;
	//Do comparison
	if (nPathInfoCount1 == nPathInfoCount2)
	{
		PathInfoToData(nPathInfoCount1, pPathInfo1, &length1, &pData1);
		PathInfoToData(nPathInfoCount2, pPathInfo2, &length2, &pData2);
		if (length1 == length2)
		{
			if (memcmp(pData1, pData2, length1) == 0)
			{
				//Grid topology are equal
				return true;
			}
		}
	}
	return false;
}

bool DisplayManager::CompareGridTopologies(NvU32 nGridCount1, NV_MOSAIC_GRID_TOPO* pGridTopo1, NvU32 nGridCount2, NV_MOSAIC_GRID_TOPO* pGridTopo2)
{
	//Do comparison
	if (nGridCount1 == nGridCount2)
	{
		if (memcmp(pGridTopo1, pGridTopo2, (nGridCount1 * sizeof(NV_MOSAIC_GRID_TOPO))) == 0)
		{
			//Grid topology are equal
			return true;
		}
	}
	return false;
}

DisplayManager_State DisplayManager::GetPhysicalGpus()
{
	NvAPI_Status nvapiReturn;
	if (!nvapiLibLoaded)
	{
		if (LoadNvapi() != DisplayManager_State::DM_OK)
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}

	try
	{
		if (physicalGpuCount == 0)
		{
			memset(hPhysicalGpu, 0, sizeof(NvPhysicalGpuHandle) * NVAPI_MAX_PHYSICAL_GPUS);
			nvapiReturn = NvAPI_EnumPhysicalGPUs(hPhysicalGpu, &physicalGpuCount);
			if (nvapiReturn == NVAPI_OK)
				return DisplayManager_State::DM_OK;
			else if (nvapiReturn == NVAPI_NVIDIA_DEVICE_NOT_FOUND)
				return DisplayManager_State::DM_NO_NVIDIA_GPU_ERROR;

		}
		else
		{
			return DisplayManager_State::DM_OK;
		}
	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
	}
	return DisplayManager_State::DM_GET_GPU_ERROR;

}

DisplayManager_State DisplayManager::GetConnectedDisplays(NvU32* nGetDisplayIdCount, NV_GPU_DISPLAYIDS** pGetDisplayIds)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	NvU32 DisplayGpuIndex = 0;
	NvU32 nDisplayIds = 0;
	NV_GPU_DISPLAYIDS* pDisplayIds = NULL;

	if (!nvapiLibLoaded)
	{
		if (LoadNvapi() != DisplayManager_State::DM_OK)
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}
	try
	{
		if (physicalGpuCount == 0)
		{
			if (GetPhysicalGpus() != DisplayManager_State::DM_OK)
				return DisplayManager_State::DM_GET_GPU_ERROR;
		}
		//Get all connected displays
		for (NvU32 GpuIndex = 0; GpuIndex < physicalGpuCount; GpuIndex++)
		{
			if ((NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[GpuIndex], pDisplayIds, &nDisplayIds, 0) == NVAPI_OK) && nDisplayIds)
			{
				DisplayGpuIndex = GpuIndex;
				pDisplayIds = new NV_GPU_DISPLAYIDS[nDisplayIds];
				if (pDisplayIds)
				{
					memset(pDisplayIds, 0, nDisplayIds * sizeof(NV_GPU_DISPLAYIDS));
					pDisplayIds[GpuIndex].version = NV_GPU_DISPLAYIDS_VER;
					if (NvAPI_GPU_GetConnectedDisplayIds(hPhysicalGpu[DisplayGpuIndex], pDisplayIds, &nDisplayIds, 0) != NVAPI_OK)
					{
						result = DisplayManager_State::DM_GET_DISPLAY_ERROR;
						break;
					}
				}
			}
		}

	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
		result = DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
	}
	*pGetDisplayIds = pDisplayIds;
	*nGetDisplayIdCount = nDisplayIds;

	return result;
}

DisplayManager_State DisplayManager::GetDisplayPaths(NvU32* pathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo)
{
	NvU32 path_count = 0;
	NV_DISPLAYCONFIG_PATH_INFO *path_info = NULL;

	if (!nvapiLibLoaded)
	{
		if (LoadNvapi() != DisplayManager_State::DM_OK)
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}

	try
	{
		// Retrieve the display path information

		if (NvAPI_DISP_GetDisplayConfig(&path_count, NULL) != NVAPI_OK)    
			return DisplayManager_State::DM_GET_PATHS_ERROR;

		path_info = new NV_DISPLAYCONFIG_PATH_INFO[path_count];
		if (path_info == NULL)
		{
			return DisplayManager_State::DM_OUT_OF_MEMORY;
		}

		memset(path_info, 0, path_count * sizeof(NV_DISPLAYCONFIG_PATH_INFO));
		for (NvU32 i = 0; i < path_count; i++)
		{
			path_info[i].version = NV_DISPLAYCONFIG_PATH_INFO_VER;
		}

		// Retrieve the targetInfo counts

		if (NvAPI_DISP_GetDisplayConfig(&path_count, path_info) != NVAPI_OK)
		{
			return DisplayManager_State::DM_GET_PATHS_ERROR;
		}

		for (NvU32 i = 0; i < path_count; i++)
		{
			// Allocate the source mode info

			if (path_info[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || path_info[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
			{
				path_info[i].sourceModeInfo = new NV_DISPLAYCONFIG_SOURCE_MODE_INFO();
			}
			else
			{

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
				path_info[i].sourceModeInfo = new (NV_DISPLAYCONFIG_SOURCE_MODE_INFO[path_info[i].sourceModeInfoCount];
#endif

			}
			if (path_info[i].sourceModeInfo == NULL)
			{
				return DisplayManager_State::DM_OUT_OF_MEMORY;
			}
			memset(path_info[i].sourceModeInfo, 0, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));

			// Allocate the target array
			path_info[i].targetInfo = new NV_DISPLAYCONFIG_PATH_TARGET_INFO[path_info[i].targetInfoCount];
			if (path_info[i].targetInfo == NULL)
			{
				return DisplayManager_State::DM_OUT_OF_MEMORY;
			}
			// Allocate the target details
			memset(path_info[i].targetInfo, 0, path_info[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
			for (NvU32 j = 0; j < path_info[i].targetInfoCount; j++)
			{
				path_info[i].targetInfo[j].details = new NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO();
				memset(path_info[i].targetInfo[j].details, 0, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
				path_info[i].targetInfo[j].details->version = NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO_VER;
			}
		}

		// Retrieve the full path info
		if (NvAPI_DISP_GetDisplayConfig(&path_count, path_info) != NVAPI_OK)
		{
			return DisplayManager_State::DM_GET_PATHS_ERROR;
		}
		*pathInfoCount = path_count;
		*pGetPathInfo = path_info;
	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
		return DisplayManager_State::DM_GET_PATHS_ERROR;
	}
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::GetGridTopos(NvU32* gridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	NvU32 GridCount = 0u;
	NV_MOSAIC_GRID_TOPO* pGridTopologies = NULL;

	if (!nvapiLibLoaded)
	{
		if(LoadNvapi() != DisplayManager_State::DM_OK)		
			return DisplayManager_State::DM_NOT_INITIALIZED;
	}

	try
	{
		NvAPI_Status retVal = NvAPI_Mosaic_EnumDisplayGrids(NULL, &GridCount);
		if (NvAPI_Mosaic_EnumDisplayGrids(NULL, &GridCount) != NVAPI_OK)
		{
			return DisplayManager_State::DM_GET_GRID_TOPO_ERROR;
		}

		pGridTopologies = new NV_MOSAIC_GRID_TOPO[GridCount];
		if (pGridTopologies == NULL)
		{
			return DisplayManager_State::DM_OUT_OF_MEMORY;
		}
		memset(pGridTopologies, 0, GridCount * sizeof(NV_MOSAIC_GRID_TOPO));

		for (NvU32 i = 0; i < GridCount; i++)
		{
			pGridTopologies[i].version = NV_MOSAIC_GRID_TOPO_VER;
		}

		if (NvAPI_Mosaic_EnumDisplayGrids(pGridTopologies, &GridCount) != NVAPI_OK)
		{
			return DisplayManager_State::DM_GET_GRID_TOPO_ERROR;
		}

		*gridCount = GridCount;
		*pGetGridTopo = pGridTopologies;
	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
		result = DisplayManager_State::DM_GET_GRID_TOPO_ERROR;
	}
	return result; 
}

DisplayManager_State DisplayManager::GetWindows(std::vector<WindowPos> *pGetWindowList)
{
	if (EnumWindows(EnumWindowProc, reinterpret_cast<LPARAM>(pGetWindowList)))
	{
		return DisplayManager_State::DM_OK;
	}
	return DisplayManager_State::DM_GET_WINDOW_POS_ERROR;
}

DisplayManager_State DisplayManager::SetDisplayPaths(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo)
{
	NvU32 nGetPathCount = 0;
	NV_DISPLAYCONFIG_PATH_INFO* pGetPathInfo = NULL;
	GetDisplayPaths(&nGetPathCount, &pGetPathInfo);
	//Do comparison
	try
	{
		if (CompareDisplayPaths(nGetPathCount, pGetPathInfo, nPathInfoCount, pSetPathInfo))
		{
			FreePathInfo(nGetPathCount, pGetPathInfo);
			return DisplayManager_State::DM_OK;
		}

		if (NvAPI_DISP_SetDisplayConfig(nPathInfoCount, pSetPathInfo, (NvU32)flags) != NVAPI_OK)
		{
			return DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
		}
	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
		return DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
	}
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SetGridTopos(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo)
{
	NvAPI_Status result = NVAPI_OK;
	NvU32 nGetGridCount = 0;
	NV_MOSAIC_GRID_TOPO* pGetGridTopo = NULL;

	GetGridTopos(&nGetGridCount, &pGetGridTopo);

	try
	{
		//Do comparison
		if (CompareGridTopologies(nGetGridCount, pGetGridTopo, nGridCount, pSetGridTopo))
		{
			delete pGetGridTopo;
			return DisplayManager_State::DM_OK;
		}

		NV_MOSAIC_DISPLAY_TOPO_STATUS *pTopoStatus = new NV_MOSAIC_DISPLAY_TOPO_STATUS[nGridCount];

		for (NvU32 i = 0; i < nGridCount; i++)
		{
			pTopoStatus[i].version = NV_MOSAIC_DISPLAY_TOPO_STATUS_VER;
		}

		if (NvAPI_Mosaic_ValidateDisplayGrids(NV_MOSAIC_SETDISPLAYTOPO_FLAG_CURRENT_GPU_TOPOLOGY, pSetGridTopo, pTopoStatus, nGridCount) != NVAPI_OK)
		{
			return DisplayManager_State::DM_GRID_TOPO_INVALID;
		}
		for (NvU32 i = 0; i < nGridCount; i++)
		{
			if (pTopoStatus[i].errorFlags != 0 || pTopoStatus[i].warningFlags != 0)
			{
				delete pTopoStatus;
				return DisplayManager_State::DM_GRID_TOPO_INVALID;
			}
		}
		//Set Display Grid
		result = NvAPI_Mosaic_SetDisplayGrids(pSetGridTopo, nGridCount, NV_MOSAIC_SETDISPLAYTOPO_FLAG_CURRENT_GPU_TOPOLOGY);
		if (result != NVAPI_OK)
		{
			return DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
		}

		//Get current setup to confirm Display was setup
		GetGridTopos(&nGetGridCount, &pGetGridTopo);
		//Do comparison
		if (!CompareGridTopologies(nGetGridCount, pGetGridTopo, nGridCount, pSetGridTopo))
		{
			delete pGetGridTopo;
			return DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
		}
	}
	catch (...)
	{
		UnLoadNvapi();
		nvapiLibLoaded = false;
		return DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET;
	}
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SetWindows(std::vector<WindowPos> *pSetWindowList)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;

	for (unsigned int i = 0; i < pSetWindowList->size(); i++)
	{
		//Check window exists first might have been closed by now
		if (IsWindow((*pSetWindowList)[i].hWnd))
		{
			//Update the position of the window
			if (!SetWindowPlacement((*pSetWindowList)[i].hWnd, &(*pSetWindowList)[i].winPlacement))
			{
				windowsError = GetLastError();
				result = DisplayManager_State::DM_WINDOWS_ERROR;
			}
		}
	}
	return result;
}

DisplayManager_State DisplayManager::ReadDataToSetup(unsigned char* pData, NvU32* pPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	DisplayManager_Header header;
	DisplayManager_GridTopo gridTopo;
	DisplayManager_PathInfo pathInfo;
	unsigned int index = 0;
	NV_MOSAIC_GRID_TOPO *gridTopos = NULL;
	NV_DISPLAYCONFIG_PATH_INFO *path_info = NULL;

	//Copy header info from beginning of file
	memcpy(&header, pData, sizeof(DisplayManager_Header));
	index += sizeof(DisplayManager_Header);

	if (header.fileType != NVAPI_DataType::combined)
	{
		return DisplayManager_State::DM_INCORRECT_FILE_TYPE;
	}
	//Read GridTopo Header
	memcpy(&gridTopo, &pData[index], sizeof(DisplayManager_GridTopo));
	index += sizeof(DisplayManager_GridTopo);

	if (gridTopo.header.fileType != NVAPI_DataType::gridTopo)
	{
		return DisplayManager_State::DM_INCORRECT_FILE_TYPE;
	}
	//Read data to structs
	*pGridCount = gridTopo.gridCount;
	result = DataToGridTopo(gridTopo.gridCount, pGetGridTopo, &pData[index]);
	if (result != DisplayManager_State::DM_OK)
		return result;
	index += gridTopo.header.size - sizeof(DisplayManager_GridTopo);

	//Read PathInfo Header
	memcpy(&pathInfo, &pData[index], sizeof(DisplayManager_PathInfo));
	index += sizeof(DisplayManager_PathInfo);

	if (pathInfo.header.fileType != NVAPI_DataType::pathInfo)
	{
		return DisplayManager_State::DM_INCORRECT_FILE_TYPE;
	}
	//Read data to structs
	*pPathInfoCount = pathInfo.pathCount;
	result = DataToPathInfo(pathInfo.pathCount, pGetPathInfo, &pData[index]);
	if (result != DisplayManager_State::DM_OK)
		return result;

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::SaveSetupToData(unsigned int* dataSize, unsigned char** pData, NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo, NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	DisplayManager_Header header;
	DisplayManager_GridTopo gridTopo;
	DisplayManager_PathInfo pathInfo;
	unsigned int index = 0;
	unsigned char *pDataGridTopo = NULL;
	unsigned char *pDataPathInfo = NULL;

	unsigned int dataLengthGridTopo = 0;
	unsigned int dataLengthPathInfo = 0;

	if (!nGridCount || pSetGridTopo == NULL)
	{
		return DisplayManager_State::DM_NO_GRID_TOPOS;
	}
	if (!nPathInfoCount || pSetPathInfo == NULL)
	{
		return DisplayManager_State::DM_NO_PATH_INFO;
	}

	//Update file header info
	memset(&header, 0, sizeof(DisplayManager_Header));
	header.fileType = NVAPI_DataType::combined;
	header.size += sizeof(DisplayManager_Header);

	//Create Data from structs
	result = GridTopoToData(nGridCount, pSetGridTopo, &dataLengthGridTopo, &pDataGridTopo);
	if (result != DisplayManager_State::DM_OK)
	{
		if (pDataGridTopo != NULL)
		{
			delete pDataGridTopo;
			pDataGridTopo = NULL;
		}
		return result;
	}

	//Update grid topology file header info
	memset(&gridTopo, 0, sizeof(DisplayManager_GridTopo));
	gridTopo.header.fileType = NVAPI_DataType::gridTopo;
	gridTopo.gridCount = nGridCount;

	//Calculate size of file
	gridTopo.header.size += sizeof(DisplayManager_GridTopo);
	gridTopo.header.size += dataLengthGridTopo;

	//Create Data from structs
	result = PathInfoToData(nPathInfoCount, pSetPathInfo, &dataLengthPathInfo, &pDataPathInfo);
	if (result != DisplayManager_State::DM_OK)
	{
		if (pDataGridTopo != NULL)
		{
			delete pDataGridTopo;
			pDataGridTopo = NULL;
		}
		if (pDataPathInfo != NULL)
		{
			delete pDataPathInfo;
			pDataPathInfo = NULL;
		}
		return result;
	}

	//Update path info file header info
	memset(&pathInfo, 0, sizeof(DisplayManager_PathInfo));
	pathInfo.header.fileType = NVAPI_DataType::pathInfo;
	pathInfo.pathCount = nPathInfoCount;

	//Calculate size of file
	pathInfo.header.size += sizeof(DisplayManager_PathInfo);
	pathInfo.header.size += dataLengthPathInfo;

	header.size += gridTopo.header.size;
	header.size += pathInfo.header.size;
	*pData = new unsigned char[header.size];
	if (*pData == NULL)
	{
		if (pDataGridTopo != NULL)
		{
			delete pDataGridTopo;
			pDataGridTopo = NULL;
		}
		if (pDataPathInfo != NULL)
		{
			delete pDataPathInfo;
			pDataPathInfo = NULL;
		}
		return DisplayManager_State::DM_OUT_OF_MEMORY;
	}
	memset(*pData, 0, header.size);

	//Start writing to data
	//Write  header
	memcpy(&(*pData)[index], &header, sizeof(DisplayManager_Header));
	index += sizeof(DisplayManager_Header);

	//Write Grid topology file header
	memcpy(&(*pData)[index], &gridTopo, sizeof(DisplayManager_GridTopo));
	index += sizeof(DisplayManager_GridTopo);
	//Write Grid topology file data
	memcpy(&(*pData)[index], pDataGridTopo, dataLengthGridTopo);
	index += dataLengthGridTopo;

	//Write Path Info file header
	memcpy(&(*pData)[index], &pathInfo, sizeof(DisplayManager_PathInfo));
	index += sizeof(DisplayManager_PathInfo);
	//Write Path info file data
	memcpy(&(*pData)[index], pDataPathInfo, dataLengthPathInfo);
	index += dataLengthPathInfo;
	
	*dataSize = header.size;
	//clear data memory
	if (pDataGridTopo != NULL)
	{
		delete pDataGridTopo;
		pDataGridTopo = NULL;
	}
	if (pDataPathInfo != NULL)
	{
		delete pDataPathInfo;
		pDataPathInfo = NULL;
	}	

	return result;
}

DisplayManager_State DisplayManager::ReadFileToSetup(const char* pFilePath, NvU32* nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo)
{
	unsigned char* pData = NULL;
	DisplayManager_State result = DisplayManager_State::DM_OK;	

	if (!ReadFromFile(pFilePath, &pData))return DisplayManager_State::DM_READ_FILE_ERROR;
	result = ReadDataToSetup(pData, nPathInfoCount, pGetPathInfo, pGridCount, pGetGridTopo);

	return result;
}

DisplayManager_State DisplayManager::SaveSetupToFile(const char* pFilePath, NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo, NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	unsigned int dataSize = 0;
	unsigned char *pDataLocal = NULL;

	if (!nGridCount || pSetGridTopo == NULL)
	{
		return DisplayManager_State::DM_NO_GRID_TOPOS;
	}
	if (!nPathInfoCount || pSetPathInfo == NULL)
	{
		return DisplayManager_State::DM_NO_PATH_INFO;
	}

	result = SaveSetupToData(&dataSize, &pDataLocal, nPathInfoCount, pSetPathInfo, nGridCount, pSetGridTopo);
	if (result == DisplayManager_State::DM_OK)
	{
		//Save to file
		if (!SaveToFile(pFilePath, pDataLocal, dataSize))
			result = DisplayManager_State::DM_SAVE_FILE_ERROR;
	}
	else 
		result = DisplayManager_State::DM_SAVE_FILE_ERROR;
	//clear data memory	
	if (pDataLocal != NULL)
	{
		delete pDataLocal;
		pDataLocal = NULL;
	}

	return result;
}

DisplayManager_State DisplayManager::DataToPathInfo(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO** pGetPathInfo, unsigned char* pData)
{
	unsigned int index = 0;
	NV_DISPLAYCONFIG_PATH_INFO *path_info = NULL;

	if (*pGetPathInfo != NULL)
	{
		delete(*pGetPathInfo);
	}

	path_info = new NV_DISPLAYCONFIG_PATH_INFO[nPathInfoCount];
	memset(path_info, 0, nPathInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_INFO));

	for (NvU32 i = 0; i < nPathInfoCount; i++)
	{
		memcpy(&path_info[i].version, &pData[index], sizeof(NvU32));
		index += sizeof(NvU32);
		memcpy(&path_info[i].sourceId, &pData[index], sizeof(NvU32));
		index += sizeof(NvU32);
		memcpy(&path_info[i].targetInfoCount, &pData[index], sizeof(NvU32));
		index += sizeof(NvU32);

		if (path_info[i].targetInfoCount > 0)
		{
			path_info[i].targetInfo = new NV_DISPLAYCONFIG_PATH_TARGET_INFO[path_info[i].targetInfoCount];
			if (path_info[i].targetInfo == NULL)
			{
				return DisplayManager_State::DM_OUT_OF_MEMORY;
			}
			// Allocate the target details
			memset(path_info[i].targetInfo, 0, path_info[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO));
			//Write target info
			for (NvU32 u = 0; u < path_info[i].targetInfoCount; u++)
			{
				//Copy display id
				memcpy(&path_info[i].targetInfo[u].displayId, &pData[index], sizeof(NvU32));
				index += sizeof(NvU32);
				//Create data area for detailed info
				path_info[i].targetInfo[u].details = new NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO();
				memset(path_info[i].targetInfo[u].details, 0, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
				if (path_info[i].targetInfo[u].details == NULL)
				{
					return DisplayManager_State::DM_OUT_OF_MEMORY;
				}
				//Copy detailed info
				memcpy(path_info[i].targetInfo[u].details, &pData[index], sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
				index += sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO);

				//Copy targetId
				memcpy(&path_info[i].targetInfo[u].targetId, &pData[index], sizeof(NvU32));
				index += sizeof(NvU32);
			}
		}

		if (path_info[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || path_info[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
		{
			path_info[i].sourceModeInfo = new NV_DISPLAYCONFIG_SOURCE_MODE_INFO();
			if (path_info[i].sourceModeInfo == NULL)
			{
				return DisplayManager_State::DM_OUT_OF_MEMORY;
			}
			memcpy(path_info[i].sourceModeInfo, &pData[index], sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
			index += sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
		}
		else
		{

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
			path_info[i].sourceModeInfo = (NV_DISPLAYCONFIG_SOURCE_MODE_INFO[path_info[i].sourceModeInfoCount * sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
			memcpy(&path_info[i].sourceModeInfoCount, &pData[index], sizeof(NvU32));
			index += sizeof(NvU32);
			if (path_info[i].sourceModeInfoCount > 0 && path_info[i].sourceModeInfo != NULL)
			{
				for (int u = 0; u < path_info[i].sourceModeInfoCount; u++)
				{
					memcpy(path_info[i].sourceModeInfo[u], &pData[index], sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
					index += sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
				}
			}
#endif

		}
		//Not writing last bit word IsNonNVIDIAAdapter : 1 and reserved : 31
	}
	*pGetPathInfo = path_info;

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::DataToGridTopo(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo, unsigned char* pData)
{
	unsigned int index = 0;
	NV_MOSAIC_GRID_TOPO *gridTopos = NULL;

	if (*pGetGridTopo != NULL)
	{
		delete(*pGetGridTopo);
	}
	gridTopos = new NV_MOSAIC_GRID_TOPO[nGridCount];
	memset(gridTopos, 0, (nGridCount * sizeof(NV_MOSAIC_GRID_TOPO)));
	memcpy(gridTopos, pData, (nGridCount * sizeof(NV_MOSAIC_GRID_TOPO)));

	*pGetGridTopo = gridTopos;

	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::PathInfoToData(NvU32 nPathInfoCount, NV_DISPLAYCONFIG_PATH_INFO* pSetPathInfo, unsigned int *pDataLength, unsigned char** pOutData)
{
	unsigned int index = 0;
	unsigned int fileSize = 0;
	unsigned char* pData = NULL;

	if (!nPathInfoCount || pSetPathInfo == NULL)
	{
		return DisplayManager_State::DM_NO_PATH_INFO;
	}

	//Calculate size of file
	for (NvU32 i = 0; i < nPathInfoCount; i++)
	{
		if (pSetPathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || pSetPathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
		{
			if (pSetPathInfo[i].sourceModeInfo != NULL)
				fileSize += sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
		}
		else
		{

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
			if (path_info[i].sourceModeInfoCount > 0 && pSetPathInfo[i].sourceModeInfo != NULL)
				fileSize += path_info[i].sourceModeInfoCount * sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
#endif

		}
		if (pSetPathInfo[i].targetInfoCount > 0 && pSetPathInfo[i].targetInfo != NULL)
		{
			fileSize += pSetPathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_TARGET_INFO);
			fileSize += pSetPathInfo[i].targetInfoCount * sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO);
		}
	}

	//Create Data memory

	pData = new unsigned char[fileSize];
	if (pData == NULL) return DisplayManager_State::DM_OUT_OF_MEMORY;
	memset(pData, 0, fileSize);

	//Start writing to data
	for (NvU32 i = 0; i < nPathInfoCount; i++)
	{
		//Write version, id and targerInfoCount
		memcpy(&pData[index], &pSetPathInfo[i].version, sizeof(NvU32));
		index += sizeof(NvU32);
		memcpy(&pData[index], &pSetPathInfo[i].sourceId, sizeof(NvU32));
		index += sizeof(NvU32);
		memcpy(&pData[index], &pSetPathInfo[i].targetInfoCount, sizeof(NvU32));
		index += sizeof(NvU32);

		if (pSetPathInfo[i].targetInfoCount > 0 && pSetPathInfo[i].targetInfo != NULL)
		{
			//Write target info
			for (NvU32 u = 0; u < pSetPathInfo[i].targetInfoCount; u++)
			{
				memcpy(&pData[index], &pSetPathInfo[i].targetInfo[u].displayId, sizeof(NvU32));
				index += sizeof(NvU32);
				memcpy(&pData[index], pSetPathInfo[i].targetInfo[u].details, sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO));
				index += sizeof(NV_DISPLAYCONFIG_PATH_ADVANCED_TARGET_INFO);
				memcpy(&pData[index], &pSetPathInfo[i].targetInfo[u].targetId, sizeof(NvU32));
				index += sizeof(NvU32);
			}
		}

		if (pSetPathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER1 || pSetPathInfo[i].version == NV_DISPLAYCONFIG_PATH_INFO_VER2)
		{
			if (pSetPathInfo[i].sourceModeInfo != NULL)
			{
				memcpy(&pData[index], pSetPathInfo[i].sourceModeInfo, sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
				index += sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
			}
		}
		else
		{

#ifdef NV_DISPLAYCONFIG_PATH_INFO_VER3
			memcpy(&pData[index], &pSetPathInfo[i].sourceModeInfoCount, sizeof(NvU32));
			index += sizeof(NvU32);
			if (path_info[i].sourceModeInfoCount > 0 && pSetPathInfo[i].sourceModeInfo != NULL)
			{
				for (int u = 0; u < pSetPathInfo[i].sourceModeInfoCount; u++)
				{
					memcpy(&pData[index], pSetPathInfo[i].sourceModeInfo[u], sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO));
					index += sizeof(NV_DISPLAYCONFIG_SOURCE_MODE_INFO);
				}
			}
#endif

		}
		//Not writing last bit word IsNonNVIDIAAdapter : 1 and reserved : 31
	}
	//Clear data if exists	
	*pOutData = pData;
	*pDataLength = fileSize;

	if (*pOutData == NULL)return DisplayManager_State::DM_ERROR;
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::GridTopoToData(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO* pSetGridTopo, unsigned int *pDataLength, unsigned char** pOutData)
{
	unsigned int fileSize = 0;
	unsigned char* pData = NULL;

	if (!nGridCount || pSetGridTopo == NULL)
	{
		return DisplayManager_State::DM_NO_GRID_TOPOS;
	}

	//Calculate size of file
	fileSize += (nGridCount * sizeof(NV_MOSAIC_GRID_TOPO));
	//Create Data memory	
	pData = new unsigned char[fileSize];
	if (pData == NULL) return DisplayManager_State::DM_OUT_OF_MEMORY;
	memset(pData, 0, fileSize);

	//Start writing to data
	//Write grid topology's
	memcpy(&pData[0], pSetGridTopo, fileSize);

	//Clear data if exists	
	*pOutData = pData;
	*pDataLength = fileSize;

	if (*pOutData == NULL)return DisplayManager_State::DM_ERROR;
	return DisplayManager_State::DM_OK;
}

DisplayManager_State DisplayManager::PathInfoToDataFile(NvU32 nSetPathCount, NV_DISPLAYCONFIG_PATH_INFO *pSetPathInfo, const char* pFilePath)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	DisplayManager_PathInfo pathInfo;
	unsigned char *pDataLocal = NULL;
	unsigned char *pData = NULL;
	unsigned int index = 0;
	unsigned int dataLength = 0;

	if (!nSetPathCount || pSetPathInfo == NULL)
	{
		return DisplayManager_State::DM_NO_PATH_INFO;
	}

	//Create Data from structs
	result = PathInfoToData(nSetPathCount, pSetPathInfo, &dataLength, &pData);
	if (result != DisplayManager_State::DM_OK)
	{
		if (pData != NULL)
		{
			delete pData;
			pData = NULL;
		}
		return result;
	}

	//Update file header info
	memset(&pathInfo, 0, sizeof(DisplayManager_PathInfo));
	pathInfo.header.fileType = NVAPI_DataType::pathInfo;
	pathInfo.pathCount = nSetPathCount;

	//Calculate size of file
	pathInfo.header.size += sizeof(DisplayManager_PathInfo);
	pathInfo.header.size += dataLength;
	pDataLocal = new unsigned char[pathInfo.header.size];
	if (pDataLocal == NULL) return DisplayManager_State::DM_OUT_OF_MEMORY;

	memset(pDataLocal, 0, pathInfo.header.size);

	//Start writing to data
	memcpy(&pDataLocal[index], &pathInfo, sizeof(DisplayManager_PathInfo));
	index += sizeof(DisplayManager_PathInfo);

	memcpy(&pDataLocal[index], pData, dataLength);
	//Save to file
	if (!SaveToFile(pFilePath, pDataLocal, pathInfo.header.size))
		result = DisplayManager_State::DM_SAVE_FILE_ERROR;

	//clear data memory
	if (pData != NULL)
	{
		delete pData;
		pData = NULL;
	}
	if (pDataLocal != NULL)
	{
		delete pDataLocal;
		pDataLocal = NULL;
	}

	return result;
}

DisplayManager_State DisplayManager::DataFileToPathInfo(NvU32 *pGetPathCount, NV_DISPLAYCONFIG_PATH_INFO **pGetPathInfo, const char* pFilePath)
{
	unsigned char* pData = NULL;
	DisplayManager_PathInfo pathInfo;
	unsigned int index = 0;

	if (!ReadFromFile(pFilePath, &pData)) return DisplayManager_State::DM_READ_FILE_ERROR;

	memcpy(&pathInfo, pData, sizeof(DisplayManager_PathInfo));
	index += sizeof(DisplayManager_PathInfo);

	if (pathInfo.header.fileType != NVAPI_DataType::pathInfo)
	{
		return DisplayManager_State::DM_INCORRECT_FILE_TYPE;
	}

	*pGetPathCount = pathInfo.pathCount;

	return DataToPathInfo(pathInfo.pathCount, pGetPathInfo, &pData[sizeof(DisplayManager_PathInfo)]);
}

DisplayManager_State DisplayManager::GridTopoToDataFile(NvU32 nGridCount, NV_MOSAIC_GRID_TOPO *pSetGridTopo, const char* pFilePath)
{
	DisplayManager_State result = DisplayManager_State::DM_OK;
	DisplayManager_GridTopo gridTopo;
	unsigned char *pDataLocal = NULL;
	unsigned char *pData = NULL;
	unsigned int index = 0;
	unsigned int dataLength = 0;

	if (!nGridCount || pSetGridTopo == NULL)
	{
		return DisplayManager_State::DM_NO_GRID_TOPOS;
	}

	//Create Data from structs
	result = GridTopoToData(nGridCount, pSetGridTopo, &dataLength, &pData);
	if (result != DisplayManager_State::DM_OK)
	{
		if (pData != NULL)
		{
			delete pData;
			pData = NULL;
		}
		return result;
	}

	//Update file header info
	memset(&gridTopo, 0, sizeof(DisplayManager_GridTopo));
	gridTopo.header.fileType = NVAPI_DataType::gridTopo;
	gridTopo.gridCount = nGridCount;

	//Calculate size of file
	gridTopo.header.size += sizeof(DisplayManager_GridTopo);
	gridTopo.header.size += dataLength;
	pDataLocal = new unsigned char[gridTopo.header.size];
	if (pDataLocal == NULL) return DisplayManager_State::DM_OUT_OF_MEMORY;
	memset(pDataLocal, 0, gridTopo.header.size);

	//Start writing to data
	memcpy(&pDataLocal[index], &gridTopo, sizeof(DisplayManager_GridTopo));
	index += sizeof(DisplayManager_GridTopo);

	memcpy(&pDataLocal[index], pData, dataLength);
	//Save to file
	if (!SaveToFile(pFilePath, pDataLocal, gridTopo.header.size))
		result = DisplayManager_State::DM_SAVE_FILE_ERROR;

	//clear data memory
	if (pData != NULL)
	{
		delete pData;
		pData = NULL;
	}
	if (pDataLocal != NULL)
	{
		delete pDataLocal;
		pDataLocal = NULL;
	}

	return result;
}

DisplayManager_State DisplayManager::DataFileToGridTopo(NvU32* pGridCount, NV_MOSAIC_GRID_TOPO** pGetGridTopo, const char* pFilePath)
{
	unsigned char* pData = NULL;
	DisplayManager_GridTopo gridTopo;
	unsigned int index = 0;
	NV_MOSAIC_GRID_TOPO *gridTopos = NULL;

	if (!ReadFromFile(pFilePath, &pData))return DisplayManager_State::DM_READ_FILE_ERROR;

	memcpy(&gridTopo, pData, sizeof(DisplayManager_GridTopo));
	index += sizeof(DisplayManager_GridTopo);

	if (gridTopo.header.fileType != NVAPI_DataType::gridTopo)
	{
		return DisplayManager_State::DM_INCORRECT_FILE_TYPE;
	}

	*pGridCount = gridTopo.gridCount;

	return DataToGridTopo(gridTopo.gridCount, pGetGridTopo, &pData[index]);
}

bool DisplayManager::SaveToFile(const char* pFilePath, unsigned char* data, unsigned int dataLength)
{
	std::ofstream file;
	file.open(pFilePath, std::ios::out | std::ios::binary);

	if (file)
	{
		file.write((char*)data, dataLength);
		file.close();
		return true;
	}
	return false;

}

bool DisplayManager::ReadFromFile(const char* pFilePath, unsigned char** data)
{
	DisplayManager_Header fileHeader;
	std::ifstream file;
	file.open(pFilePath, std::ios::in | std::ios::binary);
	char* buffer;

	if (*data != NULL)
	{
		return false;
	}
	if (file)
	{
		//Read only header
		file.read((char*)&fileHeader, sizeof(DisplayManager_Header));
		file.seekg(0);
		//Create buffer according to header.size
		buffer = new char[fileHeader.size];
		memset(buffer, 0, fileHeader.size);
		//Read everything into buffer
		file.read(buffer, fileHeader.size);

		*data = (unsigned char*)buffer;
		return true;
	}
	return false;
}
