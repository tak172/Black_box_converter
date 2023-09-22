#include "stdafx.h"
#ifdef LINUX
#include <fcntl.h>
#include <boost/filesystem/operations.hpp>
#endif
#include "bbx_Requirements.h"
#include "bbx_Reader.h"
#include "bbx_FileChain.h"

using namespace Bbx::Impl;

ReaderImpl::ReaderImpl(const Bbx::Location& locator)
:location(locator), forward(true), fileReader(), 
result(Bbx::ReadResult::NoDataAvailable), mutex()
{
}

Bbx::ReadResult& ReaderImpl::saveResult(Bbx::ReadResult res)
{
    result = res;
    return result;
}

Bbx::ReadResult ReaderImpl::lastResult() const
{
    boost::mutex::scoped_lock lock(mutex);
    return result;
}

void ReaderImpl::setDirection(bool goForward)
{
    boost::mutex::scoped_lock lock(mutex);
	forward = goForward;
}

bool ReaderImpl::getDirection() const
{
    return forward;
}

ReaderImpl::~ReaderImpl(void)
{
    boost::mutex::scoped_lock lock(mutex);
}

Bbx::ReadResult ReaderImpl::rewind(const Bbx::Stamp& stamp)
{
    boost::mutex::scoped_lock lock(mutex);
    std::wstring targetFileName = selectFileBy(stamp);
    if ( targetFileName.empty() )
        return saveResult(Bbx::ReadResult::NoDataAvailable);

    if (fileReader.isOpened() && fileReader.getFilePath() == targetFileName)
    {
        /* Если файл уже открыт, выполняем обновление его заголовков из файла */
        fileReader.update();
    }
    else
    {
        resetEoD(); 
        Bbx::ReadResult openFileResult = fileReader.tryOpenFile(targetFileName);
        /* Нужный файл не открыт, открываем */
        if (!openFileResult)
        {
            /* Найденный файл открыть не удалось по какой-либо причине, возвращаем код ошибки */
            return saveResult(openFileResult);
        }
    }

    if (fileReader.rewindToStamp(stamp))
        return saveResult(fileReader.currentCursorStamp() == stamp 
        ? Bbx::ReadResult::Success : Bbx::ReadResult::FoundApproximateValue);
    else
        return saveResult(Bbx::ReadResult::NoDataAvailable);
}

Bbx::ReadResult ReaderImpl::rewindToAny(const Bbx::Stamp& stamp)
{
    boost::mutex::scoped_lock lock(mutex);
    std::wstring targetFileName = selectFileBy(stamp);
    if ( targetFileName.empty() )
        return saveResult(Bbx::ReadResult::NoDataAvailable);

    if (fileReader.isOpened() && fileReader.getFilePath() == targetFileName)
    {
        /* Если файл уже открыт, выполняем обновление его заголовков из файла */
        fileReader.update();
    }
    else
    {
        /* Нужный файл не открыт, открываем */
        resetEoD(); 
        if (!fileReader.tryOpenFile(targetFileName))
        {
            /* TODO: Найденный файл открыть не удалось, реализовать обработку */
            return saveResult(Bbx::ReadResult::NoDataAvailable);
        }
    }

    if (fileReader.rewindToStampAnyRecordType(stamp))
        return saveResult(fileReader.currentCursorStamp() == stamp 
        ? Bbx::ReadResult::Success : Bbx::ReadResult::FoundApproximateValue);
    else
        return saveResult(Bbx::ReadResult::NoDataAvailable);
}

Bbx::Stamp ReaderImpl::getCurrentStamp() const
{
    boost::mutex::scoped_lock lock(mutex);
    return fileReader.currentCursorStamp();
}

Bbx::Identifier ReaderImpl::getCurrentIdentifier() const
{
    boost::mutex::scoped_lock lock(mutex);
    return fileReader.currentCursorIdentifier();
}

Bbx::RecordType ReaderImpl::getCurrentType() const
{
    boost::mutex::scoped_lock lock(mutex);
    return fileReader.currentCursorType();
}

