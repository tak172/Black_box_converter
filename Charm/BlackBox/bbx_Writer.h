#pragma once

#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <atomic>

#include "bbx_BlockingPtrQueue.h"
#include "bbx_Requirements.h"
#include "bbx_Record.h"
#include "bbx_File.h"

#pragma pack(push, 1)
namespace Bbx
{
    class Stamp;

    namespace Impl
    {
        static const unsigned int c_maximumQueuePagesCapability = 64u;
        
        /**
        @brief Регистратор чёрного ящика, используется для сохранения данных в требуемом формате.
        Разрешено создавать не более одного экземпляра на программу с одинаковыми комбинациями
        "папка, префик и суффикс файлов", можно использовать из любого числа потоков.

        @todo Доделать следующие пункты:
        2.1.	Класс BbxOut предоставляет методы:
        2.1.10.	желаемый размер файла – в байтах и/или в секундах (используется только при создании нового файла); до первой установки – размер по умолчанию; при некорректных параметрах – ближайший корректный размер;  
        2.6.	BbxOut: задание интервала хранения. После вызова данного метода при создании нового 
        (очередного) файла должны удаляться все файлы, где все записи старше интервала хранения 
        ((от штампа последней записи). Удаление начинать с самых ранних файлов, чтобы обеспечить 
        ((целостность данных при сбоях и обеспечить блокировку от класса BbxIn (см. п.3.5).
        */
        class WriterImpl : boost::noncopyable
        {
        public:
            static WriterImpl* create(const Bbx::Location& location);
            WriterImpl(const Bbx::Location& location, FileId verificationFile);
            ~WriterImpl(void);

            bool pushReference(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);
            bool pushIncrement(const Buffer& caption, const Buffer& before, const Buffer& after, const Stamp& stamp, const Identifier id);
            bool pushIncomingPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);
            bool pushOutboxPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id);

            bool isDead() const;
            std::string getErrorMessage() const;
            void flush();
                        
            unsigned getPageSize() const;
            void setPageSize(unsigned page_size);
            void setRecomendedFileSize(unsigned file_size);
            bool setDiskLimit( const char * disk_size );
			bool setDiskLimit(BBX_DISK_SIZE disk_size);
			BBX_DISK_SIZE getDiskLimit() const;
            void setLifeTime(time_t life_time);
            void setTimeZone( std::string textTZ );
            const Location& getLocation() const;

            bool needReference( time_t curr_moment ) const;
            
            std::tuple<unsigned, unsigned, unsigned, unsigned> getWrittenMessagesCounts() const;
        private:
            Location location;
            FileId verificationFile;
            FileWriter *filewriter;
            unsigned pageSize;
            unsigned recomendedFileSize;
            BBX_DISK_SIZE limitDiskSize;
            mutable boost::mutex fileLock;
            time_t recomendedFilesAge;
            std::string timeZone;

            time_t nextReferenceWriteTime; // момент следующего требования опорных данных
            size_t referenceFlushInterval; // интервал записи опорных данных в черный ящик
            static const size_t DEFAULT_REF_INTERVAL = 5 * 60;

            boost::thread work;
            BlockingPtrQueue<WriterTask> tasks;
            std::atomic_size_t queueWeight;

            std::atomic_bool fatalError;
            std::string errorMessage;
            std::atomic_bool referenceAdded;
            std::atomic_bool flushRequest;

            boost::mutex flushMutex;
            boost::condition_variable flushCondition;

            boost::mutex taskProcessedMutex;
            boost::condition_variable taskProcessedCondition;

            time_t calcNextReferenceMoment( time_t last_write );

            bool pushTask(std::shared_ptr<WriterTask> task);

            bool processTask(const WriterTask& task);
            bool processReference(const WriterTask& referenceRecord);
            bool processNonReferenceRecord(const WriterTask& dataRecord);
            void storeError(std::string errorText);

            void run();
            bool pushDataRecord(RecordOut& record);
            void deleteOutdatedFiles(const Stamp& currentStamp) const;
            bool createFileWriter(const Stamp& firstTime);
            void deleteFileWriter();
            bool alive() const;

            size_t getMaximumQueueWeight() const;
        };

		inline BBX_DISK_SIZE WriterImpl::getDiskLimit() const
        {
            return limitDiskSize;
        }
        inline const Location& WriterImpl::getLocation() const
        {
            return location;
        }
        inline bool WriterImpl::needReference( time_t curr_moment ) const
        {
            return 0==nextReferenceWriteTime
                || curr_moment >= nextReferenceWriteTime;
        }

        inline time_t WriterImpl::calcNextReferenceMoment( time_t last_write )
        {
            // подровнять к точке, кратной интервалу записи опорных данных
            time_t t = last_write + referenceFlushInterval * 3/2;
            t -= t % referenceFlushInterval;
            return t;
        }

        inline std::tuple<unsigned, unsigned, unsigned, unsigned> WriterImpl::getWrittenMessagesCounts() const
        {
            boost::mutex::scoped_lock lock(fileLock);

            if (filewriter)
                return filewriter->getWrittenMessagesCounts();
            else
                return std::make_tuple(0u, 0u, 0u, 0u);
        }

        inline size_t WriterImpl::getMaximumQueueWeight() const
        {
            return static_cast<size_t>(pageSize * c_maximumQueuePagesCapability);
        }
    }
}
#pragma pack(pop)
