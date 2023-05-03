//#pragma once

#ifndef ERROR__H
#define ERROR__H

#include <string>
#include <errno.h>

const int SV_FAILURE            = -9999;
const int SV_SUCCESS            = 20000;


const int ERR_IO_PENDING        = 20001;
const int ERR_MORE_DATA         = 20002;
const int ERR_HANDLE_EOF        = 20003;
const int ERR_INVALID_FUNCTION  = 20004;
const int ERR_INVALID_PARAMETER = 20005;
const int ERR_INVALID_HANDLE    = 20006;
const int ERR_NOT_READY		= 20007;
const int ERR_BUSY		= 20008;
const int ERR_AGAIN             = 20009;
const int ERR_NODEV             = 20010;

class Lockable;

class Error
{
public:
    static std::string Msg();
    static std::string Msg(const int);
    static const int OSToUser(const int);
    static const int UserToOS(const int);
    static const int Code();
    static const void Code(int);
    static void TrimEnd(char* buf);
private:
    static  Lockable m_MsgLock;
    static  Lockable m_CodeLock;
};

#endif

