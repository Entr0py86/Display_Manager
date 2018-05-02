#pragma once
#include <stdio.h>
#include <string>
#include <msclr\marshal_cppstd.h>
#include "..\includes\defines.h"
#include "DisplayManager.h"
using namespace System;

public ref class DisplayManager_Exception : public System::Exception
{
public:
	DisplayManager_State State;
	System::String^ Message;
	DisplayManager_Exception(DisplayManager_State state);
	DisplayManager_Exception(DisplayManager_State state, std::string message);

	System::String^ ToString() override;
private:
	void StateToErrorMessage(DisplayManager_State errorState);
};

