#include "vdev_scsi.h"
#define INMAGE_EMD_VENDOR "InMage  " /* Vendor ID Len should be minimum of 8 bytes */

void
print_scsi_inquiry(struct scsi_inquiry *buf)
{
	int i = 0;

        DBG("buf->inq_dtype = %u", (inm_u32_t)buf->inq_dtype);
	DBG("buf->inq_qual = %u", (inm_u32_t)buf->inq_qual);
	DBG("buf->inq_rmb = %u", (inm_u32_t)buf->inq_rmb);
        DBG("buf->inq_ansi = %u", (inm_u32_t)buf->inq_ansi);
	DBG("buf->inq_ecma = %u", (inm_u32_t)buf->inq_ecma);
	DBG("buf->inq_iso = %u", (inm_u32_t)buf->inq_iso);
	DBG("buf->inq_rdf = %u", (inm_u32_t)buf->inq_rdf);
	DBG("buf->inq_normaca = %u", (inm_u32_t)buf->inq_normaca);
        DBG("buf->inq_trmiop = %u", (inm_u32_t)buf->inq_trmiop);
	DBG("buf->inq_aenc = %u", (inm_u32_t)buf->inq_aenc);
	DBG("buf->inq_len = %u", (inm_u32_t)buf->inq_len);
	DBG("buf->inq_addr16 = %u", (inm_u32_t)buf->inq_addr16);
        DBG("buf->inq_addr32 = %u", (inm_u32_t)buf->inq_addr32);
	DBG("buf->inq_ackqreqq = %u", (inm_u32_t)buf->inq_ackqreqq);
	DBG("buf->inq_mchngr = %u", (inm_u32_t)buf->inq_mchngr);
	DBG("buf->inq_dualp = %u", (inm_u32_t)buf->inq_dualp);
	DBG("buf->inq_port = %u", (inm_u32_t)buf->inq_port);
	DBG("buf->inq_sftre = %u", (inm_u32_t)buf->inq_sftre);
        DBG("buf->inq_cmdque = %u", (inm_u32_t)buf->inq_cmdque);
	DBG("buf->inq_trandis = %u", (inm_u32_t)buf->inq_trandis);
	DBG("buf->inq_linked = %u", (inm_u32_t)buf->inq_linked);
	DBG("buf->inq_sync = %u", (inm_u32_t)buf->inq_sync);
	DBG("buf->inq_wbus16 = %u", (inm_u32_t)buf->inq_wbus16);
	DBG("buf->inq_wbus32 = %u", (inm_u32_t)buf->inq_wbus32);
	DBG("buf->inq_reladdr = %u", (inm_u32_t)buf->inq_reladdr);

        DBG("buf->inq_vid = %s", buf->inq_vid);             /* vendor ID */
        DBG("buf->inq_pid = %s", buf->inq_pid);            /* product ID */
        DBG("buf->inq_revision = %s", buf->inq_revision);        /* revision level */

        /*
         * Bytes 36-47 are reserved:
         *      For Sun qualified hard disk drives the inq_serial field contains
         *              two bytes of mfg date year code (ascii)
         *              two bytes of mfg date week code (ascii)
         *              six bytes of mfg serial number (ascii)
         *              two bytes unused
         */
        DBG("buf->inq_serial = %s", buf->inq_serial);
}

static void
inm_handle_read_capacity_16(inm_vdev_t *vdev, unsigned char *cdb, inm_32_t cdb_len,
			unsigned char *buf, inm_32_t buf_len)
{
	struct inm_scsi_capacity_16 *cptr = (struct inm_scsi_capacity_16 *)buf;

	cptr->isc16_capacity = INM_BE_64(INM_VDEV_VIRTUAL_SIZE(vdev)/DEV_BSIZE);
	cptr->isc16_bsize = INM_BE_32(DEV_BSIZE);

	DEV_DBG("Num Blocks = %llu, Block Size = %u",
	    INM_BE_64(cptr->isc16_capacity), INM_BE_32(cptr->isc16_bsize));
}

