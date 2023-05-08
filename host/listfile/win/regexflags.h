
///
///  \file win/regexflags.h
///
///  \brief boost regex flags to use on windows
///

#ifndef REGEXFLAGS_H
#define REGEXFLAGS_H

#include <boost/regex.hpp>

/// \brief use perl reqular expression syntax and be cas insensitive since
/// this is ued for windows file/path names
#define REGEX_FLAGS (boost::regex::perl | boost::regex::icase)


#endif // REGEXFLAGS_H
