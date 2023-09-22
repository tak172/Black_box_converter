#include "stdafx.h"

#include "bbx_FileReader.h"
#include "bbx_Extension.h"

using namespace Bbx::Impl;

FileReader::FileReader()
: BaseFile(), path(), cursor(), currentPage()
{
}

void FileReader::swap(FileReader& other)
{
    BaseFile::swap(other);
    std::swap(path, other.path);
    std::swap(cursor, other.cursor);
    std::swap(currentPage, other.currentPage);
    ASSERT( !path.empty() );
}

Bbx::ReadResult FileReader::unsafeOpenFileAndReadHeader(const std::wstring& filePath)
{
    if (isOpened())
    {
        if (filePath == getFilePath())
            return Bbx::ReadResult::Success;
        close();
    }

    path = filePath;
    ASSERT( !path.empty() );

    ASSERT(!isOpened());
    if (safeOpen_ModeRead(path))
    {
        cursor = Cursor();
        currentPage = PageReader();
        if (readHeader())
        {
            if (readAndVerifyVersion())
            {
                return currentPage.read(getHandle(), *begin()) ? Bbx::ReadResult::Success : Bbx::ReadResult::PageRead;
            }
            else
            {
                // Обобщение: версия не распознана или не поддерживается. Стоит детализировать?
                return Bbx::ReadResult::VersionNotSupported;
            }
        }
        else
        {
            // Не удалось считать заголовок открытого файла
            return Bbx::ReadResult::ReadingFileHeader;
        }
    }
    else
    {
        return Bbx::ReadResult::NoDataAvailable;
    }
}

Bbx::ReadResult FileReader::tryOpenFile(const std::wstring& filePath)
{
    FileReader tmp;
    Bbx::ReadResult openFileResult = tmp.unsafeOpenFileAndReadHeader(filePath);
    if (openFileResult)
    {
        swap(tmp);
        return Bbx::ReadResult::Success;
    }
    else
        return openFileResult;
}

ReadFileInfo FileReader::getFileInfo( const std::wstring& filePath )
{
    ReadFileInfo info;
    FileReader fReader;
    if (fReader.tryOpenFile(filePath))
    {
        info.fileName  = fReader.getFilePath();
        info.startTime = fReader.startsFrom();
        info.endTime   = fReader.endsWith();
        info.fileSize  = fReader.readFileSize();
    }
    return info;
}

class PageContainsRef
{
public:
    explicit PageContainsRef(const FileId& handle) : pr(), file(handle) {}
    bool operator()(const FileAddress& addr)
    {
        if (!pr.read(file, addr))
        {
            return false;
        }
        return pr.containsReferenceBeginningParts();
    }
private:
    PageReader pr;
	FileId file;
};

class PageContainsAnyRecordStart
{
public:
    explicit PageContainsAnyRecordStart(const FileId& handle) : pr(), file(handle) {}
    bool operator()(const FileAddress& addr)
    {
        if (!pr.read(file, addr))
        {
            return false;
        }
        return pr.containsAnyBeginningParts();
    }
private:
    PageReader pr;
	FileId file;
};

FileReader::page_iterator& getClosest(const FileReader::page_iterator& target, FileReader::page_iterator& first, FileReader::page_iterator& second)
{
    return std::distance(first, target) < std::distance(target, second) ? first : second;
}

bool FileReader::readPage(reverse_page_iterator revPageIt)
{
    return readPage(revPageIt.base() - 1);
}

bool FileReader::readPage(page_iterator pageIt)
{
    if (currentPage.read(getHandle(), *pageIt))
    {
        cursor.page = std::distance(pageIt, begin());
        return true;
    }
    else
        return false;
}

void FileReader::setPage(const PageReader& pr, reverse_page_iterator pageIt)
{
    setPage(pr, pageIt.base() - 1);
}

void FileReader::setPage(const PageReader& pr, page_iterator pageIt)
{
    currentPage = pr;
    cursor.page = std::distance(pageIt, begin());
}

Bbx::ReadResult FileReader::setPart(PageReader& pr, size_t partIndex)
{
    ASSERT(pr.getPartsNumber() > partIndex);
    if (pr[partIndex].header.containsBeginning())
    {
        cursor.part = partIndex;
        return ReadResult::Success;
    }
    else
        return ReadResult::NotContainsBeginning;
}

Bbx::ReadResult FileReader::setPartWithMoreThanTimeCheck(PageReader& pr, size_t partIndex, const Bbx::Stamp& stamp, bool normalSequence)
{
    if (pr[partIndex].getStamp() >= stamp)
    {
        if (normalSequence)
            return setPart(pr, partIndex);
        else
            return Bbx::ReadResult::NormalSequenceFound;
    }
    else
    {
        if (normalSequence)
            return Bbx::ReadResult::TimeSequenceViolation;
        else
            return setPart(pr, partIndex);
    }
}

