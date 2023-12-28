///
/// Define helper functions to work on imds functions
#include "stdafx.h"
#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <set>
#include <map>
#include "imdshelpers.h"
#include <curl/curl.h>
#include "errorexception.h"
#include "inmsafeint.h"
#include "inmsafecapis.h"

// create own trace method
// imdslib::trace_init(filename, callback) similar to ErrorLogger::INit

size_t WriteMemoryCallbackFileReplication(void* ptr, size_t size, size_t nmemb, void* data)
{
    size_t realsize;
    INM_SAFE_ARITHMETIC(realsize = InmSafeInt<size_t>::Type(size) * nmemb, INMAGE_EX(size)(nmemb))
        MemoryStruct* mem = (MemoryStruct*)data;

    size_t memorylen;
    INM_SAFE_ARITHMETIC(memorylen = InmSafeInt<size_t>::Type(mem->size) + realsize + 1, INMAGE_EX(mem->size)(realsize))
        mem->memory = (char*)realloc(mem->memory, memorylen);

    if (mem->memory) {
        inm_memcpy_s(&(mem->memory[mem->size]), realsize + 1, ptr, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;
    }

    return realsize;
}

std::string GetImdsMetadata(const std::string& pathStr, const std::string& apiVersion)
{
    std::string imdsUrl;
    if (!pathStr.empty() && !apiVersion.empty()) {
        imdsUrl = IMDS_ENDPOINT + pathStr + "?" + apiVersion;
    }
    else if (!apiVersion.empty()) {
        imdsUrl = IMDS_ENDPOINT + std::string("?") + apiVersion;
    }
    else {
        imdsUrl = IMDS_URL;
    }

//    TRACE("Imds url is %s\n", imdsUrl.c_str());

    MemoryStruct chunk = { 0 };
    CURL* curl = curl_easy_init();
    try
    {
        chunk.size = 0;
        chunk.memory = NULL;
        if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_URL, imdsUrl.c_str())) {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to set curl options IMDS_URL.\n";
        }

        if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_NOPROXY, "*")) {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to set curl options CURLOPT_NOPROXY.\n";
        }

        struct curl_slist* pheaders = curl_slist_append(NULL, IMDS_HEADERS);
        if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pheaders)) {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to set curl options CURLOPT_HEADERDATA.\n";
        }

        if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallbackFileReplication)) {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to set curl options CURLOPT_WRITEFUNCTION.\n";
        }

        if (CURLE_OK != curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&chunk))) {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to set curl options CURLOPT_WRITEDATA.\n";
        }

        CURLcode curl_code = curl_easy_perform(curl);

        if (curl_code == CURLE_ABORTED_BY_CALLBACK)
        {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to perfvorm curl request, request aborted.\n";
        }

        long response_code = 0L;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != HTTP_OK)
        {
            throw ERROR_EXCEPTION << FUNCTION_NAME << ": Failed to perform curl request, curl error "
                << curl_code << ": " << curl_easy_strerror(curl_code)
                << ", status code " << response_code
                << ((chunk.memory != NULL) ? (std::string(", error ") + chunk.memory) : "") << ".\n";
        }
    }
    catch (std::exception& e)
    {
        //TRACE_ERROR("%s: Failed  with exception: %s.\n", FUNCTION_NAME, e.what());
    }
    catch (...)
    {
        //TRACE_ERROR("%s: Failed  with exception.\n", FUNCTION_NAME);
    }

    std::string ret;
    if (chunk.memory != NULL)
    {
        ret = std::string(chunk.memory, chunk.size);
        free(chunk.memory);
    }
    curl_easy_cleanup(curl);

    //TRACE_FUNC_END;

    return ret;
}