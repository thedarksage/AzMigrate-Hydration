///
/// @file imdshelpers.h
/// Define helper functions to work on imds functions
///
#ifndef IMDSHELPERS__H
#define IMDSHELPERS__H

#include <iostream>
#include <string>
#include <vector>
#include <iterator>
#include <set>
#include <map>

typedef struct tagMemoryStruct
{
    char* memory;
    size_t insize;
    size_t size;
} MemoryStruct;

const long HTTP_OK = 200L;

#ifdef SV_WINDOWS
#define FUNCTION_NAME __FUNCTION__
#else
#define FUNCTION_NAME __func__
#endif /* SV_WINDOWS */



const std::string external_ip_address = "external_ip_address";
const char HTTPS[] = "https://";
const char IMDS_URL[]						= "http://169.254.169.254/metadata/instance?api-version=2021-12-13";
const char IMDS_ENDPOINT[]                  = "http://169.254.169.254/metadata/instance/";
const char IMDS_HEADERS[]					= "Metadata: true";
const char IMDS_COMPUTE_ENV[]				= "compute.azEnvironment";
const char IMDS_AZURESTACK_NAME[]			= "AzureStack";
const char IMDS_COMPUTE_TAGSLIST[]			= "compute.tagsList";
const char IMDS_FAILOVER_TAG_PREFIX[]		= "ASR-Failover";
const char IMDS_FAILOVER_TAG_SUFFIX[]		= "Failed-over by Azure Site Recovery.";
const char IMDS_AZURESTACK_APIVERSION[]     = "api-version=2021-02-01";

std::string GetImdsMetadata(const std::string& pathStr = std::string(), const std::string& apiVersion = std::string());

#endif //IMDSHELPERS__H
