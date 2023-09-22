#include "stdafx.h"
#include "DuePath.h"
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
/* Построить соответствующее имя модуля:
   Примеры:

   1. source = E:\Trunk\Charm\_out_\Fund64.Debug\Fund64.exe
      x64 = true
      target = "Actor"
      result --> "E:\Trunk\Charm\_out_\Actor64.Debug\Actor64.exe" (по дереву проекта)
             --> "E:\Trunk\Charm\_out_\Fund64.Debug\Actor64.exe"  (соответствующее имя в папке source)
             --> "E:\Trunk\Charm\_out_\Fund64.Debug\Actor.exe"    (редуцированное имя в папке source)

   3. source = C:\Retime\Charm\Fund.exe // редуцированное
      x64 = true
      target = "Guess.dll"
      result --> "C:\Retime\Charm\Guess64.dll" (соответствующее имя в папке source)
             --> "C:\Retime\Charm\Guess.dll"   (редуцированное имя в папке source)
*/ 
vector<wstring> DuePath( const wstring& myExe, bool x64, const wstring& request )
{
    namespace fs = boost::filesystem;
    namespace ba = boost::algorithm;
    const wstring _64(L"64");
    const wstring _32(L"");
    vector<wstring> result;
    fs::path origPath( boost::algorithm::trim_copy( request ) );
    result.push_back( origPath.generic_wstring() );

    // извлечь сердцевину source
    fs::path exePath( myExe );
    wstring exeStem = exePath.stem().wstring();
    if ( ba::ends_with(exeStem,_64) )
        exeStem.resize( exeStem.rfind(_64) );

    // построить нужные части request'a
    fs::path reqPath( boost::algorithm::trim_copy(request) );
    wstring reqStemFull    = reqPath.stem().wstring();
    wstring reqStemReduced = reqStemFull;
    if ( ba::ends_with(reqStemFull,_64) )
    {
        if ( !x64 ) // текущий режим работы 32, а запрашивается 64
        {
            reqStemFull.resize( reqStemFull.rfind(_64) ); // отрезать 64
            reqStemReduced = reqStemFull;                 // отрезать 64
        }
    }
    else 
    {
        if ( x64 ) // текущий режим 64, а запрос редуцирован до 32
        {
            reqStemFull += _64;
            ;// reqStemReduced - уже редуцирован
        }
    }
    if ( reqPath.extension() == L".dll"  )
        reqStemReduced = reqStemFull; // библиотеки редуцировать нельзя
    const wstring reqExt = reqPath.has_extension()? reqPath.extension().wstring() : L".exe";

    // породить имена файла для известных шаблонов
    const wstring DF( L".Debug" );
    const wstring RF( L".Release" );
    wstring upFolder = exePath.parent_path().filename().wstring();
    bool deb = ( upFolder == (exeStem + (x64? _64:_32) + DF ) );
    bool rel = ( upFolder == (exeStem + (x64? _64:_32) + RF ) );
    fs::path rootPath = exePath.parent_path().parent_path();
    if ( deb || rel )
    {
        auto FF = (deb? DF:RF);
        result.push_back( ( rootPath / (reqStemFull + FF) / (reqStemFull    + reqExt) ).generic_wstring() );
        result.push_back( ( rootPath / (reqStemFull + FF) / (reqStemReduced + reqExt) ).generic_wstring() );
    }
    // породить имена файла для папки source
    result.push_back( (exePath.parent_path() / (reqStemFull    + reqExt) ).generic_wstring() );
    result.push_back( (exePath.parent_path() / (reqStemReduced + reqExt) ).generic_wstring() );

    // убрать дубликаты
    result.erase( unique( result.begin(), result.end() ), result.end() );
    // 
    return result;
}


wstring DuePath_firstExisted( const vector<wstring>& dirt )
{
    for( auto fname : dirt )
    {
        boost::system::error_code ec;
        if ( boost::filesystem::exists( fname, ec ) )
            return fname;
    }
    return wstring();
}
