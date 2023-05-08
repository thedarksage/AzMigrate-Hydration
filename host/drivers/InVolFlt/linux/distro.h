#ifndef _DISTRO_H
#define _DISTRO_H

/* SLES 11 */
#if (defined suse && DISTRO_VER == 11)
#define SLES11
#if (PATCH_LEVEL == 3)
#define SLES11SP3
#elif (PATCH_LEVEL == 4)
#define SLES11SP4
#endif
#endif

/* SLES 12 */
#if (defined suse && DISTRO_VER == 12)
#define SLES12
#if (PATCH_LEVEL == 1)
#define SLES12SP1
#elif (PATCH_LEVEL == 2)
#define SLES12SP2
#elif (PATCH_LEVEL == 3)
#define SLES12SP3
#elif (PATCH_LEVEL == 4)
#define SLES12SP4
#elif (PATCH_LEVEL == 5)
#define SLES12SP5
#endif 
#endif

#if (defined suse && DISTRO_VER == 15)
#define SLES15
#if (PATCH_LEVEL == 1)
#define SLES15SP1
#elif (PATCH_LEVEL == 2)
#define SLES15SP2
#elif (PATCH_LEVEL == 3)
#define SLES15SP3
#elif (PATCH_LEVEL == 4)
#define SLES15SP4
#endif
#endif

#if (defined redhat && DISTRO_VER == 9)
#define RHEL9
#elif (defined redhat && DISTRO_VER == 8)
#define RHEL8
#elif (defined redhat && DISTRO_VER == 7)
#define RHEL7
#elif (defined redhat && DISTRO_VER == 6)
#define RHEL6
#elif (defined redhat && DISTRO_VER == 5)
#define RHEL5
#endif

#ifndef RHEL5
#define INITRD_MODE
#endif

#endif

