#include "stdafx.h"

#ifndef LINUX
#else
#include <sys/stat.h>
#endif
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/filesystem.hpp>

#include "../helpful/Log.h"
#include "../helpful/FilesByMask.h"
#include "../helpful/Utf8.h"
#include "../helpful/RT_ThreadName.h"
#include "../helpful/StrToTime.h"

namespace bt = boost::posix_time;
namespace bfs = boost::filesystem;

ALLOCATE_SINGLETON(InterlockedLog);

const auto LATER = bt::hours(480); // несуразно большой интервал
const size_t SPLIT = 25 * 1024 * 1024; // мин.размер файла для перехода к следующему
const size_t TOTAL_NUMBER = 40; // ограничение общего числа файлов
const std::wstring TIMESTAMP_FMT  = L"%02d%02d%02d-%02d%02d%02d";
const size_t       TIMESTAMP_SIZE = 6 + 1 + 6;
static bt::ptime liveClock() {
	return bt::microsec_clock::local_time();
}

std::function< bt::ptime(void) > InterlockedLog::m_clock = liveClock;
static boost::filesystem::path LastUsedFolder; ///< последняя использованная папка логов
static std::wstring            LastUsedPrefix; ///< последний использованный префикс

InterlockedLog::InterlockedLog()
    : m_fileFolder( LastUsedFolder.wstring() ), m_filePrefix( LastUsedPrefix ), mc_fileSuffix(L".log"),
    m_fileOrder(1),m_fileSize(0), m_daylySplit(false), m_console(false),
  m_header(),
  m_buffer(),
  m_writer_id(),
  m_subthread(),
  m_flushRequest( 0 ),
  m_back_stamp( m_clock() ),
  m_big_delay( LATER ),
  mc_splitSize( SPLIT ), // минимальный размер файла для создания следующего файла
  m_start( m_clock() ),
  mc_delayFileWrite( bt::millisec( 500 ) ),
  mc_delayLoopTurn(  bt::millisec( 100 ) )
{    
    // создатель синглтона будет первым в списке нитей
    threadSymbol( boost::this_thread::get_id() );
    //
    boost::mutex::scoped_lock lock(m_mutex);
	m_subthread = boost::thread( boost::bind(&InterlockedLog::run, this) );
}

bool InterlockedLog::setPath( const std::wstring& path )
{
    boost::mutex::scoped_lock lock(m_mutex);
    boost::filesystem::path fold( path.empty()? L"." : path );
    boost::system::error_code ec;
    if ( (exists(fold,ec) && is_directory(fold,ec)) || create_directories(fold, ec) ) {
        m_fileFolder = (fold/L"x").wstring();
        m_fileFolder.pop_back();

        if ( !path.empty() )
            LastUsedFolder = fold;
        else
            LastUsedFolder = "./";
		return true;
    } else {
        return false;
    }
}

InterlockedLog::~InterlockedLog(void)
{           
    if ( m_subthread.joinable() )
    {
        m_subthread.interrupt();
        m_subthread.join();
    }
}

void InterlockedLog::write( const wchar_t* out )
{
    write_u8( ToUtf8(out).c_str() );
}

void InterlockedLog::write_u8( const char* out )
{
    bt::ptime mom = m_clock();
    boost::thread::id thr = boost::this_thread::get_id();

    boost::mutex::scoped_lock lock(m_mutex);
    m_buffer.emplace_back( boost::make_tuple(mom,thr,out) );
}

void InterlockedLog::run()
{
    RT_SetThreadName( "InterlockedLog" );
    try
    {
        bt::ptime m_next_write; // момент очередной записи в файл
        m_next_write = bt::microsec_clock::universal_time() + mc_delayFileWrite;
        while( !boost::this_thread::interruption_requested() )
        {
            if ( 0 == m_flushRequest && bt::microsec_clock::universal_time() < m_next_write )
            {
                boost::this_thread::sleep( mc_delayLoopTurn );
            }
            else
            {
                TYPE_BUFF outbuffer;
                {
                    boost::mutex::scoped_lock lock(m_mutex);
                    m_buffer.swap(outbuffer);
                }
                doWrite(outbuffer);
                if ( !outbuffer.empty() )
                {
                    boost::mutex::scoped_lock lock(m_mutex);
                    m_buffer.insert(m_buffer.begin(), outbuffer.begin(), outbuffer.end());
                }
                m_next_write = bt::ptime(m_next_write) + mc_delayFileWrite;
                if ( 0 < m_flushRequest )
                    --m_flushRequest;
            }
        }
    }
    catch( boost::thread_interrupted& /*e*/ )
    {
        RT_SetThreadName( "InterlockedLog[interrupt]" );
    }
    catch( ... )
    {
        RT_SetThreadName( "InterlockedLog[exception]" );
    }
    // finish
    {
        boost::mutex::scoped_lock lock(m_mutex);
        doWrite(m_buffer);
        doWrite(m_buffer); // повтор из-за возможности расщепления
    }
}

