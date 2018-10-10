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

	array<System::Byte>^ Surround_Manager::SaveSetup()
	{
		unsigned char* pData = NULL;
		unsigned int dataSize = 0;

		state = displayManager->SaveSetupToData(&dataSize, &pData);
		if (state != DisplayManager_State::DM_OK)
		{
			throw gcnew DisplayManager_Exception(state);
		}

		if (pData != NULL && dataSize > 0)
		{
			array<System::Byte>^ data = gcnew array<System::Byte>(dataSize);

			System::Runtime::InteropServices::Marshal::Copy(IntPtr((void *)pData), data, 0, dataSize);
			return data;
		}
		else
		{			
			return nullptr;
		}
	}

	void Surround_Manager::LoadSetup(bool surround, array<System::Byte> ^ data)
	{
		if (data != nullptr)
		{
			pin_ptr<unsigned char> pData = &data[0];

			state = displayManager->LoadSetup(surround, pData);
			if (state != DisplayManager_State::DM_OK)
			{
				throw gcnew DisplayManager_Exception(state);
			}
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

	void Surround_Manager::ApplySetup(array<System::Byte> ^ data)
	{
		if (data != nullptr)
		{
			pin_ptr<unsigned char> pData = &data[0];

			state = displayManager->ApplySetup(pData);
			if (state != DisplayManager_State::DM_OK)
			{
				throw gcnew DisplayManager_Exception(state);
			}
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
			surroundEnabled = true;
		else if (state == DisplayManager_State::DM_SURROUND_NOT_ACTIVE)
			surroundEnabled = false;
		else
		{
			throw gcnew DisplayManager_Exception(state);
		}
		return surroundEnabled;
	}

	bool Surround_Manager::IsSurroundActive(array<System::Byte> ^ data)
	{
		if (data != nullptr)
		{
			pin_ptr<unsigned char> pData = &data[0];

			state = displayManager->IsSurroundActive(pData);
			if (state == DisplayManager_State::DM_SURROUND_ACTIVE)
				surroundEnabled = true;
			else if (state == DisplayManager_State::DM_SURROUND_NOT_ACTIVE)
				surroundEnabled = false;
			else
			{
				throw gcnew DisplayManager_Exception(state);
			}
		}
		return surroundEnabled;
	}
}