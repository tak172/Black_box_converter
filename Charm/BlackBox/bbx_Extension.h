#pragma once

#include "bbx_BlackBox.h"

/**
    Changelog:
    version | changes
        1.0 | Введена система версирования, изменён заголовок страницы (добавлен Identifier),
            |   потеряна обратная совместимость с ЧЯ версией 0
    */

namespace pugi
{
    class xml_node;
}

namespace Bbx
{
    namespace Impl
    {
        /** @brief Содержит версию чёрного ящика */
        class Version
        {
        public:
            Version();
            Version(unsigned _major, unsigned _minor);
            ~Version();
            bool load(const pugi::xml_node& parentNode);
            bool isSupported() const;

            void serialize(pugi::xml_node& parentNode) const;

        private:
            unsigned m_major;
            unsigned m_minor;

            static const char* c_nodeVersion;
            static const char* c_attrMajor;
            static const char* c_attrMinor;
        };

        /** @brief Текущая версия чёрного ящика */
        const Version c_currentVersion = Version(1u, 0u);

        /** @brief Метаинформация о записанном чёрном ящике, включает в себя версию */
        class Extension
        {
        public:
            static const char* c_nodeRoot;
            static const char* c_nodeLocalize;
            static const char* c_attrTZ;
            Extension();
            ~Extension();
            bool load(const Bbx::Buffer& extensionBuffer);
            void setActualVersion();
            void setTimeZone( std::string textTZ );

            const Version getVersion() const;
            std::string getTimeZone() const;

            std::string serialize() const;
            
        private:
            Version version;
            std::string timeZone;
        };
    }
}