bool ReaderImpl::resolveToSearchReference(const Bbx::Stamp& desiredStamp) const
{
    boost::mutex::scoped_lock lock(mutex);

    /* В случае запуска метода из неинициализированного экземпляра, обязательно надо искать опорную запись */
    if (!fileReader.isOpened())
        return true;

    /* В случае, когда в файле искомой записи нет, советуем искать опорную запись */
    if (fileReader.startsFrom() > desiredStamp || fileReader.endsWith() < desiredStamp)
        return true;

    return fileReader.isReferenceSearchBetter(desiredStamp);
}

void ReaderImpl::update()
{
    boost::mutex::scoped_lock lock(mutex);
    if (fileReader.isOpened())
        fileReader.update();
}

Bbx::ReadResult ReaderImpl::moveNext(bool normalSequence)
{
    if (!fileReader.isOpened())
        return Bbx::ReadResult::NoFileOpened;

    auto fileChain = location.getCPtrChain();
    if (forward && fileReader.comesToTruncated() )
        fileReader.update();

    if (fileReader.hasMoreRecords(forward))  
    {
        /* Перемещение по файлу */
        return fileReader.moveToNextRecord(forward, normalSequence);
    }
    else
    {
        std::wstring nextFile = selectNextFile(*fileChain,sizeof(FileHeader));
        if ( nextFile.empty() )
            return Bbx::ReadResult::NoDataAvailable;
        if (knownEoD(*fileChain,sizeof(FileHeader), normalSequence))
            return Bbx::ReadResult::NoDataAvailable;
        else
        {
            /* Перемещение между файлами */
            Bbx::ReadResult res = nextFileMove(nextFile, normalSequence);
            if (Bbx::ReadResult::NoDataAvailable == res )
                setEoD(*fileChain,sizeof(FileHeader), normalSequence);
            return res;
        }
    }
}

bool ReaderImpl::knownEoD( const Bbx::FileChain& fileChain, size_t minFileSize, bool normalSequence ) const
{
    return forward == eod_forward &&
           normalSequence == eod_normalSequence &&
           !fileChain.empty() &&
           eod_front == fileChain.getEarliestFile() &&
           eod_back  == fileChain.getLatestFile(minFileSize);
}

void ReaderImpl::setEoD( const Bbx::FileChain& fileChain, size_t minFileSize, bool normalSequence )
{
    ASSERT( !fileChain.empty() );
    eod_front = fileChain.getEarliestFile();
    eod_back  = fileChain.getLatestFile(minFileSize);
    eod_forward = forward;
    eod_normalSequence = normalSequence;
}

Bbx::ReadResult ReaderImpl::nextFileMove( const std::wstring& nextFile, bool normalSequence )
{
    ASSERT( !nextFile.empty() );
    Stamp oldStamp = fileReader.currentCursorStamp();
    FileReader tmpReader;
    resetEoD(); 
    if (tmpReader.tryOpenFile(nextFile) && tmpReader.rewindToExtreme(forward))
    {
        if (tmpReader.currentCursorStamp() == oldStamp)
        {
            if (normalSequence)
            {   
                /* Новый файл успешно открыт и новая опорная запись совпадает по времени со старой 
                   Нужно добавить проверку других параметров, т.к. при совпадении времени
                   опорных записей файлов после разрыва мы потеряем начало новой сессии */
                fileReader.swap(tmpReader);
                return Bbx::ReadResult::Success;
            }
            else
            {
                /* Нормальный переход возможен, когда пользователь использовал силовой метод */
                return Bbx::ReadResult::NormalSequenceFound;
            }
        }
        else
        {
            if (normalSequence)
            {
                /* Обнаружен разрыв черного ящика, нормальная последовательность перехода */
                return Bbx::ReadResult::NewSession;
            }
            else
            {
                /* Пользователь сказал переходить к следующему файлу через разрыв ЧЯ */
                fileReader.swap(tmpReader);
                return Bbx::ReadResult::Success;
            }
        }
    }
    else
    {
        /* Открыть выбранный следующим файл не удалось (или не удалось осуществить перемотку) */
        return Bbx::ReadResult::ErrorOpeningFile;
    }
}

Bbx::ReadResult ReaderImpl::next()
{
    boost::mutex::scoped_lock lock(mutex);
    return saveResult(moveNext(true));
}

Bbx::ReadResult ReaderImpl::forceNext()
{
    boost::mutex::scoped_lock lock(mutex);
    return saveResult(moveNext(false));
}