static void
inm_handle_read_capacity_10(inm_vdev_t *vdev, unsigned char *cdb, inm_32_t cdb_len,
			unsigned char *buf, inm_32_t buf_len)
{
	struct inm_scsi_capacity_10 *cptr = (struct inm_scsi_capacity_10 *)buf;

	if( INM_VDEV_VIRTUAL_SIZE(vdev) <= INM_READ_CAP_10_LIMIT ) {
		cptr->isc10_capacity =
		  INM_BE_32((inm_u32_t)(INM_VDEV_VIRTUAL_SIZE(vdev)/DEV_BSIZE));
		cptr->isc10_bsize = INM_BE_32(DEV_BSIZE);
	} else {
		cptr->isc10_capacity = INM_BE_32(0xFFFFFFFF);
		cptr->isc10_bsize = INM_BE_32(DEV_BSIZE);
	}

	DEV_DBG("Num Blocks = %u, Block Size = %u", INM_BE_32(cptr->isc10_capacity),
	    INM_BE_32(cptr->isc10_bsize));
}

static void
inm_handle_scsi_inquiry_page00(inm_vdev_t *vdev, unsigned char *buf)
{
	struct inm_p00_descriptor *desc = (struct inm_p00_descriptor *)buf;

	DEV_DBG("Page 00");

	desc->ip00_devqual = INM_BE_8(0);
	desc->ip00_devtype = INM_BE_8(0);
	desc->ip00_page = INM_BE_8(0x00);
	desc->ip00_reserved = INM_BE_8(0);
	desc->ip00_num_pages_supported = INM_BE_8(2);
	desc->ip00_pages_supported[0] = INM_BE_8(0x00);
	desc->ip00_pages_supported[1] = INM_BE_8(0x83);
}

static int
inm_handle_scsi_inquiry_page83(inm_vdev_t *vdev, unsigned char *buf)
{
        int32_t len, i, j, rlen = 0;
        char tc_guid[32] = "INMAGE_000000";
	int32_t minor = 0;

        minor =  (vdev->vd_index & 0xF0) >> 4;
	minor = minor >= 10 ? minor + 7 : minor;
        tc_guid[11] = tc_guid[11] + minor;
        minor = vdev->vd_index & 0x0F;
	minor = minor >= 10 ? minor + 7 : minor;
        tc_guid[12] = tc_guid[12] + minor;

        /* Two identification descriptors: */
        /* T10 vendor identifier field format  */
        buf[0] = 0x2;   /* ASCII */
        buf[1] = 0x1;
        buf[2] = 0x0;
        memcpy_s(&buf[4], 8, INMAGE_EMD_VENDOR, 8);
        memset(&buf[12], ' ', 16);
        i = strlen(tc_guid);
        i = i < 16 ? i : 16;
        memcpy_s(&buf[12], i, tc_guid, i);
        rlen = buf[3] = 8 + 16; // 8 (Vendor name) + 16 (guid)
        rlen += 4;              // 4 => Identification descriptor header.
        buf += rlen;
        /* NAA IEEE registered identifier */
        buf[0] = 0x1;           /* binary */
        buf[1] = 0x3;
        buf[2] = 0x0;
        buf[3] = 0x8;

        buf[4] = 0x50;  /* ieee company id=0x002383 (INMAGE) */
        buf[5] = 0x02;
        buf[6] = 0x38;
        buf[7] = 0x30;

        len = strlen(tc_guid);
        i = len < 5 ? 0 : len - 5;
        j = 7;
        buf[j] |= tc_guid[i] & 0x0f;
        j++;
        i++;
        for (; i < len; i++, j++)
        {
                buf[j]=  tc_guid[i];
        }

        rlen = rlen + 12;         /* Page length */

        return rlen;
}