void InterlockedLog::doWrite(TYPE_BUFF& outbuffer)
{
    if ( !outbuffer.empty() )
    {
		// вывод строки большой задержки
		putBigDelay( outbuffer.front() );
		// запись по одной строке
		for( auto& elem : outbuffer )
		{
			// выполнить переключение файлов ежедневное или по размеру
			makeSplit( elem );
			// вывести одну строку
            std::tie( m_fileSize, m_back_stamp ) = directWrite( elem.get<0>(), serializeOne( elem ) );
		}
		outbuffer.clear();
	}
}

void InterlockedLog::putBigDelay( const ELEM_BUFF& _elem )
{
	// индикация больших задержек на временной шкале лога
	if ( m_back_stamp + m_big_delay < _elem.get<0>() )
        std::tie( m_fileSize, m_back_stamp ) = directWrite( m_back_stamp, "             big delay\n" );
}

void InterlockedLog::makeSplit(const ELEM_BUFF& _elem)
{
	if ( !makeSplitDaily( _elem ) )
		makeSplitSize( _elem );
}

bool InterlockedLog::makeSplitDaily(const ELEM_BUFF& elem)
{
	if ( m_daylySplit && m_back_stamp.date() < elem.get<0>().date() )
	{
		// имена файлов
		bt::ptime next_stamp = bt::ptime( elem.get<0>().date(), bt::time_duration(0,0,0) ) ;
		unsigned next_order = 1;
		// построить подвал и заголовок
		std::string bottom;
		std::string top;
		build_bottom_top(next_stamp, next_order, bottom, top);
		directWrite( m_back_stamp, bottom );
		init_file( next_stamp, next_order );
		std::tie( m_fileSize, m_back_stamp ) = directWrite( elem.get<0>(), top );
		return true;
	}
	else
	{
		return false;
	}
}

void InterlockedLog::makeSplitSize( const ELEM_BUFF& elem )
{
	if ( m_fileSize >= mc_splitSize )
	{
		std::string bottom;
		std::string top;
		build_bottom_top( m_start, m_fileOrder+1, bottom, top);
		directWrite( m_back_stamp, bottom );
		init_file( m_start, m_fileOrder+1 );
		std::tie( m_fileSize, m_back_stamp ) = directWrite( elem.get<0>(), top );
        total_no_more(TOTAL_NUMBER);
	}
}

void InterlockedLog::build_bottom_top(bt::ptime next_stamp, unsigned next_order, std::string &bottom, std::string &top )
{
	bottom.clear();
	top.clear();
	std::wstring next_fn = build( next_stamp, next_order );
	std::string st = strFromTime( m_back_stamp );
	bottom = st + ToUtf8( L"     ... далее см. " + next_fn + L"\n" );
	std::wstring curr_fn = build( m_start, m_fileOrder );
	top += serializeOne( m_header );
	top += serializeOne( boost::make_tuple(
		m_back_stamp,                         // момент по последней строке
		m_header.get<1>(),                    // идентиф. исходного (главного) потока
		ToUtf8( L"... ранее см. " + curr_fn ) // имя предыдущего файла
		) );
}

std::string InterlockedLog::serializeOne( const ELEM_BUFF& _elem )
{
	std::string for_write;

	const std::string st = strFromTime( _elem.get<0>() );
	const std::string flow = threadSymbol( _elem.get<1>() );
	std::string fulltext = _elem.get<2>();
	if ( !fulltext.empty() && '\n' == fulltext.back() )
		fulltext.pop_back();
	typedef boost::tokenizer<boost::char_separator<char>,std::string::const_iterator,std::string > TOKENIZER;
	boost::char_separator<char> sep("\n", nullptr, boost::keep_empty_tokens);
	TOKENIZER tok(fulltext,sep);
	char symlast = ' ';
	for(TOKENIZER::iterator it2=tok.begin(); it2!=tok.end(); ++it2 )
	{
		for_write += st + symlast + flow + ' ' + *it2 + '\n';
		// для следующих строк 
		symlast = '+'; 
	}
	return for_write;
}

