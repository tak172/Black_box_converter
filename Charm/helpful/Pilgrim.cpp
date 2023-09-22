#include "stdafx.h"

#include <vector>
#ifndef LINUX
#include <shlwapi.h>
#endif // !LINUX
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/process/search_path.hpp>

//#include <regex>
#include "../helpful/Pilgrim.h"
#include "../helpful/RT_Macros.h"


namespace bfs = boost::filesystem;

// Все разделители (slash и backslash) сделать прямыми (slash)
static boost::filesystem::path Slashing( boost::wstring_ref wstr )
{
    std::wstring tmp;
    std::replace_copy( wstr.begin(), wstr.end(), std::back_inserter( tmp ), L'\\', L'/' );
    return bfs::path( tmp );
}

ALLOCATE_SINGLETON(Pilgrim);

Pilgrim::Pilgrim()
{
    Startup_CurrentDir = bfs::current_path().generic_wstring();

#ifndef LINUX
    wchar_t szModuleFilePath[MAX_PATH];
	::GetModuleFileName(NULL, szModuleFilePath, size_array(szModuleFilePath));
    wchar_t full2[ _MAX_PATH ];
    GetFullPathName( szModuleFilePath, size_array( full2 ), full2, NULL );
    Current_Exe = Slashing(full2).generic_wstring();
#else
    char szModuleFilePath[FILENAME_MAX];
    szModuleFilePath[0] = '\0';
	ssize_t size = readlink("/proc/self/exe", szModuleFilePath, FILENAME_MAX - 1);
	if (size != -1)
        szModuleFilePath[size] = 0;
    boost::system::error_code ec;
    if( bfs::exists( bfs::path( szModuleFilePath ), ec ) && !ec )
    {
        Current_Exe = bfs::path( szModuleFilePath ).generic_wstring();
    } else {
        auto tp = boost::this_process::path();
        auto ff = boost::process::search_path( szModuleFilePath );
        Current_Exe = ff.generic_wstring();
        if( Current_Exe.empty() )
            throw L"Can't build self full path name";
    }
#endif
}

std::wstring Pilgrim::FileToPath( const std::wstring& wfile )
{
#ifndef LINUX
    boost::system::error_code ec;
    auto path = bfs::absolute( wfile, ec ).parent_path(); // абсолютный путь
#else
    auto path = bfs::absolute( wfile ).parent_path(); // абсолютный путь
#endif
    std::wstring res = path.generic_wstring();            // все разделители сделать slash
    res.push_back( '/' );
    return res;
}

void Pilgrim::Set_HomeFile(const std::wstring& wstr)
{
	Path_2_Home = FileToPath(wstr);
    boost::system::error_code ec;
    bfs::current_path(Path_2_Home, ec);
}

void Pilgrim::Set_EmfPath( const std::wstring& wstr )
{
    Path_2_Emf = Slashing( wstr ).generic_wstring();
}

void Pilgrim::Set_BvsFilePath( const std::wstring& wstr )
{
	Path_2_BvsFile = Slashing( wstr ).generic_wstring();
    Path_2_Bvs = bfs::path( Path_2_BvsFile ).parent_path().generic_wstring();
}

void Pilgrim::Set_SoundPath( boost::wstring_ref wstr )
{
    Path_2_Sound = Slashing( wstr ).generic_wstring();
}

void Pilgrim::Set_LastOpenPath( const std::wstring & wstr )
{
	Path_2_LastOpen = Slashing( wstr ).generic_wstring();
}

std::wstring Pilgrim::fullizePath(const wchar_t* somepath, const std::wstring& rootpath) const
{
    boost::system::error_code ec;
#ifndef LINUX
    auto path = bfs::absolute( Slashing(somepath), rootpath, ec ); // абсолютный путь
#else
    auto path = bfs::absolute( Slashing(somepath), rootpath ); // абсолютный путь
#endif
    path = path.lexically_normal(); // почистить . ..
    std::wstring res = path.generic_wstring();                         // все разделители сделать slash
    if( bfs::is_directory( path, ec ) )
        res += '/';
    return res;
}

