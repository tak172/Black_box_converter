#include <boost/utility/string_ref_fwd.hpp>

// вывод time_t в стандартном виде
std::string  time_to_iso( time_t val );
std::wstring time_to_isow( time_t val );
time_t time_from_iso( boost::string_ref str );