Bbx::ReadResult ReaderImpl::readReference(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    boost::mutex::scoped_lock lock(mutex);
    if (!fileReader.isOpened())
        return saveResult(Bbx::ReadResult::NoFileOpened);

    if (RecordType::Reference != fileReader.currentCursorType())
        return saveResult(Bbx::ReadResult::WrongRecordType);
    
    return readCurrentRecordImpl(stamp, caption, data);
}

Bbx::ReadResult ReaderImpl::readIncrementOriented(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    boost::mutex::scoped_lock lock(mutex);
    if (!fileReader.isOpened())
        return saveResult(Bbx::ReadResult::NoFileOpened);

    if (RecordType::Increment != fileReader.currentCursorType())
        return saveResult(Bbx::ReadResult::WrongRecordType);

    return readIncrementOrientedImpl(stamp, caption, data);
}

Bbx::ReadResult ReaderImpl::readPackage(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    boost::mutex::scoped_lock lock(mutex);
    if (!fileReader.isOpened())
        return saveResult(Bbx::ReadResult::NoFileOpened);

    auto currentType = fileReader.currentCursorType();
    if (RecordType::IncomingPackage != currentType && RecordType::OutboxPackage != currentType)
        return saveResult(Bbx::ReadResult::WrongRecordType);

    return readCurrentRecordImpl(stamp, caption, data);
}

Bbx::ReadResult ReaderImpl::readAnyRecord(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    boost::mutex::scoped_lock lock(mutex);
    if (!fileReader.isOpened())
        return saveResult(Bbx::ReadResult::NoFileOpened);

    if (fileReader.currentCursorType() == Bbx::RecordType::Increment)
        return readIncrementOrientedImpl(stamp, caption, data);
    else
        return readCurrentRecordImpl(stamp, caption, data);
}

std::pair<Bbx::Stamp,Bbx::Stamp> ReaderImpl::getAvailableTimeInterval() const
{
    boost::mutex::scoped_lock lock(mutex);
    return getAvailableTimeInterval(location);
}

std::pair<Bbx::Stamp,Bbx::Stamp> ReaderImpl::getAvailableTimeInterval(const Bbx::Location& location)
{
    const time_t c_InvalidTimeValue = 0;

    Bbx::Stamp one = Bbx::Stamp(c_InvalidTimeValue);
    Bbx::Stamp two = Bbx::Stamp(c_InvalidTimeValue);

    auto fileChain = location.getCPtrChain();
    std::wstring earliestFile = fileChain->getEarliestFile();
    std::wstring latestFile = fileChain->getLatestFile( sizeof( FileHeader ) );
    if ( !earliestFile.empty() )
    {
        auto info = FileReader::getFileInfo( earliestFile );
        if ( info.isCorrect() )
            one = info.startTime;
    }
    if ( !latestFile.empty() )
    {
        auto info  = FileReader::getFileInfo( latestFile );
        if ( info.isCorrect() )
            two = info.endTime;
    }
    return std::make_pair( one, two );
}

bool ReaderImpl::existActualWriter() const
{
#ifndef LINUX
	std::wstring filePath = location.verificationFilePath();
	BBX_SIZE dw = GetFileAttributes(filePath.c_str());
	return (dw != INVALID_FILE_ATTRIBUTES);
#else
    bool res = false;
    boost::filesystem::path vp( location.verificationFilePath() );
    boost::system::error_code ec;
    if( boost::filesystem::exists( vp, ec ) )
    {
        FileId temp = open( vp.string().c_str(), O_RDONLY );
        // поставить блокировку чтения нельзя, если есть писатель
        bool mylock = SharedSection::trylock( temp, 0, 10 );
        if( !mylock )
            res = true;
        close( temp );
    }
    return res;
#endif // !LINUX
}

FileReader::Cursor ReaderImpl::getCurrentCursor() const
{
    boost::mutex::scoped_lock lock(mutex);
    ASSERT(fileReader.isOpened());

    return fileReader.getCursor();
}

const std::wstring& ReaderImpl::getCurrentFilePath() const
{
    boost::mutex::scoped_lock lock(mutex);
    ASSERT(fileReader.isOpened());

    return fileReader.getFilePath();
}

