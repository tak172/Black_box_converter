#include "stdafx.h"

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include "DuePath.h"
#include "Dump.h"
#include "DumpSupport.h"
#include "FilesByMask.h"
#include "Pilgrim.h"
#include "RT_Macros.h"
#include "../_version_.h"

#ifndef LINUX
// Файл <atlfile.h> вообще-то не требуется, но без него не собирается Hem.dll
#include <atlfile.h>

#pragma comment(lib, "dbghelp.lib")

class Locker
{
public:
    Locker( const std::wstring& partName );
    ~Locker();
private:
    HANDLE m_namedMtx;
    bool m_owned;
};

Locker::Locker( const std::wstring& partName )
    : m_namedMtx(INVALID_HANDLE_VALUE), m_owned(false)
{
    std::wstring temp = partName + L"~~~lock" + CProcessDump::DumpExtension();
    {
        // замена недопустимого символа
        const char BACKSLASH = '\\';
        const char PERCENT = '%';
        for( auto pos = temp.find( BACKSLASH ); std::string::npos != pos; pos = temp.find( BACKSLASH ) )
            temp[pos] = PERCENT;
    }
    m_namedMtx = CreateMutex( NULL, FALSE, temp.data() );
    ASSERT( m_namedMtx != NULL );

    // на постановку блокировки отведено 20 секунд, далее работаем без блокировки
    const boost::posix_time::ptime from = boost::posix_time::microsec_clock::universal_time();
    const boost::posix_time::ptime to = from + boost::posix_time::seconds( 20 );
    while( !m_owned && boost::posix_time::microsec_clock::universal_time() < to )
    {
        DWORD dwWaitResult = WaitForSingleObject( m_namedMtx, 200 );
        m_owned = ( WAIT_OBJECT_0 == dwWaitResult );
    }
    //ASSERT( m_owned );
}

Locker::~Locker()
{
    if ( m_owned )
        ::ReleaseMutex( m_namedMtx );
    ::CloseHandle( m_namedMtx );
    m_owned = false;
    m_namedMtx = INVALID_HANDLE_VALUE;
}

class PhantomLink
{
public:
    PhantomLink();
    ~PhantomLink();
    bool exist() const;
    bool run( DWORD threadId, bool detailed, _EXCEPTION_POINTERS* pExPointers ); // запись дампа внешним процессом
private:
    enum LinkState
    {
        pls_none,    // исходное состояние
        pls_started, // запущен свой экземпляр фантома
        pls_already, // найден запущенный экземпляр фантома
        pls_error    // запустить фантом не удалось
    };
    LinkState m_linkState; // состояние связи с фантомом
    HANDLE m_eventRequest; // событие заказа дампа
    HANDLE m_eventFinal;   // событие уведомления об окончании записи дампа
    HANDLE m_mapFile;      // файл общей памяти

    bool waitFinal();
    LinkState start( unsigned pid, std::wstring modVer ); // запуск фантома на исполнение
    bool putTransit( DWORD threadId, bool detailed, _EXCEPTION_POINTERS* pExPointers );
};

PhantomLink::PhantomLink()
    : m_linkState(pls_none),
    m_eventRequest(INVALID_HANDLE_VALUE),
    m_eventFinal(INVALID_HANDLE_VALUE),
    m_mapFile(NULL)
{
    DWORD pid = GetCurrentProcessId();
    std::wstring modVer = CProcessDump::ModeAndVersion();
    m_eventRequest  = DumpSupport::makeEvent( DumpSupport::EventName( pid, modVer, DumpSupport::SuffixRequest() ) );
    if ( m_eventRequest )
    {
        m_eventFinal = DumpSupport::makeEvent( DumpSupport::EventName( pid, modVer, DumpSupport::SuffixFinal() ) );
        m_mapFile = DumpSupport::makeShMem( DumpSupport::EventName( pid, modVer, DumpSupport::SuffixShMem() ) );
        m_linkState = start( pid, modVer );
        //OutputDebugString( L"PhantomLink - create\n" );
    }
    else if ( HANDLE temp = DumpSupport::findEvent( DumpSupport::EventName( pid, modVer, DumpSupport::SuffixRequest() ) ) )
    {
        m_eventRequest = temp;
        m_eventFinal = DumpSupport::findEvent( DumpSupport::EventName( pid, modVer, DumpSupport::SuffixFinal() ) );
        m_linkState = pls_already;
        //OutputDebugString( L"PhantomLink - reuse\n" );
    }
}

PhantomLink::~PhantomLink()
{
    if ( pls_started == m_linkState )
    {
        CloseHandle( m_eventRequest );
        CloseHandle( m_eventFinal );
        CloseHandle( m_mapFile );
    }
}

