#include "stdafx.h"
#include "DisplayManager_Exception.h"


DisplayManager_Exception::DisplayManager_Exception(DisplayManager_State state, std::string message)
{
	State = state;
	Message = msclr::interop::marshal_as<System::String^>(message);	
}

DisplayManager_Exception::DisplayManager_Exception(DisplayManager_State state)
{
	State = state;
	StateToErrorMessage(state);
}

System::String^ DisplayManager_Exception::ToString()
{
	return Message;
}

void DisplayManager_Exception::StateToErrorMessage(DisplayManager_State errorState)
{
	switch (errorState)
	{
	case DisplayManager_State::DM_OK:
		Message = "OK";
		break;
	case DisplayManager_State::DM_ERROR:
		Message = "General Error";
		break;
	case DisplayManager_State::DM_LIB_NOT_FOUND:
		Message = "NVidia API Library not found";
		break;
	case DisplayManager_State::DM_NOT_INITIALIZED:
		Message = "Display Manager not initialized";
		break;
	case DisplayManager_State::DM_GET_GPU_ERROR:
		Message = "Could not get GPU information";
		break;
	case DisplayManager_State::DM_GET_DISPLAY_ERROR:
		Message = "Could not get information about attached displays";
		break;
	case DisplayManager_State::DM_GET_PATHS_ERROR:
		Message = "Could not get information about display setup";
		break;
	case DisplayManager_State::DM_GET_GRID_TOPO_ERROR:
		Message = "Could not get information about topology setup";
		break;
	case DisplayManager_State::DM_GET_WINDOW_POS_ERROR:
		Message = "Could not get window positions";
		break;
	case DisplayManager_State::DM_NO_PATH_INFO:
		Message = "No display setup loaded";
		break;
	case DisplayManager_State::DM_NO_GRID_TOPOS:
		Message = "No topology setup loaded";
		break;
	case DisplayManager_State::DM_INCORRECT_FILE_TYPE:
		Message = "File has incorrect file type";
		break;
	case DisplayManager_State::DM_OUT_OF_MEMORY:
		Message = "Out of memory";
		break;
	case DisplayManager_State::DM_DISPLAY_CONFIG_NOT_SET:
		Message = "Could not set display setup";
		break;
	case DisplayManager_State::DM_SAVE_FILE_ERROR:
		Message = "Saving the file was not possible";
		break;
	case DisplayManager_State::DM_READ_FILE_ERROR:
		Message = "Reading the file was not possible";
		break;
	case DisplayManager_State::DM_INVALID_ARGUMENT:
		Message = "Invalid argument passed to function";
		break;
	case DisplayManager_State::DM_GRID_TOPO_INVALID:
		Message = "Invalid grid setup";
		break;
	default:
		Message = "ERROR";
		break;
	}
}