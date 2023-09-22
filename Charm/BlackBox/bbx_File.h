#pragma once

#include "bbx_BlackBox.h"
#include "bbx_Page.h"
#include "bbx_Record.h"


namespace Bbx
{
    class Stamp;

    namespace Impl
    {
        /** 
        @brief Класс, отражающий представление заголовка файла в черном ящике,
        изменение структуры этого класса уничтожит обратную совместимость версий черного ящика.
        */
#pragma pack(push, 1)
        class FileHeader
        {
        public:
            FileHeader();

            void setPageSize(unsigned size);
            void setExtensionSize(unsigned size);
            void setFirstRecordTime(const Stamp&  time);
            void setLastRecordTime(const Stamp&  time);

            unsigned getPageSize() const;
            unsigned getExtensionSize() const;
            unsigned getHeaderSize() const;
            Stamp getFirstRecordTime() const;
            Stamp getLastRecordTime() const;

        private:
            unsigned pageSize;
            unsigned extensionSize;
            time_t timeBegin;
            time_t timeEnd;
        };
#pragma pack(pop)

        /** @brief Базовый класс для любого используемого файла черного ящика */
        class BaseFile : boost::noncopyable
        {
        public:
            bool isOpened() const;
            static bool safeRemove(const std::wstring& path);

        protected:
            FileHeader header;

            BaseFile();
            virtual ~BaseFile();
            void swap( BaseFile& other );
            bool safeOpen_ModeRead (const std::wstring& path);
            bool safeOpen_ModeWrite(const std::wstring& path);
            void close();
            FileId getHandle() const
            {
                return handle;
            }

        private:
            FileId handle;
        };

        /** @brief Файл для записи */
        class FileWriter : public BaseFile
        {
        public:
            static std::string generateName(const Location& location);

            FileWriter(const Location& bbx_location, unsigned page_size);
            ~FileWriter();

            void update(bool force);
            bool writeRecord(RecordOut& msg);
            bool timeToCloseTheFile(const Stamp& stamp) const;
            bool readyToBeClosed() const;
            void setRecomendedFileSize(unsigned fileSize);
            void setTimeZone( std::string textTZ );
            std::tuple<unsigned, unsigned, unsigned, unsigned> getWrittenMessagesCounts() const;

        private:
            Location location;
            PageWriter page;
            bool lastWroteWasReference;
            unsigned maximumFileSizeBytes;
            unsigned bytesWritten;
            std::map<RecordType, unsigned> messagesWritten;
            time_t startTime;
            std::string timeZone;

            std::string generateExtensionZone() const;
            unsigned getReferenceMessagesCount() const;
            unsigned getMessagesCount(RecordType recordType) const;
            bool create(const Stamp& stamp);
            bool writeExtensionZone(const Bbx::Buffer& extensionData);
            void registerDataRecord(RecordType type, unsigned bytes);
            bool exceedFileAge(const Stamp& stampWrite) const;
            bool exceedFileSize() const;
            bool processMessageIntoPages(RecordOut& record);
        };

        /** @brief Класс блокирует участок файла и перемещает курсор на его начало */
        class SectionLocker : boost::noncopyable
        {
        public:
			SectionLocker(bool Exclusive, const FileId& file, BBX_SIZE offset, unsigned size);
            ~SectionLocker();

            /** @brief Блокирующее чтение из заблокированной зоны файла в буфер
            @param buf Корретный буфер для чтения
            @return true если данные были считаны в полном объеме, false если нет */
            bool read( Buffer buf ) const;

            /** @brief Блокирующая запись буфера в файл в заблокированную зону 
            @param data Корректный буфер для записи
            @return true если данные были записаны правильно, false если возникла ошибка */
            bool write(const Buffer& data) const;

