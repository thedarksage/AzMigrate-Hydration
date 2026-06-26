///
///  \file inmsafeint.h
///
///  \brief interface header to be included by applications to use InmSafeInt<T>::Type class
///

#ifndef INM_SAFEINT_H
#define INM_SAFEINT_H

#ifdef USE_3018p_SAFEINT
#include "SafeInt.hpp"
#else
#include "SafeInt_3028p.hpp"
#endif

// Use InmSafeInt<T>::Type object
template<class T>
 struct InmSafeInt
 {
     typedef SafeInt<T> Type;
 };

//Use InmSafeIntException same as SafeIntException
typedef SafeIntException InmSafeIntException;

//Applications use this
#define INM_SAFE_ARITHMETIC(expr, exception)                                                          \
    try {                                                                                             \
        expr;                                                                                         \
    } catch (InmSafeIntException &e) {                                                                \
        throw exception("Arithmetic exception for mentioned variables with error code:")(e.m_code);   \
    }

#endif /* INM_SAFEINT_H */
