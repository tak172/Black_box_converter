#include "stdafx.h"

#include "bbx_BlackBox.h"
#include "bbx_Reader.h"
#include "bbx_Writer.h"
#include "bbx_Page.h"

using namespace Bbx;

// Reader implementation

Reader::Reader(const Location& location)
    : pImpl(new Impl::ReaderImpl(location))
{
}

Reader::~Reader()
{
}

ReadResult Reader::lastResult() const
{
    return pImpl->lastResult();
}

ReadResult Reader::readReference(Stamp& stamp, char_vec& caption, char_vec& data)
{
    return pImpl->readReference(stamp, caption, data);
}

ReadResult Reader::readIncrementOriented(Stamp& stamp, char_vec& caption, char_vec& data)
{
    return pImpl->readIncrementOriented(stamp, caption, data);
}

ReadResult Reader::readPackage(Stamp& stamp, char_vec& caption, char_vec& data)
{
    return pImpl->readPackage(stamp, caption, data);
}

ReadResult Reader::readAnyRecord(Stamp& stamp, char_vec& caption, char_vec& data)
{
    return pImpl->readAnyRecord(stamp, caption, data);
}

ReadResult Reader::rewind(const Stamp& where)
{
    return pImpl->rewind(where);
}

ReadResult Reader::next()
{
    return pImpl->next();
}

ReadResult Reader::forceNext()
{
    return pImpl->forceNext();
}

void Reader::setDirection(bool goForward)
{
    pImpl->setDirection(goForward);
}

bool Reader::getDirection() const
{
    return pImpl->getDirection();
}

std::pair<Stamp,Stamp> Reader::getBoundStamp()
{
    return pImpl->getAvailableTimeInterval();
}

bool Reader::existActualWriter() const
{
    return pImpl->existActualWriter();
}

Stamp Reader::getCurrentStamp() const
{
    return pImpl->getCurrentStamp();
}

Identifier Reader::getCurrentIdentifier() const
{
    return pImpl->getCurrentIdentifier();
}

RecordType Reader::getCurrentType() const
{
    return pImpl->getCurrentType();
}

bool Reader::resolveToSearchReference(const Stamp& decidesStamp) const
{
    return pImpl->resolveToSearchReference(decidesStamp);
}

void Reader::update()
{
    pImpl->update();
}

bool Reader::isOpened() const
{
    return pImpl->isOpened();
}

bool Reader::hasMoreRecords() const
{
    return pImpl->hasMoreRecords();
}

std::string Bbx::Reader::getTimeZone() const
{
    return pImpl->getTimeZone();
}

Bbx::Impl::Cursor Bbx::Reader::getCurrentCursor() const
{
    return pImpl->getCurrentCursor();
}

// Writer implementation

std::shared_ptr<Writer> Writer::create(const Bbx::Location& location)
{
    Impl::WriterImpl* impl = Impl::WriterImpl::create(location);
    if ( impl )
        return std::shared_ptr<Writer>( new Writer( impl ) );
    else
        return std::shared_ptr<Writer>();
}

Writer::Writer(Impl::WriterImpl* pImpl)
    : pImpl(pImpl)
{
}

Writer::~Writer()
{
}

void Writer::setCacheDeviateDelay( boost::posix_time::time_duration delay )
{
    Impl::PageWriter::setDeviateDelay( delay );
}

boost::posix_time::time_duration Writer::getCacheDeviateDelay()
{
    return Impl::PageWriter::getDeviateDelay();
}

bool Writer::pushReference(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id)
{
    return pImpl->pushReference(caption, data, stamp, id);
}

bool Writer::pushIncrement(const Buffer& caption, const Buffer& before, const Buffer& after, const Stamp& stamp, const Identifier id)
{
    return pImpl->pushIncrement(caption, before, after, stamp, id);
}

bool Writer::pushIncomingPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id)
{
    return pImpl->pushIncomingPackage(caption, data, stamp, id);
}

bool Writer::pushOutboxPackage(const Buffer& caption, const Buffer& data, const Stamp& stamp, const Identifier id)
{
    return pImpl->pushOutboxPackage(caption, data, stamp, id);
}

bool Writer::isDead() const
{
    return pImpl->isDead();
}

std::string Writer::getErrorMessage() const
{
    return pImpl->getErrorMessage();
}

void Writer::flush()
{
    pImpl->flush();
}

unsigned Writer::getPageSize() const
{
    return pImpl->getPageSize();
}

void Writer::setPageSize(unsigned page_size)
{
    pImpl->setPageSize(page_size);
}

void Writer::setRecomendedFileSize(unsigned file_size)
{
    pImpl->setRecomendedFileSize(file_size);
}

void Writer::setTimeZone( std::string textTZ )
{
    pImpl->setTimeZone(textTZ);
}

bool Writer::setDiskLimit(const char * disk_size)
{
    return pImpl->setDiskLimit(disk_size);
}

bool Writer::setDiskLimit(BBX_DISK_SIZE disk_size)
{
    return pImpl->setDiskLimit(disk_size);
}

BBX_DISK_SIZE Writer::getDiskLimit() const
{
    return pImpl->getDiskLimit();
}

void Writer::setLifeTime(time_t life_time)
{
    pImpl->setLifeTime(life_time);
}

const Location& Writer::getLocation() const
{
    return pImpl->getLocation();
}

bool Writer::needReference( time_t curr_moment ) const
{
    return pImpl->needReference(curr_moment);
}

std::tuple<unsigned, unsigned, unsigned, unsigned> Writer::getWrittenMessagesCounts() const
{
    return pImpl->getWrittenMessagesCounts();
}
