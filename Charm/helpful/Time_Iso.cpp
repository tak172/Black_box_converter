#include "stdafx.h"

#include <boost/version.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/utility/string_ref.hpp>
//#include "RT_Macros.h"
#include "Time_Iso.h"

namespace bt = boost::posix_time;

// fix bug Boost 1.76
static bt::ptime fixed_from_time_t( time_t val )
{
    return bt::ptime( boost::gregorian::date( 1970, 1, 1 ) ) + bt::seconds( val );
}

// вывод time_t в стандартном виде
std::string time_to_iso( time_t val )
{
    const auto YMD = 4+2+2; // знаков в год+месяц+день
    const auto HMS = 2+2+2; // знаков в часы+минуты+секунды

    // самоделка для оптимизации
    boost::posix_time::ptime pt = fixed_from_time_t( val );
    // буфер для результата
    char buffer[ YMD+1+HMS+1 ];
    char* p = &*std::begin( buffer );

#ifndef LINUX
	auto twain = [&p](unsigned __int64 _val) {
#else
	auto twain = [&p](__uint64 _val) {
#endif // !LINUX
        *p++ = char( '0' + (_val/10) %10 );
        *p++ = char( '0' + _val%10 );
    };
    const auto ymd = pt.date().year_month_day();
    const auto tod = pt.time_of_day();
    twain( ymd.year/100 );
    twain( ymd.year );
    twain( ymd.month );
    twain( ymd.day );
    *p++ = 'T';    
    twain( tod.hours() );
    twain( tod.minutes() );
    twain( tod.seconds() );
    *p++ = 'Z';

    return std::string( buffer, p );
}

std::wstring time_to_isow( time_t val )
{
    const auto YMD = 4+2+2; // знаков в год+месяц+день
    const auto HMS = 2+2+2; // знаков в часы+минуты+секунды

    // самоделка для оптимизации
    boost::posix_time::ptime pt = fixed_from_time_t( val );
    // буфер для результата
    wchar_t buffer[ YMD+1+HMS+1 ];
    wchar_t* p = &*std::begin( buffer );

#ifndef LINUX
    auto twain = [&p](unsigned __int64 _val) {
#else
    auto twain = [&p](__uint64 _val) {
#endif // !LINUX
        *p++ = wchar_t( '0' + (_val/10) %10 );
        *p++ = wchar_t( '0' + _val%10 );
    };
    const auto ymd = pt.date().year_month_day();
    const auto tod = pt.time_of_day();
    twain( ymd.year/100 );
    twain( ymd.year );
    twain( ymd.month );
    twain( ymd.day );
    *p++ = 'T';    
    twain( tod.hours() );
    twain( tod.minutes() );
    twain( tod.seconds() );
    *p++ = 'Z';

    return std::wstring( buffer, p );
}

time_t time_from_iso( boost::string_ref s )
{
    time_t res = 0;
    try
    {
        // самоделка для оптимизации
        const unsigned YMD = 4+2+2; // год месяц день (без учета T)
        const unsigned HMS = 2+2+2; // час минута секунда (без последнего Z)
        if ( s.size() >= YMD+1+HMS && 'T' == s[YMD] )
        {
            auto it = s.cbegin();
            int y =  *(it++) - '0'; // год
            y = y*10 + *(it++) - '0';
            y = y*10 + *(it++) - '0';
            y = y*10 + *(it++) - '0';
            int m =  *(it++) - '0'; // месяц
            m = m*10 + *(it++) - '0'; 
            int d =  *(it++) - '0'; // день
            d = d*10 + *(it++) - '0';

            it++; // разделитель T

            int hour=      *(it++) - '0'; // десятки часов
            hour= hour*10+ *(it++) - '0'; // единицы часов
            int min =      *(it++) - '0'; // десятки минут
            min = min*10 + *(it++) - '0'; // единицы минут
            int sec =      *(it++) - '0'; // десятки секунд
            sec = sec*10 + *(it++) - '0'; // единицы секунд

            // наличие завершающего Z не обязательно

            namespace bt = boost::posix_time;
            static const bt::ptime zero = bt::from_time_t( 0 );
            bt::ptime full( boost::gregorian::date( short(y), short(m), short(d) ), bt::time_duration(hour, min, sec) );
            res = ( full - zero ).total_seconds();

            // перед 2038-01-19 03:14:07 UTC 
#ifndef BOOST_VERSION
#error Undefined Boost version!   
#elif BOOST_VERSION >= 106700
            /* Version 1.67.0 Fixed various year 2038 (32-bit) issues */
#else
            if ( res < 0 && full > zero )
            {
                full = bt::ptime( boost::gregorian::date( 2038, 1, 19 ), bt::time_duration(3, 14, 7) );
                res = ( full - zero ).total_seconds();
            }
#endif
        }
    }
    catch(...)
    {
        // bad time format
        res = 0;
    }
    return res;
}
