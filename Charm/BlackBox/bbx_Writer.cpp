#include "stdafx.h"
#include "bbx_Requirements.h"
#include "bbx_Writer.h"
#include "bbx_Reader.h"
#include "bbx_BlackBox.h"
#include "bbx_FileChain.h"
//#include "../helpful/ErrorHandler.h"
#include "../helpful/RT_ThreadName.h"
#include "boost/chrono.hpp"
#ifdef LINUX
#include "../helpful/Utf8.h"
#include <sys/stat.h>
#include <fcntl.h>
#endif // LINUX

#include <boost/filesystem.hpp>
namespace bfs = boost::filesystem;
namespace bt  = boost::posix_time;

using namespace Bbx::Impl;

// #define SINGLE_THREAD /*однопоточный режим*/

const bt::time_duration c_ThreadSleepDuration = bt::milliseconds(30u);

/** @brief Умолчание времени жизни файлов */
const time_t c_DefaultLifeTime = 30 * 24 * 60 * 60; // 30 суток (в секундах)

/** @brief Размер страницы файла по умолчанию */
const size_t c_DefaultPageSize = 256 * Bbx::c_KB;

/** @brief Минимально допустимое значение размера страницы данных в файле */
const unsigned c_MinimumPageSize = 256;

/** @brief Максимально допустимое значение размера страницы данных в файле */
const unsigned c_MaximumPageSize = 1 * Bbx::c_GB;

/** @brief Умолчание размера файла */
const size_t c_DefaultFileSize = 32 * Bbx::c_MB;

/** @brief Максимальный размер файла */
const unsigned c_MaximumFileSize = 2u * Bbx::c_GB;

/** @brief Максимально возможное значение места на диске */
const long long c_MaximumDiskSize = 1 * Bbx::c_PB;


WriterImpl::WriterImpl(const Bbx::Location& location, FileId verificationFile)
    : location(location), verificationFile(verificationFile),
      filewriter(nullptr), pageSize(c_DefaultPageSize), recomendedFileSize(c_DefaultFileSize),
      limitDiskSize(c_MaximumDiskSize),
      fileLock(), recomendedFilesAge(c_DefaultLifeTime),
      nextReferenceWriteTime(0),
      referenceFlushInterval(DEFAULT_REF_INTERVAL), 
      work(), tasks(), queueWeight(0u), fatalError(), errorMessage(""), referenceAdded(), flushRequest(),
      flushMutex(), flushCondition(), taskProcessedMutex(), taskProcessedCondition()
{
    fatalError.store(false);
    referenceAdded.store(false);
    flushRequest.store(false);
#if !defined(SINGLE_THREAD)
    work = boost::thread(boost::bind(&WriterImpl::run, this));
#endif
}

WriterImpl::~WriterImpl()
{
    if (work.joinable())
    {
        work.interrupt();
        work.join();
    }

    if (filewriter)
    {
        deleteFileWriter();
    }
    if (alive())
    {
#ifndef LINUX
        CloseHandle(verificationFile);
#else
        OwnSection::unlock( verificationFile, 0, 1024 );
        close(verificationFile);
        unlink( ToUtf8( location.verificationFilePath() ).c_str() );
#endif // !LINUX
    }
}

#ifndef LINUX
WriterImpl* WriterImpl::create(const Bbx::Location& location)
{
    std::wstring filePath = location.verificationFilePath();
	FileId verificationFile = CreateFile( filePath.c_str(), 0, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL );
	if( INVALID_HANDLE_VALUE == verificationFile )
        return nullptr;
    else
        return new WriterImpl(location, verificationFile);
}

#else

WriterImpl* WriterImpl::create( const Bbx::Location& location )
{
    std::wstring filePath = location.verificationFilePath();
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    FileId verificationFile = open( ToUtf8( filePath ).c_str(), O_WRONLY | O_CREAT, mode );
    if( -1 == verificationFile ) {
        return nullptr;
    }
    // попытка поставить свою блокировку
    std::string sample = "Lock blackbox";
    write( verificationFile, sample.c_str(), sample.size() );
    bool mylock = OwnSection::trylock( verificationFile, 0, sample.size() );
    if ( mylock ) {
        return new WriterImpl( location, verificationFile );
    } else {
        close( verificationFile );
        return nullptr;
    }
}
#endif // !LINUX

