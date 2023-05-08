/// \file  inmcryptotraits.h
///
/// \brief utility routines for base64 conversion
///
///

#ifndef INM_BASE64_TRAITS_H
#define INM_BASE64_TRAITS_H

#include <map>
#include <utility>

class inm_base64traits
{
public:

    // note:
    // we neeed this method as c++ standatd does not support 
    // static map initialization
    // once we upgrade all our compilers to c+11 standard,we may
    // change this piece of code

    static std::map<char,unsigned char> initialize_binaryvalues()
    {
        std::map<char,unsigned char> m;
        for(unsigned int i = 0;  i < sizeof(base64values) ; i++)
            m.insert(std::make_pair(base64values[i],i));

        return m;
    }

    static char base64(unsigned int idx) { return base64values[idx] ; }
    static  unsigned char binary(char input)  { std::map<char,unsigned char>::const_iterator it = binaryvalues.find(input); return it ->second; }

private:

    static const char base64values[64];
    static const std::map<char,unsigned char> binaryvalues;
};


const char inm_base64traits::base64values[64] = { 
    'A', 'B', 'C', 'D', 'E', 'F', 'G','H','I','J','K',
    'L','M','N','O','P','Q','R','S','T','U','V','W',
    'X', 'Y', 'Z','a','b','c','d','e','f','g','h','i',
    'j','k','l','m','n','o','p','q','r','s','t','u',
    'v','w','x','y','z','0','1','2','3','4','5','6','7',
    '8','9','+','/' 
};


const std::map<char,unsigned char> inm_base64traits::binaryvalues =  inm_base64traits::initialize_binaryvalues();

#endif
