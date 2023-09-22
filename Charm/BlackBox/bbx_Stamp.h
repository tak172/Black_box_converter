#pragma once

#pragma pack(push, 1)
namespace Bbx
{
    class Stamp
    {
    public:
        Stamp() : stamp(13) {}; // инициализация произвольной константой
        Stamp(time_t time) : stamp(time) {};

        bool operator <(const Stamp& other) const;
        bool operator >(const Stamp& other) const;
        bool operator ==(const Stamp& other) const;
        bool operator !=(const Stamp& other) const;
        bool operator <=(const Stamp& other) const;
        bool operator >=(const Stamp& other) const;
        operator time_t() const;

        time_t getTime() const;
        Stamp modDifference(const Stamp& other) const;

    private:
        time_t stamp;
    };

    inline Stamp::operator time_t() const
    {
        return stamp;
    }

    inline bool Stamp::operator <(const Stamp& other) const
    {
        return stamp < other.stamp;
    }

    inline bool Stamp::operator >(const Stamp& other) const
    {
        return stamp > other.stamp;
    }

    inline bool Stamp::operator ==(const Stamp& other) const
    {
        return stamp == other.stamp;
    }

    inline bool Stamp::operator !=(const Stamp& other) const
    {
        return !operator ==(other);
    }

    inline bool Stamp::operator <=(const Stamp& other) const
    {
        return operator <(other) || operator ==(other);
    }

    inline bool Stamp::operator >=(const Stamp& other) const
    {
        return operator >(other) || operator ==(other);
    }

    inline time_t Stamp::getTime() const
    {
        return stamp;
    }

    inline Stamp Stamp::modDifference(const Stamp& other) const
    {
        return stamp > other ? stamp - other : other - stamp;
    }
}
#pragma pack(pop)
