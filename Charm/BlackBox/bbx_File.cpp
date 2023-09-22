#include "stdafx.h"
#ifdef LINUX
#include <fcntl.h>
#include <boost/filesystem/path.hpp>
#endif
#include <boost/filesystem/operations.hpp>
#include <iomanip>
#include "bbx_Requirements.h"
#include "bbx_BlackBox.h"
#include "bbx_File.h"
#include "bbx_Page.h"
#include "bbx_Extension.h"

using namespace Bbx::Impl;

/** @brief Размер файла, по достижении которого 
           файл следует завершать и создавать следующий */
const size_t c_DefaultMaxFileSize = 32 * 1024 * 1024;

/** @brief Размер зоны расширения файла */
const size_t c_DefaultExtensionSizeBytes = 32;

FileHeader::FileHeader()
:pageSize(0), extensionSize(c_DefaultExtensionSizeBytes), timeBegin(0), timeEnd(0)
{
}


/************************************************************************/
/*                      Методы классов файлов                           */
/************************************************************************/

BaseFile::BaseFile() : header()
{
}

BaseFile::~BaseFile()
{
    if( isOpened() )
        close();
}

void BaseFile::swap( BaseFile& other )
{
    ASSERT( other.isOpened() );
    std::swap( handle, other.handle );
    std::swap( header, other.header );
}

