#pragma once

#pragma pack(push, 1)
namespace Bbx
{
    class Identifier
    {
    public:
        enum Source {
            HaronInput = 0,
            HaronOutput = 1,
            FundInput = 1 << 1,
            FundOutput = 1 << 2,
            Undefined = 1 << 3
        };

        /** @brief Создаёт нулевой идентификатор указанного типа */
        explicit Identifier(Source sourceType = Undefined);
        
        /** @brief Увеличивает значение идентификатора на 1 или обнуляет, если значение достигло максимума */
        void increment();
        Identifier& operator ++();
        Identifier operator ++(int);
      
        uint32_t getId() const;
        Source getSource() const;
        
        /** @brief Только для использования внутри ЧЯ */
        uint32_t asSerializedValue() const;

        /** @brief Создать идентификатор из считанного буфера */
        void deserialize(uint32_t value);

        std::wstring getDescription() const;

        /** @brief Только для юнит-тестов; устанавливает значение идентификатора, не меняя тип идентификатора */
        void unsafeSet(uint32_t id);
        static uint32_t getMaximumValue();

        bool operator ==(const Identifier& other) const;
        bool operator < (const Identifier& other) const;
    private:
        /** @brief 32 бита, старшие 4 хранят тип идентификатора, младшие 28 содержат номер */
        uint32_t value;

        const static unsigned short c_sourceBitsCount = 4;
        const static unsigned short c_valueBitsCount = 28;
        const static uint32_t c_maxValue = (1 << c_valueBitsCount) - 1;
        const static uint32_t c_valueMask = (static_cast<uint32_t>(~0) >> c_sourceBitsCount);

        std::wstring getSourceDescription() const;
    };

    inline Identifier::Identifier(Source sourceType) 
        : value(static_cast<uint32_t>(sourceType) << c_valueBitsCount) { 
    }

    inline uint32_t Identifier::getId() const
    {
        return value & c_valueMask;
    }

    inline uint32_t Identifier::asSerializedValue() const
    {
        return value;
    }

    inline void Identifier::deserialize(uint32_t _value)
    {
        this->value = _value;
    }

    inline Identifier::Source Identifier::getSource() const
    {
        return static_cast<Source>(value >> c_valueBitsCount);
    }

    inline void Identifier::increment()
    {
        uint32_t id = getId();
        unsafeSet(c_maxValue > id ? id + 1 : 0);
    }

    inline Identifier& Identifier::operator++()
    {
        increment();
        return *this;
    }

    inline Identifier Identifier::operator++(int)
    {
        Identifier copy(*this);
        increment();
        return copy;
    }

    inline void Identifier::unsafeSet(uint32_t id)
    {
        value = ((value >> c_valueBitsCount) << c_valueBitsCount) | id;
    }

    inline uint32_t Identifier::getMaximumValue()
    {
        return c_maxValue;
    }

    inline bool Identifier::operator ==(const Identifier& other) const
    {
        return value == other.value;
    }

    inline bool Identifier::operator <(const Identifier& other) const
    {
        return value < other.value;
    }
}
#pragma pack(pop)
