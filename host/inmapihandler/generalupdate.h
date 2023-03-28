#ifndef GENERAL__UPDATE__H_
#define GENERAL__UPDATE__H_

#include "confschema.h"
#include "confschemamgr.h"
#include "inmapihandlerdefs.h"
#include "InmXmlParser.h"

class GeneralUpdater
{
public:
    GeneralUpdater();
    void UpdatePgToSchema(const char *key, const ParameterGroup &pg, 
                          ConfSchema::Group &group, AppToSchemaAttrs_t &apptoschemaattrs);
    void CopyAttrsFromPgToSchema(const ParameterGroup &pg, ConfSchema::Object &o, 
                                 AppToSchemaAttrs_t &apptoschemaattrs);
    void CopyAttrsFromSchemaToPg(ParameterGroup &pg, const ConfSchema::Object &o, 
                                                SchemaToAppAttrs_t &schematoappattrs);

};

#endif /* GENERAL__UPDATE__H_ */
