#include "stdafx.h"
#include <cctype>
#include <iomanip>
#include <mutex>
#ifdef LINUX
#include <sys/inotify.h>
#include <poll.h>
#endif
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/utility/string_ref.hpp>
#include "../helpful/FilesByMask.h"
#include "../helpful/Utf8.h"
#include "bbx_Location.h"
#include "bbx_Stamp.h"
#include "bbx_FileChain.h"

namespace bt = boost::posix_time;
namespace bfs = boost::filesystem;

// автообновляемый доступ к цепочке файлов каталога

namespace { // анонимное пространство - только внутри этого исходного файла
    typedef std::shared_ptr<Bbx::FileChain>        PFileChain;
    typedef std::shared_ptr<const Bbx::FileChain>  PCFileChain;

    // наблюдатель за изменениями в каталоге
    class Looker
    {
    public:
        Looker();
        ~Looker();
        static void create(); // создание общего дескриптора
        static void remove(); // удаление общего дескриптора
        void set( const std::wstring& _folder );
        bool occur();
        const std::wstring& folder() const;

    private:
        boost::posix_time::ptime m_next_scan; // момент следующего сканирования
        std::wstring m_folder;
#ifndef LINUX
        HANDLE       m_handle;
#else
        static int m_fd; // общий дескриптор inotify
        int m_wd;        // сторожок данного наблюдателя
#endif
    };


    class FreshedFolder
    {
    public:
        FreshedFolder();
        ~FreshedFolder();
        void add( const std::wstring& directory, const std::wstring& fileMask );
        PCFileChain getFileChain( const std::wstring& directory, const std::wstring& fileMask );
        void clear();

    private:
        // вспомогательные типы
        struct SubData
        {
            std::wstring m_mask;
            PFileChain   m_curr; // текущие собранные данные (для внешних потребителей)
            PFileChain   m_work; // рабочие данные
        };
        struct NodeData
        {
            Looker               m_looker;
            std::vector<SubData> m_subdata;
        };
    private:
        std::mutex                         m_mtx;
        std::map< std::wstring, NodeData > m_data; // папка|маска и ее данные
    };
};

#ifndef LINUX
Looker::Looker()
    : m_folder(), m_handle(INVALID_HANDLE_VALUE), m_next_scan ( bt::microsec_clock::universal_time() )

