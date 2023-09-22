#include "stdafx.h"
#include "bbx_Requirements.h"
#include "bbx_Record.h"
#include "bbx_Page.h"
#include "bbx_File.h"

using namespace Bbx::Impl;
namespace bt = boost::posix_time;

/** @brief Задержка между поступлением данных и их записью в файл */
bt::time_duration Page::DeviateDelay = bt::milliseconds(500);// текущее значение

PartHeader::PartHeader(bool containsBeginOfTheRecord, const Bbx::Identifier id, time_t time, Bbx::RecordType recordType, unsigned buf_size, bool containsEndOfTheRecord)
    : tag(Tag::Full), type(recordType), stamp(time), id(id.asSerializedValue()), size(buf_size)
{
    if (containsBeginOfTheRecord)
        tag = containsEndOfTheRecord ? Tag::Full : Tag::Begin;
    else
        tag = containsEndOfTheRecord ? Tag::End : Tag::Middle;
}

void Page::setDeviateDelay( bt::time_duration delayMs)
{
    const bt::time_duration Lo = bt::milliseconds(30);// нижняя граница
    const bt::time_duration Hi = bt::seconds(15);     // верхняя граница

    bt::time_duration temp = delayMs;
    if ( temp < Lo )
        temp = Lo;
    if ( temp > Hi )
        temp = Hi;
    std::swap( temp, DeviateDelay );
}

void PageWriter::init()
{
    if (!data.data_ptr)
        data.data_ptr = new char[address.size];

    header.write(reserve(sizeof(PageHeader)));

    // Заполнение зоны расширения нулями (пока нет необходимости заполнять чем-то еще)
    reserve(header.getExtensionSize()).fillWithNulls();
}

void PageWriter::createNextPage()
{
    address.offset += address.size;
    data.size = 0;
    writtenBytes = 0;
    elderRecordMoment = bt::ptime();
    header = PageHeader(header.getExtensionSize());

    init();
}

bool PageWriter::willWriteToFile(const RecordOut& record) const
{
    ASSERT(record.untouched());
    return (record.getSize() + sizeof(PartHeader) >= dataSizeRemainsToFill()
        || shouldBeFlushedNow());
}

void PageWriter::appendRecord(RecordOut& record)
{
    ASSERT(sizeof(PartHeader) < dataSizeRemainsToFill());

    /* Выделение буферов для записи заголовка и данных кусочка */
    Buffer headerBuf = reserve(sizeof(PartHeader));
    Buffer dataBuf = reserve(std::min(record.getRemainingSize(), (unsigned)dataSizeRemainsToFill()));
    
    /* Запись тела кусочка (в момент записи состояние записи меняется) */
    bool recordStarted = record.untouched();
    record.write(dataBuf);
    bool recordFinished = record.completed();

    /* Формирование заголовка кусочка */
    time_t recordTime = record.getStamp().getTime();
    new (headerBuf.data_ptr) PartHeader(recordStarted, record.getId(), recordTime, record.getType(), dataBuf.size, recordFinished);

    header.addRecordTime(recordTime);
    if ( elderRecordMoment.is_not_a_date_time() )
        elderRecordMoment = bt::microsec_clock::universal_time();

    /* Если остаточный размер кеша слишком мал, заполняем его нулями */
    if( cacheCanTakeNoMoreRecords() )
        fillRemainingSpaceWithNulls();
}

void PageWriter::processRecord(const FileId& file, RecordOut& record)
{
    while( !record.completed() )
    {
        /* Запись данных в кеш ведется пока есть место хотя бы для 1 байта полезной информации */
        if (!cacheCanTakeNoMoreRecords())
            appendRecord(record);

        /* Метод априори сбрасывает кеш на диск */
        writeCacheToFile(file);
    }
    ASSERT(record.completed());
}

Bbx::Buffer PageWriter::reserve(unsigned bytes)
{
    ASSERT(bytes <= dataSizeRemainsToFill());
    Bbx::Buffer result(end(data), bytes);
#ifdef _DEBUG
    result.fillWith(char('+'));
#endif
    data.size += bytes;
    return result;
}

bool PageWriter::cacheCanTakeNoMoreRecords() const
{
    /* Для записи еще одного куска данных необходимо убедиться, что
       у страницы хватит места для заголовка её кусочка и как минимум такого же числа байт */
    return dataSizeRemainsToFill() <= largestUnusedSpace();
}