std::pair<long,InterlockedLog::ptime> InterlockedLog::directWrite( ptime _stamp, boost::string_ref contents ) const
{
    boost::filesystem::path currFN( build( m_start, m_fileOrder ) );
    std::ofstream logFile(currFN.c_str(), std::ios::app | std::ios::in | std::ios::out );
	if ( m_fileSize < 3 )
	{
		logFile << "\xEF\xBB\xBF"; // это BOM для utf-8
	}

//     // Фильтр логируемых строк (по префиксу)
//     if( contents.starts_with( "!!!" ) )
    {
        logFile << contents;
        if( m_console )
            std::cout << contents << std::flush;
#ifdef _DEBUG
#ifndef LINUX
        const unsigned CUTSZ = 1000;
        std::wstring ws = FromUtf8( contents );
        for( unsigned i = 0; i < ws.size(); i += CUTSZ )
        {
            std::wstring wcut = ws.substr( i, CUTSZ );
            OutputDebugString( wcut.c_str() );
        }
#else
        std::cout << contents << std::flush;
#endif // !LINUX
#endif //_DEBUG
    }
	if ( logFile.fail() )
	{
#ifndef LINUX
		OutputDebugString(L"logFile.fail()\n");
#else
		std::cerr << "logFile.fail()\n";
#endif // !LINUX
        ASSERT( false );
        logFile.close();
        return std::make_pair( m_fileSize, m_back_stamp ); // т.к. новых данных нет
	}
	else
	{
        long fsz = (long)logFile.tellp();
        ptime bmom = _stamp;
        logFile.close();
#ifndef LINUX
#else
        chmod( currFN.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH );
#endif
        return std::make_pair( fsz, bmom );
	}
}

std::string InterlockedLog::strFromTime( bt::ptime moment )
{
    std::string st = to_iso_extended_string( moment );
    bool just = ( 0 != moment.time_of_day().fractional_seconds() );
    if ( just )
        st.resize(st.size() - bt::time_duration::num_fractional_digits() + 3 ); // оставляем миллисекунды из микросекунд
    else
        st += ".000";
    return st;
}

size_t InterlockedLog::threadIndex( boost::thread::id writer )
{
    TYPE_WRID::iterator wi = std::find(m_writer_id.begin(), m_writer_id.end(), writer );
    size_t res = wi - m_writer_id.begin();
    if ( m_writer_id.end()==wi )
        m_writer_id.push_back( writer );
    return res;
}

std::string InterlockedLog::threadSymbol( boost::thread::id writer )
{
    size_t res = threadIndex( writer );
    std::string s;
    if ( res==0 )
        s = "   ";
    else 
    {
        const std::string LIT( "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" );
        const size_t SZ = LIT.size();
        s += '(';
        for( ; res>0; res /= SZ )
            s += LIT[ res % SZ ];
        s += ')';
    }
    return s;
}

bool InterlockedLog::initialize_path( const std::wstring& path, const std::string& app, const std::string& vers )
{
    bool result = false;
    if ( !path.empty() && !setPath( path ) ) {
        WriteLog( L"Папка %s не существует и не может быть создана", path );
    }
    m_filePrefix = FromUtf8( app + ' ' );
    LastUsedPrefix = m_filePrefix;

    std::wstring wtxt = L"Запуск " + FromUtf8( app + ' ' + vers);
#ifndef LINUX
	if (IsDebuggerPresent())
		wtxt += L" - под отладчиком";
#endif // !LINUX
    // запомнить версию для повтора при разделении файлов    
    bt::ptime mom = m_clock();
    boost::thread::id thr = boost::this_thread::get_id();
    std::string out = ToUtf8(wtxt);
    m_header = boost::make_tuple( mom, thr, out );

    WriteLog( wtxt );
    if ( path.empty() )
    {
        WriteLog(L"В конфигурационном файле не найдены настройки лога, использованы установки по умолчанию.");
        WriteLog(L"Вставьте строку вида <Log Path=\".\\log\" /> для настройки.");
    }
    else
    {
        result = true;
    }
    if ( m_console )
        WriteLog( L"Duplicate to console - ON" );
    return result;
}

