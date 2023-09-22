#pragma once
#ifdef LINUX

//    #define UNREFERENCED_PARAMETER(P)          (P)
typedef unsigned long       DWORD;
typedef void* HANDLE;
typedef long long __int64;
typedef unsigned long long __uint64;
typedef __int64 LONGLONG;
typedef __int64 LONG_PTR, * PLONG_PTR;
typedef DWORD COLORREF;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef __int64             LONG64;
typedef int                 BOOL;

typedef __uint64 ULONGLONG;

typedef struct _ULARGE_INTEGER
{
	struct {
		DWORD LowPart;
		DWORD HighPart;
	} DUMMYSTRUCTNAME;
	struct {
		DWORD LowPart;
		DWORD HighPart;
	} u;
	ULONGLONG QuadPart;
} 	ULARGE_INTEGER;

typedef wchar_t WCHAR;
typedef WCHAR   TCHAR;
typedef WCHAR* LPWSTR;
typedef TCHAR* LPTSTR;
typedef const WCHAR* LPCWSTR;
typedef const TCHAR* LPCTSTR;
typedef __uint64 ULONG_PTR, * PULONG_PTR;
//typedef __uint64 UINT_PTR, * PUINT_PTR;
typedef ULONG_PTR DWORD_PTR, * PDWORD_PTR;
typedef unsigned char byte;

typedef void* HWND;
// typedef UINT_PTR            WPARAM;
// typedef LONG_PTR            LPARAM;
// typedef LONG_PTR            LRESULT;



#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

#ifndef GetRValue
#define GetRValue(rgb)      (LOBYTE(rgb))
#endif
#ifndef GetGValue
#define GetGValue(rgb)      (LOBYTE(((WORD)(rgb)) >> 8))
#endif
#ifndef GetBValue
#define GetBValue(rgb)      (LOBYTE((rgb)>>16))
#endif

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

#define MAX_PATH          260
#define _MAX_PATH   260 /* max. length of full pathname */
#define _MAX_DRIVE  3   /* max. length of drive component */
#define _MAX_DIR    256 /* max. length of path component */
#define _MAX_FNAME  256 /* max. length of file name component */
#define _MAX_EXT    256 /* max. length of extension component */

#ifndef MAX_COMPUTERNAME_LENGTH
#define MAX_COMPUTERNAME_LENGTH 31
#endif	//MAX_COMPUTERNAME_LENGTH

// winnt.h:2078
// #define STATUS_WAIT_0                           ((DWORD   )0x00000000L) 
// WinBase.h:104
#define WAIT_OBJECT_0       ((DWORD   )0x00000000L)


#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#define CE_RXOVER           0x0001  // Receive Queue overflow
#define CE_OVERRUN          0x0002  // Receive Overrun Error
#define CE_RXPARITY         0x0004  // Receive Parity Error
#define CE_FRAME            0x0008  // Receive Framing error
#define CE_BREAK            0x0010  // Break Detected
#define CE_TXFULL           0x0100  // TX Queue is full
#define CE_PTO              0x0200  // LPTx Timeout
#define CE_IOE              0x0400  // LPTx I/O Error
#define CE_DNS              0x0800  // LPTx Device not selected
#define CE_OOP              0x1000  // LPTx Out-Of-Paper
#define CE_MODE             0x8000  // Requested mode unsupported

#ifndef __cplusplus
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef NULL
#define NULL    0
#endif // !NULL

#ifndef INFINITE
#define INFINITE            0xFFFFFFFF  // Infinite timeout
#endif // !INFINITE


#include <sys/time.h>
unsigned long GetTickCount();

#endif // LINUX
