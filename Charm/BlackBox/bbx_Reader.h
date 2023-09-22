#pragma once

#include "bbx_FileReader.h"

#pragma pack(push, 1)
namespace Bbx
{
    namespace Impl
    {

        struct ReadFileInfo;

        class ReaderImpl : boost::noncopyable
        {
        public:
            explicit ReaderImpl(const Location& location);
            ~ReaderImpl(void);

            /** @brief Получение кода результата последней операции, возвращающей код результата */
            ReadResult lastResult() const;

            /** @brief Чтение опорных данных из текущей позиции */
            ReadResult readReference(Stamp& stamp, char_vec& caption, char_vec& data);

            /** @brief Чтение инкрементных данных из текущей позиции И в текущем направлении.
            Если направление "вперед", то читаются данные фрагмента "после";
            Если направление "назад", то читаются данные фрагмента "до" */
            ReadResult readIncrementOriented(Stamp& stamp, char_vec& caption, char_vec& data);

            /** @brief Чтение посылки из текущей позиции */
            ReadResult readPackage(Stamp& stamp, char_vec& caption, char_vec& data);

            /** @brief Чтение любой записи из текущей позиции, включая опорные, инкрементные (направленные),
            и все посылки */
            ReadResult readAnyRecord(Stamp& stamp, char_vec& caption, char_vec& data);

            /** @brief Перемотка до опорной записи с указанным штампом
            Если опорная запись с точно таким же штампом не найдена, поиск ближайшей к ней,
            вне зависимости от направления чтения */
            ReadResult rewind(const Stamp& where);

            /** @brief Перемотка до любой записи с указанным штампом
            Если запись с таким же штампом не найдена, поиск ближайшей меньшей */
            ReadResult rewindToAny(const Stamp& where);

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

            /** @brief Получение доступного временного интервала имеющейся на диске записи */
            std::pair<Stamp,Stamp> getAvailableTimeInterval() const;
            static std::pair<Stamp,Stamp> getAvailableTimeInterval(const Location& location);

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

            const std::wstring& getCurrentFilePath() const;
            FileReader::Cursor getCurrentCursor() const;
            ReadResult rewindToCursor(const std::wstring& filePath, const FileReader::Cursor& cursor);

            const Location& getLocation() const;
            std::string getTimeZone() const;

        private:
            Location location;
            bool forward;
            FileReader fileReader;
            ReadResult result;
            mutable boost::mutex mutex;
            // сведения о достигнутом конце черного ящика
            std::wstring eod_front; // первый известный файл
            std::wstring eod_back;  // последний известный файл
            bool eod_forward;                       // направление чтения
            bool eod_normalSequence;                // режим чтения

            ReadResult& saveResult(ReadResult res);
            ReadResult moveNext(bool normalSequence);
            ReadResult nextFileMove( const std::wstring& nextFile, bool normalSequence);
            std::wstring selectNextFile(const FileChain& fileChain, size_t minFileSize) const;
            std::wstring selectFileBy(const Stamp& stamp) const;
            static std::vector<std::wstring> selectFilesCloserTo(const Location& location, const Stamp& stamp);
            // обслуживание данных о достижении конца черного ящика
            bool knownEoD( const FileChain& fileChain, size_t minFileSize, bool normalSequence ) const;
            void setEoD(   const FileChain& fileChain, size_t minFileSize, bool normalSequence );
            void resetEoD();

            ReadResult readIncrementOrientedImpl(Stamp& stamp, char_vec& caption, char_vec& data);
            ReadResult readCurrentRecordImpl(Stamp& stamp, char_vec& caption, char_vec& data);
        };

        inline bool ReaderImpl::isOpened() const
        {
            return fileReader.isOpened();
        }

        inline bool ReaderImpl::hasMoreRecords() const
        {
            return fileReader.hasMoreRecords(forward);
        }
        inline void ReaderImpl::resetEoD()
        {
            eod_front.clear();
            eod_back.clear();
        }

    }
}
#pragma pack(pop)
