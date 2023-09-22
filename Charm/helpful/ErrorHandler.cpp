#include "stdafx.h"
#include <string>
#include <vector>
#include <signal.h>
#include <boost/thread/thread.hpp>
#include <boost/format.hpp>

#ifndef LINUX
#include "Dump.h"
#endif // !LINUX
#include "AssertHandler.h"
#include "ErrorHandler.h"
#include "../helpful/X_translate.h"
#ifndef LINUX
#include "../helpful/BriefMsgBox.h"
#else
#include <iostream>
#endif // !LINUX
#include "../helpful/synch.h"

#ifndef LINUX
// Получить адрес критической секции для любых операций
static CRITICAL_SECTION* getSysExit_CS()
{
    static CRITICAL_SECTION cs;
#pragma pack(push,16)
    struct alignedStruct_t
    {
        bool dummy;
        long init;
    };
    static alignedStruct_t myStruct = {0, 0};

    if ( 0 == InterlockedExchange( &myStruct.init, 1 ) )
    {
        // инициализация выполняется быстро ИМХО
        InitializeCriticalSection( &cs );
    }
    else
        // обращение к уже инициализированному - специально задержана
        Sleep( 100 );
    return &cs;
#pragma pack(pop)
}

//
// Всё выше - заимствовано из RDC
//


#ifndef UNITTEST
static void DoFatalError( LPEXCEPTION_POINTERS lpEP, const std::wstring &error_txt, bool ShowMsgBox )
{
#ifdef DEBUG
    AfxEnableMemoryLeakDump(FALSE);
#endif // DEBUG
    CProcessDump dump( lpEP, true );
    CProcessDump::RestrictSize( 10 * 1024 ); 

    if ( ShowMsgBox )
        BriefMsgBox( NULL, error_txt.c_str(), trx(L"Ошибка").c_str(), MB_ICONERROR, 60 );
    signal( SIGABRT, SIG_DFL );
    abort();
}

static bool error_active = false;

bool IsFatalErrorActive()
{
    return error_active;
}

void FatalError( LPEXCEPTION_POINTERS lpEP, bool showMsgBox )
{
    error_active = true;
    LockBy_CriticalSection myLock( getSysExit_CS() );
    if ( IsDebuggerPresent() )
        DebugBreak();
    std::wstring error_txt = trx(L"Обнаружена критическая ошибка!\nПрограмма будет завершена.");
    DoFatalError(lpEP, error_txt, showMsgBox);
}

void FatalError( const wchar_t* error_str, bool showMsgBox )
{
    error_active = true;
    LockBy_CriticalSection myLock( getSysExit_CS() );
    if ( IsDebuggerPresent())
        DebugBreak();
    std::wstring error_txt = trx(L"Обнаружена критическая ошибка!") + L'\n';
    if ( error_str )
    {
        error_txt += error_str;
        error_txt += L'\n';
    }
    error_txt += trx(L"Программа будет завершена.");
    DoFatalError( nullptr, error_txt, showMsgBox );
}
#endif

static LPTOP_LEVEL_EXCEPTION_FILTER PrevExceptionFilter = NULL;
typedef void (*PureCH)(void);
typedef void (*IinvPH)(
    const wchar_t * /*expression*/,
    const wchar_t * /*function*/, 
    const wchar_t * /*file*/, 
    unsigned int /*line*/,
    uintptr_t /*pReserved*/
    );
static PureCH Prev_purecall_handler = NULL;
static IinvPH Prev_invalid_parameter_handler = nullptr;

static void PureCallHandler()
{
    FatalError( L"Некорректный виртуальный вызов" );
}

static void InvalidParameterHandler(
                                    const wchar_t * expression,
                                    const wchar_t * function, 
                                    const wchar_t * file, 
                                    unsigned int /*line*/,
                                    uintptr_t /*pReserved*/
                                    )
{
    std::wstring ws = L"Некорректный вызов";
    if ( expression )
        ws = ws + L"\n expression:" + expression;
    if ( function )
        ws = ws + L"\n function:" + function;
    if ( file )
        ws = ws + L"\n file:" + file;
    FatalError( ws.c_str() );
}

