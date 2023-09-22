#pragma once

// Трассировка всех необработанных исключений в любой нити с выводом дампа.
// Достаточно включить в одной нити (любой).
// При неоднократном включении число дампов увеличится кратно.
class ExceptionTracer
{
public:
    ExceptionTracer();
    ~ExceptionTracer();
private:
#ifndef LINUX
    static LONG WINAPI veh(_EXCEPTION_POINTERS* p);
    PVOID MyHandler;
#endif // !LINUX
};
