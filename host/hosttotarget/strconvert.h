
///
/// \file strconvert.h
///
/// \brief
///

#ifndef STRCONVERT_H
#define STRCONVERT_H

#include <clocale>
#include <locale>
#include <string>

/// \brief converts char to wide char
char toNarrow(wchar_t wide,          ///< wchar to convert to char
              char defaultChar = '.' ///< char to use if conversion fails
              )
{
    std::locale const loc("");
    return std::use_facet<std::ctype<wchar_t> >(loc).narrow(wide, defaultChar);

}

/// \brief converts wchar_t* to std::string
inline std::string toNarrow(wchar_t const* wide,     ///< wide char buffer to convert must be NULL termindate
                            char defaultChar = '.'   ///< char to use if conversion fails
                            )
{
    std::string narrow;    
    for (wchar_t const* wCh = wide; L'\0' != *wCh; ++wCh) {
        narrow += toNarrow(*wCh, defaultChar);
    }
    return narrow;
}

/// \brief converts char to wide char
wchar_t toWide(char narrow) ///< char to convert to wide char
{
    std::locale const loc("");
    return std::use_facet<std::ctype<char> >(loc).widen(narrow);
}

/// \brief converts wchar_t* to std::string
inline std::wstring toWide(char const* narrow) ///< char buffer to convert, must be NULL terminated
{
    std::wstring wide;    
    for (char const* ch = narrow; '\0' != *ch; ++ch) {
        wide += toWide(*ch);
    }
    return wide;
}

#endif // STRCONVERT_H
