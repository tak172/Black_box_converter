#pragma once

#ifndef LINUX
void FatalError(LPEXCEPTION_POINTERS lpEP, bool showMsgBox = true);
void FatalError(LPCTSTR error_str = NULL, bool showMsgBox = true);
#else
void FatalError( const wchar_t * error_str, bool showMsgBox = true );
#endif // !LINUX


class ExceptionProcessHandler
{
public:
    ExceptionProcessHandler ();
    ~ExceptionProcessHandler();
};

void ExceptionInstall_CurrentThread();
bool IsFatalErrorActive();

#ifndef LINUX
    #define CALL_ERROR  L"Ошибка вызова " __FUNCTIONW__ L"()!"
#else
    #define CALL_ERROR  L"Ошибка вызова функции ZZZZ()!"
#endif // !LINUX