void PageWriter::fillRemainingSpaceWithNulls()
{
    if (dataSizeRemainsToFill())
        reserve(static_cast<unsigned>(dataSizeRemainsToFill())).fillWithNulls();
}

bool PageWriter::shouldBeFlushedNow() const
{
    return cacheCanTakeNoMoreRecords() || 
          ( !elderRecordMoment.is_not_a_date_time() && bt::microsec_clock::universal_time() - elderRecordMoment >= getDeviateDelay() );
}

Bbx::Buffer PageWriter::getHeaderBuffer() const
{
    ASSERT(data.size >= sizeof(PageHeader));
    return Bbx::Buffer(begin(data), sizeof(PageHeader));
}

unsigned PageWriter::dataSizeRemainsToWrite() const
{
    return data.size - (unsigned)sizeof(PageHeader) - writtenBytes;
}

unsigned long PageWriter::dataSizeRemainsToFill() const
{
    return address.size - data.size;
}

Bbx::Buffer PageWriter::getDataBufferForWriting()
{
    return Bbx::Buffer(begin(data) + sizeof(PageHeader) + writtenBytes,
        dataSizeRemainsToWrite());
}

void PageWriter::writeCacheToFile(const FileId& file)
{
    ASSERT(dataSizeRemainsToWrite());

    /* Обновление содержания заголовка страницы */
    Bbx::Buffer headerData = Bbx::Buffer(begin(data), sizeof(PageHeader));
    new (reinterpret_cast<void *>(headerData.data_ptr)) PageHeader(header);

    OwnSection headerSection(file, address.offset, sizeof(PageHeader));
    if (writeNewDataToFile(file))
    {
        headerSection.write(headerData);
        elderRecordMoment = bt::ptime();
    }

    if( cacheFullyFilledAndWroteToFile() )
        createNextPage();
}

bool PageWriter::writeNewDataToFile(const FileId& file)
{
    Bbx::Buffer dataForWriting = getDataBufferForWriting();
    unsigned long dataOffset = address.offset + sizeof(PageHeader) + writtenBytes;
    
    OwnSection dataLocked(file, dataOffset, dataForWriting.size);

    if (dataLocked.write(dataForWriting))
    {
        writtenBytes += dataForWriting.size;
        return true;
    }
    else
    {
        return false;
    }
}

bool PageWriter::cacheFullyFilledAndWroteToFile() const
{
    return (!dataSizeRemainsToFill()) && !dataSizeRemainsToWrite();
}

bool PageReader::read(const FileId& file, const FileAddress& pageAddress)
{
#ifndef LINUX
    ASSERT(INVALID_HANDLE_VALUE != file);
#else
    ASSERT(-1 != file);
#endif // !LINUX
    setPageAddress(pageAddress);
    return readHeader(file) && readAllPartsHeaders(file);
}

bool PageReader::update(const FileId& file)
{
    return valid() && updatePartsHeadersFromPage(file);
}

bool PageReader::valid() const
{
    return headerRead;
}

bool PageReader::readHeader(const FileId& file)
{
    SharedSection headerSection(file, address.offset, sizeof(PageHeader));
    headerRead = headerSection.read(Bbx::Buffer::create(header));
    return headerRead;
}

bool PageReader::readOnePartHeader(const FileId& file, PartHeaderTableRecord& partRec)
{
    SharedSection headerSection(file, partRec.offset, sizeof(PartHeader));
    return (headerSection.read(Bbx::Buffer::create(partRec.header)));
}

size_t PageReader::getPartsNumber() const
{
    return partHeaders.size();
}

bool PageReader::readAllPartsHeaders(const FileId& file)
{
    ASSERT(valid());
    partHeaders.clear();
    return updatePartsHeadersFromPage(file);
}