std::wstring InterlockedLog::get_path() const // используемая папка для файлов
{
    return m_fileFolder;
}

void InterlockedLog::flush()
{
    write( L"Log: flush output buffers" );
    ++m_flushRequest;
    // ожидание на случай медленной записи в файл
    const bt::ptime fence = bt::microsec_clock::local_time() +
                            ( mc_delayFileWrite + mc_delayLoopTurn )*2;

    while( 0 != m_flushRequest && bt::microsec_clock::local_time() < fence )
        boost::this_thread::sleep( mc_delayLoopTurn/10 );
}

void InterlockedLog::set_big_delay(bt::time_duration dur)
{
    m_big_delay = dur;
}

void InterlockedLog::limit_logs( size_t cnt ) const
{
    // защита от плохого значения
    if ( cnt < 3 )
        cnt = 3;
    // Собрать все файлы логов
    std::vector< std::wstring > vfn = file_names();

    // выбрать только начальные файлы каждой серии
    std::vector< std::wstring > hdr;
    std::wstring last = L")" + mc_fileSuffix;
	std::remove_copy_if( vfn.begin(), vfn.end(), std::back_inserter(hdr),
		[last](const std::wstring& fn){ return boost::wstring_ref(fn).ends_with( last ); }
	);
    // удалить лишние файлы
    if ( hdr.size() > cnt ) {
        hdr.resize( hdr.size() - cnt );
        // без суффикса проще сравнивать
        for( auto& h : hdr )
            h.resize( h.size() - mc_fileSuffix.size() );
        // и выполнить удаление
        auto it_h = hdr.begin();
        auto it_f = vfn.begin();
        while( hdr.end() != it_h && vfn.end() != it_f ) {
            const std::wstring& f = *it_f;
            const std::wstring& h = *it_h;
            if ( boost::wstring_ref( f ).starts_with( h ) )
            {
                bfs::path ff(m_fileFolder);
                ff /= f;
                boost::system::error_code ec;
                bfs::remove( ff, ec );
                ++it_f;
            } else if ( f < h ) {
                ASSERT(!"Недопустимая ситуация - ошибка алгоритма");
                break;
            } else if ( f > h ) {
                ++it_h;
            } else {
                ASSERT(!"Недопустимая ситуация - ошибка алгоритма");
                break;
            }
        }
    }
}

std::vector<std::wstring> InterlockedLog::file_names() const
{
	std::vector<std::wstring> vfn;
	const std::wstring pfx_all = m_fileFolder + m_filePrefix;
	auto func = [&vfn]( const FilesByMask_Data& res ) {
		vfn.push_back( res.fname.to_string() );
		return true;
	};
    FilesByMask(pfx_all + L"*" + mc_fileSuffix, func);
    std::sort( vfn.begin(), vfn.end() );
	return vfn;
}

void InterlockedLog::set_dayly_split()
{
    m_daylySplit = true;
}

void InterlockedLog::duplicate_to_console()
{
    m_console = true;
}

void InterlockedLog::start_thread( const boost::thread& thr )
{
    std::string msg = "Log: start thread -> ";
    msg += threadSymbol( thr.get_id() );
    write_u8( msg.c_str() );
}

void InterlockedLog::end_thread()
{
    write_u8( "Log: end thread" );
}

#if defined(UNITTEST)
static bt::ptime save_start;
static unsigned  save_order;

void InterlockedLog::set_clock( std::function<bt::ptime(void)> _clk )
{
	save_start = m_start;
	save_order = m_fileOrder;

	m_clock = _clk;
	detect_reverse_moment();
}

void InterlockedLog::set_clock()
{
	if ( bt::ptime() != save_start )
	{
		m_clock = liveClock;
		detect_reverse_moment();
		// следующая точная секунда
		bt::time_duration rest( 0, 0, 0, m_clock().time_of_day().fractional_seconds() );
		bt::time_duration wait( bt::seconds(1) - rest );
		boost::this_thread::sleep( wait );
		// переход к новому файлу
		std::string bottom;
		std::string top;
		build_bottom_top(save_start, save_order+1, bottom, top);
		directWrite( m_back_stamp, bottom );
		init_file( save_start, save_order+1 );
		std::tie( m_fileSize, m_back_stamp ) = directWrite( m_back_stamp, top );

		save_start = bt::ptime();
	}
}

