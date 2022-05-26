#pragma once
extern "C"
{
#include "windowedqueue.h"
}

namespace ALTUS {
	int LoadFile(const char* path, WQ_Window* window, WQ_Node_Pool* pool);
	//this loads entry of that file to this window. it doesn't actually reads data from it.

	int SendFile(WQ_Window);
}