Bbx::ReadResult FileReader::setPartWithLessThanTimeCheck(PageReader& pr, size_t partIndex, const Bbx::Stamp& stamp, bool normalSequence)
{
    if (pr[partIndex].getStamp() <= stamp)
    {
        if (normalSequence)
            return setPart(pr, partIndex);
        else
            return ReadResult::NormalSequenceFound;
    }
    else
    {
        if (normalSequence)
            return ReadResult::TimeSequenceViolation;
        else
            return setPart(pr, partIndex);
    }
}

Bbx::ReadResult FileReader::setFollowingPage(const Bbx::Stamp& oldStamp, bool normalSequence)
{
    if (!isOpened())
        return ReadResult::NoFileOpened;

    /* Так как необходимо найти страницу с началом записи, следующую за текущей, 
       надо знать текущий размер файла */
    readHeader();

    PageReader pr;
    size_t newPageIndex = 0;
    page_iterator theEnd = end();
    for (page_iterator pageIt = begin() + (int)cursor.page + 1; pageIt != theEnd; ++pageIt)
    {
        if (!pr.read(getHandle(), *pageIt))
        {
            /* Чтение заголовка страницы, которая должна быть доступна, провалилось */
            return Bbx::ReadResult::ReadingPageHeader;
        }

        if (pr.getFirstRecord(newPageIndex))
        {
            Bbx::ReadResult partSetResult = setPartWithMoreThanTimeCheck(pr, newPageIndex, oldStamp, normalSequence);
            if (partSetResult)
                setPage(pr, pageIt);
            return partSetResult;
        }
    }

    /* Был вызов установки следующей страницы, однако страницы с началом записи найдено не было */
    return ReadResult::PageNotFound;
}

Bbx::ReadResult FileReader::setPrecedePage(const Bbx::Stamp& oldStamp, bool normalSequence)
{
    if (!isOpened())
        return Bbx::ReadResult::NoFileOpened;

    PageReader pr;
    size_t newPageIndex = 0;

    for (reverse_page_iterator revPageIt = reverse_page_iterator(begin() + (int)cursor.page); revPageIt != rend(); ++revPageIt)
    {
        if (!pr.read(getHandle(), *(revPageIt.base() - 1)))
        {
            /* Чтение заголовка страницы, которая должна быть доступна, провалилось */
            return Bbx::ReadResult::ReadingPageHeader;
        }

        if (pr.getLastRecord(newPageIndex))
        {
            Bbx::ReadResult partSetResult = setPartWithLessThanTimeCheck(pr, newPageIndex, oldStamp, normalSequence);
            if (partSetResult)
                setPage(pr, revPageIt);
            return partSetResult;
        }
    }

    /* Был вызов установки следующей страницы, однако страницы с началом записи найдено не было */
    return Bbx::ReadResult::PageNotFound;
}

class PageStampsLesser
{
public:
	explicit PageStampsLesser(const FileId& handle) : pr(), file(handle) {}
    bool operator()(const FileAddress& addr, const Bbx::Stamp& stamp)
    {
        pr.read(file, addr);
        return pr < stamp;
    }
    bool operator()(const Bbx::Stamp& stamp, const FileAddress& addr)
    {
        pr.read(file, addr);
        return stamp < pr;
    }
    bool operator()(const FileAddress& addr1, const FileAddress& addr2)
    {
        return addr1 < addr2;
    }
private:
    PageReader pr;
	FileId file;
};

bool FileReader::isReferenceSearchBetter(const Bbx::Stamp& desiredStamp) const
{
    ASSERT(isOpened());

    /* Обнаружение ближайшей к временному штампу страницы */
    PageStampsLesser lowerComp(getHandle());
    page_iterator foundPageIter = std::lower_bound(begin(), end(), desiredStamp, lowerComp);
    if (end() == foundPageIter)
        --foundPageIter;

    FileAddress desired = *foundPageIter;
    FileAddress current = begin()[(int)cursor.page];
    if ( desired < current )
        std::swap( desired, current ); // далее только увеличение от current к desired 

    // Замерим количество данных при движении от текущей позиции к желаемой.
    // Инкрементные данные суммируются, а референсные обновляют известный максимум.
    // Если сумма инкрементов достигнет удвоенного референса, то выгоднее начать с референса
    // (поскольку инкрементные данные содержат двойное состояние - старое+новое).
    // P.S. Суммируем только промежуточные страницы.
    bool suggestReference = false;
    size_t distanceInPages = (desired.offset - current.offset) / current.size;
    if ( distanceInPages <= 1 )
        suggestReference = false; // на соседних страницах - сразу имеем ответ
    else
    {
        size_t knownReferenceSize = 0;
        size_t totalIncrementSize = 0;

        page_iterator inter( current.offset, current.size );
        for( ++inter; *inter < desired && !suggestReference; ++inter )
        {
            PageReader pr;
            pr.read( getHandle(), *inter );
            for( size_t i=0; i < pr.getPartsNumber(); ++i )
            {
                auto& part = pr[i];
                size_t sz = part.header.getSize();
                if ( part.header.isReference() )
                    knownReferenceSize = (std::max)(knownReferenceSize, sz);
                else
                    totalIncrementSize += sz;
            }
            suggestReference = ( totalIncrementSize > 2*knownReferenceSize );
        }
    }

    return suggestReference;
}

