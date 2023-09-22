#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <sstream>

#include "../helpful/RT_Macros.h"
#include "../helpful/FileId.h"
#include "bbx_Identifier.h"
#include "bbx_Location.h"
#include "bbx_Stamp.h"

#pragma pack(push, 1)
namespace Bbx
{
    namespace Impl {
        class ReaderImpl;
        class WriterImpl;
        struct Cursor;
    }

    class ReadResult;
    struct Buffer;

    typedef std::vector<char> char_vec;

    enum class RecordType : char
    {
        /* Инкременные данные состояния состоят из двух частей: 
           состояния ДО и ПОСЛЕ, они используются для воспроизведения ситуации
           в купе с опорными данными. */
        Increment = 0,
        /* Опорные данные состояния содержат достаточно информации для воспроизведения
           хранимой ситуации, все файлы начинаются и заканчиваются опорными данными (в нормальном состоянии),
           опорные данные дублируются при создании следующего файла. */
        Reference = 1,
        /* Входящие посылки не связаны с инкрементными и опорными данными, являются "сквозными" сообщениями.
		   Входящие посылки рассматриваются не для приложения, а для системы целиком: "> H > F".  */
        IncomingPackage = 2,	
        /* Исходящие посылки не связаны с инкрементными и опорными данными, являются "сквозными" сообщениями.
           Аналогично входящим - исходящие это направление: "< H < F" */
        OutboxPackage = 4
    };

    /** @brief Читатель ЧЯ. Допускается создание любого количества читателей для каждого ЧЯ. */
    class Reader
    {
    public:
        explicit Reader(const Location& location);
        ~Reader();

        /** @brief Получение кода результата последней операции, возвращающей код результата */
        ReadResult lastResult() const;

        /** @brief Чтение опорных данных из текущей позиции */
        ReadResult readReference(Stamp& stamp, char_vec& caption, char_vec& data);

        /** @brief Чтение инкрементных данных из текущей позиции И в текущем направлении.
        Если направление "вперед", то читаются данные фрагмента "после";
        Если направление "назад", то читаются данные фрагмента "до" */
        ReadResult readIncrementOriented(Stamp& stamp, char_vec& caption, char_vec& data);

        /** @brief Чтение сообщения из текущей позиции. */
        ReadResult readPackage(Stamp& stamp, char_vec& caption, char_vec& data);

        /** @brief Чтение любой записи из текущей позиции, включая опорные, инкрементные (направленные),
            и все посылки */
        ReadResult readAnyRecord(Stamp& stamp, char_vec& caption, char_vec& data);

        /** @brief Перемотка до опорной записи с указанным штампом
        Если опорная запись с точно таким же штампом не найдена, поиск ближайшей к ней,
        вне зависимости от направления чтения */
        ReadResult rewind(const Stamp& where);

        /** @brief Перемещение курсора на следующую запись в соответствие с установленным
        направлением чтения.
        Возвращает код ошибки в случае нестандартной ситуации 
        (нарушения временной последовательности или разрыве ЧЯ) 
        и успех в случае нормального перехода */
        ReadResult next();

        /** @brief Перемещение курсора на следующую запись в нестандартном случае 
        (нарушение временной последовательности или разрыве ЧЯ)
        Возвращает неудачу в случае нормального состояния ЧЯ и успех
        в случае нестанартной ситуации */
        ReadResult forceNext();

        /** @brief Установка направления чтения */
        void setDirection(bool goForward);

        /** @brief Получение текущего направление чтения */
        bool getDirection() const;

        /** @brief Получение штампа самой первой и штампа самой последней имеющейся на диске записи */
        std::pair<Stamp,Stamp> getBoundStamp();

        /** @brief Есть ли писатель для этого ящика */
        bool existActualWriter() const;

        /** @brief Получение временного штампа текущей позиции */
        Stamp getCurrentStamp() const;

        /** @brief Получение идентификатора текущей позиции */
        Identifier getCurrentIdentifier() const;

        /** @brief Получение типа данных в текущей позиции */
        RecordType getCurrentType() const;

        /** @brief Советует каким способом из текущей позиции попасть к желаемой
        @return true если нужно использовать поиск опорной записи, false если лучше 
        прокрутиться по инкрементным данным */
        bool resolveToSearchReference(const Stamp& decidesStamp) const;

        /** @brief Осуществляет обновление информации о текущем файле */
        void update();

        /** @brief Открыт ли файл черного ящика? */
        bool isOpened() const;

        /** @brief Если ли ещё записи в текущем направлении? */
        bool hasMoreRecords() const;

        /** @brief Прочитать временнУю зону из заголовка черного ящика */
        std::string getTimeZone() const;

        /** @brief Прочитать текущий курсор (для отладки) */
        Bbx::Impl::Cursor getCurrentCursor() const;
    private:
        Reader(const Reader&);
        Reader& operator =(const Reader&);

        std::unique_ptr<Impl::ReaderImpl> pImpl;
    };

    class Writer
    {
    public:
        static std::shared_ptr<Writer> create(const Location& location);

        ~Writer();

        static void setCacheDeviateDelay( boost::posix_time::time_duration delay );
        static boost::posix_time::time_duration getCacheDeviateDelay();

