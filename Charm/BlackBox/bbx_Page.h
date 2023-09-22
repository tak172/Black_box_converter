#pragma once

#include <time.h>
#include "bbx_Record.h"
#include "bbx_PartHeader.h"

#pragma pack(push, 1)

namespace Bbx
{
    const size_t   c_KB = 1024;
    const size_t   c_MB = 1024 * c_KB;
    const size_t   c_GB = 1024 * c_MB;

    const BBX_DISK_SIZE c_TB = 1024 * BBX_DISK_SIZE(c_GB);
    const BBX_DISK_SIZE c_PB = 1024 * BBX_DISK_SIZE(c_TB); // 2^50 - должно хватить надолго

    namespace Impl
    {

        /** @brief Размер страничной зоны расширения */
        const unsigned c_DefaultPageExtensionSize = 16;

        class PageHeader
        {
        public:
            explicit PageHeader(unsigned extSize = c_DefaultPageExtensionSize);

            void write(const Buffer& buf);
            void addRecordTime(time_t time);
            unsigned getExtensionSize() const;
            Stamp getFirstStamp() const;
            Stamp getSecondStamp() const;

        private:
            unsigned extensionSize;
            time_t timeBegin;
            time_t timeEnd;
        };

        class Page
        {
        public:
            static void setDeviateDelay( boost::posix_time::time_duration delayMs);
            static boost::posix_time::time_duration getDeviateDelay();

        protected:
            Page();
            void setPageAddress(const FileAddress& pageAddress);
            static size_t largestUnusedSpace();

            FileAddress address;
            PageHeader header;
        private:
            /** @brief Задержка между поступлением данных и их записью в файл */
            static boost::posix_time::time_duration DeviateDelay; // текущее значение
        };

        class PageWriter : public Page, boost::noncopyable
        {
        public:
            PageWriter();
            ~PageWriter();

            void setAddress(const FileAddress& pageAddress);

            bool willWriteToFile(const RecordOut& record) const;
			void processRecord(const FileId& file, RecordOut& record);
            void appendRecord(RecordOut& record);
            bool needsUpdate() const;
            void update(const FileId& file);

        private:
            Buffer data;
            unsigned writtenBytes;
            boost::posix_time::ptime elderRecordMoment;

            void init();
            void createNextPage();
			void writeCacheToFile(const FileId& file);
            bool shouldBeFlushedNow() const;
            bool cacheFullyFilledAndWroteToFile() const;

            Buffer reserve(unsigned bytes);
            bool cacheCanTakeNoMoreRecords() const;
            void fillRemainingSpaceWithNulls();
			bool writeNewDataToFile(const FileId& file);
            unsigned dataSizeRemainsToWrite() const;
            unsigned long dataSizeRemainsToFill() const;
            Buffer getDataBufferForWriting();
            Buffer getHeaderBuffer() const;
        };

        class PageReader : public Page
        {
        public:
            PageReader();
			bool read(const FileId& file, const FileAddress& pageAddress);
			bool update(const FileId& file);
            bool valid() const;
            bool truncated() const;
            const PageHeader& getHeader() const;
            bool containsReferenceBeginningParts() const;
            bool containsAnyBeginningParts() const;
            bool containsBeginningPartsAfter(size_t from) const;
            bool containsBeginningPartsBefore(size_t until) const;
            bool containsTruncationAfter(size_t pos) const;
            bool getFirstRecord(size_t& outIndex) const;
            bool getLastRecord(size_t& outIndex) const;
            size_t getClosestReferencePartIndex(const Stamp& stamp);
            size_t getClosestAnyRecordTypePartIndex(const Stamp& stamp);
            size_t getPartsNumber() const;
            const PartHeaderTableRecord& operator[](size_t pos) const;

        private:
            /// \todo partHeaders неэффективен - надо читать заголовок страницы или всю страницу сразу.
            std::vector<PartHeaderTableRecord> partHeaders;
            bool headerRead;
            bool clipped; // страница неполная т.е. обрезана

			bool readHeader(const FileId& file);
			bool readOnePartHeader(const FileId& file, PartHeaderTableRecord& partRec);
			bool readAllPartsHeaders(const FileId& file);
			bool updatePartsHeadersFromPage(const FileId& file);
        };

        inline bool operator <( const PageReader& pr, const Stamp& stamp )
        {
            return pr.valid() ? pr.getHeader().getSecondStamp() < stamp : true;
        }

        inline bool operator <( const Stamp& stamp, const PageReader& pr )
        {
            return pr.valid() ? pr.getHeader().getSecondStamp() > stamp : false;
        }

        inline PageHeader::PageHeader(unsigned extSize)
            :extensionSize(extSize), timeBegin(0), timeEnd(0)
        {}

        inline void PageHeader::write(const Buffer& buf)
        {
            ASSERT(buf.data_ptr && buf.size >= sizeof(PageHeader));
            new (buf.data_ptr) PageHeader(*this);
        }

        inline void PageHeader::addRecordTime(time_t time)
        {
            if (!timeBegin)
                timeBegin = time;
            if (timeEnd < time)
                timeEnd = time;
        }

        inline unsigned PageHeader::getExtensionSize() const
        { return extensionSize; }

        inline Stamp PageHeader::getFirstStamp() const
        { return timeBegin; }

        inline Stamp PageHeader::getSecondStamp() const
        { return timeEnd; }

        inline Page::Page()
            : address(), header()
        { }

        inline void Page::setPageAddress(const FileAddress& pageAddress)
        {
            address = pageAddress;
        }

        inline boost::posix_time::time_duration Page::getDeviateDelay()
        {
            return DeviateDelay;
        }

        inline size_t Page::largestUnusedSpace()
        {
            return sizeof(PartHeader) * 2;
        }

        inline PageReader::PageReader()
            : Page(), partHeaders(), headerRead( false ), clipped(false)
        { }

        inline bool PageReader::truncated() const
        {
            return clipped;
        }

        inline PageWriter::PageWriter()
            : Page(), data(), writtenBytes(0),
            elderRecordMoment()
        {
        }

        inline PageWriter::~PageWriter()
        {
            delete []data.data_ptr;
        }

        inline void PageWriter::setAddress(const FileAddress& pageAddress)
        {
            setPageAddress(pageAddress);
            init();
        }

        inline bool PageWriter::needsUpdate() const
        {
            return shouldBeFlushedNow();
        }

		inline void PageWriter::update(const FileId& file)
		{
			if (dataSizeRemainsToWrite())
				writeCacheToFile(file);
		}

    }
}

#pragma pack(pop)