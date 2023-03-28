#ifndef HCAP_CODES_H
#define HCAP_CODES_H
#include <curl/curl.h>
#include<map>

struct hcapHttpResponse
{
private:
	static std::map<int, hcapHttpResponse> hcapResponseMap  ;
	std::string m_meaning ;
	std::string m_description ;
	hcapHttpResponse(const std::string& meaning, const std::string& description)
	{
		m_meaning = meaning ;
		m_description = description ;
	}
public:
	std::string static meaning(int responseCode)
	{
		return ((hcapHttpResponse)hcapResponseMap.find(responseCode)->second).m_meaning ;
	}
	std::string static description(int responseCode) 
	{
		return ((hcapHttpResponse)hcapResponseMap.find(responseCode)->second).m_description ;
	}
	static void init()
	{
		hcapResponseMap.insert(std::make_pair(200, hcapHttpResponse("OK", "HCAP successfully retrieved the specified data object, metafile, or directory.") )) ;
		hcapResponseMap.insert(std::make_pair(201, hcapHttpResponse("Created", "HCAP Successfully Added the new object to archive"))) ;
		hcapResponseMap.insert(std::make_pair(206, hcapHttpResponse("Partial Content", "For a byte range request, HCAP successfully retrieved the data in the specified range."))) ;
		hcapResponseMap.insert(std::make_pair(300, hcapHttpResponse("Multiple choice", "For a request by cryptographic hash value, HCAP found two or more objects with the specified hash value."))) ;
		hcapResponseMap.insert(std::make_pair(400, hcapHttpResponse("Bad Request", "URL request is not well-formed or request contains unsupported param"))) ;
		hcapResponseMap.insert(std::make_pair(403, hcapHttpResponse("Forbidden", "You dont have permissions to read/write specified data object, metafile or directory."))) ;
		hcapResponseMap.insert(std::make_pair(404, hcapHttpResponse("Not Found", "HCAP could not find the specified data object, metafile, or directory."))) ;
		hcapResponseMap.insert(std::make_pair(405, hcapHttpResponse("Method/Request Not Supported", "Request specified incorrectly") ));
		hcapResponseMap.insert(std::make_pair(409, hcapHttpResponse("Conflict", "HCAP could not add data object to archive because it already exists"))) ;
		hcapResponseMap.insert(std::make_pair(413, hcapHttpResponse("File too large", "Not enough space available to store the data object. Free up some space"))) ;
		hcapResponseMap.insert(std::make_pair(414, hcapHttpResponse("Request URI is too long", "The portion of the URL following fcfs_data or fcfs_metadata is longer than 4,095 bytes."))) ;
		hcapResponseMap.insert(std::make_pair(416, hcapHttpResponse("Requested range not satisfiable", "The specified start position is greater than the size of the requested data or size of specified range is 0"))) ;
		hcapResponseMap.insert(std::make_pair(500, hcapHttpResponse("Internal server error", "An internal error occurred. If this happens repeatedly, please contact your authorized service provider"))) ;
		hcapResponseMap.insert(std::make_pair(503, hcapHttpResponse("Service unavailable", "HCAP is temporarily unable to handle the request, probably to due to system overload or node maintenance."))) ;
	}
} ;
std::map<int, hcapHttpResponse> hcapHttpResponse::hcapResponseMap  ;
#endif

