#include "stdafx.h"

#include "../helpful/RT_ThreadName.h"
#include "../helpful/WatchDog.h"

using namespace boost::posix_time;

WatchDog::WatchDog( time_duration dead_delay, std::function<void(std::wstring)> alert_func )
    : m_allowable_delay(dead_delay), m_subthread(), m_hang(), m_active(true),
    m_detail_info(), m_alert_func(alert_func)
{
#ifndef LINUX
    if (!IsDebuggerPresent())
#endif // !LINUX
        m_subthread = boost::thread( boost::bind(&WatchDog::run, this) );
}

WatchDog::~WatchDog()
{
    if ( m_subthread.joinable() )
    {
        m_subthread.interrupt();
        m_subthread.join();
    }
}

// установить дополнительное логирование
void WatchDog::attach_detail( std::function<std::wstring()> detail_func )
{
    m_detail_info = detail_func;
}

// стереть дополнительное логирование
void WatchDog::remove_detail()
{
    m_detail_info = std::function<std::wstring()>();
}

void WatchDog::run()
{
    RT_SetThreadName( "WatchDog" );
    ptime last = second_clock::universal_time();
    try
    {
        alive();
        while( !boost::this_thread::interruption_requested() )
        {
            boost::this_thread::sleep_for( boost::chrono::milliseconds(250) ); // снижение загрузки процессора
            // нормальная работа или сторож не активен
            if ( !m_hang.test_and_set() || !m_active )
            {
                last = second_clock::universal_time();
            }
            // Задержка превышена
            else if ( m_allowable_delay < ( second_clock::universal_time() - last ) )
            {
                alert();
                // и выходим из цикла
                break;
            }
        }
    }
    catch( boost::thread_interrupted& /*e*/ )
    {
        RT_SetThreadName( "WatchDog[interrupt]" );
    }
    catch( ... )
    {
        RT_SetThreadName( "WatchDog[exception]" );
    }
}

void WatchDog::alert()
{
    std::wstring temp;
    if ( m_detail_info )
        temp = m_detail_info();
    m_alert_func( temp );
    throw std::logic_error( "WatchDog alert" );
}