template <typename T>
FileReader::page_iterator FileReader::back_find_if(page_iterator itFrom, page_iterator itTo, T comparer)
{
    for (; itFrom != itTo; --itFrom)
    {
        if (comparer(*itFrom))
            break;
    }
    return itFrom;
}

bool FileReader::rewindToStamp(const Bbx::Stamp& where)
{
    ASSERT(isOpened());
    /* Обнаружение ближайшей к временному штампу страницы */
    PageStampsLesser lowerComp(getHandle());
    page_iterator itBegin = begin();
    page_iterator itEnd = end();
    ASSERT(itEnd != itBegin);
    page_iterator itBoundPage = std::lower_bound(itBegin, itEnd, where, lowerComp);
    if (end() == itBoundPage)
        --itBoundPage;

    /* Поиск ближайших страниц с опорными записями в обоих направлениях */
    PageContainsRef comparer(getHandle());
    page_iterator itForwardResult = std::find_if(itBoundPage, itEnd, comparer);
    page_iterator itBackwardResult = 
        itBegin == itBoundPage ? itBegin : back_find_if(itBoundPage - 1, itBegin, comparer);

    /* Грубый выбор ближайшей страницы с опорной записью */
    page_iterator itChoosen = (itEnd == itForwardResult) ? itBackwardResult : getClosest(itBoundPage, itForwardResult, itBackwardResult);
    if (!readPage(itChoosen))
        return false;

    setPart(currentPage, currentPage.getClosestReferencePartIndex(where));
    return true;
}

// Лютый копипаст
bool FileReader::rewindToStampAnyRecordType(const Bbx::Stamp& where)
{
    ASSERT(isOpened());
        /* Обнаружение ближайшей к временному штампу страницы */
    PageStampsLesser lowerComp(getHandle());
    page_iterator itBegin = begin();
    page_iterator itEnd = end();
    ASSERT(itEnd != itBegin);
    page_iterator itBoundPage = std::lower_bound(itBegin, itEnd, where, lowerComp);
    if (end() == itBoundPage)
        --itBoundPage;

    /* Поиск ближайших страниц с любыми записями в обоих направлениях */
    PageContainsAnyRecordStart comparer(getHandle());
    //page_iterator itForwardResult = std::find_if(itBoundPage, itEnd, comparer);
    page_iterator itBackwardResult = 
        itBegin == itBoundPage ? itBegin : back_find_if(itBoundPage - 1, itBegin, comparer);

    /* Грубый выбор ближайшей страницы с любой записью */
    //page_iterator itChoosen = (itEnd == itForwardResult) ? itBackwardResult : getClosest(itBoundPage, itForwardResult, itBackwardResult);
    if (!readPage(itBackwardResult))
        return false;

    setPart(currentPage, currentPage.getClosestAnyRecordTypePartIndex(where));
    return true;
}

Bbx::ReadResult FileReader::rewindToCursor(const FileReader::Cursor& targetCursor)
{
    ASSERT(isOpened());

    if (targetCursor.page == cursor.page && currentPage.getPartsNumber() > targetCursor.part)
    {
        return setPart(currentPage, targetCursor.part);
    }
    else
    {
        page_iterator itPage = begin() + (int)targetCursor.page;
        PageReader pr;
        if (pr.read(getHandle(), *itPage))
        {
            setPage(pr, itPage);
            ASSERT(this->cursor.page == targetCursor.page);

            return setPart(pr, targetCursor.part);
        }
        else 
        {
            // Не удалось считать заголовок страницы
            return ReadResult::ReadingPageHeader;
        }
    }
}