        bool pushReference(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);
        bool pushIncrement(const Buffer& caption, const Buffer& before, const Buffer& after, const Stamp& stamp, const Identifier id);
        bool pushIncomingPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);
        bool pushOutboxPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);

        // Returns true if fatal error occures and writer can't work for any longer
        bool isDead() const;
        std::string getErrorMessage() const;

        // Blocks until all pushed records are processed by worker thread
        void flush();
        
        unsigned getPageSize() const;
        void setPageSize(unsigned page_size);
        void setRecomendedFileSize(unsigned file_size);
        bool setDiskLimit( const char * disk_size );
		bool setDiskLimit(BBX_DISK_SIZE disk_size);
		BBX_DISK_SIZE getDiskLimit() const;
        void setLifeTime(time_t life_time);
        const Location& getLocation() const;
        void setTimeZone( std::string textTZ );
        bool needReference( time_t curr_moment ) const;
        std::tuple<unsigned, unsigned, unsigned, unsigned> getWrittenMessagesCounts() const;
    private:
        explicit Writer(Impl::WriterImpl *pImpl);

        std::unique_ptr<Impl::WriterImpl> pImpl;
    };


    class ReadResult
    {
    public:
        enum ErrorType
        {
            // успешные операции
            Success = 0,                 /* Операция успешна (true) */
            FoundApproximateValue = 1,   /* Найдено приблизительное значение (true) */
            // ошибки и неудачи
            NoDataAvailable = 2,         /* Нет данных (конец ЧЯ) */
            WrongRecordType = 3,         /* Заказан неправильный тип данных */
            TimeSequenceViolation = 4,   /* Обнаружено нарушение порядка временных штампов */
            NewSession = 5,              /* Обнаружен разрыв ЧЯ */
            NormalSequenceFound = 6,     /* Неожиданно нормальная последовательность штампов */
            VersionNotSupported = 7,     /* Версия ЧЯ не поддерживается */
            InternalError = 0x80,        /* Произошла внутренняя ошибка работы ЧЯ */
            NoFileOpened = 0x81,         /* нет открытого файла */
            ErrorOpeningFile = 0x82,     /* ошибка при открытии файла */
            NotContainsBeginning = 0x83, /* кусок не содержит начало записи */
            ReadingFileHeader = 0x84,    /* ошибка чтения заголовка файла */
            ReadingPageHeader = 0x85,    /* ошибка чтения заголовка страницы */
            BadPageHeader = 0x86,        /* некорректный заголовок страницы */
            PageRead = 0x87,             /* ошибка чтения страницы */
            PageNotFound = 0x88,         /* страница не найдена */
        };

        ReadResult() 
            : state(InternalError)
        {}

        ReadResult(ErrorType from) 
            : state(from)
        {}

        ErrorType get() const {
            return state;
        }

        bool operator ==(const ErrorType other) const {
            return other == state;
        }

        bool operator !=(const ErrorType other) const {
            return !operator ==(other);
        }

        typedef void (*unspecified_bool_type)();
        static void unspecified_bool_true() {}

        operator unspecified_bool_type() const
        {
            return (Success == state || FoundApproximateValue == state)? unspecified_bool_true : nullptr; 
        }
    private:
        ErrorType state;
    };

    inline bool operator ==(const ReadResult::ErrorType res, const ReadResult& r_res)
    {
        return r_res == res;
    }

    inline bool operator !=(const ReadResult::ErrorType res, const ReadResult& r_res)
    {
        return r_res != res;
    }


    /** @brief Структура, указывающая на область в памяти, не владеет ресурсами*/
    struct Buffer
    {
        Buffer();
        Buffer(char* buf_ptr, unsigned buf_size);
        Buffer(const std::string& str);
        Buffer(const char_vec& vec);
        void fillWithNulls();
        void fillWith(char ch);
        void copyTo(std::vector<char>& destination) const;

        template<class T>
        static Buffer create(T& inst)
        {
            return Buffer(reinterpret_cast<char *>(&inst), sizeof(T));
        }

        template<class T>
        static Buffer createConst(const T& inst)
        {
            return Buffer(reinterpret_cast<char *>(const_cast<T*>(&inst)), sizeof(T));
        }

        unsigned size;
        char* data_ptr;
    };

    inline char *begin(const Buffer& buf)
    {
        return buf.data_ptr;
    }

    inline char *end(const Buffer& buf)
    {
        return buf.data_ptr + buf.size;
    }

    inline Buffer::Buffer(char* buf_ptr, unsigned buf_size)
        : size(buf_size), data_ptr(buf_ptr)
    {
        if (buf_size)
        {
            assert(buf_ptr);
        }
    }

    inline Buffer::Buffer(const std::string& str)
        : size(size32(str)), data_ptr(const_cast<char*>(str.c_str()))
    {
    }

    inline Buffer::Buffer(const char_vec& vec)
        : size(size32(vec)), data_ptr(const_cast<char*>(&*vec.begin()))
    {
    }

    inline Buffer::Buffer()
        : size(0), data_ptr(nullptr)
    {
    }

    inline void Buffer::fillWithNulls()
    {
        const char filler = 0;
        if (data_ptr && size)
            memset(data_ptr, filler, size);
    }

    inline void Buffer::fillWith(char ch)
    {
        assert(data_ptr && size);
        memset(data_ptr, ch, size);
    }

    inline void Buffer::copyTo(std::vector<char>& destination) const
    {
        if (size != 0)
            destination.assign(data_ptr, data_ptr + size);
        else
            destination.clear();
    }
}
#pragma pack(pop)
