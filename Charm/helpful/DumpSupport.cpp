#include "stdafx.h"
#pragma warning(disable:4091)
#include <DbgHelp.h>
#pragma warning(disable:4091)
#include <Psapi.h>
#include <string>
#include <time.h>
#include "Dump.h"
#include "DumpSupport.h"
#include "RT_Macros.h"

#pragma comment(lib, "dbghelp.lib")

std::wstring DumpSupport::EventName(DWORD _processId, const std::wstring& modVer, const std::wstring& suffix )
{
    std::wstring exe_name = FullPathWithoutExtension( _processId );

    // выделение только имени программы (расширение уже срезано ранее)
    wchar_t drive[_MAX_DRIVE];
    wchar_t dir[_MAX_DIR];
    wchar_t fname[_MAX_FNAME];
    wchar_t ext[_MAX_EXT];

    _wsplitpath_s( exe_name.c_str(), drive, dir, fname, ext );
    std::wstring temp = std::wstring(fname) + L"[" + std::to_wstring(_processId) + L"]";

    return temp + modVer + L" " + suffix;
}

// начальная часть имени файла дампа
std::wstring DumpSupport::FullPathWithoutExtension( DWORD _processId )
{
    std::wstring res;
    if ( HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ , FALSE, _processId ) )
    {
        res.resize( MAX_PATH+1, wchar_t(0) );
        DWORD aftSz = GetModuleFileNameEx( hProcess, NULL, &*res.begin(), size32(res) );
        if ( 0 != aftSz )
            res.resize(aftSz);
        else
            res.clear();
        CloseHandle( hProcess );
        if ( !res.empty() )
        {
            auto backDot = res.find_last_of('.');
            if ( std::wstring::npos != backDot )
                res.resize( backDot );
            else
                res.clear();
        }
    }
    return res;
}

void DumpSupport::WriteExplicitDump( DWORD procId, DWORD threadId, const std::wstring& modeVers, bool detailed, bool clientFlag, _EXCEPTION_POINTERS * pExPointers)
{
    std::wstring PartName = FullPathWithoutExtension( procId ) + modeVers;
    HANDLE hFile = OpenDumpFile( PartName );
    WriteDumpImp( hFile, detailed, procId, threadId, clientFlag, pExPointers );
    CloseDumpFile( hFile );
}

std::wstring DumpSupport::SuffixRequest()
{
    return L"request";
}

std::wstring DumpSupport::SuffixFinal()
{
    return L"final";
}

std::wstring DumpSupport::SuffixShMem()
{
    return L"shMem";
}

static MINIDUMP_TYPE GetType( bool detailed )
{
    unsigned type = MiniDumpNormal;
    if ( detailed )
    {
        type |= MiniDumpWithDataSegs;
        type |= MiniDumpWithPrivateReadWriteMemory;
        type |= MiniDumpWithHandleData;
        type |= MiniDumpWithFullMemory;
        type |= MiniDumpWithFullMemoryInfo;
        type |= MiniDumpWithThreadInfo;
        type |= MiniDumpWithProcessThreadData;
        type |= MiniDumpIgnoreInaccessibleMemory;
    }
    return (MINIDUMP_TYPE) type;
}

//создание файла для записи дампа
HANDLE DumpSupport::OpenDumpFile( const std::wstring& partialName )
{
    std::wstring name = partialName;
    //дополнение дата+время
    {
        struct tm curr;
        time_t t = time(NULL);
        localtime_s( &curr, &t );

        wchar_t buf[100];
        if ( wcsftime( buf, size_array(buf), L"_%y%m%d_%H%M%S", &curr ) )
            name += buf;
    }

    std::wstring temp = name;
    for( int pass = 0; pass<999; ++pass )
    {
        temp.resize( name.size() );
        if ( pass )
            temp += L"_"+std::to_wstring(pass);
        temp += CProcessDump::DumpExtension();

        HANDLE hFile = ::CreateFile( temp.c_str(), 
            FILE_WRITE_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );

        if ( INVALID_HANDLE_VALUE != hFile )
            return hFile;
    }
    return INVALID_HANDLE_VALUE;
}

