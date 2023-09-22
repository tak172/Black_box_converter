#ifndef _RT_SINGLETON_H_
#define _RT_SINGLETON_H_
#include <boost/thread/mutex.hpp>
/**
 Описание методов и данных для шаблона одиночки (Singleton).
 \nПример использования для описания:
 \code
 class ExampleClass
 {
   ...
   DECLARE_SINGLETON(ExampleClass);
   ...
 }
 \endcode
*/
#define DECLARE_SINGLETON(myClass)                                          \
    private:                                                                \
        static myClass* volatile pInstance; /** единственный экземпляр */   \
        static boost::mutex mt_sync;                                        \
        myClass(); /** закрытый конструктор */                              \
        ~myClass(); /** закрытый деструктор */                              \
                                                                            \
    public:                                                                 \
        static myClass* instance() /** доступ к экземпляру */               \
        {                                                                   \
            if ( !pInstance )                                               \
            {                                                               \
                boost::mutex::scoped_lock lock(mt_sync);                    \
                if ( !pInstance )                                           \
                {                                                           \
                    myClass* volatile p( new myClass );                     \
                    pInstance = p;                                          \
                }                                                           \
            }                                                               \
            return pInstance;                                               \
        }                                                                   \
                                                                            \
        static const myClass& instanceCRef() /** доступ к экземпляру */     \
        {                                                                   \
            return *instance();                                             \
        }                                                                   \
                                                                            \
        /** уничтожитель единственного экземпляра */                        \
        static void Shutdowner()                                            \
        {                                                                   \
            /* Атомарно */                                                  \
            myClass* volatile p(nullptr);                                   \
            boost::mutex::scoped_lock lock(mt_sync);                        \
            std::swap(p,pInstance);                                         \
            delete p;                                                       \
        }

/**
Макрос для размещения глобальных данных шаблона одиночки (Singleton)
 \nПример использования:
 \code
 #include "ExampleClass.h"

 ALLOCATE_SINGLETON(ExampleClass);
 
 void ExampleClass::Func()
 {
 ...
 }
 
 \endcode
*/
#define ALLOCATE_SINGLETON(myClass)                 \
    myClass* volatile myClass::pInstance = nullptr; \
    boost::mutex      myClass::mt_sync

#endif // _RT_SINGLETON_H_
