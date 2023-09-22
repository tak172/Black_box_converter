#pragma once

#include "bbx_File.h"
#include "bbx_Page.h"

#pragma pack(push, 1)
namespace Bbx
{
    namespace Impl
    {
        struct ReadFileInfo
        {
        public:
            ReadFileInfo()
                : fileName(), startTime(0), endTime(0),fileSize(0)  {
            }

            bool isCorrect() const {
                return (startTime.getTime()!=0 && endTime.getTime()!=0);
            }

            std::wstring fileName;
            Stamp startTime;
            Stamp endTime;
            size_t fileSize; // размер файла
        };

        /** @brief Курсор для чтения */
        struct Cursor
        {
            size_t page;
            size_t part;

            Cursor() : page(0), part(0) {}
            bool operator <( const Cursor& other ) const
            {
                return (page < other.page) ||
                       (page == other.page && part < other.part);
            }
        };

        /** @brief Файл для чтения */
        class FileReader : public BaseFile
        {
        public:
            typedef Bbx::Impl::Cursor Cursor;
            class page_iterator 
            {
            public:
                using iterator_category = std::random_access_iterator_tag;
                using value_type = FileAddress; // crap
#ifndef LINUX
				using difference_type = int;
#else
                using difference_type = unsigned long;
#endif // LINUX
                using pointer = FileAddress*;
                using reference = FileAddress&;

                page_iterator();
                page_iterator(BBX_SIZE offset, BBX_SIZE size);
                page_iterator(const page_iterator& other);
                page_iterator& operator =(const page_iterator& other);
                bool operator ==(const page_iterator& other) const;
                bool operator !=(const page_iterator& other) const;
                bool operator <(const page_iterator& other) const;
                reference operator *();
                pointer operator ->();
                value_type const* operator ->() const;
                value_type const& operator *() const;
                page_iterator& operator +=( difference_type n);
                page_iterator& operator -=( difference_type n);
                page_iterator& operator ++();
                page_iterator operator ++(int);
                page_iterator& operator --();
                page_iterator operator --(int);
                difference_type operator -(const page_iterator& other) const;

                value_type operator[](difference_type offset) const;

            private:
                FileAddress addr;
            };

            typedef std::reverse_iterator<page_iterator> reverse_page_iterator;

            FileReader();

            Bbx::ReadResult tryOpenFile(const std::wstring& filePath);
            void swap(FileReader& other);
            void update();

            static ReadFileInfo getFileInfo( const std::wstring& onefile );

            bool rewindToStamp(const Stamp& where);
            bool rewindToStampAnyRecordType(const Stamp& where);
            ReadResult rewindToCursor(const Cursor& cursor);
            template <typename T>
            page_iterator back_find_if(page_iterator itFrom, page_iterator itTo, T comparer);
            bool rewindToExtreme(bool toStart);
            Stamp startsFrom() const;
            Stamp endsWith() const;
            const std::wstring& getFilePath() const;
            Cursor getCursor() const;
            size_t readFileSize() const;
            std::string getTimeZone() const;
            Stamp currentCursorStamp() const;
            Identifier currentCursorIdentifier() const;
            Bbx::RecordType currentCursorType() const;
            bool hasMoreRecords(bool directionForward) const;
            ReadResult moveToNextRecord(bool directionForward, bool normalSequence);
            bool comesToTruncated() const;
            bool readCurrentRecord(Stamp& stamp, char_vec& caption, char_vec& before, char_vec& after);
            bool readCurrentRecord(Stamp& stamp, char_vec& caption, char_vec& data);
            bool isReferenceSearchBetter(const Stamp& desiredStamp) const;

        private:
            std::wstring path;
            Cursor cursor;
            PageReader currentPage;

            Bbx::ReadResult unsafeOpenFileAndReadHeader(const std::wstring& filePath);

            page_iterator begin() const;
            page_iterator end() const;
            reverse_page_iterator rbegin() const;
            reverse_page_iterator rend() const;
            bool readCurrentRecord( RecordIn& record );
            bool readHeader();
            bool readAndVerifyVersion();
            bool readExtensionZone(Buffer& buf) const;
            bool fileSizeIsEnoughToRead() const;
            size_t getPagesCount() const;
            bool readPage(reverse_page_iterator revPageIt);
            bool readPage(page_iterator pageIt);
            void setPage(const PageReader& pr, reverse_page_iterator revPageIt);
            void setPage(const PageReader& pr, page_iterator revPageIt);
            ReadResult setPart(PageReader& pr, size_t partIndex);
            ReadResult setPartWithMoreThanTimeCheck(PageReader& pr, size_t partIndex, const Stamp& stamp, bool normalSequence);
            ReadResult setPartWithLessThanTimeCheck(PageReader& pr, size_t partIndex, const Stamp& stamp, bool normalSequence);


            ReadResult setFollowingPage(const Stamp& oldStamp, bool normalSequence);
            ReadResult setPrecedePage(const Stamp& oldStamp, bool normalSequence);
        };

        inline FileReader::page_iterator::page_iterator()
            : addr()
        { }

        inline FileReader::page_iterator::page_iterator(BBX_SIZE offset, BBX_SIZE size)
            : addr(offset, size)
        { }

        inline FileReader::page_iterator::page_iterator(const page_iterator& other)
            : addr(other.addr)
        { }

        inline FileReader::page_iterator operator +(const FileReader::page_iterator& it, FileReader::page_iterator::difference_type n)
        {
            FileReader::page_iterator tmp(it);
            tmp += n;
            return tmp;
        }

        inline FileReader::page_iterator operator -(const FileReader::page_iterator& it, FileReader::page_iterator::difference_type n)
        {
            FileReader::page_iterator tmp(it);
            tmp -= n;
            return tmp;
        }

        inline FileReader::page_iterator::reference FileReader::page_iterator::operator *()
        {
            return addr;
        }

        inline FileReader::page_iterator::value_type const& FileReader::page_iterator::operator *() const
        {
            return addr;
        }

        inline FileReader::page_iterator::pointer FileReader::page_iterator::operator ->()
        {
            return &addr;
        }

        inline FileReader::page_iterator& FileReader::page_iterator::operator =(const page_iterator& other)
        {
            addr = other.addr;
            return *this;
        }

        inline FileReader::page_iterator::value_type const* FileReader::page_iterator::operator ->() const
        {
            return &addr;
        }

        inline bool FileReader::page_iterator::operator ==(const page_iterator& other) const
        {
            return addr == other.addr;
        }

        inline bool FileReader::page_iterator::operator !=(const page_iterator& other) const
        {
            return !operator ==(other);
        }

        inline bool FileReader::page_iterator::operator <(const page_iterator& other) const
        {
            return addr < other.addr;
        }

        inline FileReader::page_iterator& FileReader::page_iterator::operator -=(difference_type n)
        {
            return operator +=(-n);
        }

        inline FileReader::page_iterator& FileReader::page_iterator::operator ++()
        {
            addr.offset += addr.size;
            return *this;
        }

        inline FileReader::page_iterator FileReader::page_iterator::operator ++(int)
        {
            page_iterator tmp(*this);
            operator++();
            return tmp;
        }

        inline const std::wstring& FileReader::getFilePath() const
        {
            return path;
        }

        inline FileReader::Cursor FileReader::getCursor() const
        {
            return cursor;
        }

        inline bool FileReader::comesToTruncated() const
        {
            return currentPage.containsTruncationAfter(cursor.part);
        }
    }
}
#pragma pack(pop)
