#ifndef _INMAGE_SCOPE_TRACE_H_
#define _INMAGE_SCOPE_TRACE_H_

// this class will automatically print function entry and exit log entries
class ScopeTrace {
public:
char * ourFunc;
char * ourFile;
int ourLine;
int * ourCounter;

    __inline ScopeTrace(int * counter, char * func, char * file, int line)
    { ourFunc = func; ourFile = file; ourLine = line;ourCounter = counter;
    DbgPrint("(%c)%p|%8X->InVolFlt!%s:%i\n", KeGetCurrentIrql() + '0',PsGetCurrentThread(),*ourCounter, ourFunc, ourLine);}
    __inline ~ScopeTrace()
    { DbgPrint("(%c)%p|%8X<-InVolFlt!%s:%i\n", KeGetCurrentIrql() + '0',PsGetCurrentThread(),*ourCounter, ourFunc, ourLine);*ourCounter += 1;}
};

#ifdef FUNCTRACE
#define TRC static int coverage##__FUNCTION__##__LINE__; ScopeTrace scopeTrace(&coverage##__FUNCTION__##__LINE__,__FUNCTION__, __FILE__, __LINE__);
#else
#define TRC
#endif

#endif //_INMAGE_SCOPE_TRACE_H_