bool FileReader::rewindToExtreme(bool toStart)
{
    ASSERT(isOpened());
    if (!fileSizeIsEnoughToRead() || begin() == end())
        return false;

    if (toStart)
    {
        if (!readPage(begin()) || !currentPage.getPartsNumber())
            return false;
        return setPart(currentPage, 0)? true : false;
    }
    else
    {
        PageReader pr;
        size_t recIndex = 0;
        for (reverse_page_iterator rPageIt = rbegin(), rEnd = rend(); rPageIt != rEnd; ++rPageIt)
        {
            if (pr.read(getHandle(), *(rPageIt.base() - 1)) && pr.getLastRecord(recIndex))
            {
                setPage(pr, rPageIt);
                return setPart(currentPage, recIndex)? true : false;
            }
        }
        return false;
    }
}

void FileReader::update()
{
    ASSERT(isOpened());
    readHeader();
    currentPage.update(getHandle());
}

bool FileReader::readHeader()
{
    ASSERT(isOpened());
    SharedSection headerLocked(getHandle(), 0, sizeof(FileHeader));
    return (headerLocked.read(Bbx::Buffer::create(header)));
}

bool FileReader::readAndVerifyVersion()
{
    std::vector<char> extensionVec(header.getExtensionSize());
    Bbx::Buffer extensionBuffer(extensionVec);
    if (readExtensionZone(extensionBuffer))
    {
        Extension extension;
        if ( extension.load(extensionBuffer) )
        {
            return extension.getVersion().isSupported();
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool FileReader::readExtensionZone(Bbx::Buffer& buf) const
{
    ASSERT(isOpened());
    unsigned extSize = header.getExtensionSize();
    if (extSize && buf.size >= extSize && buf.data_ptr)
    {
        SharedSection extLocked(getHandle(), sizeof(FileHeader), extSize);
        return (extLocked.read(buf));
    }
    else
        return false;
}

size_t FileReader::readFileSize() const
{
    ASSERT(isOpened());
#ifndef LINUX
	DWORD hiPart;
	return GetFileSize(getHandle(), &hiPart);
#else
    off_t fsize = lseek( getHandle(), 0, SEEK_END );
    ASSERT( fsize != -1 );
    return size_t(fsize);
#endif // !LINUX
}

std::string FileReader::getTimeZone() const
{
    if ( isOpened() )
    {
        std::vector<char> extensionVec(header.getExtensionSize());
        Bbx::Buffer extensionBuffer(extensionVec);
        if (readExtensionZone(extensionBuffer))
        {
            Extension extension;
            if ( extension.load(extensionBuffer) )
                return extension.getTimeZone();
        }
    }
    return std::string(); // чтение безуспешно
}

bool FileReader::fileSizeIsEnoughToRead() const
{
    ASSERT(isOpened());
    /* Чтобы файл можно было читать, в нём должен находиться полноценный заголовок */
    return readFileSize() > sizeof(FileHeader);
}

FileReader::page_iterator FileReader::begin() const
{
    ASSERT(isOpened());
    return page_iterator(header.getHeaderSize(), header.getPageSize());
}

FileReader::page_iterator FileReader::end() const
{
    ASSERT(isOpened());
    return begin() + (int)getPagesCount();
}

FileReader::reverse_page_iterator FileReader::rbegin() const
{
    return reverse_page_iterator(end());
}

FileReader::reverse_page_iterator FileReader::rend() const
{
    return reverse_page_iterator(begin());
}

size_t FileReader::getPagesCount() const
{
    ASSERT(isOpened());
    size_t pagesDataSize = readFileSize() - header.getHeaderSize();
    return (pagesDataSize + header.getPageSize() -1 )/ header.getPageSize();
}

Bbx::Stamp FileReader::startsFrom() const
{
    ASSERT(isOpened());
    return header.getFirstRecordTime();
}

Bbx::Stamp FileReader::endsWith() const
{
    ASSERT(isOpened());
    return header.getLastRecordTime();
}

Bbx::Stamp FileReader::currentCursorStamp() const
{
    ASSERT(currentPage.getPartsNumber() > cursor.part);
    return currentPage[cursor.part].getStamp();
}

Bbx::Identifier FileReader::currentCursorIdentifier() const
{
    ASSERT(currentPage.getPartsNumber() > cursor.part);
    Bbx::Identifier identifier;
    identifier.deserialize(currentPage[cursor.part].getIdentifer());
    return identifier;
}

Bbx::RecordType FileReader::currentCursorType() const
{
    return currentPage[cursor.part].header.getType();
}

bool FileReader::hasMoreRecords(bool directionForward) const
{
    ASSERT(isOpened());
    if (directionForward)
    {
        /* Быстрая проверка */
        if (currentPage.containsBeginningPartsAfter(cursor.part))
            return true;
        /* Проверка всех последующих страниц только при полной текущей странице.
         * Усеченная страница может быть только
         * - последняя и тогда далее записей нет,
         * - неполностью прочитанная текущая и тогда ее надо дочитать
        */
        if ( !comesToTruncated() )
        {
            PageReader pr;
            for (page_iterator it = begin() + (int)cursor.page + 1; it != end(); ++it)
            {
                if (pr.read(getHandle(), *it) && pr.containsAnyBeginningParts())
                    return true;
            }
        }
        return false;
    }
    else
    {
        /* Быстрая проверка */
        if (currentPage.containsBeginningPartsBefore(cursor.part))
            return true;

        /* Проверка всех страниц до этой */
        PageReader pr;
        for (page_iterator it = begin(), _end = begin() + (int)cursor.page; it != _end; ++it)
        {
            if (pr.read(getHandle(), *it) && pr.containsAnyBeginningParts())
                return true;
        }
        return false;
    }
}

Bbx::ReadResult FileReader::moveToNextRecord(bool directionForward, bool normalSequence)
{
    if (!isOpened())
        return ReadResult::NoFileOpened;

    if (!currentPage.getPartsNumber())
        return ReadResult::BadPageHeader;
    if (directionForward)
    {
        if ( comesToTruncated() )
            return Bbx::ReadResult::NoDataAvailable; // конец неполной страницы, а такая только в конце файла
        else if (currentPage.containsBeginningPartsAfter(cursor.part))
            return setPartWithMoreThanTimeCheck(currentPage, cursor.part + 1, currentCursorStamp(), normalSequence);
        else
            return setFollowingPage(currentCursorStamp(), normalSequence);
    }
    else
    {
        if (currentPage.containsBeginningPartsBefore(cursor.part))
            return setPartWithLessThanTimeCheck(currentPage, cursor.part - 1, currentCursorStamp(), normalSequence);
        else
            return setPrecedePage(currentCursorStamp(), normalSequence);
    }
}

bool FileReader::readCurrentRecord( RecordIn& record)
{
    if (!isOpened() || !currentPage.update(getHandle()))
        return false;

    if (cursor.part >= currentPage.getPartsNumber())
        return false;

    const PartHeaderTableRecord& startPart = currentPage[cursor.part];
    ASSERT(startPart.header.containsBeginning());
    if (!record.readPart(getHandle(), startPart.getPartAddress()))
        return false;

    record.setStamp(startPart.getStamp());
    if ( record.readed() )
        return true;

    PageReader pr;
    page_iterator theEnd = end();
    for (page_iterator it = begin() + (int)cursor.page + 1; it != theEnd; ++it)
    {
        if (!pr.read(getHandle(), *it) || !pr.getPartsNumber())
            return false;

        const PartHeaderTableRecord& contPart = pr[0];
        ASSERT(contPart.header.isContinuationPart());
        if ( !record.readPart( getHandle(), contPart.getPartAddress() ) )
            return false;
        if ( record.readed() )
            return true;
        ASSERT(!contPart.header.containsEnd());
    }
    return false;
}

bool FileReader::readCurrentRecord(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& before, Bbx::char_vec& after)
{
    RecordIn rec(stamp, caption, before, after);
    return readCurrentRecord( rec );
}

bool FileReader::readCurrentRecord(Bbx::Stamp& stamp, Bbx::char_vec& caption, Bbx::char_vec& data)
{
    RecordIn rec(stamp, caption, data);
    return readCurrentRecord( rec );
}

FileReader::page_iterator& FileReader::page_iterator::operator +=(difference_type n)
{
    BBX_SIZE __offset = static_cast<BBX_SIZE>(addr.offset) + static_cast<BBX_SIZE>(addr.size * n);
    addr.offset = __offset;
    return *this;
}

FileReader::page_iterator& FileReader::page_iterator::operator --()
{
    ASSERT(addr.offset >= addr.size);
    addr.offset -= addr.size;
    return *this;
}

FileReader::page_iterator FileReader::page_iterator::operator --(int)
{
    page_iterator tmp(*this);
    operator--();
    return tmp;
}

FileReader::page_iterator::value_type FileReader::page_iterator::operator[](FileReader::page_iterator::difference_type offset) const
{
    return *(*this + offset);
}

FileReader::page_iterator::difference_type FileReader::page_iterator::operator -(const FileReader::page_iterator& other) const
{
    ASSERT(addr.size == other.addr.size);
    return (std::max(addr.offset, other.addr.offset) - std::min(addr.offset, other.addr.offset)) / addr.size;
}
