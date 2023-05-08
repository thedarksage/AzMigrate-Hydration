#ifndef _VDEV_SCSI_H_
#define _VDEV_SCSI_H_

#include "common.h"
#include "vsnap.h"
#include "vdev_endian.h"
#include "vdev_disk_helpers.h"

#define	INQUIRY_SIZE	(sizeof(struct scsi_inquiry))

struct inm_scsi_capacity_10 {
	inm_u32_t isc10_capacity;
	inm_u32_t isc10_bsize;
};

struct inm_scsi_capacity_16 {
	inm_u64_t isc16_capacity;
	inm_u32_t isc16_bsize;
};

#ifdef _BIG_ENDIAN_
struct inm_p83_desig_desc {
	unsigned char	ip83dd_protocol				:4,
			ip83dd_code_set				:4;

	unsigned char	ip83dd_piv				:1,
			ip83dd_reserved				:1,
			ip83dd_association			:2,
			ip83dd_designator			:4;

	unsigned char	ip83dd_reserved2;

	unsigned char	ip83dd_designator_len;

	union ip83_dd_data {

		struct ip83_dd_name {
			char 	ip83dd_vendor_name[8];
			char	ip83dd_guid[16];
		} ip83dd_name;

		struct ip83_dd_naa_info {
			unsigned char	ip83dd_naa_type		:4,
					ip83dd_ieee_id_msb	:4;

			inm_u16_t	ip83dd_ieee_id_int;

			unsigned char	ip83dd_ieee_id_lsb	:4,
					ip83dd_vid_msb		:4;

			inm_u32_t	ip83dd_vid_lsb;
		} ip83dd_naa_info;
	} ip83dd_data;
};

#else
#ifdef _LITTLE_ENDIAN_
struct inm_p83_desig_desc {
	unsigned char	ip83dd_code_set				:4,
			ip83dd_protocol				:4;

	unsigned char	ip83dd_designator			:4,
			ip83dd_association			:2,
			ip83dd_reserved				:1,
			ip83dd_piv				:1;

	unsigned char	ip83dd_reserved2;

	unsigned char	ip83dd_designator_len;

	union	ip83_dd_data {

		struct ip83_dd_name {
			char 	ip83dd_vendor_name[8];
			char	ip83dd_guid[16];
		} ip83dd_name;

		struct ip83_dd_naa_info {
			unsigned char	ip83dd_ieee_id_msb	:4,
					ip83dd_naa_type		:4;

			inm_u16_t	ip83dd_ieee_id_int;

			unsigned char	ip83dd_vid_msb		:4,
					ip83dd_ieee_id_lsb	:4;

			inm_u32_t	ip83dd_vid_lsb;
		} ip83dd_naa_info;
	} ip83dd_data;
};

#endif /* LITTLE */
#endif /* else */

struct inm_p83_descriptor {
#ifdef _BIG_ENDIAN_
	unsigned char	ip83_devqual				:3,
			ip83_devtype				:5;
#else
#ifdef _LITTLE_ENDIAN_
	unsigned char	ip83_devtype				:5,
			ip83_devqual				:3;
#endif /* LITTLE */
#endif /* else */

	unsigned char	ip83_page;

	inm_u16_t	ip83_num_desig_desc;

	struct inm_p83_desig_desc ip83_desc[];
};

struct inm_p00_descriptor {
#ifdef _BIG_ENDIAN_
	unsigned char	ip00_devqual				:3,
			ip00_devtype				:5;
#else
#ifdef _LITTLE_ENDIAN_
	unsigned char	ip00_devtype				:5,
			ip00_devqual				:3;
#endif /* LITTLE */
#endif /* else */

	unsigned char	ip00_page;
	unsigned char	ip00_reserved;

	unsigned char	ip00_num_pages_supported;

	unsigned char	ip00_pages_supported[];
};

#define INM_READ_CAP_10_LIMIT 	( 0xFFFFFFFEll * 512 )

#ifdef _BIG_ENDIAN_
struct inm_fmtunit_cdb {
	unsigned char 	ifu_opcode;

	unsigned char 	ifu_unused1	:1,
			ifu_unused2	:1,
			ifu_unused3	:1,
			ifu_fmtdata	:1,
			ifu_unused4	:4;

	unsigned char 	ifu_unused5[4];
};

#else
#ifdef _LITTLE_ENDIAN_
struct inm_fmtunit_cdb {
	unsigned char 	ifu_opcode;

	unsigned char 	ifu_unused4	:4,
			ifu_fmtdata	:1,
			ifu_unused3	:1,
			ifu_unused2	:1,
			ifu_unused1	:1;

	unsigned char 	ifu_unused5[4];
};

#endif
#endif

#define SCSI_SUCCESS	0
#define SCSI_CHECK_CONDITION	2

/* Prototypes */
void inm_process_request_sense(void *, inm_32_t );
inm_32_t inm_process_scsi_command(inm_vdev_t *, unsigned char *, inm_32_t cdb_len,
				  unsigned char *, inm_32_t, inm_minor_t);

#endif
