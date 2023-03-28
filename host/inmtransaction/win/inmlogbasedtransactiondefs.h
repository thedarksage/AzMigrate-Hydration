#ifndef INMLOGBASEDTRANSACTION_DEFS_H
#define INMLOGBASEDTRANSACTION_DEFS_H

const char * const OPENMODES[] = {"wtN", "r+tN"};

/* this 3 is because even though 
 * we open in text mode and write
 * \n, it is writing \r \n */
const int STATUSBACKPOS = 3;

#endif /* INMLOGBASEDTRANSACTION_DEFS_H */
