
///
///  \file cxtransportimp.cpp
///
///  \brief implements the cx transport factory for instantiating
///  the correct transport implementation to be used
///

#include "cxtransportimp.h"
#include "cxtransportimpclient.h"

CxTransportImp::ptr CxTransportImpFactory(TRANSPORT_PROTOCOL transportProtocol,
                                          TRANSPORT_CONNECTION_SETTINGS const& settings,
                                          bool secure,
                                          bool useCfs,
                                          std::string const& psId)
{
    switch (transportProtocol) {
        case TRANSPORT_PROTOCOL_HTTP:
        case TRANSPORT_PROTOCOL_FILE:
        case TRANSPORT_PROTOCOL_BLOB:
            return CxTransportImp::ptr(new CxTransportImpClient(transportProtocol, settings, secure, useCfs, psId));

        default:
            throw ERROR_EXCEPTION << "unsupported transport protocol code: " << transportProtocol << '(' << GetStrTransportProtocol(transportProtocol) << ')';
    }
}
