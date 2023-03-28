///
///  \file  IFileDIValidator.h
///
///  \brief contains IFileDIValidator interface
///

#ifndef IFILEDIVALIDATOR_H
#define IFILEDIVALIDATOR_H

#include <cstdio>
#include <string>

#include "boost/shared_ptr.hpp"

/// \brief IFileDIValidator interface
class IFileDIValidator
{
public:
	/// \brief Pointer type
    typedef boost::shared_ptr<IFileDIValidator> Ptr;

	virtual bool Validate(const std::string &sourceFile, ///< source file
		                  const std::string &targetFile  ///< target file
                          ) = 0;
};

#endif /* IFILEDIVALIDATOR_H */
