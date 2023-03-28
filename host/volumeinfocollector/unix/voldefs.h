#ifndef _COMMON__DEFS__H_
#define _COMMON__DEFS__H_

#include <string>
#include "volumedefines.h"

/* disk types like cXtYdZs2, emcpower0c, hda1 */
typedef enum eDiskType
{
    E_S2ISFULL,
    E_CISFULL,
    E_SLICEHASNUM,
    E_SLICEHASP,
    E_SLICEHASCHAR,
    E_SLICEUNKNOWN,

} E_DISKTYPE;

/* TODO: Make these names readable
 * peculiar name to avoid match of actual diskset name */
const std::string SVMLOCALDISKSETNAME = "__SVM_INMAGE_LOCAL__DEFAULT__";
const std::string VXVMDEFAULTVG = "__VXVM_INMAGE__DEFAULT__";
const std::string FULLDISKSLICE = "s2";

/*
[root@imits216 ~]# dmsetup table
2226e000155038e3e: 0 20971520 multipath 0 0 1 1 round-robin 0 2 1 65:192 100 65:0 100
*/
#define DMMULTIPATHDISKTYPE "multipath"
#define DMRAIDDISKTYPE "dmraid"
char const DMLASTCHAR = ':';
char const DMMAJORMINORSEP = ':';

/**
* Although solaris sparc has only 8 slices but solaris x86 has 16.
* Going with 16 currently to avoid any kind of conditional compilation
*/
#define MAX_NUM_SLICES_VTOC 16

/* Different types of slices to try: 
 1. cXtYdZs0 
 2. emcpower0a
 3. hda0
 4. mpath4p2 --  dev mapper partition
*/
const char * const Slices[][MAX_NUM_SLICES_VTOC] = {
                   {"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "s12", "s13", "s14", "s15"},
                   {"a",  "b",  "c",  "d",  "e",  "f",  "g",  "h",  "i",  "j",  "k",   "l",   "m",   "n",   "o",   "p"},
                   {"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10",  "11",  "12",  "13",  "14",  "15"},
                   {"p0", "p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9", "p10", "p11", "p12", "p13", "p14", "p15"}
                                                   };

const std::string VXVMDSKDIR = "/dev/vx/dsk";
const char CONTINUECHAR = '\\';
/* const char SPACE = ' '; */

/* directories where disk/partitions reside */
std::string const DEV_MAPPER_PATH("/dev/mapper");
std::string const VXDMP_PATH("/dev/vx/dmp");
std::string const MDPATH("/dev/md/dsk");
std::string const DEVDID_PATH("/dev/did/dsk");
std::string const DEVGLBL_PATH("/dev/global/dsk");
std::string const DEVDSK_PATH("/dev/dsk");
std::string const MDONLYPATH("/dev/md");
std::string const DSK("dsk");
std::string const DEV_MAPPER_DOCKER_PATTERN("/dev/mapper/docker");
/* directories to look in for solaris */
const char * const DiskDirs[] = {DEVDSK_PATH.c_str(), VXDMP_PATH.c_str()};

const char * const DidDirs[] = {DEVDID_PATH.c_str() /* , DEVGLBL_PATH.c_str() */ };


#define SCDIDADMARGS "-l"
#define DIDPATTERN "\'{for (i = 1; i <= NF; i++) if ($i ~ /\\/dev\\/did/) print $i}\'"

/* hsp keyword in solaris volume manager */
#define HSP "hsp"

/* To check if slices end in any of below */
const std::string SlicesChar = "abcdefghijklmnop";

/* emcpower0c is full of SMI */
#define CFULL 'c'
/* For devices having emcpower as pseudo names */
#define EMCPOWER "emcpower"
/* slice character in solaris */
#define SLICECHAR 's'

/* Fdisk partitions for solaris x86 */
#define NFDISKPARTIONS 5
const char * const FdiskPartitions[] = {"p0", "p1", "p2", "p3", "p4"};

/* offset points for validating sizes */
#define SIZE_LIMIT_TOP     0x100000000000LL
#define NUM_SECTORS_PER_TB    (1LL << 31)

#define TOKEN_DELIM " \t\n"

/* linux vxdmp slices from s1 to s15 */
#define NPARTITIONS 16
/* starting slice in vxdmp */
#define STARTPARTITIONNUM 1
/* vxdmp slice char in linux */
#define VXDMPSLICECHAR 's'

/* To dlopen the libefi.so */
#ifdef _LP64
#define LIBEFI_DIRPATH "/lib/64"
#else
#define LIBEFI_DIRPATH "/lib"
#endif /* _LP64 */
#define LIBEFINAME "libefi.so"

/* Generic defines */
#define NELEMS(ARR) ((sizeof (ARR)) / (sizeof (ARR[0])))
#define NUMSTR "0123456789"


#endif /* _COMMON__DEFS__H_ */
