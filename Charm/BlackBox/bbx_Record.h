#pragma once

#include "bbx_Requirements.h"
#include "bbx_BlackBox.h"

#pragma pack(push, 1)

namespace Bbx
{
    namespace Impl
    {
        struct WriterTask : boost::noncopyable
        {
            // Размер буфера и копия данных
            typedef std::pair<unsigned, Bbx::char_vec> Source;

            WriterTask(Bbx::Identifier id, Bbx::Stamp stamp, Bbx::RecordType type, const Bbx::Buffer& caption, const Bbx::Buffer& data);
            WriterTask(Bbx::Identifier id, Bbx::Stamp stamp, Bbx::RecordType type, const Bbx::Buffer& caption, const Bbx::Buffer& data1, const Bbx::Buffer& data2);

            const Bbx::Identifier id;
            const Bbx::Stamp stamp;
            const Bbx::RecordType type;
            std::vector<Source> sources;
            unsigned getWeight() const;

        private:
            void addCopyOfBuffer(const Bbx::Buffer& data);
        };

        class RecordOut
        {
        public:
            explicit RecordOut(const WriterTask& task);

            void write(const Buffer& outBuffer);

            unsigned getSize() const;
            unsigned getRemainingSize() const;
            bool completed() const;
            bool untouched() const;
            Stamp getStamp() const;
            Identifier getId() const;
            RecordType getType() const;

        protected:
            typedef std::vector<Buffer> BuffersVec;
            void addBuffer(const Buffer& buf);

            BuffersVec buffers;
            unsigned size;
            unsigned completedSize;
            Stamp time;
            Identifier id;
            RecordType type;
        };

        struct FileAddress
        {
			BBX_SIZE offset;
			BBX_SIZE size;

			FileAddress(BBX_SIZE fileOffset, BBX_SIZE pageSize);

            FileAddress();
            bool operator ==(const FileAddress& other) const;
            bool operator <(const FileAddress& other) const;
            BBX_SIZE nextOffset() const { return offset + size; };
        };

        class RecordIn : boost::noncopyable
        {
        public:
            RecordIn( Stamp& recStamp, char_vec& caption, char_vec& before, char_vec& after );
            RecordIn( Stamp& recStamp, char_vec& caption, char_vec& data );

			bool readPart(const FileId& file, const FileAddress& address);
            bool readed() const;
            void setStamp(const Stamp& time);

        private:
            struct ContainerIn
            {
                ContainerIn(char_vec& buffer)
                    : size(0), sizeBytesRead(0), buffer(&buffer) {}

                unsigned size;
                unsigned sizeBytesRead;
                char_vec *buffer;
            };

            Stamp& stamp;
            std::queue<ContainerIn> buffers;

            void addContainer(char_vec& container);
        };
        
        inline void RecordOut::addBuffer(const Buffer& buf)
        {
            buffers.push_back(buf);
            size += buf.size;
        }

        inline unsigned RecordOut::getSize() const
        {
            return size;
        }

        inline unsigned RecordOut::getRemainingSize() const
        {
            return size - completedSize;
        }

        inline bool RecordOut::completed() const
        {
            return !getRemainingSize();
        }

        inline bool RecordOut::untouched() const
        {
            return 0 == completedSize;
        }

        inline Stamp RecordOut::getStamp() const
        {
            return time;
        }

        inline Identifier RecordOut::getId() const
        {
            return id;
        }

        inline RecordType RecordOut::getType() const
        {
            return type;
        }

        inline FileAddress::FileAddress(BBX_SIZE fileOffset, BBX_SIZE pageSize)
            : offset(fileOffset), size(pageSize) 
        { }

        inline FileAddress::FileAddress() 
            : offset(0), size(0)
        { }

        inline bool FileAddress::operator ==(const FileAddress& other) const
        {
            return other.offset == offset && other.size == size;
        }

        inline bool FileAddress::operator <(const FileAddress& other) const
        {
            assert(size == other.size);
            return offset < other.offset;
        }

        inline bool RecordIn::readed() const
        {
            return buffers.empty();
        }

        inline void RecordIn::setStamp(const Stamp& time)
        {
            stamp = time;
        }
    }
}

#pragma pack(pop)