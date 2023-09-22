#include "stdafx.h"
#ifndef LINUX
    #include <Windows.h>
#endif
#include <algorithm>
#include <cwctype>
#include <functional>
#include <string>
#include <regex>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/utility/string_ref.hpp>
#include "FilesByMask.h"

namespace fs = boost::filesystem;

// компаратор
static bool comp( wchar_t ch_mask, wchar_t ch_file )
{
    bool eq = ( ch_mask == ch_file );
    if( !eq )
        eq = ( '?' == ch_mask && '.' != ch_file );
#ifndef LINUX
    // под Windows регистр не важен
    if( !eq )
        eq = ( std::towupper( ch_mask ) == std::towupper( ch_file ) );
#endif
    return eq;
};

static bool wildcards_match( boost::wstring_ref fname, boost::wstring_ref mask )
{
    if( mask == L"*.*" )
        return true;

    // обработать суффикс
    while( !mask.empty() && !fname.empty() && comp( mask.back(), fname.back() ) ) {
        mask.remove_suffix( 1 );
        fname.remove_suffix( 1 );
    }
    if( !mask.empty() && mask.back() != L'*' )
        return false;

    // обработать префикс
    while( !mask.empty() && !fname.empty() && comp( mask.front(), fname.front() ) ) {
        mask.remove_prefix( 1 );
        fname.remove_prefix( 1 );
    }
    if( !mask.empty() && mask.front() != L'*' )
        return false;
    return true;
}

#ifndef LINUX
void FilesByMask( const std::wstring& full_path_filemask, std::function< bool( const FilesByMask_Data & )> func )
{
    fs::path full( full_path_filemask );
    std::wstring ws_name_mask = full.filename().wstring();
    boost::wstring_ref name_mask( ws_name_mask );
    WIN32_FIND_DATA blk;
    HANDLE searchHandle = FindFirstFile( full_path_filemask.c_str(), &blk );
    if ( INVALID_HANDLE_VALUE != searchHandle ) {
        bool more = true;
        do {
            if( wildcards_match( blk.cFileName, name_mask ) ) {
                const size_t shift = CHAR_BIT * sizeof( blk.nFileSizeLow );
                auto sz = FILE_SIZE( blk.nFileSizeHigh ) << shift;
                sz |= blk.nFileSizeLow;
                FilesByMask_Data fd( blk.cFileName, sz );
                more = func( fd );
            } else {
                more = true;
            }
        }
        while( more && FindNextFile( searchHandle, &blk ) );
        FindClose( searchHandle );
    }
}
#else
void FilesByMask( const std::wstring& full_path_filemask, std::function< bool( const FilesByMask_Data& ) > func )
{
    fs::path full( full_path_filemask );
    std::wstring name = full.filename().wstring();
    boost::algorithm::replace_first( name, L"*.*", L"*" );
#ifdef _DEBUG
    auto pos = name.find('*');
    auto pos2 = name.rfind('*');
    assert( pos == pos2 );
#endif

    bool more = true;
    try
    {
        boost::system::error_code ec;
        fs::path dir = full.parent_path();
        if ( dir.empty() )
            dir = fs::current_path();
        size_t off_to_fname = 0;
        for( const auto& entry : fs::directory_iterator( dir, ec ) ) {
            // Skip if not a file
            if( fs::is_regular_file( entry.status() ) ) {
                if ( !off_to_fname ) {
                    std::wstring full = entry.path().wstring();
                    std::wstring part = entry.path().filename().wstring();
                    off_to_fname = full.find(part);
                }

                auto fn_base = entry.path().wstring();
                boost::wstring_ref fn(fn_base);
                fn.remove_prefix( off_to_fname );
                // select by mask
                if ( wildcards_match( fn, name ) )
                {
                    FilesByMask_Data temp( fn, fs::file_size( entry.path(), ec ) );
                    more = func( temp );
                    if ( !more )
                        break;
                }
            }
        }
    }
    catch( ... )
    {}
}
#endif

bool FileMatch( const std::wstring& fname, const std::wstring& wildcard_mask )
{
    return wildcards_match( fname, wildcard_mask );
}
