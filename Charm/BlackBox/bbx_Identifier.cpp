#include "stdafx.h"
#include "bbx_Identifier.h"


std::wstring Bbx::Identifier::getSourceDescription() const
{
    Identifier::Source source = getSource();
    switch (source)
    {
    case HaronInput:
        return L"hi";
    case HaronOutput:
        return L"ho";
    case FundInput:
        return L"fi";
    case FundOutput:
        return L"fo";
    case Undefined:
        return L"UNDEFINED";
    default:
        return L"ERROR";
    }
}

std::wstring Bbx::Identifier::getDescription() const
{
    std::wstringstream ss;
    ss << getSourceDescription() << L":" << getId();
    return ss.str();
}