static void
inm_handle_scsi_inquiry_page83_orig(inm_vdev_t *vdev, unsigned char *buf)
{
	struct inm_p83_descriptor *desc = (struct inm_p83_descriptor *)buf;
	struct inm_p83_desig_desc *desig;
	inm_32_t guid;
	inm_32_t i = 15;

	DEV_DBG("Page 83");

	desc->ip83_devqual = INM_BE_8(0);
	desc->ip83_devtype = INM_BE_8(0);
	desc->ip83_page = INM_BE_8(0x83);
	desc->ip83_num_desig_desc = INM_BE_16(2);

	desig = &(desc->ip83_desc[0]);

	desig->ip83dd_protocol = INM_BE_8(0);
	desig->ip83dd_code_set = INM_BE_8(0x2);
	desig->ip83dd_piv = INM_BE_8(0);
	desig->ip83dd_reserved = INM_BE_8(0);
	desig->ip83dd_association = INM_BE_8(0);
	desig->ip83dd_designator = INM_BE_8(1);
	desig->ip83dd_reserved2 = INM_BE_8(0);
	desig->ip83dd_designator_len = INM_BE_8(24);

	memcpy_s(desig->ip83dd_data.ip83dd_name.ip83dd_vendor_name, 8, "INMAGE  ", 8);
	memcpy_s(desig->ip83dd_data.ip83dd_name.ip83dd_guid, 16, "0000000000000000", 16);

	guid = vdev->vd_index;

	while(guid) {
		desig->ip83dd_data.ip83dd_name.ip83dd_guid[i--] =
						(inm_u8_t)((guid % 10) + '0');
		guid = guid / 10;
	}

	desig = &(desc->ip83_desc[1]);

	desig->ip83dd_protocol = INM_BE_8(0);
	desig->ip83dd_code_set = INM_BE_8(0x1);
	desig->ip83dd_piv = INM_BE_8(0);
	desig->ip83dd_reserved = INM_BE_8(0);
	desig->ip83dd_association = INM_BE_8(0);
	desig->ip83dd_designator = INM_BE_8(3);
	desig->ip83dd_reserved2 = INM_BE_8(0);
	desig->ip83dd_designator_len = INM_BE_8(8);

	desig->ip83dd_data.ip83dd_naa_info.ip83dd_naa_type = INM_BE_8(5);

	/* Inmage ID = 0x002383 */
	desig->ip83dd_data.ip83dd_naa_info.ip83dd_ieee_id_msb = INM_BE_8(0);
	desig->ip83dd_data.ip83dd_naa_info.ip83dd_ieee_id_int = INM_BE_16(0x238);
	desig->ip83dd_data.ip83dd_naa_info.ip83dd_ieee_id_msb = INM_BE_8(3);

	desig->ip83dd_data.ip83dd_naa_info.ip83dd_vid_msb = INM_BE_8(0);
	desig->ip83dd_data.ip83dd_naa_info.ip83dd_vid_lsb =
					INM_BE_32((inm_u32_t)vdev->vd_index);
}



static inm_32_t
inm_handle_scsi_inquiry_evpd(inm_vdev_t *vdev, unsigned char *buf,
			     inm_u32_t page)
{
	inm_32_t retval = 0, rlen;

	switch(page) {
		case 0x00:	inm_handle_scsi_inquiry_page00(vdev, buf);
				break;

                case 0x83:      rlen = inm_handle_scsi_inquiry_page83(vdev, buf + 4);
                                buf[1] = 0x83; //page number
                                buf[2] = (rlen >> 8) & 0xFF;
                                buf[3] = rlen & 0xFF;
                                break;

/*              case 0x83:      inm_handle_scsi_inquiry_page83(vdev, buf);
                                break;
*/
		default:	ERR("Unsupported Page 0x%x", page);
				retval = -ENOTSUP;
				break;
	}

	return retval;
}

inm_32_t
inm_handle_standard_inquiry(inm_vdev_t *vdev, unsigned char *sbuf, inm_32_t buf_len)
{
	struct scsi_inquiry *buf;
//	struct scsi_inquiry *buf = (struct scsi_inquiry *)sbuf;
	inm_u32_t guid = 0;
	inm_32_t i = 0;
	inm_32_t len = 0;
	inm_32_t retval = 0;

	DBG("Standard Inquiry");

	buf = INM_ALLOC(INQUIRY_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		retval = -ENOMEM;
		goto allo_fail;
	}
	bzero(buf, INQUIRY_SIZE);

	buf->inq_rdf = INM_BE_8(2);
	buf->inq_len = INM_BE_8(31); /* total 36 bytes buf, 36-4 ) */
	buf->inq_sync = INM_BE_8(1);
	buf->inq_wbus16 = INM_BE_8(1);
	buf->inq_wbus32 = INM_BE_8(1);

	if (strcpy_s(buf->inq_vid, 9, "INMAGE  ")) {
		retval = -EFAULT;
		INM_FREE(&buf, INQUIRY_SIZE);
		goto allo_fail;
	}

	memcpy_s(buf->inq_pid, 16, "0000000000000000", 16);

	guid = vdev->vd_index;
	i = 15;

	while(guid) {
		buf->inq_pid[i--] = (inm_u8_t)((guid % 10) + '0');
		guid = guid / 10;
	}

	/*
	 * Since the remaining bytes are zeroed, serial
	 * will always be unique
	 */
	sprintf(buf->inq_serial, "1010%d", vdev->vd_index);
#ifdef _VSNAP_DEV_DEBUG_
	print_scsi_inquiry(buf);
#endif
	len = buf_len > INQUIRY_SIZE ? INQUIRY_SIZE : buf_len;
	memcpy_s(sbuf, len, buf, len);

allo_fail:
	INM_FREE(&buf, INQUIRY_SIZE);
	return retval;
}

