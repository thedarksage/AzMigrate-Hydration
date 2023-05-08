#ifndef _RCM_CLIENT_UTILS_H
#define _RCM_CLIENT_UTILS_H

#include <curl/curl.h>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>

#include "RcmClientErrorCodes.h"

#define RCM_CLIENT_CATCH_EXCEPTION(errMsg, status) \
    catch (const JSON::json_exception& jsone) \
        { \
        errMsg += jsone.what(); \
        status = SVE_ABORT; \
        } \
    catch (const char *msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (const std::string &msg) \
        { \
        errMsg += msg; \
        status = SVE_ABORT; \
        } \
    catch (const std::exception &e) \
        { \
        errMsg += e.what(); \
        status = SVE_ABORT; \
        } \
    catch (...) \
        { \
        errMsg += "unknonw error"; \
        status = SVE_ABORT; \
        }

namespace RcmClientLib {

    class RcmClientUtils{
    public:
        static void TrimRcmResponse(std::string& response)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: ENTERED\n", FUNCTION_NAME);

            /// remove the UTF-8 byte-order-marker, if any
            size_t pos = std::string::npos;
            pos = response.find_first_of('{');

            if (pos != std::string::npos)
            {
                if (pos > 0)
                {
                    const char b1 = 0xEF, b2 = 0xBB, b3 = 0xBF;
                    const char utf8bom[] = { b1, b2, b3, 0 };

                    DebugPrintf(SV_LOG_DEBUG, "%s: found first '{' at position %d\n", FUNCTION_NAME, pos);

                    std::string marker = response.substr(0, pos);

                    if (marker.compare(utf8bom) == 0)
                    {
                        DebugPrintf(SV_LOG_DEBUG, "Found UTF-8 marker before JSON string.\n");
                    }
                    else
                    {
                        std::ostringstream ss;
                        ss << std::hex << std::showbase << std::uppercase;
                        for (unsigned int i = 0; i < pos; ++i)
                            ss << (0xff & response.at(i)) << " ";
                        DebugPrintf(SV_LOG_ERROR, "Unknown marker before JSON string: %s\n", ss.str().c_str());
                    }

                    response.erase(0, pos);
                }
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: EXITED\n", FUNCTION_NAME);

            return;
        }

        static RCM_CLIENT_STATUS GetRcmClientErrorForCurlError(long responseErrorCode)
        {
            RCM_CLIENT_STATUS errorCode = RCM_CLIENT_POST_FAILED;

            if (responseErrorCode == CURLE_SSL_CACERT)
                errorCode = RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR;
            else if (responseErrorCode == CURLE_COULDNT_CONNECT)
                errorCode = RCM_CLIENT_COULDNT_CONNECT;
            else if (responseErrorCode == CURLE_COULDNT_RESOLVE_HOST)
                errorCode = RCM_CLIENT_COULDNT_RESOLVE_HOST;
            else if (responseErrorCode == CURLE_COULDNT_RESOLVE_PROXY)
                errorCode = RCM_CLIENT_COULDNT_RESOLVE_PROXY;
            else if (responseErrorCode == CURLE_OPERATION_TIMEDOUT)
                errorCode = RCM_CLIENT_ERROR_NETWORK_TIMEOUT;

            return errorCode;
        }

        /// \brief find an attribute value for a given key
        /// \returns
        /// \li string if found
        /// \li empty string if not found
        /// \throws exception if key not found and throwIfNotFound is set
        static std::string GetAttributeValue(Attributes_t const& attrs,
            const std::string& key,
            bool throwIfNotFound = false)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: ENTERED\n", FUNCTION_NAME);
            ConstAttributesIter_t iter = attrs.find(key);
            if (iter != attrs.end())
            {
                return iter->second;
            }

            if (throwIfNotFound)
            {
                std::stringstream err;
                err << "Error: no attribute found for key "
                    << key
                    << ".";

                throw std::runtime_error(err.str().c_str());
            }
            
            DebugPrintf(SV_LOG_DEBUG, "%s: EXITED\n", FUNCTION_NAME);

            return std::string("");
        }


        /// \brief return an array of tokens for a given input and delimiter
        /// \returns
        /// \li vector<string> if found
        /// \li empty vector<string> if not able to tokenize
        static std::vector<std::string> GetTokens(const std::string &input, const std::string &delimiters)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: ENTERED with input %s and delim %s \n",
                FUNCTION_NAME,
                input.c_str(),
                delimiters.c_str());

            const boost::char_separator<char> delm(delimiters.c_str());
            boost::tokenizer< boost::char_separator<char> > strtokens(input, delm);

            std::vector<std::string> tokens;
            BOOST_FOREACH(std::string t, strtokens)
            {
                tokens.push_back(t);
                DebugPrintf(SV_LOG_DEBUG, "%s: %s\n", FUNCTION_NAME, t.c_str());
            }

            DebugPrintf(SV_LOG_DEBUG, "%s: EXITED\n", FUNCTION_NAME);

            return tokens;
        }
    };
}
#endif
