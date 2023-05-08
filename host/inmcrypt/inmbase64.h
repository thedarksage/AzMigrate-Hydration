/// \file  inmbase64.h
///
/// \brief for converting from binary to base64 format and vice versa
///
///

#ifndef INM_BASE64_H
#define INM_BASE64_H

#include <vector>
#include <memory>

#include "inmbase64traits.h"

#include "inmsafeint.h"
#include "inmageex.h"

template <typename BASE64_TRAITS >
class inmbase64
{
public:

    static std::auto_ptr<char> to_base64(const unsigned char * binary_input, size_t input_len, size_t & out_len)
    {

        // base64 encoding converts three octets into four encoded characters
        // that is, it converts groups of 6 bits (6 bits have a maximum of 26 = 64 different binary values) 
        // into individual numbers 

        // when the number of bytes to encode is not divisible by 3 
        // for the last block,extra bytes with value zero are appened to input so there are three bytes and conversion to base64 is performed.
        // if there was only one significant input byte, only the first two base64 digits are picked (12 bits),
        // and if there were two significant input bytes, the first three base64 digits are picked (18 bits). 
        // '=' characters might be added to make the last block contain four base64 characters.

        size_t remaining_bytes = input_len %3;
        INM_SAFE_ARITHMETIC(out_len = 4 * InmSafeInt<size_t>::Type(input_len) / 3, INMAGE_EX(input_len))

        if(remaining_bytes) {
            INM_SAFE_ARITHMETIC(out_len +=InmSafeInt<int>::Type(4), INMAGE_EX(out_len))
        }

        std::auto_ptr<char> bufptr;
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<size_t>::Type(out_len)+1, INMAGE_EX(out_len))
        bufptr.reset(new char[buflen]);
        char * buffer = bufptr.get();

        size_t counter = 0;
        size_t i = 0;

        if(input_len >=3) 
        {
            for( i = 0; i <= input_len -3 ; i+=3)
            {
                unsigned char first_octet = binary_input[i];
                unsigned char second_octet = binary_input[i+1];
                unsigned char third_octet = binary_input[i+2];

                // first 6 bits from first_octet
                buffer[counter++] = BASE64_TRAITS::base64(first_octet >> 2 );

                // remaining 2 bits from first_octet and 4 bits from second_octet
                buffer[counter++] = BASE64_TRAITS::base64( ((first_octet&0x3)<<4) + (second_octet >>4) );

                // remaining 4 bits from second_octet and 2 bits from third_octet
                buffer[counter++] = BASE64_TRAITS::base64( ((second_octet&0x0f)<<2) + (third_octet >>6) );

                // remaining 6 bits from third octet
                buffer[counter++] = BASE64_TRAITS::base64( third_octet&0x3f );
            }
        }

        if(1 == remaining_bytes )
        {
            unsigned char last_octet = binary_input[i];
            // 6 bits from last remaining octet
            buffer[counter++] = BASE64_TRAITS::base64( last_octet >> 2 );

            // remaining 2 bits padded with zeros
            buffer[counter++] = BASE64_TRAITS::base64( (last_octet &0x3)<<4 );

            // next two bytes will be == to make one block of 4 encoded character
            buffer[counter++] = '=';
            buffer[counter++] = '=';
        }

        if(2 == remaining_bytes )
        {
            unsigned char last_but_one_octet = binary_input[i];
            unsigned char last_octet = binary_input[i+1];

            // 6 bits from last but one octet
            buffer[counter++] = BASE64_TRAITS::base64( last_but_one_octet >> 2 );

            // remaining 2 bits from last but one octet and 4 bits from last octet
            buffer[counter++] = BASE64_TRAITS::base64( ((last_but_one_octet &0x3)<<4) + (last_octet >>4) );

            // remaining 4 bits padded with zeros
            buffer[counter++] = BASE64_TRAITS::base64( (last_octet &0x0f)<<2 );

            // next bytes will be = to make one block of 4 encoded character
            buffer[counter++] = '=';
        }

        buffer[counter]='\0';
        return bufptr;

    }

    static std::auto_ptr<unsigned char> to_binary(const char * base64_input, size_t input_len, size_t & out_len)
    {
        // base64 encoding converts three octets into four encoded characters
        // so decoding would convert four encoded character to three octets

        // first check: input_len is atleast 4 and multiple of 4


        // during the encoding process, when the number of bytes to encode is not divisible by 3 
        // for the last block,extra bytes with value zero are appened to input so there are three bytes and conversion to base64 is performed.
        // if there was only one significant input byte, only the first two base64 digits are picked (12 bits),
        // and if there were two significant input bytes, the first three base64 digits are picked (18 bits). 
        // '=' characters might be added to make the last block contain four base64 characters.
        // so during decoding, we have to ignore '=' characters

        size_t ignore_chars = 0;
        if('=' == base64_input[ input_len - 1 ]) ++ignore_chars;
        if('=' == base64_input[ input_len - 2 ]) ++ignore_chars;

        out_len = 3 * input_len / 4 - ignore_chars;

        std::auto_ptr<unsigned char> bufptr(new unsigned char[out_len]);
        unsigned char * buffer = bufptr.get();


        size_t counter = 0;
        size_t i = 0;

        if(input_len >= (4 + ignore_chars))
        {
            for( i = 0; i <= input_len - 4 - ignore_chars ; i+=4)
            {
                unsigned char first_six_bits = BASE64_TRAITS::binary(base64_input[i]); 
                unsigned char second_six_bits = BASE64_TRAITS::binary(base64_input[i+1]); 
                unsigned char third_six_bits = BASE64_TRAITS::binary(base64_input[i+2]);
                unsigned char fourth_six_bits = BASE64_TRAITS::binary(base64_input[i+3]);

                // first binary byte (6 bits + 2 bits)
                buffer[counter++] =  (first_six_bits << 2) | (second_six_bits >> 4);

                // second binary byte ( 4 bits + 4 bits)
                buffer[counter++] =  (second_six_bits <<4) | (third_six_bits >> 2);

                // third binary byte ( 2 bits + 6 bits )
                buffer[counter++] = (third_six_bits <<6) | (fourth_six_bits);
            }
        }

        if(1 == ignore_chars)
        {
            unsigned char first_six_bits = BASE64_TRAITS::binary(base64_input[i]); 
            unsigned char second_six_bits = BASE64_TRAITS::binary(base64_input[i+1]); 
            unsigned char third_six_bits = BASE64_TRAITS::binary(base64_input[i+2]);

            // last but one binary byte (6 bits + 2 bits)
            buffer[counter++] =  (first_six_bits << 2) | (second_six_bits >> 4);

            // last binary byte ( 4 bits + 4 bits)
            buffer[counter++] =  (second_six_bits <<4) | (third_six_bits >> 2);
        }

        if(2 == ignore_chars)
        {
            unsigned char first_six_bits = BASE64_TRAITS::binary(base64_input[i]); 
            unsigned char second_six_bits = BASE64_TRAITS::binary(base64_input[i+1]); 

            // last binary byte (6 bits + 2 bits)
            buffer[counter++] =  (first_six_bits << 2) | (second_six_bits >> 4);
        }

        return bufptr;
    }

protected:

private:

};
#endif
