#pragma once
#include <atomic>

class WatchDog
{
public:
    explicit WatchDog( boost::posix_time::time_duration dead_delay, std::function<void(std::wstring)> alert_func );
    ~WatchDog();

    void on();   // включить сторожа
    void off();  // выключить сторожа
    void alive();// уведомление о нормальной работе
    void attach_detail( std::function<std::wstring()> detail_func ); // установить дополнительное логирование
    void remove_detail();                                    // стереть дополнительное логирование

private:
    const boost::posix_time::time_duration m_allowable_delay; // задержка срабатывания
    boost::thread    m_subthread; // нить сторожа 
    std::atomic_flag m_hang;   // программа зависла
    bool             m_active;    // активность сторожа
    std::function<std::wstring ()> m_detail_info; // функция для дополнительного логирования
    std::function< void(std::wstring) > m_alert_func; // функция при срабатывании сторожа

    void run();
    void alert();
};

// управление работой
inline void WatchDog::on()
{
    alive();
    m_active = true;
}
inline void WatchDog::off()
{
    m_active = false;
}
inline void WatchDog::alive()
{
    m_hang.clear();
}
