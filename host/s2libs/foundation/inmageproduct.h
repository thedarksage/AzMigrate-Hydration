/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : inmageproduct.h
 *
 * Description: 
 */

#ifndef INMAGE_PRODUCT__H
#define INMAGE_PRODUCT__H

#include <string>

#include "entity.h"
#include "synchronize.h"

class InmageProduct : public Entity 
{
public:
    /*
     * destructor
     */
    virtual ~InmageProduct();
    /*
     * singleton instance
     */
    static InmageProduct& GetInstance();
    static bool Destroy();
    static bool GetProductVersionFromResource(char*, char*, int);

    /* 
     * Product version
     */
    const std::string& GetProductVersionStr();
    const std::string& GetProductVersion();
    const std::string& GetDriverVersion();
protected:
    InmageProduct();
private:
    InmageProduct(const InmageProduct &right);
    InmageProduct & operator=(const InmageProduct &right);
    int operator==(const InmageProduct &right) const;
    int operator!=(const InmageProduct &right) const;
    int operator<(const InmageProduct &right) const;
    int operator>(const InmageProduct &right) const;
    int operator<=(const InmageProduct &right) const;
    int operator>=(const InmageProduct &right) const;
    void operator->() const;
    void operator*() const;

    const std::string GetFilterDriverVersion() const;
    
    /*
    * Initialize the singleton
    * Note: Throws excepton in error case
    */
    void Init();

private: 
    static InmageProduct* theProduct;
    static std::string m_sProductVersionStr;
    static std::string m_sProductVersion;
    static std::string m_sDriverVersion;
    static Lockable m_CreateLock;

};

#endif