#pragma warning(disable:4702)  
LONG WINAPI OwnUEF( _EXCEPTION_POINTERS* lpexpExceptionInfo ) // address of exception info
{
    if ( PrevExceptionFilter )
    {
        SetUnhandledExceptionFilter( PrevExceptionFilter );
        PrevExceptionFilter = NULL;
    }
    FatalError( lpexpExceptionInfo );
    return EXCEPTION_EXECUTE_HANDLER;
}
#pragma warning(default:4702)  

extern "C" void HandleSignalAbort( int )
{
    FatalError( L"Получен SIGABORT" );
}
extern "C" void HandleSignalTerm( int )
{
    FatalError( L"Получен SIGTERM" );
}

static int MyAtExit_active = true;

void MyAtExit()
{
    if ( MyAtExit_active )
        FatalError( L"Неожиданный exit()" );
}

static void ExceptionInstall()
{
    if ( !PrevExceptionFilter )
    {
        SetErrorMode( SEM_FAILCRITICALERRORS
            | SEM_NOGPFAULTERRORBOX
            | SEM_NOOPENFILEERRORBOX );
        PrevExceptionFilter = SetUnhandledExceptionFilter( OwnUEF );
    }
    Prev_purecall_handler = _set_purecall_handler( PureCallHandler );
    Prev_invalid_parameter_handler = _set_invalid_parameter_handler( InvalidParameterHandler );
    AssertHandlerInstall();
    signal( SIGABRT, HandleSignalAbort );
    signal( SIGTERM, HandleSignalTerm );
    MyAtExit_active = true;
    atexit( MyAtExit );
}

static void ExceptionUninstall()
{
    MyAtExit_active = false;
    signal( SIGABRT, SIG_DFL );
    signal( SIGTERM, SIG_DFL );
    AssertHandlerUninstall();
    _set_invalid_parameter_handler( Prev_invalid_parameter_handler );
    _set_purecall_handler( Prev_purecall_handler );
    SetUnhandledExceptionFilter( PrevExceptionFilter );
}

static void se_trans_func( unsigned int /*U*/, struct _EXCEPTION_POINTERS* pExc )
{
	FatalError( (boost::wformat(trx(L"Критическая ошибка (0x%X) IP=0x%X\n"
		L"parms=%d: x%02X x%02X x%02X x%02X x%02X\n")) %
		pExc->ExceptionRecord->ExceptionCode %
		pExc->ExceptionRecord->ExceptionAddress %
		pExc->ExceptionRecord->NumberParameters %
		pExc->ExceptionRecord->ExceptionInformation[0] %
		pExc->ExceptionRecord->ExceptionInformation[1] %
		pExc->ExceptionRecord->ExceptionInformation[2] %
		pExc->ExceptionRecord->ExceptionInformation[3] %
		pExc->ExceptionRecord->ExceptionInformation[4] ).str().c_str() );
}

static void unexp_handler()
{
	FatalError( L"unexp_handler" );
}

static void term_func()
{
	FatalError( L"term_func" );
}

static void term_new_handler()
{
    FatalError( L"Memory allocation fail!" );
}

#pragma warning(disable:4702)  
static int mfc_new_handler(size_t)
{
    FatalError( L"Memory allocation fail!" );
    return 0;
}
#pragma warning(default:4702)  

void ExceptionInstall_CurrentThread()
{
	_set_error_mode( _OUT_TO_STDERR );
	set_unexpected( unexp_handler );
	set_terminate( term_func );
    std::set_new_handler( term_new_handler );
#ifdef __AFX_H__
    AfxSetNewHandler( mfc_new_handler );
#endif
#pragma warning(push)
#pragma warning(disable:4535)
	_set_se_translator( se_trans_func );
#pragma warning(pop)
}
#else
void FatalError( const wchar_t* error_str, bool /*ShowMsgBox*/ )
{
	//LockBy_CriticalSection myLock(getSysExit_CS());
    std::cerr << "Обнаружена критическая ошибка!" << std::endl;
    std::wcerr << error_str << std::endl;
    std::cerr << "Программа будет завершена." << std::endl;
    abort();
}
void ExceptionInstall_CurrentThread()
{
    // Not implemented on Linux
}
#endif // !LINUX

ExceptionProcessHandler::ExceptionProcessHandler()
{
#ifndef LINUX
    ExceptionInstall();
#endif // !LINUX
}

ExceptionProcessHandler::~ExceptionProcessHandler()
{
#ifndef LINUX
    ExceptionUninstall();
#endif // !LINUX
}
