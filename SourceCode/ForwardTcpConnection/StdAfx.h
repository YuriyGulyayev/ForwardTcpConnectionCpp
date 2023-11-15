#pragma once

#include "./TargetVer.h"

// This includes <Windows.h> .
// This does not include <WinSock.h> .
#include <WinSock2.h>

#include <MSTcpIP.h>

//#include <cstdio>

// yg? No such thing in Microsoft C++: #include <cconio>
#include <conio.h>

//#include <tchar.h>

#if( _MSC_VER != 1600 )
   #error yg?? Use the {NoFence} interlocked functions.
#endif
