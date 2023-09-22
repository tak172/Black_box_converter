#pragma once

#include <atomic>
#include <boost/utility/string_ref_fwd.hpp>
#include <boost/format/format_fwd.hpp>
#include "RT_Singleton.h"

/* Запись в лог при любом режиме сборки */
#define WriteLog        (*InterlockedLog::instance())
#define NullLog         NullLog_Imp()

/* Запись в лог при конкретном режиме сборки */
#ifdef _DEBUG
    #define DebugLog        (*InterlockedLog::instance())
    #define ReleaseLog      if ( true ) /*none*/; else NullLog
#else
    #define DebugLog        if ( true ) /*none*/; else NullLog
    #define ReleaseLog      (*InterlockedLog::instance())
#endif

class InterlockedLog
{
public:
    // прокладка определения указуемого типа
    template<typename T>
    struct base_type_traits
    {
        typedef void base_type;
    };
    template<typename T>
    struct base_type_traits<T *>
    {
        typedef T base_type;
    };
    template<typename T>
    struct base_type_traits<T const *>
    {
        typedef T base_type;
    };
    // прокладка получения строки

public:
    // 0 - простые случаи
    void operator()( const char    * s  )        { write_u8( s ); }
    void operator()( const wchar_t * w )         { write( w ); }
    void operator()( const std::string&  ss )    { write_u8( ss.c_str() ); }
    void operator()( const std::wstring& ws )    { write( ws.c_str() ); }
	void operator()( const std::vector<std::wstring>& vStr )    
	{ 
		for( const auto& msg : vStr )
			write( msg.c_str() ); 
	}
    void operator()( const boost::format& fmt )  { write_u8( fmt.str().c_str() ); }
    void operator()( const boost::wformat& fmt ) { write( fmt.str().c_str() ); }
    // 1
    template < typename T0, typename T1 >
    void operator()( T0 a0, T1 a1 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 );
    }
    // 2
    template < typename T0, typename T1, typename T2 >
    void operator()( T0 a0, T1 a1, T2 a2 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 % a2 );
    }
    // 3
    template < typename T0, typename T1, typename T2, typename T3 >
    void operator()( T0 a0, T1 a1, T2 a2, T3 a3 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 % a2 % a3 );
    }
    // 4
    template < typename T0, typename T1, typename T2, typename T3, typename T4 >
    void operator()( T0 a0, T1 a1, T2 a2, T3 a3, T4 a4 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 % a2 % a3 % a4 );
    }
    // 5
    template < typename T0, typename T1, typename T2, typename T3, typename T4, typename T5 >
    void operator()( T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 % a2 % a3 % a4 % a5 );
    }
    // 6
    template < typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 >
    void operator()( T0 a0, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6 )
    {
        typedef typename base_type_traits<T0>::base_type item_type;
        write( boost::basic_format<item_type>( a0 ) % a1 % a2 % a3 % a4 % a5 % a6 );
    }

    void write_u8( const char*     str );
    void write( const wchar_t* wstr );
    void write( const boost::format& fmt )  { write_u8( fmt.str().c_str() ); }
    void write( const boost::wformat& fmt ) { write( fmt.str().c_str() ); }

    bool initialize_path( const std::wstring& path, const std::string& app, const std::string& vers );
    std::wstring get_path() const; // используемая папка для файлов
    void flush(); // записать накопленные данные в файл
    void set_big_delay( boost::posix_time::time_duration dur ); // задать размер "big delay" для логирования
    void limit_logs( size_t cnt ) const; // ограничение количества логов
	std::vector<std::wstring> file_names() const; // существующие лог-файлы
    void set_dayly_split(); // включить посуточное разбиение на файлы
    void set_clock( std::function<boost::posix_time::ptime(void)> _clk ); // читатель часов (для тестов)
	void set_clock(); // читатель фактических часов компьютера (для тестов)
	void detect_reverse_moment();
    void duplicate_to_console(); // дублировать весь вывод на std::cout
    void start_thread( const boost::thread& thr );
    void end_thread();
    std::string Read() const;

    DECLARE_SINGLETON(InterlockedLog)

