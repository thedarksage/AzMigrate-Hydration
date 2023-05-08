#ifndef INMAPI__HANDLER__H__
#define INMAPI__HANDLER__H__

#include <map>

typedef std::map<const char *, const char *> AppToSchemaAttrs_t;
typedef AppToSchemaAttrs_t::const_iterator ConstAppToSchemaAttrsIter_t;
typedef AppToSchemaAttrs_t::iterator AppToSchemaAttrsIter_t;

typedef std::map<const char *, const char *> SchemaToAppAttrs_t;
typedef SchemaToAppAttrs_t::const_iterator ConstSchemaToAppAttrsIter_t;
typedef SchemaToAppAttrs_t::iterator SchemaToAppAttrsIter_t;

#endif /* INMAPI__HANDLER__H__ */