void InterlockedLog::detect_reverse_moment()
{
	auto m = m_clock();
	if ( m_back_stamp > m )
		std::tie( m_fileSize, m_back_stamp ) = directWrite( m, "   clock moved back\n");
}

#endif

std::wstring InterlockedLog::build( bt::ptime _stamp, unsigned _order ) const
{
	auto da = _stamp.date();
	auto ti = _stamp.time_of_day();
	unsigned y = da.year() % 100;
	unsigned m = da.month();
	unsigned d = da.day();
	boost::wformat wf( TIMESTAMP_FMT );
	wf % y % m % d
		% ti.hours() % ti.minutes() % ti.seconds();

    std::wstring temp = m_fileFolder + m_filePrefix + wf.str();
    if ( _order > 1 ) {
        temp += '(';
        if ( _order < 10 )
            temp += '0';
        temp += std::to_wstring( _order );
        temp += ')';
    }
    temp += mc_fileSuffix;
    return temp;
}

void InterlockedLog::init_file(bt::ptime _stamp, unsigned _order)
{
	m_start = _stamp;
	m_fileOrder = _order;
	m_fileSize = 0;
}

void InterlockedLog::total_no_more( size_t count )
{
    const size_t SZHEADER = m_filePrefix.size() + TIMESTAMP_SIZE;
    const size_t SZFOOTER = mc_fileSuffix.size();
    ASSERT( SZFOOTER > 1 );
    std::vector<std::wstring> vfn;
    auto func = [SZHEADER, SZFOOTER, &vfn]( const FilesByMask_Data& res ) {
        if( res.fname.size() >= SZHEADER + SZFOOTER )
            vfn.push_back( res.fname.to_string() );
        return true;
    };
    FilesByMask( m_fileFolder + m_filePrefix + L"*" + mc_fileSuffix, func );
    if ( vfn.size() > count )
    {
        auto cmp_w_bracket = [SZHEADER](const std::wstring& one, const std::wstring& two ) {
            bool res;
            if( one.size() == two.size() )
                res = ( one < two );
            else {
                // пример:
                // UT 230506-222222.log
                // UT 230506-111111(02).log" 
                // UT 230506-222222(23).log
                // UT 230506-222222(104).log"
                // сравнить основную часть
                boost::wstring_ref vo( one );
                boost::wstring_ref vt( two );
                vo.remove_suffix( vo.size() - SZHEADER );
                vt.remove_suffix( vt.size() - SZHEADER );
                if( vo != vt )
                    return vo < vt;
                // сравнить уже номера
                boost::wstring_ref bo( one );
                boost::wstring_ref bt( two );
                bo.remove_prefix( SZHEADER );
                bt.remove_prefix( SZHEADER );
                auto numb = []( boost::wstring_ref ws ) {
                    size_t n = 0;
                    if( !ws.empty() && ws.front() == '(' )
                    {
                        for( ws.remove_prefix( 1 );
                             !ws.empty() && std::iswdigit( ws.front() );
                             ws.remove_prefix( 1 )
                             )
                            n = n * 10 + ( ws.front() - L'0' );
                    }
                    return n;
                };
                size_t no = numb( bo );
                size_t nt = numb( bt );
                res = ( no < nt );
            }
            return res;
        };
        std::sort( vfn.begin(), vfn.end(), cmp_w_bracket );
        while( vfn.size()>count ) {
            bfs::path ff( m_fileFolder );
            ff /= vfn.front();
            boost::system::error_code ec;
            if( bfs::remove( ff, ec ) )
                vfn.erase( vfn.begin() );
            else
                break;
        }
    }
}

std::string InterlockedLog::Read() const
{
    //m_flushRequest.store(true)
    std::wstring currFN = build(m_start, m_fileOrder);
#ifndef LINUX
    std::ifstream logFile(currFN.c_str(), std::ios::app | std::ios::in);
#else
    std::ifstream logFile(ToUtf8(currFN.c_str()), std::ios::app | std::ios::in);
#endif // !LINUX
    
    
	std::ostringstream sstr;
	sstr << logFile.rdbuf();
    std::string contents = sstr.str();

    if (logFile.fail())
    {
#ifndef LINUX
        OutputDebugString(L"logFile.fail()\n");
#else
        std::cerr << "logFile.fail()\n";
#endif // !LINUX
    }

    logFile.close();
    return contents;
}
