#ifndef _INMAGE_VDEV_DISK_HELPERS_H
#define _INMAGE_VDEV_DISK_HELPERS_H

#include "common.h"
#include "vsnap_kernel_helpers.h"
#include "vsnap.h"
#include "vsnap_control.h"
#include "vv_control.h"
#include "vsnap_kernel.h"
#include "vdev_scsi.h"
#include "vdev_helper.h"
#include "vdev_endian.h"

#define VDEV_NAME_SIZE 100 /* This is used to create device and is not the same
			      as the one stored in disk_info structure */

#define NBLOCKS	"Nblocks"
#define SIZE	"Size"

#ifndef EFI_PART_NAME_LEN
#define EFI_PART_NAME_LEN 36
#endif

#define INM_DISK_NAME_SIZE  EFI_PART_NAME_LEN

/*
 * VTOC can be stored at block 0 or 1 based on system
 */
#define INM_VTOC_LOCATION DK_LABEL_LOC
/*
 * EFI is strored from block 1
 */
#define INM_EFI_GPT_LOC	1
#define INM_EFI_GPE_LOC	2

#ifndef SUNIXOS2
#define SUNIXOS2        191
#endif

#ifndef EFI_NUMPAR
#define EFI_NUMPAR      9
#endif

#define INM_PARTITION_MINOR(BASE,PRTIDX)	(BASE + PRTIDX + 1)

/*
 * Store all disk relate info here
 * and typedef it later to inm_disk_t
 */
/*
 * This structure contains partition related info */
struct inm_sol_part {
	inm_u64_t	isp_start_block;
	inm_u64_t	isp_size;	/* No of blocks in partition/full disk */
	inm_32_t	isp_in_use;
	unsigned long	isp_flag;	/* should be same type as flag in vdev */
};

struct inm_sol_disk {
	struct vtoc 		*isd_vtoc; /* vtoc ptr */
#ifndef _SOLARIS_8_
	efi_gpt_t 		*isd_gpt; /* efi gpt ptr */
	efi_gpe_t 		*isd_gpe; /* efi gpe ptr */
#endif
	struct ipart		*isd_mbr; /* DOS MBR, declared void since not
					     supported everywhere */
	struct mboot		*isd_mboot; /* mboot ptr */
	struct dk_geom 		*isd_dkg; /* geom ptr */
	struct dk_cinfo 	*isd_cinfo; /* cinfo ptr */
	struct inm_sol_part	*isd_partition; /* array of partition info */
	inm_u64_t		isd_virtual_size; /* size to report for read_capacity */
	inm_32_t		isd_numpart; /* num partitions */
	inm_32_t		isd_full_disk_part; /* partition corresponding to full disk */
	inm_32_t		isd_label_part; /* for x86, which mbr partition contains vtoc */
	char			isd_name[INM_DISK_NAME_SIZE];
};


#define INM_VDEV_NAME(vdev)		((vdev)->vd_disk->isd_name)
#define INM_VDEV_FULL_PART_IDX(vdev)	((vdev)->vd_disk->isd_full_disk_part)
#define INM_VDEV_VIRTUAL_SIZE(vdev)	((vdev)->vd_disk->isd_virtual_size)

/* Macros to access common disk info */
#define INM_VDEV_GEOM_PTR(vdev) 	((vdev)->vd_disk->isd_dkg)
#define INM_VDEV_CINFO_PTR(vdev)	((vdev)->vd_disk->isd_cinfo)

/* Partition Tables */
#define INM_VDEV_VTOC_PTR(vdev) 	((vdev)->vd_disk->isd_vtoc)
#define INM_VDEV_MBR_PTR(vdev) 		((vdev)->vd_disk->isd_mbr)
#define INM_VDEV_MBOOT_PTR(vdev)	((vdev)->vd_disk->isd_mboot)
#define INM_VDEV_LABEL_PARTITION(vdev)	((vdev)->vd_disk->isd_label_part)


#ifndef _SOLARIS_8_
#define INM_VDEV_GPT_PTR(vdev) 		((vdev)->vd_disk->isd_gpt)
#define INM_VDEV_GPE_PTR(vdev) 		((vdev)->vd_disk->isd_gpe)
#endif

