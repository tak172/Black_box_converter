#pragma once
#include <functional>
#include <boost/utility/string_ref.hpp>

typedef unsigned long long FILE_SIZE; // тип размера файла

struct FilesByMask_Data
{
    boost::wstring_ref fname;
    FILE_SIZE    fsize;
    FilesByMask_Data( boost::wstring_ref _n, FILE_SIZE _s )
        : fname(_n), fsize(_s)
    {}
};

// Перебор всех файлов по маске с учетом предиката
// Обработка продолжается до тех пор пока func()==true
void FilesByMask( const std::wstring& full_path_filemask, std::function< bool( const FilesByMask_Data& ) > func );
// Подходит ли имя файла под маску (важно: файл без пути)
bool FileMatch( const std::wstring& fname, const std::wstring& wildcard_mask );