bool PhantomLink::exist() const
{
    return pls_started == m_linkState || pls_already == m_linkState;
}

bool PhantomLink::run( DWORD threadId, bool detailed, _EXCEPTION_POINTERS* pExPointers )
{
    if ( putTransit( threadId, detailed, pExPointers ) )
    {
        //OutputDebugString(L"PhantomLink - call remote dump\n");
        SetEvent( m_eventRequest );
        while( !waitFinal() )
                ;
        return ResetEvent( m_eventFinal )? true : false;
    }
    else
        return false;
}

bool PhantomLink::waitFinal()
{
    return WAIT_OBJECT_0 == WaitForSingleObject(m_eventFinal, 1000 );
}

PhantomLink::LinkState PhantomLink::start( unsigned pid, std::wstring modVer )
{
#ifdef _WIN64
    bool x64 = true;
#else
    bool x64 = false;
#endif
    auto selfExe = Pilgrim::instance()->ExeFileName();
    auto phantomExe = DuePath_firstExisted( DuePath( selfExe, x64, _TEXT(PHANTOM_EXE) ) );

    const wchar_t Q = '"';
    std::wstring phantomCmdLine;
    phantomCmdLine += Q + phantomExe + Q;
    phantomCmdLine += L" ";
    phantomCmdLine += std::to_wstring( pid );
    phantomCmdLine += L" ";
    phantomCmdLine += Q + modVer + Q;
    STARTUPINFO si;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory( &pi, sizeof(pi) );

    auto bSuccess = CreateProcess(  NULL, 
        &*phantomCmdLine.begin(),   // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW,  // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &si,  // STARTUPINFO pointer 
        &pi);  // receives PROCESS_INFORMATION 
    if ( bSuccess )
    {
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    return bSuccess? pls_started : pls_error;
}

bool PhantomLink::putTransit( DWORD threadId, bool detailed, _EXCEPTION_POINTERS* pExPointers )
{
    DumpSupport::TransData td;
    td.tdSize = sizeof(td);
    td.tdThreadId = threadId;
    td.tdPtrEP = pExPointers;
    td.tdReqDetailed = detailed;

    DWORD pid = GetCurrentProcessId();
    std::wstring modVer = CProcessDump::ModeAndVersion();

    std::wstring shName = DumpSupport::EventName( pid, modVer, DumpSupport::SuffixShMem() );
    return DumpSupport::writeShMem( shName, td );
}

static PhantomLink phantomLink;
#endif

static bool only_detalized = false;


// дамп текущего процесса
CProcessDump::CProcessDump( _EXCEPTION_POINTERS * pExPointers, bool detailed )
{
    if ( detailed || !only_detalized )
    {
        if ( !ExistPhantom() || !MakeRemoteDump( detailed, pExPointers ) )
            MakeLocalDump( detailed, pExPointers );
    }
}

bool CProcessDump::ExistPhantom() const
{
#ifndef LINUX
    return phantomLink.exist();
#else
    return false;
#endif
}

// дамп текущего процесса
void CProcessDump::MakeLocalDump( bool detailed, _EXCEPTION_POINTERS * pExPointers )
{
#ifndef LINUX
    DWORD   processId = GetCurrentProcessId();
    std::wstring PartFName = DumpSupport::FullPathWithoutExtension( processId ) + ModeAndVersion();
    Locker exclusive( PartFName );
    HANDLE hFile = DumpSupport::OpenDumpFile( PartFName );
    WriteLocal( hFile, detailed, pExPointers );
    DumpSupport::CloseDumpFile( hFile );
#else
    if( detailed || pExPointers )
    {
        ASSERT( !L"Dump called" );
        std::exit( 13 );
    }
    else
    {
        std::exit( 17 );
    }
#endif
}

// дамп с помощью внешнего процесса
bool CProcessDump::MakeRemoteDump( bool detailed, _EXCEPTION_POINTERS* pExPointers )
{
#ifndef LINUX
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId(); 
    std::wstring PartFName = DumpSupport::FullPathWithoutExtension( processId ) + ModeAndVersion();
    Locker exclusive( PartFName );
    return phantomLink.run( threadId, detailed, pExPointers );
#else
    return false;
#endif
}

// режим и версия
std::wstring CProcessDump::ModeAndVersion()
{
    std::wstring res;
    {
        // предполагаем, что все символы ASCII
        for( const char* mode = CHARM_BUILD_MODE; *mode; ++mode )
            res += wchar_t(*mode);
    }
    res += '_';

    const int v[4]={CHARM_VERSION_NUM};

    res += std::to_wstring( v[0] );
    res += '.';
    res += std::to_wstring( v[1] );
    res += '.';
    res += std::to_wstring( v[3] );
    return res;
}


void CProcessDump::SetDetailizedOnly( bool det )
{
    only_detalized = det;
}

#ifndef LINUX
int CProcessDump::WriteLocal( HANDLE hFile, bool detailed, _EXCEPTION_POINTERS* pExPointers ) const
{
    if ( !pExPointers )
    {
        // Generate exception to get proper context in dump
        __try 
        {
            RaiseException( 0, 0, 0, NULL );
        }
        __except( WriteLocal( hFile, detailed, GetExceptionInformation() ) )
        {}
    }
    else
    {
        bool clientFlag = false; // т.к. адреса местные, а не чужие
        DumpSupport::WriteDumpImp( hFile, detailed, GetCurrentProcessId(), GetCurrentThreadId(), clientFlag, pExPointers );
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static boost::filesystem::path getFolderWithMask()
{
    boost::filesystem::path selfExe = Pilgrim::instance()->ExeFileName();
    std::wstring mask = selfExe.stem().generic_wstring() + L"*" + CProcessDump::DumpExtension();
    boost::filesystem::path res = selfExe.parent_path()/mask;
    return res;
}


std::pair<size_t,bool> CProcessDump::RestrictSize( unsigned totalSize_Mb )
{
    // опираемся на имя модуля
    boost::filesystem::path fold_mask = getFolderWithMask();

    // собрать имена файлов с размерами
    std::vector< std::pair<std::wstring,uint64_t>  > info;

    auto collect = [ &info ]( const FilesByMask_Data& blk ){
        info.emplace_back( std::make_pair( blk.fname.to_string(), blk.fsize ) );
        return true;
    };
    FilesByMask( fold_mask.generic_wstring(), collect );

    const uint64_t totalSize = totalSize_Mb * uint64_t( 1024*1024 );
    uint64_t currSize = 0;
    for( auto& ns : info )
        currSize += ns.second;

    size_t resDeleted = 0;
    bool   resSuccess = true;
    if ( currSize > totalSize )
    {
        std::wstring fold = fold_mask.parent_path().generic_wstring() + L"/";
        std::sort( info.begin(), info.end() );
        for( auto it = info.begin();
            info.end() != it && currSize > totalSize;
            /*none!*/ )
        {
            boost::system::error_code ec;
            if ( boost::filesystem::remove( ( fold + it->first ).c_str(), ec ) )
            {
                currSize -= it->second;
                it = info.erase( it );
                ++resDeleted;
            }
            else
            {
                ++it;
                resSuccess = false;
            }
        }
    }

    return std::make_pair( resDeleted, resSuccess );
}

// ограничить количество дампов
std::pair<size_t,bool> CProcessDump::RestrictCount( unsigned down_count, unsigned up_count )
{
    // опираемся на имя модуля
    boost::filesystem::path fold_mask = getFolderWithMask();

    // собрать имена файлов с размерами
    std::vector< std::pair<std::wstring,uint64_t>  > info;

    auto collect = [ &info ]( const FilesByMask_Data& blk ){
        info.emplace_back( std::make_pair( blk.fname.to_string(), blk.fsize ) );
        return true;
    };
    FilesByMask( fold_mask.generic_wstring(), collect );
    std::sort( info.begin(), info.end() ); // по именам и потом уже по размеру

    const uint64_t LEVEL = 10 * uint64_t( 1024*1024 ); // граница между малыми и большими дампами

    size_t dn = 0;
    size_t up = 0;
    for( auto& ns : info )
        if ( ns.second < LEVEL )
            ++dn;
        else
            ++up;

    size_t resDeleted = 0;
    bool   resSuccess = true;
    if ( dn > down_count )
        dn -= down_count;
    else
        dn = 0;
    if ( up > up_count )
        up -= up_count;
    else
        up = 0;

    std::wstring fold = fold_mask.parent_path().generic_wstring() + L"/";
    for( auto it = info.begin(); info.end() != it && ( dn != 0 || up != 0 ); /*none!*/ )
    {
        boost::system::error_code ec;
        bool sm = ( it->second < LEVEL );
        if ( 0 == ( sm? dn : up ) )
            ; // этот файл не удалять
        else if( boost::filesystem::remove( ( fold + it->first ).c_str(), ec ) )
        {
            if( sm )
                --dn;
            else
                --up;
            it = info.erase( it );
            ++resDeleted;
        }
        else
        {
            ++it;
            resSuccess = false;
        }
    }

    return std::make_pair( resDeleted, resSuccess );
}