void WriterImpl::run()
{
// #ifndef LINUX
//     ExceptionInstall_CurrentThread();
// #endif
    RT_SetThreadName("Bbx::WriterImpl");
    try
    {
        // обычная работа - обработка очереди и сброса буферов
        while( !boost::this_thread::interruption_requested() )
        {
            if ( std::shared_ptr<WriterTask> task = tasks.pop() )
            {
                boost::mutex::scoped_lock lock(fileLock);
                bool procTask = processTask(*task);
                queueWeight.fetch_sub(task->getWeight());
                if (procTask)
                {
                    taskProcessedCondition.notify_all();
                }
                else
                {
                    fatalError.store(true);
                    break;
                }
            }
            else if (flushRequest)
            {
                if (filewriter)
                    filewriter->update(true);

                flushRequest.store(false);
                flushCondition.notify_all();
            }
            else
            {
                boost::this_thread::sleep(c_ThreadSleepDuration);
                if (filewriter)
                    filewriter->update(false);
            }
        }
    }
    catch( boost::thread_interrupted& /*e*/ )
    {
        RT_SetThreadName( "Bbx::WriterImpl[interrupt]" );
    }
    catch( ... )
    {
        RT_SetThreadName( "Bbx::WriterImpl[exception]" );
        ASSERT( false );
    }

    // доделка при завершении - обработка очереди
    while( std::shared_ptr<WriterTask> task = tasks.pop() )
    {
        queueWeight.fetch_sub( task->getWeight() );
        boost::mutex::scoped_lock lock(fileLock);
        if (processTask(*task))
        {
            taskProcessedCondition.notify_all();
        }
        else
        {
            fatalError.store(true);
            break;
        }
    }

    if (filewriter)
        filewriter->update(true);

    flushRequest.store(false);
    flushCondition.notify_all();

    taskProcessedCondition.notify_all();
}

bool WriterImpl::processTask(const WriterTask& task)
{
    if (RecordType::Reference == task.type)
        return processReference(task);
    else
        return processNonReferenceRecord(task);
}

void WriterImpl::deleteOutdatedFiles(const Bbx::Stamp& currentStamp) const
{
    bool cut = false;
    {
        // быстрая проверка необходимости удаления последнего файла
        auto pfc = location.getCPtrChain();
        std::wstring fn = pfc->getEarliestFile();
		auto tSz = pfc->getTotalSize();
        if ( !fn.empty() )
        {
            if ( tSz + recomendedFileSize > limitDiskSize )
                cut = true;
            else
            {
                time_t endTime = pfc->getEarliestFileEndTime();
                if ( 0 != endTime && endTime + recomendedFilesAge < currentStamp )
                    cut = true;
            }
        }
    }
    // и только если надо чтото удалять, то полный алгоритм
    if ( cut )
    {
        Bbx::FileChain fc = *location.getCPtrChain();
        // Достаточно удалять больше файлов, чем будет создано. Поэтому всего два прохода.
        // удаляется не более двух последних файлов (если их много то процесс будет растянут во времени)
        for( int pass=0; pass < 2; ++pass )
        {
            std::wstring filename = fc.getEarliestFile();
			auto totalSz = fc.getTotalSize();
            if ( !filename.empty() )
            {
                cut = false;
                if ( totalSz + recomendedFileSize > limitDiskSize )
                    cut = true;
                else
                {
                    ReadFileInfo rfInfo = FileReader::getFileInfo( filename );
                    if ( !rfInfo.isCorrect() )
                        cut = true;
                    else if ( rfInfo.endTime + recomendedFilesAge < currentStamp )
                        cut = true;
                }
                if ( cut && BaseFile::safeRemove( filename ) )
                    fc.takeEarliestFile();
                else
                    break;
            }
        }
    }
}

bool WriterImpl::createFileWriter(const Bbx::Stamp& firstTime)
{
    deleteOutdatedFiles( firstTime );
    filewriter = new FileWriter(location, pageSize);
    filewriter->setRecomendedFileSize(recomendedFileSize);
    filewriter->setTimeZone(timeZone);
    return true;
}

