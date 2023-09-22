//
// Общие настройки сборки проекта Charm 
//
#include "../helpful/std_charm_required_platform.h"

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

//AP: Предупреждения отключать нельзя!
//AP: #define _SCL_SECURE_NO_WARNINGS
//AP: #define _CRT_SECURE_NO_WARNINGS
//AP: #define _CRT_NON_CONFORMING_SWPRINTFS

// При перегрузке безопасность не нарушается.
#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES         1
#endif
#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES_COUNT   1
#endif
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES           1

// включаем контроль итераторов ( Checked iterators )
//#define _SECURE_SCL 1
// отключаем исключения при ошибке - будет вызван invalid_parameter()
#define _SECURE_SCL_THROWS 0

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef LINUX
    // workaround - alignment 8 for <boost/format.hpp> & <winsock2.h>
    #include <pshpack8.h>

    #if defined(_LIB)
    #   include <winsock2.h>
    #else
    #   include <afxwin.h>         // MFC core and standard components
    #   include <afxext.h>         // MFC extensions
    #endif
#endif

// -=- STL -=-
#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

#ifndef LINUX
    // -=- BOOST -=-
    #define BOOST_ABI_PREFIX <pshpack8.h>
    #define BOOST_ABI_SUFFIX <poppack.h>
#endif

#define BOOST_SYSTEM_USE_UTF8

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <boost/smart_ptr/scoped_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/tuple/tuple.hpp>
// -=- ZDA -=-
#include "../helpful/X_translate.h"
#include "../helpful/RT_Macros.h"
#include "../helpful/Utf8.h"
//#include "../helpful/EsrKit.h"

// -=- mask x64 warning -=-
#define  _USE_MATH_DEFINES
#ifndef LINUX
    #pragma warning (push)
    #pragma warning (disable: 4985)
#endif

#include <math.h>
#ifndef LINUX
#include <intrin.h>
#pragma warning (pop)
#endif

#ifndef LINUX
// workaround - alignment 8 for <boost/format.hpp> & <winsock2.h>
#include <poppack.h>
#endif

#ifdef _WIN64
// Windows API x64 требует четного выравнивания для (wchar_t*)
namespace {
    struct required_wstring_alignment_for_Win64_API
    { 
        int X;
        bool b;
        std::wstring ws;
    };
    static_assert( std::alignment_of<required_wstring_alignment_for_Win64_API>::value >= 2,
                   "Forbidden option /Z1 under Win|x64" );
};

#endif // _WIN64
