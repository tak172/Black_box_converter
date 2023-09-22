// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

// For correct std::min and std::max usage
#define NOMINMAX
#ifndef LINUX
#include <Windows.h>
#endif // !LINUX

// -=- STL -=-
#include <algorithm>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

// -=- BOOST -=-
// workaround - некорректное выравнивание приводит к неожиданным ошибкам boost::Wformat и Debug и Release
#ifndef LINUX
#include <pshpack8.h>
#endif // !LINUX
#  include <boost/format.hpp>
#ifndef LINUX
#include <poppack.h>
#endif // !LINUX
#include <boost/foreach.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <boost/smart_ptr/scoped_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/tuple/tuple.hpp>

// Turn off auto-linking regex and datetime libraries with boost::threads (msvs only issue)
#ifndef BOOST_REGEX_NO_LIB
    #define BOOST_REGEX_NO_LIB
#endif
#include <boost/bind/bind.hpp>
#include <boost/ptr_container/ptr_vector.hpp>


#ifndef ASSERT
#define ASSERT(a) assert(a)
#endif

#if defined(_MSC_VER)
#define strtoll _strtoi64
#endif

#include "../helpful/RT_Macros.h"

