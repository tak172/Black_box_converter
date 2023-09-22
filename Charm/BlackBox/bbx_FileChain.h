#pragma once
#include <vector>
#include <string>
#include <boost/filesystem/path.hpp>
#include "../helpful/FilesByMask.h"

namespace Bbx
{
    class FileChain
    {
    public:
        FileChain( const std::wstring& directory, const std::wstring& fileMask, size_t reservSz );
        std::wstring getEarliestFile() const;
        time_t getEarliestFileEndTime() const;
        std::wstring takeEarliestFile();
        std::wstring getLatestFile( size_t minFileSize ) const;
        size_t getNumberOfFiles() const;
        FILE_SIZE getTotalSize() const;
        
        bool empty() const;
        std::vector<std::wstring> filesAroundRange(
            const std::wstring& firstFileName,
            const std::wstring& lastFileName,
            size_t minFileSize ) const;
        std::wstring selectNextFile( const std::wstring& currFile, size_t minFileSize, bool forwardDirection ) const;
    private:
        struct NameAndSize
        {
            std::wstring name;
            FILE_SIZE    size;
            NameAndSize( const std::wstring& fn, FILE_SIZE fs )
                : name(fn), size(fs)
            {}
        };

        boost::filesystem::path         m_directory;
        std::vector< NameAndSize > m_fileAndSize;         // собранные ТОЛЬКО имена и размеры файлов
        time_t                          m_earlestFileEnd; // последний момент в первом файле (или 0)
        std::wstring addDirectory( boost::wstring_ref name ) const;
    };
};