bool PageReader::updatePartsHeadersFromPage(const FileId& file)
{
    ASSERT(valid());
    
    PartHeaderTableRecord tmpRec;
    tmpRec.offset = partHeaders.empty()
        ? (address.offset + sizeof(PageHeader) + header.getExtensionSize()) 
        : (partHeaders.back().offset + sizeof(PartHeader) + partHeaders.back().header.getSize());

    bool shouldReadNextPart = (tmpRec.offset + sizeof(PartHeader) < address.nextOffset());

    while (shouldReadNextPart && readOnePartHeader(file, tmpRec))
    {
        shouldReadNextPart = false;
        if (tmpRec.header.tagIsKnown())
        {
            /* Копирование записи */
            partHeaders.push_back(tmpRec);

            /* Расчет сдвига следующего куска */
            tmpRec.offset += sizeof(PartHeader) + tmpRec.header.getSize();
            shouldReadNextPart = (tmpRec.offset + sizeof(PartHeader) < address.nextOffset() );
        }
        else
        {
            break;
        }
    }
    // определить полноту чтения страницы
    if ( partHeaders.empty() )
        clipped = true;
    else 
    {
        const auto& last = partHeaders.back();
        clipped = (last.offset + sizeof(PartHeader) + last.header.getSize()) < ( address.nextOffset() - largestUnusedSpace() );
    }
    return !partHeaders.empty();
}

const PageHeader& PageReader::getHeader() const
{
    ASSERT(valid());
    return header;
}

bool PageReader::containsReferenceBeginningParts() const
{
    ASSERT(valid());
    for( const PartHeaderTableRecord& part : partHeaders )
    {
       if (part.header.isReference() && part.header.containsBeginning())
           return true;
    }
    return false;
}

bool containsBeginning(const PartHeaderTableRecord& partRec)
{
    return partRec.header.containsBeginning();
}

bool PageReader::containsAnyBeginningParts() const
{
    ASSERT(valid());
    return !partHeaders.empty() && (partHeaders.size() > 1 || partHeaders.front().header.containsBeginning());
}

bool PageReader::containsBeginningPartsAfter(size_t from) const
{
    ASSERT(valid());
    /* Если за куском в странице есть еще хотя бы один, следующий точно является началом записи */
    return from < partHeaders.size() - 1;
}

bool PageReader::containsTruncationAfter(size_t pos) const
{
    ASSERT(valid());
    /* После указанного куска сразу обрыв данных */
    return truncated() &&  pos == partHeaders.size() - 1;
}

bool PageReader::getFirstRecord(size_t& outIndex) const
{
    ASSERT(valid());
    if (partHeaders.empty())
        return false;

    if (partHeaders.front().header.containsBeginning())
    {
        outIndex = 0;
        return true;
    }

    if (partHeaders.size() > 1)
    {
        outIndex = 1;
        return true;
    }

    return false;
}

bool PageReader::getLastRecord(size_t& outIndex) const
{
    ASSERT(valid());
    if (partHeaders.empty())
        return false;

    /* Последняя запись на странице может быть только в последнем куске */
    if (partHeaders.size() > 1
        || partHeaders.back().header.containsBeginning())
    {
        outIndex = partHeaders.size() - 1;
        return true;
    }
    else
        return false;
}

bool PageReader::containsBeginningPartsBefore(size_t until) const
{
    ASSERT(valid());
    until = std::min(until, partHeaders.size());
    /* Если перед куском есть хотя бы два куска, один из них точно является началом записи */
    if (until > 1)
        return true;

    if (!until)
        return false;

    return partHeaders.front().header.containsBeginning();
}

size_t PageReader::getClosestReferencePartIndex(const Bbx::Stamp& stamp)
{
    ASSERT(containsReferenceBeginningParts());
    size_t found = partHeaders.size();
    Bbx::Stamp diff = INT_MAX;
    for (auto it = partHeaders.begin(); it != partHeaders.end(); ++it)
    {
        if ( it->header.isReference() && it->header.containsBeginning() )
        {
            Bbx::Stamp currentDiff = stamp.modDifference(it->header.getStamp());
            if (diff > currentDiff)
            {
                found = it - partHeaders.begin();
                diff = currentDiff;
            }
        }
    }
    return found;
}

size_t PageReader::getClosestAnyRecordTypePartIndex(const Bbx::Stamp& stamp)
{
    ASSERT(containsAnyBeginningParts());
    size_t found = partHeaders.size();
    Bbx::Stamp diff = INT_MAX;
    for (auto it = partHeaders.begin(); it != partHeaders.end(); ++it)
    {
        if (it->header.containsBeginning())
        {
            Bbx::Stamp currentDiff = stamp.modDifference(it->header.getStamp());
            if (diff > currentDiff)
            {
                found = it - partHeaders.begin();
                diff = currentDiff;
            }
        }
    }
    return found;
}

const PartHeaderTableRecord& PageReader::operator[](size_t pos) const
{
    ASSERT(valid() && pos < partHeaders.size());
    return partHeaders[pos];
}
