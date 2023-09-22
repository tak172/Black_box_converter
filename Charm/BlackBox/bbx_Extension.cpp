#include "stdafx.h"

#include "bbx_Extension.h"

#include <sstream>

#include "../../AdoptTools/PugiXML/pugixml.hpp"

using namespace Bbx::Impl;

const char* Extension::c_nodeRoot = "extension";
const char* Extension::c_nodeLocalize = "localize";
const char* Extension::c_attrTZ = "tz";
const char* Version::c_nodeVersion = "version";
const char* Version::c_attrMajor = "major";
const char* Version::c_attrMinor = "minor";

Extension::Extension()
    : version(), timeZone()
{
}

Extension::~Extension()
{
}

void Extension::setActualVersion()
{
    version = c_currentVersion;
}

void Extension::setTimeZone( std::string textTZ )
{
    timeZone = textTZ;
}

bool Extension::load(const Bbx::Buffer& extensionBuffer)
{
    pugi::xml_document doc;
    timeZone.clear();
    if (doc.load_buffer(extensionBuffer.data_ptr, extensionBuffer.size))
    {
        pugi::xml_node rootNode = doc.child(c_nodeRoot);
        if (rootNode && version.load(rootNode))
        {
            timeZone = rootNode.child(c_nodeLocalize).attribute(c_attrTZ).as_string();
            return true;
        }
    }

    return false;
}

std::string Extension::serialize() const
{
    pugi::xml_document doc; 
    pugi::xml_node rootNode = doc.append_child(c_nodeRoot);

    version.serialize(rootNode);
    rootNode.append_child(c_nodeLocalize).append_attribute(c_attrTZ).set_value( timeZone.c_str() );

    std::stringstream ss;
    doc.print(ss);
    return ss.str();
}

const Version Extension::getVersion() const
{
    return version;
}

std::string Extension::getTimeZone() const
{
    return timeZone;
}

Version::Version(unsigned _major, unsigned _minor)
    : m_major(_major), m_minor(_minor)
{
}

Version::Version()
    : m_major(0u), m_minor(0u)
{
}

Version::~Version()
{
}

bool Version::load(const pugi::xml_node& parentNode)
{
    pugi::xml_node versionNode = parentNode.child(c_nodeVersion);

    if (versionNode)
    {
        pugi::xml_attribute majorAttr = versionNode.attribute(c_attrMajor);
        if (majorAttr)
        {
            m_major = majorAttr.as_uint(0u);
            pugi::xml_attribute minorAttr = versionNode.attribute(c_attrMinor);
            if (minorAttr)
            {
                m_minor = minorAttr.as_uint(0u);
                return true;
            }
        }
    }

    return false;
}

void Version::serialize(pugi::xml_node& parentNode) const
{
    pugi::xml_node versionNode = parentNode.append_child(c_nodeVersion);
    versionNode.append_attribute(c_attrMajor).set_value(m_major);
    versionNode.append_attribute(c_attrMinor).set_value(m_minor);
}

bool Version::isSupported() const
{
    // Игнорируем minor версию. Нужно как следует продумать эту проверку, но пока будет так.
    return m_major == c_currentVersion.m_major;
}