bool WriterImpl::pushTask(std::shared_ptr<WriterTask> task)
{
    if (nullptr == task)
        return false;

#if !defined(SINGLE_THREAD)
    if (tasks.emplace(task))
    {
        queueWeight.fetch_add( task->getWeight() );

        if (Bbx::RecordType::Reference == task->type)
        {
            bool already_has_reference = referenceAdded.exchange(true);
            if ( !already_has_reference )
                flush();
            nextReferenceWriteTime = calcNextReferenceMoment(task->stamp.getTime());
        }

        if (work.joinable() && (queueWeight.load() >= getMaximumQueueWeight() ) )
        {
            boost::unique_lock<boost::mutex> lock(taskProcessedMutex);
            while (queueWeight.load() >= getMaximumQueueWeight() && !tasks.empty())
                taskProcessedCondition.wait(lock);
        }
        return true;
    }
    else
        return false;
#else
    if (Bbx::RecordType::Reference == task->type)
    {
        referenceAdded.exchange(true);
        nextReferenceWriteTime = calcNextReferenceMoment(task->stamp.getTime());
    }
    bool res = processTask(*task);
    filewriter->update(true);
    return res;
#endif
}

bool WriterImpl::pushReference(const Bbx::Buffer& caption, const Bbx::Buffer& data, const Bbx::Stamp& stamp, const Bbx::Identifier id)
{
    if (fatalError.load())
        return false;

    std::shared_ptr<WriterTask> task(new WriterTask(id, stamp, Bbx::RecordType::Reference, caption, data));
    return pushTask(task);
}

bool WriterImpl::pushIncrement(const Bbx::Buffer& caption, const Bbx::Buffer& before, const Bbx::Buffer& after, const Bbx::Stamp& stamp, const Bbx::Identifier id)
{
    if (fatalError.load() || !referenceAdded.load())
        return false;

    std::shared_ptr<WriterTask> task(new WriterTask(id, stamp, Bbx::RecordType::Increment, caption, before, after));
    return pushTask(task);
}

bool WriterImpl::pushIncomingPackage(const Bbx::Buffer& caption, const Bbx::Buffer& data, const Bbx::Stamp& stamp, const Bbx::Identifier id)
{
    if (fatalError.load() || !referenceAdded.load())
        return false;

    std::shared_ptr<WriterTask> task(new WriterTask(id, stamp, Bbx::RecordType::IncomingPackage, caption, data));
    return pushTask(task);
}

bool WriterImpl::pushOutboxPackage(const Bbx::Buffer& caption, const Bbx::Buffer& data, const Bbx::Stamp& stamp, const Bbx::Identifier id)
{
    if (fatalError.load() || !referenceAdded.load())
        return false;

    std::shared_ptr<WriterTask> task(new WriterTask(id, stamp, Bbx::RecordType::OutboxPackage, caption, data));
    return pushTask(task);
}

bool WriterImpl::isDead() const
{
    return fatalError.load();
}

std::string WriterImpl::getErrorMessage() const
{
    return errorMessage;
}

bool WriterImpl::processReference(const WriterTask& task)
{
    ASSERT(task.type == RecordType::Reference);
    
    if (!alive() || (!filewriter && !createFileWriter(task.stamp)))
    {
        storeError("Не удалось создать файл для записи опорных данных");
        return false;
    }

    RecordOut referenceRecord(task);
    if (filewriter->writeRecord(referenceRecord))
    {
        if (filewriter->timeToCloseTheFile(task.stamp))
        {
            /* Единственный момент, когда возможно создание следующего файла
               черного ящика - после записи опорной записи в предыдущий,
               если его пора закрывать (по возрасту или размеру) */
            deleteFileWriter();
            createFileWriter(task.stamp);
            RecordOut nextReferenceRecord(task);
            if (filewriter->writeRecord(nextReferenceRecord))
            {
                return true;
            }
            else
            {
                storeError("Не удалось инициализировать новый файл копией опорных данных");
                return false;
            }
        }
        else
        {
            return true;
        }
    }
    else
    {
        storeError("Не удалось записать опорные данные в файл");
        return false;
    }
}

