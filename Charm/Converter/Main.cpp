#include "stdafx.h"

#include "bbx_Requirements.h"
#include "bbx_Location.h"
#include "bbx_Stamp.h"
#include "bbx_Reader.h"

#include "Utf8.h"

const unsigned MINFILESIZE = 1;

enum class operation 
{
    division,
    filtration
};

Bbx::Location PathToLocation(const std::wstring& str)
{
    size_t folder_end = str.find_last_of('\\');
    size_t pref_end = str.find_last_of('_');
    size_t suff_begin = str.find_last_of('.');

    if (folder_end == std::wstring::npos || pref_end == std::wstring::npos || suff_begin == std::wstring::npos)
        return {};

    return { {str, 0, folder_end}, {str, folder_end + 1, pref_end - folder_end}, {str, suff_begin, str.size() - suff_begin} };
}

std::string HexToBinary(const std::string& hex_str)
{
    std::string binary_str;

    for (size_t i = 2; i < hex_str.length(); i += 2)
    {
        std::string byte_str = hex_str.substr(i, 2);
        try
        {
            uint8_t byte = std::stoi(byte_str, nullptr, 16);
            binary_str.push_back(static_cast<char>(byte));
        }
        catch (const std::exception&)
        {
            binary_str.clear();
            break;
        }
    }

    return binary_str;
}

bool PushRecord(Bbx::Reader& reader, std::shared_ptr<Bbx::Writer>& writer, operation oper, std::string& pattern = std::string())
{
    Bbx::Stamp stamp;
    Bbx::char_vec caption;
    Bbx::char_vec data;

    Bbx::ReadResult result = reader.readAnyRecord(stamp, caption, data);

    // что бы не писать в параметрах каждой функции тернарный оператор
    Bbx::Buffer caption_buff = caption.empty() ? Bbx::Buffer() : caption;
    Bbx::Buffer data_buff = data.empty() ? Bbx::Buffer() : data;

    if (result != Bbx::ReadResult::Success)
    {
        std::cerr << "Error reading data from black box." << std::endl;
        return false;
    }

    Bbx::RecordType recordType = reader.getCurrentType();

    if (recordType == Bbx::RecordType::Reference)
    {
        if (!writer->pushReference(caption_buff, data_buff, stamp, reader.getCurrentIdentifier()))
            return false;
    }
    else
    {
        if (oper == operation::filtration)
        {
            std::string find_data = data.data() ? data.data() : "";
            std::string find_caption = caption.data() ? caption.data() : "";

            if (pattern.empty()) {
                std::cerr << "Invalid hexadecimal pattern." << std::endl;
                return false;
            }

            // если не нашли - выходим
            if (std::search(find_data.begin(), find_data.end(), pattern.begin(), pattern.end()) == find_data.end() &&
                std::search(find_caption.begin(), find_caption.end(), pattern.begin(), pattern.end()) == find_caption.end())
                return true;
        }

        if (recordType == Bbx::RecordType::IncomingPackage)
        {
            if (!writer->pushIncomingPackage(caption_buff, data_buff, stamp, reader.getCurrentIdentifier()))
                return false;
        }
        else if (recordType == Bbx::RecordType::OutboxPackage)
        {
            if (!writer->pushOutboxPackage(caption_buff, data_buff, stamp, reader.getCurrentIdentifier()))
                return false;
        }
        else if (recordType == Bbx::RecordType::Increment)
        {
            Bbx::char_vec data_before, caption_before;
            Bbx::Stamp stamp_before;

            reader.setDirection(false);
            reader.readIncrementOriented(stamp_before, caption_before, data_before);
            reader.setDirection(true);

            if (!writer->pushIncrement(caption_buff, data_before.empty() ? Bbx::Buffer() : data_before, data_buff, stamp, reader.getCurrentIdentifier()))
                return false;
        }
    }

    return true;
}

bool Division(Bbx::Reader& reader, std::shared_ptr<Bbx::Writer>& writer)
{
    writer->setRecomendedFileSize(MINFILESIZE);

    while (true)
    {
        if (!PushRecord(reader, writer, operation::division))
        {
            std::cerr << "Failed to write record." << std::endl;
            return false;
        }

        auto res = reader.next();

        if (res == Bbx::ReadResult::NoDataAvailable)
            break;

        if (res == Bbx::ReadResult::NewSession)
            reader.forceNext();
    }
    return true;
}

bool Filtration(Bbx::Reader& reader, std::shared_ptr<Bbx::Writer>& writer, std::string& pattern)
{
    // если обнаружили префикс вначале
    if (pattern.find_first_of("0x") == 0)
        pattern = HexToBinary(pattern);

    while (true)
    {
        if (!PushRecord(reader, writer, operation::filtration, pattern))
        {
            std::cerr << "Failed to write record." << std::endl;
                return false;
        }

        auto res = reader.next();

        if (res == Bbx::ReadResult::NoDataAvailable)
            break;

        if (res == Bbx::ReadResult::NewSession)
            reader.forceNext();
    }
    return true;
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: Converter.exe <input_path\\pref_.suff> <operation> <output_path\\pref_.suff>\n"
            "<input_path\\pref_.suff> - enter the path to the folder where the black box files are located and specify their prefix and suffix.\n"
            "<operation> - selecting the operation to be performed on the black box:\n"
            "division - dividing the black box into smaller files.\n"
            "filtration - input black box filtering by pattern.\n"
            "This operation requires an additional parameter - pattern by which the records will be filtered(fifth parameter).\n"
            "<output_path\\pref_.suff> - enter the path to the folder where the resulting black box will be located, and specify its prefix and suffix.\n";
        return 1;
    }

    // На Windows - From1251()
    std::wstring i_path(FromUtf8(argv[1])), o_path(FromUtf8(argv[3]));
    std::string operation(argv[2]);

    Bbx::Reader reader(PathToLocation(i_path));
    auto writer = Bbx::Writer::create(PathToLocation(o_path));

    reader.setDirection(true);
    Bbx::Stamp beg = reader.getBoundStamp().first;
    reader.rewind(beg.getTime());

    if (!writer || !reader.isOpened())
    {
        std::cerr << "Failed to create Writer or Reader(wrong paths)." << std::endl;
        return 1;
    }

    if (operation == "division")
    {
        if (!Division(reader, writer))
            return 1;
    }
    else if (operation == "filtration")
    {
        if (argc < 5)
        {
            std::cerr << "This operation requires a search pattern.\n"
                "Usage: Converter.exe <input_path\\pref_.suff> <operation> <output_path\\pref_.suff> <pattern>\n"
                "<pattern> - pattern by which the records will be filtered(fifth parameter).\n"
                "Use the 0x prefix to filter on binary data, which is specified in hexadecimal, for example <0x4B>.\n";
            return 1;
        }

        std::string pattern = argv[4];

        if (!Filtration(reader, writer, pattern))
            return 1;
    }
    else
    {
        std::cerr << "Unknown operation. Enter <division> to divide or <filtration> to filter." << std::endl;
        return 1;
    }

    std::cout << "Operation completed successfully!" << std::endl;
    return 0;
}
