#include "stdafx.h"
#include "../helpful/RT_Macros.h"
#include "../helpful/RT_ThreadName.h"
#ifdef LINUX
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#endif // LINUX

#ifndef LINUX
//
// Usage: RT_SetCurrThreadName ("MainThread");
//
// Написано на основе SetThreadName из справки VS2003
// 
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // must be 0x1000
    LPCSTR szName; // pointer to name (in user addr space)
    DWORD dwThreadID; // thread ID (-1=caller thread)
    DWORD dwFlags; // reserved for future use, must be zero
}
THREADNAME_INFO;
#pragma pack(pop)
#endif // !LINUX

void RT_SetThreadName( const char* szThreadName )
{
    char temp[200];
    memset( temp, 0, sizeof(temp) );
    size_t sz = strlen( szThreadName );
    if ( sz >= size_array( temp ) )
        sz = size_array( temp ) - 1;
    memcpy( temp, szThreadName, sz );

#ifndef LINUX
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = temp;
    info.dwThreadID = (DWORD)-1;
    info.dwFlags = 0;

    __try
    {
        RaiseException( 0x406D1388, 0, sizeof( info ) / sizeof( ULONG_PTR ), ( ULONG_PTR* )&info );
    }
    __except( EXCEPTION_CONTINUE_EXECUTION )
    {}
#else
    temp[15] = 0;
    pthread_t self = pthread_self();
    int rc = pthread_setname_np( self, temp );
    if (rc != 0)
    {
        //errno = rc;
        //perror("pthread_setname_np error");
    }
#endif // !LINUX
}
