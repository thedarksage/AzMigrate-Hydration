#ifndef INMAGESDK_UTIL_H
#define INMAGESDK_UTIL_H
#include <string>
#ifdef SV_WINDOWS
bool ChangeNewRepoDriveLetter(const std::string& existingRepo,
                              const std::string& newRepo) ;

bool SetSparseAttribute( const std::string& file) ;
#endif
#endif
