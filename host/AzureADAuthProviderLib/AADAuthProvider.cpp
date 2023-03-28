#include "AADAuthProvider.h"
#include "common/Trace.h"
#include "sslsign.h"
#include "HttpClient.h"
#include "securityutils.h"
#include "defaultdirs.h"

#ifdef WIN32
#include "signmessage.h"
#endif

#include <ace/Guard_T.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace AADAuthLib
{
    const char OAUTH_TYPE2[] = "oauth2";
    const char OAUTH_TOKEN[] = "token";
    const char OAUTH_GRANT_TYPE[] = "grant_type";
    const char OAUTH_CLIENT_CREDENTIALS[] = "client_credentials";
    const char OAUTH_CLIENT_ID[] = "client_id";
    const char OAUTH_CLIENT_ASSERTION_TYPE[] = "client_assertion_type";
    const char OAUTH_ASSERTION_JWT_BEARER[] = "urn:ietf:params:oauth:client-assertion-type:jwt-bearer";
    const char OAUTH_CLIENT_ASSERTION[] = "client_assertion";
    const char OAUTH_RESOURCE[] = "resource";

    const char OAUTH_TOKEN_TYPE[] = "token_type";
    const char OAUTH_ACCESS_TOKEN[] = "access_token";
    const char OAUTH_ACCESS_TOKEN_EXPIRY_DURATION[] = "expires_in";

    const char JWT_HEADER_TOKEN_TYPE[] = "typ";
    const char JWT_HEADER_ALGORITHM[] = "alg";
    const char JWT_HEADER_X509THUMBPRINT[] = "x5t";

    const char JWT_TOKEN_TYPE[] = "JWT";
    const char ALGORITH_RS256[] = "RS256";

    const char JWT_PAYLOAD_AUDIENCE[] = "aud";
    const char JWT_PAYLOAD_ISSUER[] = "iss";
    const char JWT_PAYLOAD_SUBJECT[] = "sub";
    const char JWT_PAYLOAD_JWTID[] = "jti";
    const char JWT_PAYLOAD_NOT_BEFORE[] = "nbf";
    const char JWT_PAYLOAD_EXPIRATION[] = "exp";
    const char JWT_PAYLOAD_ISSUED_AT[] = "iat";

    const char ContentType[] = "Content-type";
    const char FormUrlEncoded[] = "application/x-www-form-urlencoded";
    const char HTTPS_PREFIX[] = "https://";
    const char OAUTH_BEARER_TOKEN[] = "Bearer";

    const char ADFS[] = "adfs";

    // OAuth2 Error code strings
    // ref: https://docs.microsoft.com/en-us/azure/active-directory/develop/reference-aadsts-error-codes
    const char OAUTH_ERROR_INVALID_CLIENT[] = "invalid_client";
    const char OAUTH_ERROR_UNAUTHORIZED_CLIENT[] = "unauthorized_client";

    //AADSTS error codes
    const long AADSTS700016 = 700016;
    const long AADSTS700024 = 700024;

#ifdef WIN32
    __declspec(thread) RcmClientLib::RCM_CLIENT_STATUS AADAuthProvider::m_errorCode;
#else
    __thread RCM_CLIENT_STATUS AADAuthProvider::m_errorCode;
#endif

    std::string GenerateUuid()
    {
        std::stringstream suuid;

        try
        {
            boost::uuids::random_generator uuid_gen;

            boost::uuids::uuid new_uuid = uuid_gen();

            suuid << new_uuid;
        }
        catch (const std::exception& e)
        {
            TRACE_ERROR("%s: could not generate UUID with exception: %s.\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            TRACE_ERROR("%s: could not generate UUID with unkown exception.\n", FUNCTION_NAME);
        }
        return suuid.str();
    }

    uint64_t GetTimeInSecSinceEpoch1970()
    {
        // time_duration is 32-bit, so in 2038 this will overflow
        uint64_t secSinceEpoch = boost::posix_time::time_duration(boost::posix_time::second_clock::universal_time()
            - boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1))).total_seconds();

        return secSinceEpoch;
    }

    bool AzureAuth::GetAccessToken(std::string& token)
    {
        TRACE_FUNC_BEGIN;

        if (!m_expiryTimeInSecSinceEpoch)
        {
            TRACE_INFO("No Access Token cached.\n");
            return false;
        }

        uint64_t timenow = GetTimeInSecSinceEpoch1970();
        if (timenow > m_expiryTimeInSecSinceEpoch)
        {
            TRACE_INFO("Access Token expired.\n");
            return false;
        }

        if (!m_accessToken.length())
        {
            TRACE_INFO("Access token is empty\n");
            return false;
        }

        token = m_accessToken;

        TRACE_FUNC_END;
        return true;
    }

    SVSTATUS AzureAuth::StoreAccessToken(std::string& response)
    {
        TRACE_FUNC_BEGIN;

        SVSTATUS status = SVE_FAIL;
        std::stringstream jsonStream(response);
        boost::property_tree::ptree propTree;

        do {
            try {
                boost::property_tree::read_json(jsonStream, propTree);
            }
            catch (boost::property_tree::json_parser_error& pe)
            {
                TRACE_ERROR("%s: error reading received token response %s.\n", FUNCTION_NAME, pe.what());
                break;
            }

            std::string token_type = propTree.get(OAUTH_TOKEN_TYPE, "");
            if (!boost::iequals(token_type, OAUTH_BEARER_TOKEN))
            {
                TRACE_ERROR("%s: unknown bearer token type: %s\n", FUNCTION_NAME, token_type.c_str());
                break;
            }

            m_accessToken = propTree.get(OAUTH_ACCESS_TOKEN, "");
            if (m_accessToken.empty())
            {
                TRACE_ERROR("%s: received empty bearer token.\n", FUNCTION_NAME);
                break;
            }

            std::string expiresIn = propTree.get(OAUTH_ACCESS_TOKEN_EXPIRY_DURATION, "");
            if (expiresIn.empty())
            {
                TRACE_ERROR("%s: received empty expiry duration in token.\n", FUNCTION_NAME);
                break;
            }

            try {
                uint64_t duration = boost::lexical_cast<uint64_t>(expiresIn);
                uint64_t timenow = GetTimeInSecSinceEpoch1970();
                m_expiryTimeInSecSinceEpoch = timenow + duration;
            }
            catch (boost::bad_lexical_cast& cast_exp)
            {
                TRACE_ERROR("%s: received bad expiry duration '%s' in token.\n", FUNCTION_NAME, expiresIn.c_str());
                m_expiryTimeInSecSinceEpoch = 0;
                break;
            }

            status = SVS_OK;

        } while (false);

        TRACE_FUNC_END;
        return status;
    }

    bool AADAuthProvider::IsUriAdfs(const std::string& AADUri)
    {
        size_t pos = 0;
        int segCnt = 0;
        std::string AADUri_parsed = AADUri;
        bool uriAdfs = false;

        // AAD: https://xxx.xxx.xxx/<AADTenantId>/
        // ADFS: https://xxx.xxx.xxx/adfs/
        while ((pos = AADUri_parsed.find(URI_DELIMITER::PATH)) != std::string::npos && segCnt != 3)
        {
            AADUri_parsed.erase(0, pos + 1);
            segCnt++;
        }

        if (segCnt == 3)
        {
            uriAdfs = boost::iequals(AADUri_parsed.substr(0, pos == std::string::npos ? AADUri_parsed.length() : pos), ADFS);
        }

        return uriAdfs;
    }

    std::string AADAuthProvider::JwtBase64Encode (const std::string& data)
    {
        std::string encData = securitylib::base64Encode(data.c_str(),
            data.length());
        boost::replace_all(encData, "+", "-");
        boost::replace_all(encData, "/", "_");
        boost::replace_all(encData, "=", "");
        return encData;
    }

    SVSTATUS AADAuthProvider::GetJwtRequest(std::string& jwtRequest)
    {
        TRACE_FUNC_BEGIN;

        SVSTATUS status = SVS_OK;
        std::string errMsg;
        boost::property_tree::ptree propTree;
        std::stringstream jwt;

        std::string fingerprint;

        /// AADAuthCert is fingerprint of cert in hex format
        if ((m_clientSettings.m_AADAuthCert.length() % 2) != 0) {
            TRACE_ERROR("%s: AAD Auth Cert length %d is not even number.\n",
                __FUNCTION__,
                m_clientSettings.m_AADAuthCert.length());
            m_errorCode = RCM_CLIENT_CREATE_JWT_FAILED;
            return SVE_INVALIDARG;
        }

        /// convert to binary format
        size_t cnt = m_clientSettings.m_AADAuthCert.length() / 2;
        for (size_t i = 0; cnt > i; ++i)
        {
            uint32_t s = 0;
            std::stringstream ss;
            ss << std::hex << m_clientSettings.m_AADAuthCert.substr(i * 2, 2);
            ss >> s;

            fingerprint.push_back(static_cast<unsigned char>(s));
        }

        std::string thumbprint = JwtBase64Encode(fingerprint);

        /// add the JWT header
        propTree.put(JWT_HEADER_TOKEN_TYPE, JWT_TOKEN_TYPE);
        propTree.put(JWT_HEADER_ALGORITHM, ALGORITH_RS256);
        propTree.put(JWT_HEADER_X509THUMBPRINT, thumbprint.c_str());

        std::ostringstream jwtBuf;
        boost::property_tree::write_json(jwtBuf, propTree);
        std::string header = jwtBuf.str();
        jwt << JwtBase64Encode(header);

        jwtBuf.str("");
        propTree.clear();

        std::string AADUri;
        if (!boost::starts_with(m_clientSettings.m_AADUri, HTTPS_PREFIX))
            AADUri += HTTPS_PREFIX;

        AADUri += m_clientSettings.m_AADUri;

        bool uriAdfs = IsUriAdfs(AADUri);

        // For AAD, AADUri should be /<AADTenantId>/oauth2/token
        // For ADFS, AADUri should be /adfs/oauth2/token
        if (!uriAdfs &&
            !boost::icontains(m_clientSettings.m_AADUri, m_clientSettings.m_AADTenantId))
        {
            AADUri += URI_DELIMITER::PATH;
            AADUri += m_clientSettings.m_AADTenantId;
        }

        AADUri += URI_DELIMITER::PATH;
        AADUri += OAUTH_TYPE2;
        AADUri += URI_DELIMITER::PATH;
        AADUri += OAUTH_TOKEN;

        /// add the payload
        propTree.put(JWT_PAYLOAD_AUDIENCE, AADUri.c_str());
        propTree.put(JWT_PAYLOAD_ISSUER, m_clientSettings.m_AADClientId.c_str());
        propTree.put(JWT_PAYLOAD_SUBJECT, m_clientSettings.m_AADClientId.c_str());

        std::string jwtGuid = GenerateUuid();
        propTree.put(JWT_PAYLOAD_JWTID, jwtGuid.c_str());

        uint64_t timenow = GetTimeInSecSinceEpoch1970();
        propTree.put(JWT_PAYLOAD_NOT_BEFORE, timenow - 60 * 60);
        propTree.put(JWT_PAYLOAD_ISSUED_AT, timenow);
        propTree.put(JWT_PAYLOAD_EXPIRATION, timenow + 60 * 60);

        write_json(jwtBuf, propTree);
        std::string payload = jwtBuf.str();
        jwt << "." << JwtBase64Encode(payload);

        securitylib::signature_t signature;

#ifdef WIN32
        if (!securitylib::signmessage(errMsg, jwt.str(), signature, fingerprint))
        {
            TRACE_ERROR("%s: signmessage failed.Thumbprint=%s Error=%s\n", __FUNCTION__, m_clientSettings.m_AADAuthCert.c_str(), errMsg.c_str());
            if (errMsg.find("The certificate is not found in system store") != std::string::npos)
                m_errorCode = RCM_CLIENT_AAD_AUTH_CERT_NOT_FOUND;
            else if (errMsg.find("The private key is not found in cert") != std::string::npos)
                m_errorCode = RCM_CLIENT_AAD_PRIVATE_KEY_NOT_FOUND;
            else
                m_errorCode = RCM_CLIENT_CREATE_JWT_FAILED;
            status = SVE_FAIL;
        }
#else
        boost::filesystem::path keyFilePath(securitylib::getPrivateDir());
        keyFilePath /= "rcm.pfx";
        if (!securitylib::sslsign(errMsg, keyFilePath.string(), jwt.str(), signature))
        {
            TRACE_ERROR("%s\n", errMsg.c_str());
            m_errorCode = RCM_CLIENT_CREATE_JWT_FAILED;
            status = SVE_FAIL;
        }
#endif
        else
        {
            TRACE_INFO("Signing successful. %s\n", errMsg.c_str());

            std::string sig(signature.begin(), signature.end());
            jwt << "." << JwtBase64Encode(sig);

            jwtRequest = jwt.str();

            //TRACE("JWT request: %s\n", jwtRequest.c_str());
        }

        TRACE_FUNC_END;

        return status;
    }

    SVSTATUS AADAuthProvider::GetBearerToken(std::string& token)
    {
        TRACE_FUNC_BEGIN;

        boost::unique_lock<boost::mutex> tokenGuard(m_authTokenMutex);
        std::string errMsg;
        SVSTATUS status = SVS_OK;

        if (m_auth.GetAccessToken(token))
        {
            return status;
        }

        try {

            std::string jwtRequest;
            status = GetJwtRequest(jwtRequest);
            if (status != SVS_OK)
                return status;

            HttpClient rcm_client(true);

            if (!m_proxy.m_address.empty() && !m_proxy.m_port.empty())
                rcm_client.SetProxy(m_proxy);

            std::string AADUri;
            if (!boost::starts_with(m_clientSettings.m_AADUri, HTTPS_PREFIX))
                AADUri += HTTPS_PREFIX;

            AADUri += m_clientSettings.m_AADUri;

            bool uriAdfs = IsUriAdfs(AADUri);

            // For AAD, AADUri should be /<AADTenantId>/oauth2/token
            // For ADFS, AADUri should be /adfs/oauth2/token
            if (!uriAdfs &&
                !boost::icontains(m_clientSettings.m_AADUri, m_clientSettings.m_AADTenantId))
            {
                AADUri += URI_DELIMITER::PATH;
                AADUri += m_clientSettings.m_AADTenantId;
            }

            AADUri += URI_DELIMITER::PATH;
            AADUri += OAUTH_TYPE2;
            AADUri += URI_DELIMITER::PATH;
            AADUri += OAUTH_TOKEN;

            std::string requestBody = OAUTH_GRANT_TYPE;
            requestBody += URI_DELIMITER::QUERY_PARAM_VAL;
            requestBody += OAUTH_CLIENT_CREDENTIALS;

            requestBody += URI_DELIMITER::QUERY_PARAM_SEP;
            requestBody += OAUTH_CLIENT_ID;
            requestBody += URI_DELIMITER::QUERY_PARAM_VAL;
            requestBody += m_clientSettings.m_AADClientId;

            requestBody += URI_DELIMITER::QUERY_PARAM_SEP;
            requestBody += OAUTH_CLIENT_ASSERTION_TYPE;
            requestBody += URI_DELIMITER::QUERY_PARAM_VAL;
            requestBody += OAUTH_ASSERTION_JWT_BEARER;

            requestBody += URI_DELIMITER::QUERY_PARAM_SEP;
            requestBody += OAUTH_CLIENT_ASSERTION;
            requestBody += URI_DELIMITER::QUERY_PARAM_VAL;
            requestBody += jwtRequest;

            requestBody += URI_DELIMITER::QUERY_PARAM_SEP;
            requestBody += OAUTH_RESOURCE;
            requestBody += URI_DELIMITER::QUERY_PARAM_VAL;

            requestBody += m_clientSettings.m_AADAudienceUri;

            HttpRequest request(AADUri);
            request.AddHeader(ContentType, FormUrlEncoded);
            request.SetHttpMethod(AzureStorageRest::HTTP_POST);

            std::vector<unsigned char> vdata(requestBody.begin(), requestBody.end());
            request.SetRequestBody(&vdata[0], vdata.size());

            HttpResponse response;

            if (!rcm_client.GetResponse(request, response))
            {
                errMsg += "Get bearer token request failed.\n Error : ";
                errMsg += boost::lexical_cast<std::string>(response.GetErrorCode());
                errMsg += "\n";

                TRACE_ERROR("Post failed: %s\n", errMsg.c_str());
                if (response.GetErrorCode() == CURLE_SSL_CACERT)
                    m_errorCode = RCM_CLIENT_SERVER_CERT_VALIDATION_ERROR;
                else if (response.GetErrorCode() == CURLE_COULDNT_CONNECT)
                    m_errorCode = RCM_CLIENT_AAD_COULDNT_CONNECT;
                else if (response.GetErrorCode() == CURLE_COULDNT_RESOLVE_HOST)
                    m_errorCode = RCM_CLIENT_AAD_COULDNT_RESOLVE_HOST;
                else if (response.GetErrorCode() == CURLE_COULDNT_RESOLVE_PROXY)
                    m_errorCode = RCM_CLIENT_AAD_COULDNT_RESOLVE_PROXY;
                else
                    m_errorCode = RCM_CLIENT_AAD_POST_FAILED;
                status = SVE_FAIL;;
            }
            else
            {
                if (response.GetResponseCode() == HttpErrorCode::OK)
                {
                    std::string jsonOutput = response.GetResponseData();

                    status = m_auth.StoreAccessToken(jsonOutput);
                    if (status != SVS_OK)
                    {
                        m_errorCode = RCM_CLIENT_GET_BEARER_TOKEN_FAILED;
                    }
                    else if (!m_auth.GetAccessToken(token))
                    {
                        status = SVE_FAIL;
                        m_errorCode = RCM_CLIENT_AAD_ERROR_CACHED_TOKEN_INVALID;
                    }
                }
                else
                {
                    status = SVE_HTTP_RESPONSE_FAILED;
                    m_errorCode = RCM_CLIENT_GET_BEARER_TOKEN_FAILED;
                    ProcessErrorResponse(response.GetResponseCode(), response.GetResponseData());
                }

            }
        }
        catch (const char *msg)
        {
            errMsg += msg;
            status = SVE_ABORT;
        }
        catch (std::string &msg)
        {
            errMsg += msg;
            status = SVE_ABORT;
        }
        catch (std::exception &e)
        {
            errMsg += e.what();
            status = SVE_ABORT;
        }
        catch (...)
        {
            errMsg += "unknonw error";
            status = SVE_ABORT;
        }

        if (status == SVE_ABORT)
        {
            m_errorCode = RCM_CLIENT_CAUGHT_EXCEPTION;
            TRACE_ERROR("%s: caught exception: %s.\n", __FUNCTION__, errMsg.c_str());
        }

        TRACE_FUNC_END;

        return status;
    }

    /// \brief processes the error response received from AAD
    /// \returns none
    void AADAuthProvider::ProcessErrorResponse(long responseCode, const std::string& strResponse)
    {
        TRACE_FUNC_BEGIN;
        std::string errMsg;
        errMsg += "HTTP response code: ";
        errMsg += boost::lexical_cast<std::string>(responseCode);
        errMsg += ", response data : \n";
        errMsg += strResponse;
        errMsg += "\n";

        TRACE_ERROR("%s: AAD request completed with %s\n", __FUNCTION__, errMsg.c_str());

        bool bUnknownError = false;
        try {
            OAuthErrorResponse error_response = JSON::consumer<OAuthErrorResponse>::convert(strResponse, true);

            if (error_response.error_codes.empty() || error_response.error_codes.size() > 1)
            {
                bUnknownError = true;
            }
            else
            {
                // ref: https://docs.microsoft.com/en-us/azure/active-directory/develop/reference-aadsts-error-codes#lookup-current-error-code-information

                if (error_response.error == OAUTH_ERROR_INVALID_CLIENT)
                {
                    if (error_response.error_codes[0] == AADSTS700024)
                    {
                        m_errorCode = RCM_CLIENT_AAD_ERROR_SYSTEM_TIME_OUT_OF_SYNC;
                    }
                    else
                    {
                        bUnknownError = true;
                    }
                }
                else if (error_response.error == OAUTH_ERROR_UNAUTHORIZED_CLIENT)
                {
                    if (error_response.error_codes[0] == AADSTS700016)
                    {
                        m_errorCode == RCM_CLIENT_AAD_ERROR_APP_ID_NOT_FOUND;
                    }
                    else
                    {
                        bUnknownError = true;
                    }
                }
                else
                {
                    bUnknownError = true;
                }
            }
        }
        catch (...)
        {
            bUnknownError = true;
        }

        if (bUnknownError)
        {
            if (m_extendedErrorCallback)
            {
                m_errorCode = RCM_CLIENT_AAD_ERROR_WITH_HTTP_RESPONSE;
                m_extendedErrorCallback(errMsg);
            }
        }

        TRACE_FUNC_END;

        return;
    }
}