#ifdef _ATL

bool Pilgrim::LoadRegistry( LPCTSTR m_pszAppName )
{
	std::wstring emf_path = LoadRegistryPath( m_pszAppName, L"PathToEmf" );
	if ( !emf_path.empty() )
		Pilgrim::instance()->Set_EmfPath( Slashing(emf_path).generic_wstring() );
	std::wstring bvs_path = LoadRegistryPath( m_pszAppName, L"PathToBvs" );
	if ( !bvs_path.empty() )
		Pilgrim::instance()->Set_BvsFilePath( Slashing(bvs_path).generic_wstring() );
    return !emf_path.empty() || !bvs_path.empty();
}

std::wstring Pilgrim::LoadRegistryPath( LPCTSTR app_name, const std::wstring& pathname )
{
	std::wstring res_path;
	//DWORD len = 255;
	const std::wstring subkey = L"Software\\Charm\\" + std::wstring(app_name) + L"\\Settings";
    {
        HKEY hKey;
        DWORD rgres = RegOpenKeyEx( HKEY_CURRENT_USER, subkey.c_str(), NULL, KEY_QUERY_VALUE, &hKey);
        if ( rgres == ERROR_SUCCESS )
        {
            res_path = std::wstring( size_t(1024), 0 );
            DWORD len = 255;
            rgres = RegQueryValueEx( hKey, pathname.c_str(), NULL, NULL, ( LPBYTE ) res_path.data(), &len );
            RegCloseKey( hKey );
            if ( ERROR_SUCCESS == rgres )
                res_path = res_path.c_str();
            else
                res_path.clear();
        }
    }

	//если каталог найден, проверяем, есть ли там что-нибудь
	bool files_exist = true;
	if ( !res_path.empty() )
	{
		std::wstring any_file( (Slashing( res_path )/L"*.*").generic_wstring() );
        WIN32_FIND_DATA wfd;
        HANDLE find_handle = FindFirstFile( any_file.c_str(), &wfd );
        if ( INVALID_HANDLE_VALUE != find_handle )
        {
            FindClose(find_handle);
            files_exist = true;
        }
        else
        {
            files_exist = false;
        }
	}

	if ( res_path.empty() || !files_exist )
	{
        HKEY hKey;
		DWORD rgres = RegOpenKeyEx( HKEY_LOCAL_MACHINE, subkey.c_str(), NULL, KEY_QUERY_VALUE, &hKey );
		if ( rgres == ERROR_SUCCESS )
        {
            res_path = std::wstring( size_t(1024), 0 );
            DWORD len = 255;
            rgres = RegQueryValueEx( hKey, pathname.c_str(), NULL, NULL, ( LPBYTE ) res_path.data(), &len );
            RegCloseKey( hKey );
            if ( ERROR_SUCCESS == rgres )
                res_path = res_path.c_str();
            else
                res_path.clear();
        }
	}
	return res_path;
}

void Pilgrim::SaveRegistry( LPCTSTR m_pszAppName )
{
	wchar_t subkey[1024];
	wsprintf( subkey, L"Software\\Charm\\%s\\Settings", m_pszAppName );
	HKEY hKey;
	DWORD creatingResult; //REG_CREATED_NEW_KEY, REG_OPENED_EXISTING_KEY
	LONG rgres = RegCreateKeyEx( HKEY_CURRENT_USER,subkey, NULL, NULL, NULL, KEY_ALL_ACCESS, NULL, &hKey, &creatingResult );
	if ( rgres == ERROR_SUCCESS )
	{
		RegSetValueEx( hKey, L"PathToEmf", NULL, REG_SZ, ( LPBYTE ) Path_2_Emf.c_str(), (DWORD)(( Path_2_Emf.length() + 1 ) * sizeof( wchar_t )) );
		RegSetValueEx( hKey, L"PathToBvs", NULL, REG_SZ, ( LPBYTE ) Path_2_BvsFile.c_str(), (DWORD)(( Path_2_BvsFile.length() + 1 ) * sizeof( wchar_t )) );
	}
}
#endif