/*Partition Data */
#define INM_VDEV_NUM_PART(vdev)		((vdev)->vd_disk->isd_numpart)
#define INM_VDEV_PART_INFO(vdev)	((vdev)->vd_disk->isd_partition)

/* Per partition data */
#define INM_PART_START_BLOCK(vdev, idx)	\
		((vdev)->vd_disk->isd_partition[idx].isp_start_block)
#define INM_PART_SIZE(vdev, idx) 		\
		((vdev)->vd_disk->isd_partition[idx].isp_size)
#define INM_PART_IN_USE(vdev, idx) 		\
		((vdev)->vd_disk->isd_partition[idx].isp_in_use)
#define INM_PART_FLAG(vdev, idx)		\
		((vdev)->vd_disk->isd_partition[idx].isp_flag)

/* Macros to map partition index to minor and vice versa */
#define INM_PART_MINOR(base, pt_idx)		((base) + (pt_idx) + 1)
#define INM_PART_IDX(base_minor, pt_minor) 	((pt_minor) - (base_minor) - 1)

#define INM_MINOR_IS_PARTITION(vdev, minor) 	( vdev->vd_index != minor )

/* NDKMAP == 16 for P0. For P1 and greater, INM_PART_IDX() > NDKMAP */
#define INM_MINOR_IS_MBR_PARTITION(vdev, minor)				\
	( INM_MINOR_IS_PARTITION(vdev, minor) && 			\
	( INM_PART_IDX(vdev->vd_index, minor) > NDKMAP ) )

/*
 * Some essential macros are not defined
 */
/*
 * Convert a uuid to/from little-endian format
 */
#ifndef UUID_LE_CONVERT
#define UUID_LE_CONVERT(dest, src)                                          \
{                                                                           \
        (dest) = (src);                                                     \
        (dest).time_low = INM_LE_32((dest).time_low);                       \
        (dest).time_mid = INM_LE_16((dest).time_mid);                       \
        (dest).time_hi_and_version = INM_LE_16((dest).time_hi_and_version); \
}
#endif

#define INM_EFI_GPT_LOC	1

#define TB 1024*1024*1024*1024
#define INM_VTOC_LIMIT	((inm_u64_t)2 * TB)

#define INM_GEOM_SECTPERCYL(geom) ((geom)->dkg_nhead * (geom)->dkg_nsect)

#define INM_CALC_GEOM_SIZE(geom) \
(((geom)->dkg_ncyl + (geom)->dkg_acyl) * INM_GEOM_SECTPERCYL(geom) *	\
						(inm_u64_t)DEV_BSIZE)
#ifdef _VSNAP_DEBUG_

static inline inm_u64_t
print_geom(struct dk_geom *geom)
{
	DBG("ncyl = %d", geom->dkg_ncyl);
	DBG("acyl = %d", geom->dkg_acyl);
	DBG("nhead = %d", geom->dkg_nhead);
	DBG("nsect = %d", geom->dkg_nsect);

	return INM_CALC_GEOM_SIZE(geom);
}

#define INM_GEOM_SIZE(geom)	print_geom(geom)

#else
#define INM_GEOM_SIZE(geom)	INM_CALC_GEOM_SIZE(geom)
#endif


/* Prototypes for disk related functions */
inm_32_t inm_alloc_disk_info(inm_vdev_t *);
inm_32_t inm_populate_disk_info(inm_vdev_t *, inm_u64_t);
void inm_free_disk_info(inm_vdev_t *);

/* ioctl handler helpers */
inm_32_t inm_get_vdev_vtoc(inm_vdev_t *, intptr_t, int, inm_minor_t);
inm_32_t inm_get_vdev_iocinfo(inm_vdev_t *, intptr_t, int, inm_minor_t);
inm_32_t inm_get_vdev_geom(inm_vdev_t *, intptr_t, int);
inm_32_t inm_get_vdev_minfo(inm_vdev_t *, intptr_t, int);
inm_32_t inm_get_vdev_state(inm_vdev_t *, intptr_t, int);
inm_32_t inm_get_vdev_efi(inm_vdev_t *, intptr_t, int, inm_minor_t);
inm_32_t inm_get_vdev_mboot(inm_vdev_t *, intptr_t, int, inm_minor_t);
inm_32_t inm_handle_scsi_request(inm_vdev_t *, intptr_t , int , inm_minor_t);

#endif