        protected:
#ifdef LINUX
            static bool trylock( bool Exclusive, FileId fd, BBX_SIZE offset, BBX_SIZE size );
#endif
            static bool lock(    bool Exclusive, FileId fd, BBX_SIZE offset, BBX_SIZE size );
            static void unlock(  FileId fd, BBX_SIZE offset, BBX_SIZE size );

        private:
            FileAddress address;
			FileId handle;
            bool locked;

            bool setPointer() const;
        };

        class SharedSection : public SectionLocker
        {
        public:
			SharedSection(const FileId& file, BBX_SIZE offset, unsigned size)
                : SectionLocker(false, file, offset, size)
            {}
#ifdef LINUX
            static bool trylock(FileId fd, BBX_SIZE offset, BBX_SIZE size );
#endif
            static bool lock(   FileId fd, BBX_SIZE offset, BBX_SIZE size );
            static void unlock( FileId fd, BBX_SIZE offset, BBX_SIZE size );
        };

        class OwnSection : public SectionLocker
        {
        public:
			OwnSection(const FileId& file, BBX_SIZE offset, unsigned size)
                : SectionLocker(true, file, offset, size)
            {}
#ifdef LINUX
            static bool trylock(FileId fd, BBX_SIZE offset, BBX_SIZE size );
#endif
            static bool lock(   FileId fd, BBX_SIZE offset, BBX_SIZE size );
            static void unlock( FileId fd, BBX_SIZE offset, BBX_SIZE size );
        };

        //
        // Implementation
        //
        inline void FileHeader::setPageSize(unsigned size)
        {
            pageSize = size;
        }

        inline void FileHeader::setExtensionSize(unsigned size)
        {
            extensionSize = size;
        }

        inline void FileHeader::setFirstRecordTime(const Stamp&  time)
        {
            timeBegin = time;
        }

        inline void FileHeader::setLastRecordTime(const Stamp&  time)
        {
            timeEnd = time;
        }

        inline unsigned FileHeader::getPageSize() const
        {
            return pageSize;
        }

        inline unsigned FileHeader::getExtensionSize() const
        {
            return extensionSize;
        }

        inline unsigned FileHeader::getHeaderSize() const
        {
            return ((unsigned)sizeof(*this))+getExtensionSize();
        }

        inline Stamp FileHeader::getFirstRecordTime() const
        {
            return timeBegin;
        }

        inline Stamp FileHeader::getLastRecordTime() const
        {
            return timeEnd;
        }

        inline unsigned FileWriter::getReferenceMessagesCount() const
        {
            return getMessagesCount(RecordType::Reference);
        }

        inline unsigned FileWriter::getMessagesCount(RecordType recordType) const
        {
            if (0u == messagesWritten.count(recordType))
                return 0u;
            return messagesWritten.at(recordType);
        }

        inline bool FileWriter::readyToBeClosed() const
        {
            return lastWroteWasReference && (getReferenceMessagesCount() > 1);
        }

        inline bool FileWriter::timeToCloseTheFile(const Stamp& stamp) const
        {
            bool maximumFileAgeReached = exceedFileAge(stamp);
            bool maximumFileSizeReached = exceedFileSize();
            return readyToBeClosed() && (maximumFileAgeReached || maximumFileSizeReached);
        }

        inline void FileWriter::setRecomendedFileSize(unsigned fileSize)
        {
            maximumFileSizeBytes = fileSize;
        }

        inline void FileWriter::setTimeZone( std::string textTZ )
        {
            timeZone = textTZ;
        }

        inline bool FileWriter::exceedFileSize() const
        {
            return (bytesWritten >= maximumFileSizeBytes);
        }

        inline std::tuple<unsigned, unsigned, unsigned, unsigned> FileWriter::getWrittenMessagesCounts() const
        {
            return std::make_tuple(getMessagesCount(RecordType::Reference), getMessagesCount(RecordType::Increment),
                getMessagesCount(RecordType::IncomingPackage), getMessagesCount(RecordType::OutboxPackage));
        }
    }
}