#ifndef LINUX
bool BaseFile::safeOpen_ModeRead( const std::wstring& path )
{
    ASSERT( !path.empty() );
    FileId tmp = CreateFile( path.c_str(),
        GENERIC_READ, ( FILE_SHARE_WRITE | FILE_SHARE_READ ), NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

    if( INVALID_HANDLE_VALUE != tmp ) {
        handle = tmp;
        return true;
    } else {
        return false;
    }
}

bool BaseFile::safeOpen_ModeWrite( const std::wstring& path )
{
    ASSERT( !path.empty() );
    FileId tmp = CreateFile( path.c_str(),
        GENERIC_WRITE, FILE_SHARE_READ, NULL,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );

    if( INVALID_HANDLE_VALUE != tmp ) {
        handle = tmp;
        return true;
    } else {
        return false;
    }
}

void BaseFile::close()
{
    CloseHandle( handle );
    handle = INVALID_HANDLE_VALUE;
}

bool BaseFile::isOpened() const
{
    return ( INVALID_HANDLE_VALUE != getHandle() );
}

bool BaseFile::safeRemove(const std::wstring& path)
{
    boost::system::error_code ec;
    return boost::filesystem::remove( path, ec );
}

#else

static const BBX_SIZE MARK_OFFSET = 3 * Bbx::c_GB; // начало маркировки для проверки владения
static const BBX_SIZE MARK_SIZE   = 1024;          // начало маркировки для проверки владения

bool BaseFile::safeOpen_ModeRead( const std::wstring& path )
{
    ASSERT( !path.empty() );
    boost::filesystem::path pp( path );
    FileId tmp = open( pp.string().c_str(), O_RDONLY );

    if( 0 <= tmp ) {
        handle = tmp;
        SharedSection::lock( handle, MARK_OFFSET, 1024 ); // отметка используемого файла
        return true;
    }
    return false;
}

bool BaseFile::safeOpen_ModeWrite( const std::wstring& path )
{
    ASSERT( !path.empty() );
    boost::filesystem::path pp( path );
    boost::system::error_code ec;
    if ( !boost::filesystem::exists( pp, ec ) )
    {
        mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH;
        FileId tmp = open( pp.string().c_str(), O_CREAT | O_RDWR, mode );
        if( 0 <= tmp ) {
            handle = tmp;
            SharedSection::lock( handle, MARK_OFFSET, 1024 ); // отметка используемого файла
            return true;
        }
    }
    return false;
}

void BaseFile::close()
{
    ::close( handle );
    handle = -1;
}

bool BaseFile::isOpened() const
{
    return ( -1 != handle );
}

bool BaseFile::safeRemove(const std::wstring& path)
{
    boost::filesystem::path pp( path );
    if ( boost::filesystem::exists(pp) )
    {
        boost::system::error_code ec;
        FileId tmp = open( pp.string().c_str(), O_RDWR );
        if( 0 <= tmp )
        {
            if ( OwnSection::trylock( tmp, MARK_OFFSET, 1024 ) ) // проверка отсутствия читателей на файле
            {
                if ( 0 == ::close( tmp ) )
                {
                    if ( boost::filesystem::remove( path, ec ) )
                        return true;
                }
            }
        }
    }
    return false;
}

#endif // !LINUX



FileWriter::FileWriter(const Bbx::Location& bbx_location, unsigned page_size)
    :BaseFile(), location(bbx_location), 
    page(), lastWroteWasReference(false),
    maximumFileSizeBytes(c_DefaultMaxFileSize),
    bytesWritten(0), messagesWritten(), startTime(0)
{
    header.setPageSize(page_size);
}

FileWriter::~FileWriter()
{
    if (isOpened()) {
        OwnSection headerLock(getHandle(), 0, sizeof(FileHeader));
        page.update(getHandle());
        headerLock.write(Buffer::create(header));
    }
}

void FileWriter::update(bool force)
{
    if (isOpened() && (force || page.needsUpdate())) {
        OwnSection headerLocked(getHandle(), 0, sizeof(FileHeader));
        page.update(getHandle());
        headerLocked.write(Buffer::create(header));
#ifndef LINUX
        FlushFileBuffers(getHandle());
#else
        int res __attribute__((unused)) = fsync( getHandle() );
        ASSERT( res >= 0 );
#endif // !LINUX
    }
}

std::string FileWriter::generateExtensionZone() const
{
    Extension extension;
    extension.setActualVersion();
    extension.setTimeZone( timeZone );
    return extension.serialize();
}

bool FileWriter::create(const Bbx::Stamp& stamp)
{
    for( unsigned attempt = 0; attempt<100; ++attempt ) {
        if (safeOpen_ModeWrite( location.filePath( stamp, attempt ) ) ) {
            startTime = stamp.getTime();
            std::string extensionString = generateExtensionZone();
            Bbx::Buffer extensionBuffer = Bbx::Buffer(extensionString);
            header.setExtensionSize(extensionBuffer.size);

            page.setAddress(FileAddress(header.getHeaderSize(), header.getPageSize()));

            OwnSection headerLock(getHandle(), 0, sizeof(FileHeader));
            headerLock.write(Bbx::Buffer::create(header));
            writeExtensionZone(extensionBuffer);
            return true;
        }
    }
    // создать файл не удалось
    return false;
}

bool FileWriter::writeExtensionZone(const Bbx::Buffer& extensionData)
{
    OwnSection extensionSection(getHandle(), sizeof(FileHeader), extensionData.size);
    return extensionSection.write(extensionData);
}

bool FileWriter::processMessageIntoPages(RecordOut& record)
{
    header.setLastRecordTime(record.getStamp().getTime());

    if (page.willWriteToFile(record)) {
        OwnSection headerLock(getHandle(), 0, sizeof(FileHeader));
        page.processRecord(getHandle(), record);
        return headerLock.write(Bbx::Buffer::create(header));
    } else {
        page.appendRecord(record);
        ASSERT(record.completed());
        return true;
    }
}

bool FileWriter::writeRecord(RecordOut& msg)
{
    if (RecordType::Reference == msg.getType()) {
        if (!isOpened() && !create(msg.getStamp()))
            return false;
        if (0u == getReferenceMessagesCount())
            header.setFirstRecordTime(msg.getStamp().getTime());
    } else {
        if (0u == getReferenceMessagesCount() || !isOpened())
            return false;
    }

    if (processMessageIntoPages(msg)) {
        registerDataRecord(msg.getType(), msg.getSize());
        return true;
    } else {
        return false;
    }
}

void FileWriter::registerDataRecord(Bbx::RecordType type, unsigned bytes)
{
    lastWroteWasReference = (Bbx::RecordType::Reference == type);
    bytesWritten += bytes;
    ++messagesWritten[type];
}

bool FileWriter::exceedFileAge(const Bbx::Stamp& stampWrite) const
{
    const time_t MAXFILEDURATION = 60*60; // не более часа
    // или достигаем границы,
    if ( stampWrite.getTime() - startTime >= MAXFILEDURATION ) {
        return true;
    } else {
        // или дошли до "красивого" уровня
        time_t t = stampWrite.getTime();
        t -= t % MAXFILEDURATION;
        return ( t > startTime );
    }
}

Bbx::Impl::SectionLocker::SectionLocker( bool Exclusive, const FileId& file, unsigned long offset, unsigned size ) : address( offset, size ), handle( file ), locked( false )
{
    locked = lock( Exclusive, handle, address.offset, address.size );
    ASSERT( locked && "Locking always works!" );
}

Bbx::Impl::SectionLocker::~SectionLocker()
{
    if( locked )
        unlock( handle, address.offset, address.size );
}

#ifndef LINUX
bool Bbx::Impl::SectionLocker::read( Buffer buf ) const
{
    ASSERT( buf.size <= address.size && "читать можно только в пределах блокированной зоны!" );
    BBX_SIZE bytesRead = 0;
    return locked && setPointer()
        && ( 0 != ReadFile( handle, reinterpret_cast<void*>( buf.data_ptr ), buf.size, &bytesRead, NULL ) )
        && ( buf.size == bytesRead );
}

bool Bbx::Impl::SectionLocker::write( const Buffer& data ) const
{
    ASSERT( data.size <= address.size && "писать можно только в пределах блокированной зоны!" );
    BBX_SIZE bytesWritten = 0;
    return locked && setPointer()
        && ( 0 != WriteFile( handle, data.data_ptr, data.size, &bytesWritten, NULL ) )
        && ( data.size == bytesWritten );
}

bool Bbx::Impl::SectionLocker::lock(bool Exclusive, FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    OVERLAPPED settings;
    memset(&settings, 0, sizeof(OVERLAPPED));
    settings.Offset = offset;
    BBX_SIZE flags = (Exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0);
    bool res = 0 != LockFileEx( fd, flags, 0, size, 0, &settings);
    ASSERT( res && "Locking always works!" );
    return res;
}

void Bbx::Impl::SectionLocker::unlock( FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    OVERLAPPED settings;
    memset(&settings, 0, sizeof(OVERLAPPED));
    settings.Offset = offset;
    UnlockFileEx( fd, 0, size, 0, &settings);
}

bool Bbx::Impl::SectionLocker::setPointer() const
{
    return INVALID_SET_FILE_POINTER != SetFilePointer( handle, address.offset, NULL, FILE_BEGIN );
}

#else

bool Bbx::Impl::SectionLocker::read( Buffer buf ) const
{
    ASSERT( buf.size <= address.size && "читать можно только в пределах блокированной зоны!" );
    return locked
        && setPointer()
        && buf.size == ::read( handle, reinterpret_cast<void*>( buf.data_ptr ), buf.size );
}

bool Bbx::Impl::SectionLocker::write( const Buffer& data ) const
{
    ASSERT( data.size <= address.size && "писать можно только в пределах блокированной зоны!" );
    return locked
        && setPointer()
        && data.size == ::write( handle, data.data_ptr, data.size );
}

bool Bbx::Impl::SectionLocker::lock( bool Exclusive, FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    struct flock fl;
    fl.l_type = Exclusive? F_WRLCK : F_RDLCK; /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.	*/
    fl.l_whence = SEEK_SET;	                  /* Where `l_start' is relative to (like `lseek').  */
    fl.l_start = offset;                      /* Offset where the lock begins.  */
    fl.l_len = size;	                      /* Size of the locked area; zero means until EOF.  */
    fl.l_pid = 0;                             /* Process holding the lock.  */

    int code = fcntl( fd, F_OFD_SETLKW, &fl );
    bool res = ( 0 == code );
    ASSERT( res && "Locking always works!" );
    return res;
}

bool Bbx::Impl::SectionLocker::trylock( bool Exclusive, FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    struct flock fl;
    fl.l_type = Exclusive ? F_WRLCK : F_RDLCK; /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.	*/
    fl.l_whence = SEEK_SET;	                  /* Where `l_start' is relative to (like `lseek').  */
    fl.l_start = offset;                      /* Offset where the lock begins.  */
    fl.l_len = size;	                      /* Size of the locked area; zero means until EOF.  */
    fl.l_pid = 0;                             /* Process holding the lock.  */

    int code = fcntl( fd, F_OFD_SETLK, &fl );
    bool res = ( 0 == code );
    return res;
}

void Bbx::Impl::SectionLocker::unlock( FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    struct flock fl;
    fl.l_type = F_UNLCK;    /* Type of lock: F_RDLCK, F_WRLCK, or F_UNLCK.	*/
    fl.l_whence = SEEK_SET;	/* Where `l_start' is relative to (like `lseek').  */
    fl.l_start = offset;    /* Offset where the lock begins.  */
    fl.l_len = size;	    /* Size of the locked area; zero means until EOF.  */
    fl.l_pid = 0;           /* Process holding the lock.  */

    int code = fcntl( fd, F_OFD_SETLKW, &fl );
    bool res __attribute__((unused)) = ( 0 == code );
    ASSERT( res && "Unlocking always works!" );
}

bool Bbx::Impl::SectionLocker::setPointer() const
{
    return EINVAL != lseek( handle, address.offset, SEEK_SET );
}

bool Bbx::Impl::SharedSection::trylock( FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    return SectionLocker::trylock( false, fd, offset, size );
}

bool Bbx::Impl::OwnSection::trylock( FileId fd, BBX_SIZE offset, BBX_SIZE size )
{
    return SectionLocker::trylock( true, fd, offset, size );
}

#endif // !LINUX


bool Bbx::Impl::SharedSection::lock(FileId fd, BBX_SIZE offset, BBX_SIZE size)
{
    return SectionLocker::lock( false, fd, offset, size );
}

void Bbx::Impl::SharedSection::unlock(FileId fd, BBX_SIZE offset, BBX_SIZE size)
{
    return SectionLocker::unlock( fd, offset, size );
}

bool Bbx::Impl::OwnSection::lock(FileId fd, BBX_SIZE offset, BBX_SIZE size)
{
    return SectionLocker::lock( true, fd, offset, size );
}


void Bbx::Impl::OwnSection::unlock(FileId fd, BBX_SIZE offset, BBX_SIZE size)
{
    SectionLocker::unlock( fd, offset, size );
}