private:
    typedef boost::posix_time::ptime ptime;

    std::wstring m_fileFolder; ///< папка логов закрытая back-slash
    std::wstring m_filePrefix; ///< префикс имени файла (обычно имя программы)
    const std::wstring mc_fileSuffix; ///< расширение с ведущей точкой
    unsigned     m_fileOrder;  ///< номер файла в серии 
    long         m_fileSize;   ///< текущая длина файла
	bool         m_daylySplit; ///< флаг разбиения посуточно
    bool         m_console;    ///< копия на консоль
    boost::mutex m_mutex;
    typedef boost::tuple<ptime,boost::thread::id,std::string> ELEM_BUFF;
    typedef std::vector<ELEM_BUFF> TYPE_BUFF;
    typedef std::vector<boost::thread::id> TYPE_WRID;
    ELEM_BUFF m_header; // заголовок файла
    TYPE_BUFF m_buffer; // запись
    TYPE_WRID m_writer_id;
    boost::thread m_subthread;
    std::atomic<unsigned> m_flushRequest; // запрос немедленной записи в файл
	static std::function< ptime(void) > m_clock; // используемые часы
    ptime m_back_stamp;  // штамп последней строки в файле
    boost::posix_time::time_duration m_big_delay; // размер большой задержки логирования

    const long mc_splitSize; // минимальный размер файла для создания следующего файла
    ptime m_start; // начальный момент файла (исп. в имени файла)
    const boost::posix_time::time_duration mc_delayFileWrite; // допустимое ожидание перед записью в файл
    const boost::posix_time::time_duration mc_delayLoopTurn;  // период обращения цикла

    std::wstring build( ptime _stamp, unsigned _order ) const; ///< полное имя файла по штампу и номеру
	void init_file( ptime _stamp, unsigned _order ); ///< настроить новый файл
    void total_no_more( size_t count ); ///< ограничение общего количества файлов
    void run();
    void doWrite(TYPE_BUFF& outbuffer);
	void putBigDelay( const ELEM_BUFF& elem );
	void makeSplit( const ELEM_BUFF& elem );
	bool makeSplitDaily( const ELEM_BUFF& elem );
	void makeSplitSize(  const ELEM_BUFF& elem );
	void build_bottom_top( ptime next_stamp, unsigned next_order, std::string &bottom, std::string &top);
	std::string serializeOne( const ELEM_BUFF& elem );
	std::pair<long, ptime> directWrite( ptime _stamp, boost::string_ref contents ) const;
    size_t threadIndex( boost::thread::id writer );
    std::string threadSymbol( boost::thread::id writer );
    static std::string strFromTime( ptime moment );
    bool setPath( const std::wstring& path );
};

class NullLog_Imp
{
public:
    template < typename T0 > 
    void operator()( T0 /*a0*/ ){};
    template < typename T0, typename T1 > 
    void operator()( T0 /*a0*/, T1 /*a1*/ ){};
    template < typename T0, typename T1, typename T2 > 
    void operator()( T0 /*a0*/, T1 /*a1*/, T2 /*a2*/ ){};
    template < typename T0, typename T1, typename T2, typename T3 > 
    void operator()( T0 /*a0*/, T1 /*a1*/, T2 /*a2*/, T3 /*a3*/ ){};
    template < typename T0, typename T1, typename T2, typename T3, typename T4 > 
    void operator()( T0 /*a0*/, T1 /*a1*/, T2 /*a2*/, T3 /*a3*/, T4 /*a4*/ ){};
    template < typename T0, typename T1, typename T2, typename T3, typename T4, typename T5 > 
    void operator()( T0 /*a0*/, T1 /*a1*/, T2 /*a2*/, T3 /*a3*/, T4 /*a4*/, T5 /*a5*/ ){};
    template < typename T0, typename T1, typename T2, typename T3, typename T4, typename T5, typename T6 > 
    void operator()( T0 /*a0*/, T1 /*a1*/, T2 /*a2*/, T3 /*a3*/, T4 /*a4*/, T5 /*a5*/, T6 /*a6*/ ){};

    bool initialize_path(const std::wstring& /*path*/, const std::string& /*app*/, const std::string& /*vers*/) { return true; }
    void flush() {}; // записать накопленные данные в файл
    void set_big_delay( boost::posix_time::time_duration /*dur*/ ) {}; // задать размер "big delay" для логирования
    void limit_logs( size_t /*cnt*/ ) const {}; // ограничение количества логов
    //std::vector<std::wstring> file_names() const { return std::vector<std::wstring>{}; }; // существующие лог-файлы
    void set_dayly_split() {}; // включить посуточное разбиение на файлы
    void set_clock( std::function<boost::posix_time::ptime(void)> /*_clk*/ ) {}; // читатель часов (для тестов)
    void set_clock() {}; // читатель фактических часов компьютера (для тестов)
    void detect_reverse_moment() {};
};