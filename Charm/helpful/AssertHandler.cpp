#include "stdafx.h"
#include "AssertHandler.h"

#ifndef LINUX
static int AssertHandler( int reportType, char * /*message*/, int * /*returnValue*/ )
{
    if (_CRT_ASSERT == reportType && IsDebuggerPresent())
        DebugBreak();
    return FALSE;
}
#endif // !LINUX

void AssertHandlerInstall()
{
#ifndef LINUX
#ifdef _DEBUG
    _CrtSetReportHook2( _CRT_RPTHOOK_INSTALL, AssertHandler );
#endif
#endif // !LINUX
}

void AssertHandlerUninstall()
{
#ifndef LINUX
#ifdef _DEBUG
    _CrtSetReportHook2( _CRT_RPTHOOK_REMOVE, AssertHandler );
#endif
#endif // !LINUX
}