bool WriterImpl::processNonReferenceRecord(const WriterTask& task)
{
    ASSERT(task.type != RecordType::Reference);

    /* В том случае, когда файл еще не создан и не занесена опорная запись, 
       запись инкрементных данных невозможна */
    if (!alive() || !filewriter)
    {
        storeError("Невозможно записать данные, пока не создан файл с опорными данными");
        return false;
    }

    RecordOut dataRecord(task);
    if (filewriter->writeRecord(dataRecord))
        return true;
    else
    {
        storeError("Ошибка записи данных в файл");
        return false;
    }
}

void WriterImpl::storeError(std::string errorText)
{
    errorMessage = errorText;
}

void WriterImpl::flush()
{
    if (work.joinable() && !flushRequest.load())
    {
        boost::unique_lock<boost::mutex> lock(flushMutex);
        flushRequest.store(true);
        while (flushRequest.load())
            flushCondition.timed_wait(lock, bt::milliseconds(100u));
    }
}

void WriterImpl::deleteFileWriter()
{
    delete filewriter;
    filewriter = nullptr;
}

unsigned WriterImpl::getPageSize() const
{
    /* Использование публичного метода возможно из любой нити */
    boost::mutex::scoped_lock lock(fileLock);
    return pageSize;
}

void WriterImpl::setPageSize(unsigned page_size)
{
    /* Использование публичного метода возможно из любой нити */
    boost::mutex::scoped_lock lock(fileLock);

    pageSize = std::max(std::min(page_size, c_MaximumPageSize), c_MinimumPageSize);
}

void WriterImpl::setRecomendedFileSize(unsigned file_size)
{
    /* Использование публичного метода возможно из любой нити */
    boost::mutex::scoped_lock lock(fileLock);

    FileHeader temp_header;
    unsigned h = temp_header.getHeaderSize(); // размер заголовка
    unsigned f = std::max(file_size, h + pageSize); // 
    unsigned n = (std::min(f, c_MaximumFileSize) - h) / pageSize; // желаемых страниц
    recomendedFileSize = h + n * pageSize;
}

bool WriterImpl::setDiskLimit(const char * disk_size)
{
    char * endptr = nullptr;
    LONGLONG len = strtoll(disk_size, &endptr, 10);
    if (LLONG_MAX == len || 0 >= len )
        return false;
    // поиск первой буквы "размера"
    std::string razm(endptr);
    size_t pos = razm.find_first_of(".PTGMK");
    char select = (std::string::npos==pos)? 0 : razm[pos];

    switch( select )
    {
    // дробные значения не допускаются
    case '.':
        return false;
    // множитель может отсутствовать
    case 0:
        break;
    //
    case 'P':   
        len *= c_KB; 
        if ( len>c_MaximumDiskSize ) 
            return false;
        // специально без break! - последовательное умножение на 1024 с проверкой
    case 'T':   
        len *= c_KB;
        if ( len>c_MaximumDiskSize )
            return false;
        // специально без break! - последовательное умножение на 1024 с проверкой
    case 'G':   
        len *= c_KB;
        if ( len>c_MaximumDiskSize )
            return false;
        // специально без break! - последовательное умножение на 1024 с проверкой
    case 'M':   
        len *= c_KB;
        if ( len>c_MaximumDiskSize )
            return false;
        // специально без break! - последовательное умножение на 1024 с проверкой
    case 'K':   
        len *= c_KB;
        if ( len>c_MaximumDiskSize )
            return false;
        break;
    default:
        return false;
    }
    return setDiskLimit(len);
}

bool WriterImpl::setDiskLimit(BBX_DISK_SIZE disk_size)
{
    // контроль размера
    if ( disk_size < recomendedFileSize*2 )
        return false;
    if ( disk_size > c_MaximumDiskSize )
        return false;

    /* Использование публичного метода возможно из любой нити */
    boost::mutex::scoped_lock lock(fileLock);
    limitDiskSize = disk_size;
    return true;
}

void WriterImpl::setLifeTime(time_t life_time)
{
    /* Использование публичного метода возможно из любой нити */
    boost::mutex::scoped_lock lock(fileLock);

    recomendedFilesAge = life_time;
}

bool WriterImpl::alive() const
{
#ifndef LINUX
    return (INVALID_HANDLE_VALUE != verificationFile);
#else
    return (-1 != verificationFile);
#endif // !LINUX
}

void WriterImpl::setTimeZone( std::string textTZ )
{
    timeZone = textTZ;        
}
