// nvsa.h

#pragma once
#include "DisplayManager.h"
#include "DisplayManager_Exception.h"
#include <msclr\marshal_cppstd.h>
using namespace System;

namespace Display_Manager
{
	//C++ wrapper class so that DispalyManager can be used by C#
	public ref class Surround_Manager
	{
	public:
		bool apiLoaded = false;		
		bool surroundEnabled = false;

		~Surround_Manager();
		!Surround_Manager();
		
		//Initialize Display manager so that it can perform requested actions
		void Initialize();
		
		//Save setup to memory. The underlying DLL has space for two sets of display config's. So surround and normal config's can be stored/read for quick access.
		void SaveSetupToMemory(bool asSurround);	
		//Save current setup to file
		array<System::Byte>^ SaveSetup();
		//Load normal setup from data
		void LoadSetup(bool surround, array<System::Byte> ^ data);
		//Apply setup in memory to hw
		void ApplySetup(bool surround);
		//Apply setup in file to hw
		void ApplySetup(array<System::Byte> ^ data);
		//Check if surround is active using the number of columns and rows the current setup has. If either is larger than 1 then two or more monitors have been made to seem as one.
		bool IsSurroundActive();
		//Check if surround config from file is currently active
		bool IsSurroundActive(array<System::Byte> ^ data);

		//Windows info for re-application later
		void SaveWindowPositions();
		//Reposition windows
		void ApplyWindowPositions();
		//Minimize Windows
		void MinimizeAllWindows();		

	private:
		DisplayManager_State state;
		DisplayManager *displayManager = new DisplayManager();
	};
}