static inm_32_t
inm_handle_scsi_inquiry(inm_vdev_t *vdev, unsigned char *cdb, inm_32_t cdb_len,
			unsigned char *sbuf, inm_32_t buf_len)
{
	inm_u32_t page = (inm_u32_t)cdb[2];
	inm_u32_t evpd = (inm_u32_t)cdb[1] & 1;
	inm_32_t retval = 0;

	DEV_DBG("EVPD = %d", evpd);

	if( evpd == 0 ) {
		if( page == 0 )
			retval = inm_handle_standard_inquiry(vdev, sbuf, buf_len);
		else
			retval = -EINVAL;
	} else {
		retval = inm_handle_scsi_inquiry_evpd(vdev, sbuf,
						     (inm_u32_t)cdb[2]);
	}

	return retval;
}

static inm_32_t
inm_handle_format_unit(inm_vdev_t *vdev, unsigned char *cdb, inm_32_t cdb_len,
		       unsigned char *sbuf, inm_32_t buf_len)
{
	struct inm_fmtunit_cdb *fcdb = (struct inm_fmtunit_cdb *)cdb;
	inm_32_t retval = 0;

	if( fcdb->ifu_fmtdata != 0 )
		retval = -ENOTSUP;

	return retval;
}

void
inm_process_request_sense(void *buf, inm_32_t error)
{
	struct scsi_extended_sense *sense = buf;

	bzero(buf, sizeof(struct scsi_extended_sense));

	sense->es_class = INM_BE_8(0x7);
	sense->es_code = INM_BE_8(0);

	sense->es_key = INM_BE_8(0x5);
	sense->es_add_len = INM_BE_8(10);

	if ( error = -ENOTSUP )
		sense->es_add_code = INM_BE_8(0x20);
	else
		sense->es_add_code = INM_BE_8(0x24);

	return;
}


inm_32_t
inm_process_scsi_command(inm_vdev_t	*vdev,
			 unsigned char 	*cdb,
			 inm_32_t 	cdb_len,
			 unsigned char	*buf,
			 inm_32_t	buf_len,
			 inm_minor_t	minor)
{
	inm_32_t	opcode = (inm_32_t)(cdb[0]);
	inm_32_t	retval = 0;

	DBG("opcode = 0x%x len = %d, buf_len = %d",
		opcode, cdb_len, buf_len);

	switch(opcode) {
		case 0x00:	DEV_DBG("TUR");
				break;

		case 0x04:	DEV_DBG("Format Unit");
				retval = inm_handle_format_unit(vdev, cdb,
								cdb_len, buf,
								buf_len);
				break;

		case 0x12: 	DBG("SCSI inquiry");
				retval = inm_handle_scsi_inquiry(vdev, cdb,
								 cdb_len, buf,
								 buf_len);
				break;

		case 0x25:	DBG("Read Capacity");
				inm_handle_read_capacity_10(vdev, cdb, cdb_len,
							    buf,buf_len);
				break;

		case 0x9e:	DEV_DBG("Read Capacity(16)");
				inm_handle_read_capacity_16(vdev, cdb, cdb_len,
							    buf, buf_len);
				break;

		default:	DEV_DBG("Unknown SCSI command 0x%x", opcode);
				retval = -ENOTSUP;
				break;
	}

	return retval;
}
