#include "stdafx.h"

#ifndef LINUX
#include <windows.h>
#endif // !LINUX

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/local_time/local_time_types.hpp>
#include <boost/date_time/local_time/local_time_io.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include "StrToTime.h"
#include "Utf8.h"

using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::local_time;
using namespace boost::gregorian;


//
// предварительные объявления
//
static ptime parse_string( const std::string& str, const std::string& facet_fmt );
static std::string get_os_timezone();


// Читать дату и время из строки
// Ошибка - возвращает defvalue
time_t StrDataTime_To_UTC(const std::string& dt_str, time_t defvalue )
{
    // допустимые формы
    // "2011-06-16 08:00:27"
    // "2011-06-16 08:00" - секунды будут нулевые
    ptime dt = parse_string( dt_str, "%Y-%m-%d %H:%M:%S" );
    if ( dt.is_not_a_date_time() )
        return defvalue;
    else
    {
        return UTC_From_Local(dt);
    }
}

time_t StrDataTime_To_UTC(const std::wstring& dt_wstr, time_t defvalue/*=BAD_TIME_T */)
{
    return StrDataTime_To_UTC( ToUtf8(dt_wstr), defvalue );
}

// для разбора L"29.06.2012 08:30:00"
time_t UTC_from_DMYhms(const std::wstring& dt__str, time_t defvalue)
{
    time_t res = UTC_from_DMYhms( ToUtf8(dt__str), defvalue );
    if ( res != BAD_TIME_T )
        return res;
    else
        return defvalue;
}

// кеш локалей для вывода
static std::map< std::string, std::locale >    cacheFmtLoc;

// общий метод форматирования времени
static std::string do_format( time_t moment, const char * fmt )
{
    ptime pt = Local_From_UTC(moment);

    if ( cacheFmtLoc.end() == cacheFmtLoc.find(fmt) )
    {
        time_facet* output_facet = new time_facet(fmt);
        cacheFmtLoc.emplace( std::make_pair( std::string(fmt), std::locale( std::locale::classic(), output_facet ) ) );
    }

    auto it = cacheFmtLoc.find(fmt);
#ifndef LINUX
	ASSERT(cacheFmtLoc.end() != it);
#endif // !LINUX
    std::stringstream ss;
    ss.imbue( it->second );
    ss.str("");
    ss << pt;
    if ( !ss.fail() )
        return ss.str();
    else
        return "???";
}

static std::string build_format( unsigned option )
{
    // дата
    std::string df;
    const std::string year = ( option & DTO_YEAR_4DIG )? "%Y" : "%y";
    const std::string sepa = 
        ( option & DTO_YEAR_DOT    )? "." :
        ( option & DTO_YEAR_SLASH  )? "/" :
        ( option & DTO_YEAR_NONE   )? ""  : "-" ;

    if ( option & DTO_ORDER_YMD )
        df = year + sepa + "%m" + sepa + "%d";
    else if ( option & DTO_ORDER_DMY )
        df = "%d" + sepa + "%m" + sepa + year;
    else
        df.clear(); // порядок даты не задан. Поэтому дата не выводится.
    // время
    std::string tf;
    if ( option & DTO_S )
        tf = "%H:%M:%S";
    else if ( option & DTO_M )
        tf = "%H:%M";
    else if ( option & DTO_H )
        tf = "%H";
    // собираем всё вместе
    if ( !(option & DTO_YEAR_NONE) && !df.empty() && !tf.empty() )
        df += " ";
    return df + tf;
}

// Дату и время вывести в строку
std::string UTC_to_String(time_t moment, unsigned option)
{
    if ( option & DTO_ROUND_M )
        moment += 60/2; // добавить полминуты для округления
    return do_format( moment, build_format(option).c_str() );
}
std::wstring UTC_to_wString(time_t moment,unsigned option )
{
    return FromUtf8( UTC_to_String( moment, option ) );
}

std::string  UTC_to_String(time_t moment, const char * fmt )
{
    return do_format( moment, fmt );
}
std::wstring  UTC_to_String(time_t moment, const wchar_t * fmt )
{
    return FromUtf8( do_format( moment, ToUtf8(fmt).c_str() ) );
}

// Читать time_t из строки
// Ошибка - возвращает BAD_TIME_T
time_t UTC_from_Str( const std::string& dt_str, const char * fmt )
{
    ptime dt = parse_string( dt_str, fmt );
    if ( dt.is_not_a_date_time() )
        return BAD_TIME_T;
    else
        return UTC_From_Local(dt);
}

// временнАя зона, используемая для вычислений
static time_zone_ptr zone( new posix_time_zone( get_os_timezone() ) );
// пример Рига:   "EET+02EEST+01,M3.5.0/03:00,M10.5.0/04:00" - [ посл. вск марта; посл. вск октября]
// пример Москва: "MSK+03"                                   - [ постоянно +3 часа ]


std::string get_TimeZone()
{
    return zone->to_posix_string();
}
void set_TimeZone( const std::string& stz )
{
    std::string t = stz.empty()? get_os_timezone() : stz;
    // лишнее преобразование только для нормализации нестандартной строки
    t = posix_time_zone(t).to_posix_string();
    zone.reset( new posix_time_zone( t ) );
}

