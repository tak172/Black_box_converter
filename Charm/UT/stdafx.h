// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Раскомментировать для поиска утечек памяти
//#include <vld/include/vld.h>

#define NOMINMAX
#include "../helpful/std_charm_compile_option.h"
#ifndef LINUX
    #include "targetver.h"
#endif

#ifndef LINUX
#include <afxcview.h>
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif

#ifndef LINUX
using std::min; 
using std::max;
//#include "../helpful/GdiPlusH.h"
#endif

#include <fstream>
#ifndef LINUX
#include <Shlwapi.h>
#endif
#include <future>

// Turn off auto-linking regex and datetime libraries with boost::threads (msvs only issue)
#ifndef BOOST_REGEX_NO_LIB
    #define BOOST_REGEX_NO_LIB
#endif
#include <boost/bind/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>


#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#ifndef LINUX
// перекрытие лишних функций
int
WINAPI
UT_MsgBox(
            __in_opt HWND hWnd,
            __in_opt LPCWSTR lpText,
            __in_opt LPCWSTR lpCaption,
            __in UINT uType);
class UT_SHOW_MSGBOX {};
#undef  MessageBox
#define MessageBox  UT_MsgBox

int BriefMsgBox( const CWnd* pParent,
                LPCTSTR lpText,
                LPCTSTR lpCaption,
                UINT uType,
                unsigned timeout =0,
                int bTimer =0
                );
// отключить нативную реализацию BriefMsgBox и MemoMsgBox
#define _BRIEFMSGBOX_H_
#define _MEMOBOX_H_
#endif

#ifndef LINUX
    #include "../Hem/HemEvent.h"
#else
    #include "../helpful/linux_defs.h"
#endif
