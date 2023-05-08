#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>

#include "CertUtil.h"
#include "HttpClient.h"
#include "logger.h"
#include "searchcertstore.h"
#include "RcmConfigurator.h"

#define HTTP_PREFIX     "http://"
#define HTTPS_PREFIX    "https://"

using namespace AzureStorageRest;
using namespace RcmClientLib;

CertUtil::CertUtil(const std::string& server,
    const std::string& port)
{

    std::string endUri(server);
    boost::algorithm::replace_all(endUri, HTTPS_PREFIX, "");
    boost::algorithm::replace_all(endUri, HTTP_PREFIX, "");
    size_t posUri = endUri.find_first_of("/");
    if (posUri != std::string::npos)
        endUri = endUri.substr(0, posUri);

    endUri = HTTPS_PREFIX + endUri + ":" + port;

    HttpClient certutilclient(false);
    HttpRequest request(endUri);
    HttpResponse response;
    HttpProxy proxy;

    if (RcmConfigurator::getInstance()->GetProxySettings(proxy.m_address, proxy.m_port, proxy.m_bypasslist))
    {
        certutilclient.SetProxy(proxy);
    }

    request.SetHttpMethod(AzureStorageRest::HTTP_GET);

    if (!certutilclient.GetResponse(request, response))
    {
        m_last_error = response.GetErrorString();
        return;
    }

    m_lastcertincertchain = certutilclient.GetLastCertInServerCertChain();
}

bool CertUtil::validate_root_cert(std::string& errmsg)
{
    return securitylib::searchRootCertInCertStore(m_lastcertincertchain, errmsg);
}