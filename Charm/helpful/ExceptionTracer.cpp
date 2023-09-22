#include "stdafx.h"
#include "Dump.h"
#include "ExceptionTracer.h"

// Векторная обработка исключений
ExceptionTracer::ExceptionTracer()
{
#ifndef LINUX
    MyHandler = AddVectoredExceptionHandler(1, veh);
#endif // !LINUX
}

ExceptionTracer::~ExceptionTracer()
{
#ifndef LINUX
    RemoveVectoredExceptionHandler(MyHandler);
#endif // !LINUX
}

#ifndef LINUX
LONG WINAPI ExceptionTracer::veh(_EXCEPTION_POINTERS *ep)
{
    do
    {
        if ( !ep || !ep->ExceptionRecord )
            break;
        auto& rec = *ep->ExceptionRecord;
        const unsigned long MS_CPP_EXCEPTION = 0xE06D7363UL; // магическое число MS для исключений C++
        if ( rec.ExceptionCode != MS_CPP_EXCEPTION )
            break;
        unsigned params = rec.NumberParameters;
        DWORD_PTR image = 0;
        if ( params == 3 )
        {
            image = 0;    // для 32 системы
        }
        else if ( params == 4 )
        {
#if _WIN64
            image = rec.ExceptionInformation[3]; // для 64 системы
#else
            image = 0; // для 32 системы
#endif
        }
        else
        {
            break;  // иначе что-то непонятное
        }
        //
        DWORD* a1 = reinterpret_cast<DWORD *>( DWORD_PTR( rec.ExceptionInformation[ 2 ] ) );
        if ( nullptr == a1 )
            break; // это повторный выброс того же самого исключения - игнорируем
        DWORD v1 = a1[3];
        DWORD* a2 = reinterpret_cast<DWORD *>( DWORD_PTR(image) + DWORD_PTR(v1) );
        DWORD v2 = a2[1];
        DWORD* a3 = reinterpret_cast<DWORD *>( DWORD_PTR(image) + DWORD_PTR(v2) );
        DWORD v3 = a3[1];
        const char * excName = reinterpret_cast<char *>( image + v3 + 2*sizeof(void*) ); // это уже строковое название класса
        
        //
        if ( !excName || IsBadStringPtrA(excName,16) || !*excName ) // размер 16 выбран произвольно
            break;
        // игнорируемые классы исключений
        typedef const char * cptr;
        static const cptr ignor[] = {
            "right_error@windows_utils",

            "thread_interrupted@boost@@",
            "@exception_detail@boost@@",
			"bad_lexical_cast@boost@@@boost@@",

            "TransportFailure@qpid@@",
            "@types@qpid@@",
            "@framing@qpid@@",
            "@messaging@qpid@@",
            "@client@qpid@@",

            "HemException@HemHelpful@@",
            "Exception@AsyncBisk@Foreign@@",
            "ParseException@Asoup@@"
        };
        // игнорируемые классы исключений пропускаем, остальным делаем маленький дамп
        auto match = [excName]( cptr sub ) {
            return nullptr != strstr( excName, sub );
        };
        if ( std::none_of(std::begin(ignor), std::end(ignor), match ) )
        {
            CProcessDump dump( ep, false );
            CProcessDump::RestrictSize(); 
        }
        break;
    }
    while(false);

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif // !LINUX