#ifndef INM_UUID_H
#define INM_UUID_H

#include <string>
#include <uuid/uuid.h>

/* Link to uuid library by putting -luuid.
 * (For host modules, add it to X_SYSTEM_LIBS of module Makefile) */

#define UUID_STR_LEN 36

inline std::string GetUuid(void)
{
    uuid_t id;
    char key[UUID_STR_LEN + 1] = "";
    uuid_generate(id);
    uuid_unparse(id, key);
    return key;
}

#endif /* INM_UUID_H */
