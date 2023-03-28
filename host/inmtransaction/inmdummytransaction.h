#ifndef INMDUMMYTRANSACTION_H
#define INMDUMMYTRANSACTION_H

#include <string>
#include "inmtransaction.h"

class InmDummyTransaction : public InmTransaction
{
public:
    bool Begin(const std::string &name)
    {
        return true;
    }

    bool AddItemToUpdate(const std::string &item)
    {
        return true;
    }

    bool SetItemAsUpdated(const std::string &item)
    {
        return true;
    }

    bool End(void)
    {
        return true;
    }

    bool Rollback(void)
    {
        return true;
    }

    std::string GetErrorMsg(void)
    {
        return std::string();
    }

    bool Reset(void)
    {
        return true;
    }
};

#endif /* INMDUMMYTRANSACTION_H */
