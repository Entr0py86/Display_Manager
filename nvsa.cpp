// This is the main DLL file.

#include "stdafx.h"
#include "nvsa.h"

namespace Display_Manager 
{
	Surround_Manager::~Surround_Manager()
	{
		
	}

	Surround_Manager::!Surround_Manager()
	{
		//Release all unmanaged resources here
		if (displayManager->nvapiLibLoaded)
			displayManager->UnLoadNvapi();
		delete displayManager;
	}	

	void Surround_Manager::Initialize()
	{		
		state = displayManager->LoadNvapi();
		if (state != DisplayManager_State::DM_OK)		
		{
			throw gcnew DisplayManager_Exception(state);
		}
		apiLoaded = true;
		IsSurroundActive();
	}

	void Surround_Manager::SaveSetupToMemory(bool asSurround)
	{		
		state = displayManager->SaveSetupToMemory(asSurround);
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}

	void Surround_Manager::SaveSetupToFile(System::String ^ filePath)
	{	
		std::string sFilePath = msclr::interop::marshal_as<std::string>(filePath);

		state = displayManager->SaveSetupToFile(sFilePath.c_str());
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}

	void Surround_Manager::SaveSetupToFile(System::String ^ filePath, bool asSurround)
	{
		std::string sFilePath = msclr::interop::marshal_as<std::string>(filePath);

		state = displayManager->SaveSetupToFile(sFilePath.c_str(), asSurround);
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}

	void Surround_Manager::LoadSetup(System::String ^ filePath, bool surround)
	{
		std::string sFilePath = msclr::interop::marshal_as<std::string>(filePath);

		state = displayManager->LoadSetup(sFilePath.c_str(), surround);
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}

	void Surround_Manager::ApplySetup(bool surround)
	{
		state = displayManager->ApplySetup(surround);
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
		IsSurroundActive();
	}

	void Surround_Manager::ApplySetup(System::String ^ filePath)
	{
		std::string sFilePath = msclr::interop::marshal_as<std::string>(filePath);

		state = displayManager->ApplySetup(sFilePath.c_str());
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
		IsSurroundActive();
	}

	void Surround_Manager::SaveWindowPositions()
	{
		state = displayManager->SaveWindowPositions();
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}
	
	void Surround_Manager::ApplyWindowPositions()
	{
		state = displayManager->ApplyWindowPositions();
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}
		
	void Surround_Manager::MinimizeAllWindows()
	{
		state = displayManager->MinimizeAllWindows();
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}
	}

	bool Surround_Manager::IsSurroundActive()
	{
		state = displayManager->IsSurroundActive();
		if (state == DisplayManager_State::DM_SURROUND_ACTIVE)
			return surroundEnabled = true;
		else if (state == DisplayManager_State::DM_SURROUND_NOT_ACTIVE)
			return surroundEnabled = false;
		else
		{
			surroundEnabled = false;
			throw gcnew DisplayManager_Exception(state);
		}
	}
}