#ifndef LINUX
static std::string get_os_timezone()
{
	TIME_ZONE_INFORMATION tzi;
	ZeroMemory(&tzi, sizeof(tzi));
	DWORD gtz = GetTimeZoneInformation(&tzi);
	ostringstream os;
	// локальное смещение
	os << "NNN" << setfill('0') << setw(2) << (-tzi.Bias / 60);
	if (tzi.Bias % 60)
		os << ':' << setfill('0') << setw(2) << abs(tzi.Bias % 60);
	if (gtz != TIME_ZONE_ID_UNKNOWN)
	{
		// летнее время
		os << "DDD";
		if (-60 != tzi.DaylightBias)
		{
			os << setfill('0') << setw(2) << (-tzi.DaylightBias / 60);
			if (tzi.DaylightBias % 60)
				os << ':' << setfill('0') << setw(2) << abs(tzi.DaylightBias % 60);
		}
		// начало летнего времени
		{
			const SYSTEMTIME& sum = tzi.DaylightDate;
			os << ",M" << sum.wMonth << '.' << sum.wDay << '.' << sum.wDayOfWeek;
			if (sum.wHour != 2 || sum.wMinute != 0 || sum.wSecond != 0)
			{
				os << '/' << sum.wHour;
				if (sum.wMinute != 0 || sum.wSecond != 0)
				{
					os << ':' << sum.wMinute;
					if (sum.wSecond != 0)
					{
						os << ':' << sum.wSecond;
					}
				}
			}
		}
		// конец летнего времени
		{
			const SYSTEMTIME& win = tzi.StandardDate;
			os << ",M" << win.wMonth << '.' << win.wDay << '.' << win.wDayOfWeek;
			if (win.wHour != 2 || win.wMinute != 0 || win.wSecond != 0)
			{
				os << '/' << win.wHour;
				if (win.wMinute != 0 || win.wSecond != 0)
				{
					os << ':' << win.wMinute;
					if (win.wSecond != 0)
					{
						os << ':' << win.wSecond;
					}
				}
			}
		}
	}

	std::string ss = os.str();
	posix_time_zone ptz(ss);
	return ptz.to_posix_string();
}
#else
static std::string get_os_timezone()
{
    return "MSK+03";
}
#endif // !LINUX


// кеш локалей для ввода времени
static std::map< std::string, std::locale >    cacheInputLocale;

static ptime parse_string( const std::string& str, const std::string& facet_fmt )
{
    if ( cacheInputLocale.end() == cacheInputLocale.find(facet_fmt) )
    {
        time_input_facet* sfacet = new time_input_facet(facet_fmt);
        cacheInputLocale.emplace( std::make_pair( std::string(facet_fmt), std::locale(std::locale::classic(), sfacet) ) );
    }

    auto it = cacheInputLocale.find(facet_fmt);
    ASSERT( cacheInputLocale.end() != it );
    std::istringstream iss;
    iss.imbue(it->second);
    iss.str(str);

    ptime dt(date_time::not_a_date_time );
    iss >> dt;
    if ( !iss.fail() && !dt.is_not_a_date_time() && !dt.is_special() )
        return dt;
    else
        return ptime(date_time::not_a_date_time );
}


// Преобразование UTC в локальное время
ptime Local_From_UTC( time_t _utc )
{
    local_date_time ldt( from_time_t(_utc), zone );
    ptime loc = ldt.local_time();
    return loc;
}

time_t UTC_From_Local_Impl( const boost::posix_time::ptime& lpt, boost::local_time::time_zone_ptr timeZone )
{
    /***********************************************************
    [                                          ]
    [                   c*****d****e           ] daylight local time
    [                  /          /            ]
    [ a************b  /       d****g*******h   ] local time
    [              ! /        ! /              ]
    [              !/         !/               ]
    [ ..зима...........лето.........зима....   ]  utc  time
    ************************************************************/

    using namespace boost::local_time;
    date d( lpt.date() );
    boost::posix_time::time_duration td( lpt.time_of_day() );
    local_date_time ldt( not_a_date_time, timeZone );
    try
    {
        ldt = local_date_time( d, td, timeZone, local_date_time::EXCEPTION_ON_ERROR );
    }
    catch( local_time::ambiguous_result& /*e*/ )
    {
        // не распознать лето/зима - считаем, что это лето
        try
        {
            ldt = local_date_time( d, td, timeZone, true );
        }
        catch(...)
        {
            ASSERT(false && "Ошибка распознавания даты и времени!\n");
#ifdef _MFC_VER
            TRACE("Ошибка распознавания даты и времени!\n");
#endif
            throw;
        }
    }
    catch( local_time::time_label_invalid& /*e*/ )
    {
        // Неприемлемое время - приводим к [b]
		boost::posix_time::time_duration deductible_td( 0, td.minutes(), td.seconds()+1 );
        td -= deductible_td;
        try
        {
            ldt = local_date_time( d, td, timeZone, local_date_time::EXCEPTION_ON_ERROR );
            ldt += seconds(1);
        }
        catch(...)
        {
            ASSERT(false && "Ошибка распознавания даты и времени!\n");
#ifdef _MFC_VER
            TRACE("Ошибка распознавания даты и времени!\n");
#endif
            throw;
        }
    }
    catch(...)
    {
        ASSERT(false && "Ошибка распознавания даты и времени!\n");
#ifdef _MFC_VER
        TRACE("Ошибка распознавания даты и времени!\n");
#endif
        throw;
    }
    // вычисления были корректны и однозначны
    ptime utc_ptime = ldt.utc_time();
    return ( utc_ptime - from_time_t( 0 ) ).total_seconds();
}


// Преобразование локального времени в UTC
time_t UTC_From_Local( const boost::posix_time::ptime& lpt )
{
    return UTC_From_Local_Impl(lpt, zone);
}

time_t UTC_From_MoscowLocal( const boost::posix_time::ptime& _local )
{
    static time_zone_ptr MoscowZone( new posix_time_zone( "MSK+03" ) );
    return UTC_From_Local_Impl(_local, MoscowZone);
}
