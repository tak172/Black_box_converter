#include "stdafx.h"
#include <numeric>
#include <boost/filesystem/path.hpp>
#include "bbx_FileChain.h"
#include "bbx_FileReader.h"
#include "bbx_Location.h"
#include "../helpful/FilesByMask.h"
#ifdef LINUX
#include "../helpful/Utf8.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif // LINUX

namespace bfs = boost::filesystem;

Bbx::FileChain::FileChain( const std::wstring& directory, const std::wstring& fileMask, size_t reservSz )
    : m_directory(directory), m_fileAndSize(), m_earlestFileEnd(0)
{
    m_fileAndSize.reserve( reservSz+2 );

	auto packer = [ this ]( const FilesByMask_Data& fileFindData )
    {
        ASSERT( fileFindData.fsize < UINT32_MAX && "Ошибка - нарушено ограничение 4Gb");
        m_fileAndSize.emplace_back( fileFindData.fname.to_string(), fileFindData.fsize );
        return true;
    };
    bfs::path p = directory;
    p /= fileMask;
    FilesByMask( p.wstring(), packer );

    auto cmp = []( const NameAndSize& one, const NameAndSize& two ){
        return Location::PathComparator()( one.name, two.name );
    };
    sort(m_fileAndSize.begin(), m_fileAndSize.end(), cmp);

    // заполнить конечный момент первого файла
    if ( !empty() )
    {
        auto rfInfo = Bbx::Impl::FileReader::getFileInfo( getEarliestFile() );
        if ( rfInfo.isCorrect() )
            m_earlestFileEnd = rfInfo.endTime;
    }
}

// получить полный путь начального файла цепи
std::wstring Bbx::FileChain::getEarliestFile() const
{
    std::wstring res;
    if ( !empty() )
        res = ( m_directory/m_fileAndSize.front().name ).wstring();
    return res;
}

time_t Bbx::FileChain::getEarliestFileEndTime() const
{
    return m_earlestFileEnd;
}

// получить полный путь начального файла цепи и удалить его из цепочки
std::wstring Bbx::FileChain::takeEarliestFile()
{
    std::wstring res = getEarliestFile();
    if ( !empty() )
        m_fileAndSize.erase( m_fileAndSize.begin() );
    m_earlestFileEnd = 0;
    return res;
}

std::wstring Bbx::FileChain::getLatestFile(size_t minFileSize) const
{
    auto it = std::find_if( m_fileAndSize.rbegin(), m_fileAndSize.rend(),
        [minFileSize]( const NameAndSize& nas ) {
            return nas.size >= minFileSize;
    });

    std::wstring res;
    if ( it != m_fileAndSize.rend() )
        res = addDirectory( it->name );
    return res;
}

size_t Bbx::FileChain::getNumberOfFiles() const
{
    return m_fileAndSize.size();
}

FILE_SIZE Bbx::FileChain::getTotalSize() const
{
    FILE_SIZE totalSz = std::accumulate( m_fileAndSize.begin(), m_fileAndSize.end(), FILE_SIZE(0),
        [](FILE_SIZE init, const NameAndSize& nas){
        return init + nas.size;
    });
    return totalSz;
}

bool Bbx::FileChain::empty() const
{
    return m_fileAndSize.empty();
}

std::vector<std::wstring> Bbx::FileChain::filesAroundRange(
    const std::wstring& firstFileName, const std::wstring& lastFileName, size_t minFileSize ) const
{
    auto pred = [](const NameAndSize& one, const NameAndSize& two ){
        return Bbx::Location::PathComparator()( one.name, two.name );
    };
    NameAndSize firstNas(firstFileName,0);
    NameAndSize lastNas(lastFileName,0);
    auto lowerIt = std::lower_bound(m_fileAndSize.begin(), m_fileAndSize.end(), firstNas, pred);
    if (m_fileAndSize.begin() != lowerIt)
        std::advance(lowerIt, -1);

    // Расширяем временной запрос из-за множественных имён файлов с совпадающими штампами
    auto upperIt = std::upper_bound(lowerIt, m_fileAndSize.end(), lastNas, pred);
    if (m_fileAndSize.end() != upperIt)
        std::advance(upperIt, 1);

    std::vector<std::wstring> result;
    for( auto it = lowerIt; upperIt != it; ++it )
    {
        if ( it->size >= minFileSize )
            result.push_back( addDirectory( it->name ) );
    }
    return result;
}

std::wstring Bbx::FileChain::selectNextFile(const std::wstring& currFile, size_t minFileSize, bool forwardDirection) const
{
    bfs::path currPath( currFile );
    std::wstring currNameOnly = currPath.filename().wstring();

    auto pred = [](const NameAndSize& one, const NameAndSize& two ){
        return Location::PathComparator()(one.name, two.name);
    };

    NameAndSize currNas( currNameOnly, 0 );
    auto fIt = std::lower_bound( m_fileAndSize.begin(), m_fileAndSize.end(), currNas, pred );
    if (fIt != m_fileAndSize.end() && fIt->name == currNameOnly )
    {
        if ( forwardDirection )
        {
            /* Выбрать следующий файл из списка, если такой есть */
            auto it = std::find_if( fIt+1, m_fileAndSize.end(), [minFileSize](const NameAndSize& elem){
                return elem.size >= minFileSize;
            } );
            if ( m_fileAndSize.end() != it )
                return addDirectory( it->name );
        }
        else
        {
            /* Выбрать предыдущий файл из списка, если такой есть */
            while( m_fileAndSize.begin() != fIt )
            {
                --fIt;
                if ( fIt->size >= minFileSize )
                    return addDirectory( fIt->name );
            }
        }
    }
    return std::wstring();
}

std::wstring Bbx::FileChain::addDirectory( boost::wstring_ref name) const
{
    return (m_directory/name.to_string()).wstring();
}