Bbx::ReadResult ReaderImpl::rewindToCursor(const std::wstring& filePath, const FileReader::Cursor& cursor)
{
    boost::mutex::scoped_lock lock(mutex);

    if (fileReader.isOpened() && fileReader.getFilePath() == filePath)
    {
        fileReader.update();
    }
    else 
    {
        resetEoD(); 
        if (!fileReader.tryOpenFile(filePath))
        {
            return ReadResult::NoDataAvailable;
        }
    }

    if (auto state = fileReader.rewindToCursor(cursor))
    {
        return ReadResult::Success;
    }
    else 
    {
        return state;
    }
}

const Bbx::Location& ReaderImpl::getLocation() const
{
    return location;
}

Bbx::ReadResult ReaderImpl::readIncrementOrientedImpl(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    /* В зависимости от направления чтения возвращаемые данные брать из правильной части */
    Bbx::char_vec trash;
    Bbx::char_vec& alt_before = forward ? trash : data;
    Bbx::char_vec& alt_after = forward ? data : trash;

    return saveResult(fileReader.readCurrentRecord(stamp, caption, alt_before, alt_after)
        ? Bbx::ReadResult::Success : Bbx::ReadResult::NoDataAvailable); 
}

Bbx::ReadResult ReaderImpl::readCurrentRecordImpl(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    return saveResult(fileReader.readCurrentRecord(stamp, caption, data)
        ? Bbx::ReadResult::Success : Bbx::ReadResult::NoDataAvailable);
}

/**
 *	Выбрать ближайшие к временному штампу файлы без чтения их заголовков (по названиям)
 */
std::vector<std::wstring> ReaderImpl::selectFilesCloserTo(const Bbx::Location& location, const Bbx::Stamp& stamp)
{
    std::wstring firstFN = location.fileName( stamp, 0 );
    // Расширяем временной запрос из-за множественных имён файлов с совпадающими штампами
    Bbx::Stamp stampMore(stamp.getTime() + 1);
    std::wstring lastFN = location.fileName( stampMore, 0 );

    std::vector<std::wstring> result;
    auto pFileChain = location.getCPtrChain();
    result = pFileChain->filesAroundRange( firstFN, lastFN, sizeof(FileHeader) );
    return result;
}

std::wstring ReaderImpl::selectNextFile( const Bbx::FileChain& fileChain, size_t minFileSize ) const
{
    return fileChain.selectNextFile( fileReader.getFilePath(), minFileSize, forward );
}

std::wstring ReaderImpl::selectFileBy(const Bbx::Stamp& stamp) const
{
    std::wstring targetFileName;
    const std::vector<std::wstring> foundFiles = selectFilesCloserTo(location, stamp);
    if ( !foundFiles.empty() )
    {
        auto comparer = [](const std::wstring& oneFile, const Bbx::Stamp& my_stamp) {
            auto pre = FileReader::getFileInfo( oneFile );
            return ( pre.startTime < my_stamp );
        };
        auto partIt = std::lower_bound(foundFiles.begin(), foundFiles.end(), stamp, comparer );
        if ( foundFiles.begin() == partIt )
        {
            targetFileName = foundFiles.front(); // выход за начало - берем начало
        }
        else if ( foundFiles.end() == partIt )
        {
            targetFileName = foundFiles.back(); // выход за конец - берем конец
        }
        else
        {
            auto some = FileReader::getFileInfo( *partIt );
            if ( some.startTime<= stamp )
                targetFileName = *partIt; // берем найденный файл
            else
                targetFileName = *prev(partIt); // или предыдущий
        }
    }

    return targetFileName;
}

std::string ReaderImpl::getTimeZone() const
{
    std::string resTZ;
    if ( fileReader.isOpened() )
    {
        resTZ = fileReader.getTimeZone();
    }
    else
    {
        auto fileChain = location.getCPtrChain();
        std::wstring latestFile = fileChain->getLatestFile( sizeof( FileHeader ) );
        if ( !latestFile.empty() )
        {
            FileReader tempFR;
            if ( tempFR.tryOpenFile(latestFile) )
                resTZ = tempFR.getTimeZone();
        }
    }
    return resTZ;
}
