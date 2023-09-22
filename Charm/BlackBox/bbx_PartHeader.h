#pragma once

#include "bbx_Record.h"

#pragma pack(push, 1)

namespace Bbx
{
    namespace Impl
    {
        /** 
        @brief Кусок страницы, состоит из признака, размера и данных 
            A - признак кусочка (1 байт)
            T - тип записи (1 байт)
            L - размер данных кусочка (4 байта)
            B - данные кусочка (произвольный размер)
        */
        class PartHeader
        {
        public:
            /* Флаг кусочка записи может содержать как всю запись, так и её начало, середину или окончание */
            enum class Tag : char
            {
                Full = 8,
                Begin = 16,
                End = 32,
                Middle = 64
            };

            PartHeader(bool containsBeginOfTheRecord, const Bbx::Identifier id, time_t time, RecordType recordType, unsigned buf_size, bool containsEndOfTheRecord);
            unsigned getSize() const;
            Tag getTag() const;
            Stamp getStamp() const;
            uint32_t getId() const;
            bool tagIsKnown() const;
            bool isReference() const;
            RecordType getType() const;
            bool containsBeginning() const;  /** Кусок содержит начало записи */
            bool containsEnd() const;        /** Кусок содержит окончание записи */
            bool isContinuationPart() const; /** Кусок содержит продолжение записи */

        private:
            Tag tag;
            RecordType type;
            time_t stamp;
            uint32_t id;
            unsigned size;
        };

        struct PartHeaderTableRecord
        {
            PartHeader header;
            BBX_SIZE offset; // suppose offsetHi==0

            PartHeaderTableRecord();
            Stamp getStamp() const;
            uint32_t getIdentifer() const;
            FileAddress getPartAddress() const;
        };

        inline unsigned PartHeader::getSize() const
        {
            return size;
        }

        inline PartHeader::Tag PartHeader::getTag() const
        {
            return tag;
        }

        inline Stamp PartHeader::getStamp() const
        {
            return stamp;
        }

        inline uint32_t PartHeader::getId() const
        {
            return id;
        }

        inline bool PartHeader::tagIsKnown() const
        {
            return Tag::Begin == tag || Tag::End == tag || Tag::Middle == tag || Tag::Full == tag;
        }

        inline bool PartHeader::containsBeginning() const
        {
            return Tag::Begin == tag || Tag::Full == tag;
        }

        inline bool PartHeader::containsEnd() const
        {
            return Tag::End == tag || Tag::Full == tag;
        }

        inline bool PartHeader::isContinuationPart() const
        {
            return Tag::End == tag || Tag::Middle == tag;
        }

        inline bool PartHeader::isReference() const
        {
            return RecordType::Reference == type;
        }

        inline RecordType PartHeader::getType() const
        {
            return type;
        }

        inline PartHeaderTableRecord::PartHeaderTableRecord()
            : header(false, Bbx::Identifier(), 0u, RecordType::Increment, 0u, false), offset()
        {}

        inline Stamp PartHeaderTableRecord::getStamp() const
        {
            return header.getStamp();
        }

        inline uint32_t PartHeaderTableRecord::getIdentifer() const
        {
            return header.getId();
        }

        inline FileAddress PartHeaderTableRecord::getPartAddress() const
        {
            return FileAddress(offset + sizeof(PartHeader), header.getSize());
        }
    }
}

#pragma pack(pop)