{
}
void Looker::create() // создание общего дескриптора
{
}
void Looker::remove() // удаление общего дескриптора
{
}
void Looker::set( const std::wstring& _folder )
{
    m_folder = _folder;
    m_handle = FindFirstChangeNotification(m_folder.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
}

Looker::~Looker()
{
	if (INVALID_HANDLE_VALUE != m_handle)
		FindCloseChangeNotification(m_handle);
}

bool Looker::occur()
{
    bool res = false;
    if ( INVALID_HANDLE_VALUE != m_handle && WAIT_OBJECT_0 == WaitForSingleObject( m_handle, 0 ) ) {
        res = true;
        if ( !FindNextChangeNotification( m_handle ) ) {
            FindCloseChangeNotification( m_handle );
            m_handle = FindFirstChangeNotification( m_folder.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
            ASSERT( m_handle != INVALID_HANDLE_VALUE );
        }
    }
    if ( m_next_scan < bt::microsec_clock::universal_time() )
    {
        m_next_scan += bt::seconds(1);
        res = true;
    }
    return res;
}

#else

int Looker::m_fd = -1;

Looker::Looker()
    : m_folder(), m_wd(-1)
{
}
void Looker::create() // создание общего дескриптора
{
    m_fd = inotify_init1(IN_NONBLOCK);
    ASSERT(m_fd != -1);
}
void Looker::remove() // удаление общего дескриптора
{
    close(m_fd);
    m_fd = -1;
}
void Looker::set(const std::wstring& _folder)
{
    m_folder = _folder;
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO;
    m_wd = inotify_add_watch(m_fd, ToUtf8(_folder).c_str(), mask );
    ASSERT(m_wd >= 0);
}

Looker::~Looker()
{
    if ( m_wd >=0 )
        inotify_rm_watch(m_fd, m_wd);
    m_wd = -1;
}

bool Looker::occur()
{
    bool res = false;

    struct pollfd fds;
    fds.fd = m_fd;
    fds.events = POLLIN;
    fds.revents = 0;
    const int timeout = 0;
    int poll_num = poll(&fds, 1, timeout);
    if( poll_num > 0 && ( fds.revents & POLLIN ) ) {
        while (true) {
            union
            {
                inotify_event i_event; // только для целей выравнивания
                char   raw[ 10*MAX_PATH + 1 ];
            };
            ssize_t len = read( m_fd, &raw, sizeof( raw ) );
            if( len == -1 )
                break;
            
            char*          ptr;
            const inotify_event* event;
            for( ptr = raw; ptr < raw + len; ptr += sizeof( inotify_event ) + event->len ) {
                event = reinterpret_cast<const inotify_event*>( ptr );
                if( event->mask & ( IN_CREATE | IN_DELETE ) )
                    res = true;
            }
        }
    }
    return res;
}
#endif //!LINUX

const std::wstring& Looker::folder() const
{
    return m_folder;
}

// общее хранилище известных каталогов
static FreshedFolder fresh;

Bbx::Location::Location(const std::wstring& folder, const std::wstring& pref, const std::wstring& suff)
    : directory(folder), prefix(pref), suffix(suff)
{
    mask_only = prefix + L"*" + suffix;
    fresh.add( folder, mask_only );
}

std::wstring Bbx::Location::defaultFolder()
{
    // Путь к черному ящику по умолчанию
    boost::system::error_code ec;
    return bfs::current_path( ec ).wstring();
}

std::wstring Bbx::Location::fileName(const Stamp& stamp, unsigned attempt ) const
{
    using namespace std;
    bt::ptime time_to = bt::from_time_t( stamp.getTime() );

    std::wostringstream result;
    result
        << setfill(L'0') << setw(2) << (time_to.date().year() % 100)
        << setfill(L'0') << setw(2) << (short) time_to.date().month()
        << setfill(L'0') << setw(2) << time_to.date().day()
        << L'-'
        << setfill(L'0') << setw(2) << time_to.time_of_day().hours()
        << setfill(L'0') << setw(2) << time_to.time_of_day().minutes()
        << markTZ();
    if( attempt )
        result << '_' << setfill(L'0') << setw(2) << to_wstring(attempt);

    return prefix + result.str() + suffix;
}

std::wstring Bbx::Location::filePath(const Stamp& stamp, unsigned attempt ) const
{
    return (bfs::path(getFolder()) / fileName(stamp, attempt)).wstring();
}

std::wstring Bbx::Location::verificationFilePath() const
{
    return (bfs::path(getFolder()) /(L"~" + prefix + suffix)).wstring();
}

bool Bbx::Location::empty() const
{
    bool found = false;
    auto detector = [&found](const FilesByMask_Data& /*fileFindData*/)
    {
        found = true;
        return false;
    };
    bfs::path temp = bfs::path( directory ) / mask_only;
    FilesByMask( temp.wstring(), detector);
    return !found;
}

std::shared_ptr<const Bbx::FileChain> Bbx::Location::getCPtrChain() const
{
    auto res = fresh.getFileChain( directory, mask_only );
    return res;
}

bool Bbx::Location::PathComparator::operator() ( boost::wstring_ref one, boost::wstring_ref two ) const
{
    // при совпадении длины - простое сравнение строк
    if ( one.size() == two.size() ) {
        return one < two;
    } else {
        // убрать совпадающее начало и конец
        while( !one.empty() && !two.empty() ) {
            if ( one.front() == two.front() ) {
                one.remove_prefix( 1 );
                two.remove_prefix( 1 );
            } else if ( one.back() == two.back() ) {
                one.remove_suffix( 1 );
                two.remove_suffix( 1 );
            } else if ( one.front()<127 && two.front()<127 && std::toupper( one.front() ) == std::toupper( two.front() ) ) {
                one.remove_prefix(1);
                two.remove_prefix(1);
            } else if ( one.back()<127 && two.back()<127 && std::toupper( one.back() ) == std::toupper( two.back() ) ) {
                one.remove_suffix( 1 );
                two.remove_suffix( 1 );
            } else {
                break;
            }
        }
        // проверить на различие в TZ
        if ( !one.empty() && !two.empty() ) {
            // совместимость со старыми ящиками - там не было символа markTZ() т.е. 'Z'
            bool z1 = std::any_of( one.begin(), one.end(), [](wchar_t ch){ return markTZ(ch); } );
            bool z2 = std::any_of( two.begin(), two.end(), [](wchar_t ch){ return markTZ(ch); } );
            if ( z1 != z2)
                return !z1; // строка без 'Z' всегда меньше строки с 'Z'
        }
        // при различиях перед буквой Z вставляем нули
        while( !one.empty() && !two.empty() ) {
            wchar_t s1 = one.front();
            wchar_t s2 = two.front();
            if ( s1 == s2 || ( s1<127 && s2<127 && std::toupper(s1) == std::toupper(s2) ) ) {
                one.remove_prefix(1);
                two.remove_prefix(1);
            } else {
                if ( markTZ(s1) )
                    s1 = '0';
                else
                    one.remove_prefix(1);
                if ( markTZ(s2) )
                    s2 = '0';
                else
                    two.remove_prefix(1);
                if ( s1 != s2 )
                    return s1<s2;
            }
        }
        // одна из строк закончилась
        if ( one.empty() )
            return !two.empty(); // one закончилась, отвечаем по two
        else
            return false; // т.к. one длиннее two
    }
}

bool Bbx::Location::operator==(const Location& other) const
{
    return directory == other.directory
        && prefix == other.prefix
        && suffix == other.suffix;
}

void Bbx::Location::clearFolderCache()
{
    fresh.clear();
}


FreshedFolder::FreshedFolder()
    : m_data()
{
    Looker::create();
}

FreshedFolder::~FreshedFolder()
{
    Looker::remove();
}

// добавить папку+маску для наблюдения
void FreshedFolder::add(const std::wstring& directory, const std::wstring& fileMask)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    // создание узла при необходимости
    auto it = m_data.find( directory );
    if ( m_data.end() == it )
    {
        m_data.emplace( std::make_pair( directory, NodeData() ) );
        it = m_data.find( directory );
        it->second.m_looker.set( directory );
    }
    // дополнение узла
    NodeData& node = it->second;
    bool already = false;
    for( auto& sd : node.m_subdata )
    {
        if ( sd.m_mask == fileMask )
        {
            already = true;
            break;
        }
    }
    if ( !already )
    {
        node.m_subdata.emplace_back( SubData() );
        SubData& sd = node.m_subdata.back();
        sd.m_mask = fileMask;
        sd.m_curr = std::make_shared<Bbx::FileChain>( directory, fileMask, 100 );
    }
}

std::shared_ptr<const Bbx::FileChain> FreshedFolder::getFileChain( const std::wstring& directory, const std::wstring& fileMask )
{
    // проверить наличие такой маски с блокировкой на чтение
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_data.find( directory );
    ASSERT( m_data.end() != it );
    if ( m_data.end() != it ) {
        NodeData& node = it->second;
        if ( node.m_looker.occur() )
        {
            // пересобрать данные этой папки/маски
            for( auto& sub : node.m_subdata )
            {
                size_t sz = 0;
                if ( sub.m_curr )
                    sz = sub.m_curr->getNumberOfFiles()+2;
                else
                    sz = 100;
                sub.m_work = std::make_shared<Bbx::FileChain>( node.m_looker.folder(), sub.m_mask, sz );
                std::swap( sub.m_curr, sub.m_work );
            }
        }
        for( auto& sd : it->second.m_subdata ) {
            if ( sd.m_mask == fileMask ) {
                return sd.m_curr;
            }
        }
    }
    return nullptr;
}

void FreshedFolder::clear()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_data.clear();
}