void DumpSupport::WriteDumpImp( HANDLE hFile, bool detailed, DWORD processId, DWORD threadId, bool clientFlag, _EXCEPTION_POINTERS* pExPointers )
{
    unsigned type = GetType( detailed );
    if ( HANDLE processHandle = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId ) )
    {
        if ( pExPointers )
        {
            //класс инкапсулирующий информацию по исключению в текущем потоке данного процесса
            MINIDUMP_EXCEPTION_INFORMATION eInfo;
            eInfo.ThreadId = threadId;
            eInfo.ExceptionPointers = pExPointers;
            eInfo.ClientPointers = clientFlag;

            MiniDumpWriteDump( processHandle, processId, hFile, MINIDUMP_TYPE(type), &eInfo, NULL, NULL ) ;
        }
        else
        {
            MiniDumpWriteDump( processHandle, processId, hFile, MINIDUMP_TYPE(type), NULL, NULL, NULL ) ;
        }
        CloseHandle( processHandle );
    }
}

void DumpSupport::CloseDumpFile( HANDLE hFile )
{
    ::CloseHandle( hFile );
}

HANDLE DumpSupport::makeEvent( std::wstring evName )
{
    HANDLE hev = CreateEvent( NULL, TRUE, FALSE, evName.c_str() );
    if ( NULL == hev )
        ;//OutputDebugString( (L"Error creation event '"+evName+L"'\n").c_str() );
    else if ( ERROR_ALREADY_EXISTS == GetLastError() )
    {
        //OutputDebugString( (L"Event already exist '"+evName+L"'\n").c_str() );
        CloseHandle( hev );
        hev = NULL;
    }
    else
        ;//OutputDebugString( (L"Event created '"+evName+L"'\n").c_str() );
    return hev;
}

HANDLE DumpSupport::findEvent( std::wstring evName )
{
    HANDLE hh = CreateEvent( NULL, TRUE, FALSE, evName.c_str() );
    if ( !hh )
        ; // событие не нашлось
    else if ( ERROR_ALREADY_EXISTS != GetLastError() )
    {
        // ранее такого события не было - поиск неудачен
        CloseHandle( hh );
        hh = NULL;
    }
    return hh;
}

HANDLE DumpSupport::makeShMem( const std::wstring shMemName )
{
    HANDLE mapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        sizeof(DumpSupport::TransData),         // maximum object size (low-order DWORD)
        shMemName.c_str() );     // name of mapping object

    if ( mapFile != NULL )
    {
        void* transitData = MapViewOfFile( mapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DumpSupport::TransData) );
        if ( transitData )
        {
            UnmapViewOfFile(transitData);
            return mapFile;
        }
        else
            CloseHandle( mapFile );
    }
    mapFile = nullptr;
    return mapFile;
}


bool DumpSupport::readShMem( std::wstring shMemName, DumpSupport::TransData* trData )
{
    bool result = false;
    if ( HANDLE hMapFile = OpenFileMapping( FILE_MAP_ALL_ACCESS, FALSE, shMemName.c_str() ) )
    {
        if ( LPVOID pBuf = MapViewOfFile( hMapFile, FILE_MAP_ALL_ACCESS, 0,0, sizeof(DumpSupport::TransData) ) )
        {
            DumpSupport::TransData* pp = (DumpSupport::TransData*) pBuf;
            *trData = *pp;
            UnmapViewOfFile(pBuf);
            result = true;
        }
        CloseHandle(hMapFile);
    }
    return result;
}

bool DumpSupport::writeShMem( std::wstring shMemName, const DumpSupport::TransData& trData )
{
    bool result = false;
    if ( HANDLE hMapFile = OpenFileMapping( FILE_MAP_ALL_ACCESS, FALSE, shMemName.c_str() ) )
    {
        if ( LPVOID pBuf = MapViewOfFile( hMapFile, FILE_MAP_ALL_ACCESS, 0,0, sizeof(DumpSupport::TransData) ) )
        {
            DumpSupport::TransData* pp = (DumpSupport::TransData*) pBuf;
            *pp = trData;
            UnmapViewOfFile(pBuf);
            result = true;
        }
        CloseHandle(hMapFile);
    }
    return result;
}
