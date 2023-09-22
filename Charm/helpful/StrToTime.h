#pragma once
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "Time_Iso.h"

const time_t BAD_TIME_T = 0;

// Читать дату и время из строки
// Ошибка - возвращает defvalue
time_t StrDataTime_To_UTC(const std:: string& dt__str, time_t defvalue=BAD_TIME_T );
// Ошибка - возвращает BAD_TIME_T
time_t UTC_from_Str( const std::string& dt_str, const char * fmt );

//
// Вспомогательные варианты
//
time_t StrDataTime_To_UTC(const std::wstring& dt_wstr, time_t defvalue=BAD_TIME_T );
// для разбора "29.06.2012 08:30:00"
inline time_t UTC_from_DMYhms( const std:: string& dt__str, time_t defvalue )
{
    time_t res = UTC_from_Str(dt__str, "%d.%m.%Y %H:%M:%S");
    if ( res != BAD_TIME_T )
        return res;
    else
        return defvalue;
}
// для разбора L"29.06.2012 08:30:00"
time_t UTC_from_DMYhms( const std::wstring& dt__str, time_t defvalue );

// опции вывода времени
enum DATETIME_OPTION
{
    // время (по умолчанию - без времени)
    DTO_H          = 0x001,    ///< часы
    DTO_M          = 0x002,    ///< минуты
    DTO_S          = 0x004,    ///< секунды
    // дата и ее порядок (по умолчанию - без даты)
    DTO_ORDER_DMY  = 0x008,   ///< дата в порядке {день месяц год} (иначе - по умолчанию - без даты)
    DTO_ORDER_YMD  = 0x010,   ///< дата в порядке {год месяц день} (иначе - по умолчанию - без даты)
    DTO_YEAR_4DIG  = 0x020,   ///< в году 4 цифры (иначе только две)
    DTO_YEAR_DOT   = 0x040,   ///< разделитель даты - точка (иначе - по умолчанию - минус)
    DTO_YEAR_SLASH = 0x080,   ///< разделитель даты - косая (иначе - по умолчанию - минус)
    DTO_YEAR_NONE  = 0x100,   ///< разделитель даты - отсутствует (иначе - по умолчанию - минус)
    // признак округления до минут
    DTO_ROUND_M    = 0x200,   ///< округление до минут

    // общеупотребительные комбинации
    DTO_HMS       = (                                 DTO_H | DTO_M | DTO_S),
    DTO_YMD_HM    = ( DTO_ORDER_YMD | DTO_YEAR_4DIG | DTO_H | DTO_M ),
    DTO_YMD_HMS   = ( DTO_ORDER_YMD | DTO_YEAR_4DIG | DTO_H | DTO_M | DTO_S),
    DTO_DMY_HMS   = ( DTO_ORDER_DMY                 | DTO_H | DTO_M | DTO_S),
    DTO_DMY_HM    = ( DTO_ORDER_DMY                 | DTO_H | DTO_M ),
};
// ДатуВремя вывести в строку
std::string  UTC_to_String(time_t moment, unsigned option = DTO_YMD_HM);
std::wstring UTC_to_wString(time_t moment,unsigned option = DTO_YMD_HM);
std::string   UTC_to_String(time_t moment, const char * fmt );
std::wstring  UTC_to_String(time_t moment, const wchar_t * fmt );

// Преобразования UTC-Local
time_t UTC_From_Local( const boost::posix_time::ptime& _local );
time_t UTC_From_MoscowLocal( const boost::posix_time::ptime& _local );
boost::posix_time::ptime Local_From_UTC( time_t _utc );
// ВременнАя зона
std::string get_TimeZone();
void set_TimeZone( const std::string& stz );
