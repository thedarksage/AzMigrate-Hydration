/* to facilitate ident */
static const char rcsid[] = "$Id:";

#include "common.h"
#include "vsnap.h"
#include "vsnap_kernel.h"
#include "VsnapShared.h"
#include "vdev_proc_common.h"

struct ioctl_stats_tag inm_proc_global;

/*
 * inc_ioctl_stat
 *
 * DESC
 *	increment the ioctl statistics
 */
void
inc_ioctl_stat(inm_32_t ioctl_number)
{
	switch(ioctl_number) {
	    case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
		REF_IOCTL_STAT(is_create_vsnap_ioctl)++;
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
		REF_IOCTL_STAT(is_delete_vsnap_ioctl)++;
		break;

	    case IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT:
		REF_IOCTL_STAT(is_verify_mount_point_ioctl)++;
		break;

	    case IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY:
		REF_IOCTL_STAT(is_verify_dev_path_ioctl)++;
		break;

	    case IOCTL_INMAGE_UPDATE_VSNAP_VOLUME:
		REF_IOCTL_STAT(is_update_map_ioctl)++;
		break;

	    case IOCTL_INMAGE_GET_VOL_CONTEXT:
		REF_IOCTL_STAT(is_num_of_context_per_ret_path_ioctl)++;
		break;

	    case IOCTL_INMAGE_GET_VOLUME_INFORMATION:
		REF_IOCTL_STAT(is_many_context_ioctl)++;
		break;

	    case IOCTL_INMAGE_GET_VOLUME_CONTEXT:
		REF_IOCTL_STAT(is_one_context_ioctl)++;
		break;

	    case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE:
		REF_IOCTL_STAT(is_create_vv_ioctl)++;
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE:
		REF_IOCTL_STAT(is_delete_vv_ioctl)++;
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL:
		REF_IOCTL_STAT(is_delete_vsnap_force_ioctl)++;
		break;
	}
}

/*
 * proc_read_global
 *
 * DESC
 *	to process the read requests for all the /proc/invirvol/global
 */
inm_32_t
proc_read_global_common(char *page)
{
	inm_32_t	len = 0;
#ifdef _VSNAP_DEBUG_
	inm_32_t	size_refernce = 0;
	inm_64_t	remainder = 0;
	inm_32_t 		i = 0;
	inm_64_t	mem_stats = MEM_STATS;
	char		mem_unit[3];
	char		proc_log[INM_PROC_PER_LOG_SIZE + 1];
	inm_32_t	errors;
#endif

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "Global Info\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "-----------\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Build Date\t\t\t\t: " \
				 __DATE__ " [ "__TIME__ " ] ");

#ifdef _VSNAP_DEBUG_
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "(DEBUG)");
#endif
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "\n");

#ifdef _VSNAP_DEBUG_
	while ((mem_stats > 1024) && (size_refernce < 3)) {
		remainder = do_div(mem_stats, 1024);
		size_refernce++;
	}
	do_div(remainder, 100);
	switch(size_refernce) {
	    case 0:
		strcpy_s(mem_unit, 3, "B");
		break;
	    case 1:
		strcpy_s(mem_unit, 3, "KB");
		break;
	    case 2:
		strcpy_s(mem_unit, 3, "MB");
		break;
	    default:
		strcpy_s(mem_unit, 3, "GB");
	}

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Memory Usage\t\t\t\t: %lld.%lld %s\n",
				 mem_stats,remainder, mem_unit);
#endif

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "\nIOCTL Statistics\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "----------------\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "CREATE VSNAP\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_create_vsnap_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "DELETE VSNAP\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_delete_vsnap_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "FORCE DELETE VSNAP\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_delete_vsnap_force_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "VERITY MOUNT POINT\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_verify_mount_point_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "VERIFY DEVICE PATH\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_verify_dev_path_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "UPDATE VSNAPs\t\t\t\t: %llu\n",
				 REF_IOCTL_STAT(is_update_map_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "NUM CONTEXTS PER RETENTION PATH\t\t: %u\n",
			REF_IOCTL_STAT(is_num_of_context_per_ret_path_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "MANY CONTEXTS\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_many_context_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "ONE CONTEXT\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_one_context_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "CREATE VOLPACK\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_create_vv_ioctl));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "DELETE VOLPACK\t\t\t\t: %u\n",
				 REF_IOCTL_STAT(is_delete_vv_ioctl));
#ifdef _VSNAP_DEBUG_
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "ERRORS\t\t\t\t\t: %u\n",
				 inm_proc_global.is_error);

	errors = inm_proc_global.is_error;
	if ( errors ) {
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "\n----------------\n");
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "LAST %d ERRORS\n",
					 (inm_32_t)INM_MAX_LOG);
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "----------------\n");

		/* Index starts at 0 */
		for ( i = 0; i < INM_MAX_LOG && (errors - i); i++ ) {
			snprintf(proc_log, INM_PROC_PER_LOG_SIZE, "%s",
			     INM_PROC_LOG(errors - i));
			proc_log[INM_PROC_PER_LOG_SIZE] = '\0';
			len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
						 "%d: %s",
				       (inm_32_t)(errors - i), proc_log);
		}
	}
#endif
	return len;
}

/*
 * this array represents the number of days in one non-leap year at
 * the beginning of each month
 */
unsigned long DaysToMonth[13] = {
	0,31,59,90,120,151,181,212,243,273,304,334,365
};

/*
 * convert
 *
 * DESC
 *	converts the unreadable time-stamp into human readable format
 */
static void
convert(long long units, char time_str[60])
{
	long long 	*temp;
	long long	whole_days_since_1601, days_since_first_of_year;
	long long	years, whole_days_without_lyear_calc, year_reminder;
	long long	number_of_leap_years_since_1601, days_to_month;
	long long	hours, days, minutes, seconds, milli_seconds, months;

	temp = &units;
	do_div(*temp, 10000);			/* nanoseconds */
	milli_seconds = do_div(*temp, 1000);	/* milli seconds */
	seconds = do_div(*temp, 60);		/* seconds */
	minutes = do_div(*temp, 60);		/* minutes */
	hours = do_div(*temp, 24);		/* hours */

	whole_days_since_1601 = *temp;
	whole_days_without_lyear_calc = do_div(*temp, 365);
	years = *temp + 1601;			/* years */

	number_of_leap_years_since_1601 = whole_days_since_1601;
	year_reminder = do_div(number_of_leap_years_since_1601, (4 * 365) + 1);
	days_since_first_of_year = whole_days_without_lyear_calc -
	    number_of_leap_years_since_1601;
	days_since_first_of_year += 32;		/* hack */

	/*
	 * calculate the month
	 */
	months = 13;
	days_to_month = 366;
	while (days_since_first_of_year < days_to_month) {
		months--;			/* months */
		days_to_month = DaysToMonth[months];
		if ((months >= 2) && (year_reminder == 3)) {
			days_to_month++;	/* leap year */
		}
	}
	days = days_since_first_of_year - days_to_month + 1;

	if (units) {
		sprintf(time_str, "%lld/%lld/%lld %lld:%lld:%lld:%lld (%lld)",
			years, months, days, hours, minutes, seconds,
			milli_seconds, units);
	} else {
		sprintf(time_str, "POINT IN TIME");
	}
}


static void
proc_get_device_flags(inm_vdev_t *vdev, char *str)
{
	unsigned long flags = vdev->vd_flags;
	*str='\0';

	sprintf(str, "%lu ( ", flags);

	if ( INM_TEST_BIT(VSNAP_DEVICE, &flags) )
		strcat_s(str, 200, "VSNAP_DEVICE | ");

	if ( INM_TEST_BIT(VV_DEVICE, &flags) )
		strcat_s(str, 200, "VV_DEVICE | ");

	if ( INM_TEST_BIT(VSNAP_FLAGS_READ_ONLY, &flags) )
		strcat_s(str, 200, "VSNAP_FLAGS_READ_ONLY | ");

	if ( INM_TEST_BIT(VSNAP_FLAGS_FREEING, &flags) )
		strcat_s(str, 200, "VSNAP_FLAGS_FREEING | ");

	if ( INM_TEST_BIT(VDEV_FULL_DISK, &flags) )
		strcat_s(str, 200, "VDEV_FULL_DISK | ");

	if ( INM_TEST_BIT(VDEV_HAS_VTOC, &flags) )
		strcat_s(str, 200, "VDEV_HAS_VTOC | ");

	if ( INM_TEST_BIT(VDEV_HAS_EFI, &flags) )
		strcat_s(str, 200, "VDEV_HAS_EFI | ");

	if ( INM_TEST_BIT(VDEV_HAS_MBR, &flags) )
		strcat_s(str, 200, "VDEV_HAS_MBR | ");

	if ( INM_TEST_BIT(VDEV_HAS_PARTITIONS, &flags) )
		strcat_s(str, 200, "VDEV_HAS_PARTITIONS | ");

	if ( INM_TEST_BIT(VDEV_BLK_OPEN, &flags) )
		strcat_s(str, 200, "VDEV_BLK_OPEN | ");

	if ( INM_TEST_BIT(VDEV_CHR_OPEN, &flags) )
		strcat_s(str, 200, "VDEV_CHR_OPEN | ");

	if ( INM_TEST_BIT(VDEV_LYR_OPEN, &flags) )
		strcat_s(str, 200, "VDEV_LYR_OPEN | ");

	/* Replace trailing " | "  with )*/
	*(str + strlen(str) - 2) = ')';
	*(str + strlen(str) - 1) = '\0';

	return;
}

/*
 * proc_read_context
 *
 * DESC
 *	common fn to process all the read requests for all the
 *	/proc/invirvol/~/context
 */
inm_32_t
proc_read_vs_context_common(char *page, void *data)
{
	inm_32_t	len = 0;
	char		*tmp;
	inm_vdev_t	*vdev = data;
	inm_vs_info_t	*vs_info = vdev->vd_private;
        inm_u32_t       inflight = 0;
        inm_u32_t       pending = 0;
        inm_u32_t       complete = 0;
        inm_u32_t       cached_nodes = 0;
        inm_u32_t       write_in_progress = 0;
	inm_32_t	sparse = 0;
	inm_interrupt_t	intr;

	tmp = INM_ALLOC(200 * sizeof(char), GFP_KERNEL);
	if ( !tmp ) {
		ERR("No mem");
		goto out;
	}

	if ( INM_TEST_BIT(VS_SPARSE, &vs_info->vsi_flags) )
		sparse = 1;

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "VSNAP Context\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "-------------\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Snapshot ID \t\t: %lld\n",
				 vs_info->vsi_snapshot_id);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Size \t\t\t: %llu\n", INM_VDEV_SIZE(vdev));
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
		       "Retention Type \t\t: %s\n", sparse ? "SPARSE" : "CDP");

	/*
	 * convert the time into human-readable format
	 */
	convert(vs_info->vsi_recovery_time, tmp);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Recovery time \t\t: %s\n", tmp);
	if( sparse ) {
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Recovery Time (ymdh) \t: %llu\n",
					 vs_info->vsi_rec_time_yymmddhh);
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "End Time(ymdhs) \t: %llu\n",
					 vs_info->vsi_sparse_end_time);
	}

	/*
	 * convert access type into human-readable format
	 */
	switch(vs_info->vsi_access_type) {
	    case VSNAP_READ_ONLY:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Access Type \t\t: READ ONLY\n");
		break;

	    case VSNAP_READ_WRITE:
	    case VSNAP_READ_WRITE_TRACKING:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Access Type \t\t: READ WRITE\n");
		break;

	    default:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Access Type \t\t: **ERROR**\n");
	}

	/*
	 * present tracking enabled field in a human readable format
	 */
	switch(vs_info->vsi_is_tracking_enabled) {
	    case 1:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Tracking \t\t: ENABLED\n");
		break;

	    case 0:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Tracking \t\t: DISABLED\n");
		break;

	    default:
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Tracking \t\t: **ERROR**\n");
	}

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Parent Device \t\t: %s\n",
				 vs_info->vsi_parent_list->pl_devpath);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Datalog Path \t\t: %s\n",
				 vs_info->vsi_data_log_path);
	if (vs_info->vsi_parent_list->pl_retention_path[0] == '\0') {
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Retention Path \t\t: %s\n",
					 "NO MEDIA RETENTION");
	} else {
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Retention Path \t\t: %s\n",
				vs_info->vsi_parent_list->pl_retention_path);
	}
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "VSNAP Device \t\t: %s\n",
				 vs_info->vsi_mnt_path);

        cached_nodes = vs_info->vsi_btree->bt_num_cached_nodes;
        INM_ACQ_SPINLOCK_IRQ(&vdev->vd_lock, intr);
        inflight = INM_ATOMIC_READ(&vdev->vd_io_inflight);
        pending = INM_ATOMIC_READ(&vdev->vd_io_pending);
        complete = vdev->vd_io_complete;
        write_in_progress = vdev->vd_write_in_progress;
        INM_REL_SPINLOCK_IRQ(&vdev->vd_lock, intr);

        len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Cached Nodes \t\t: %u\n", cached_nodes);
        len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "I/O In Progress \t: %u\n", inflight);
        len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "I/O Queued \t\t: %u\n", pending - inflight);
        len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "I/O Complete \t\t: %u\n", complete);
        len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Write in Progress \t: %s\n",
				 write_in_progress == 1 ? "YES" : "NO");

	proc_get_device_flags(vdev, tmp);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Flags \t\t\t: %s\n", tmp);

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Reference Count \t: %d\n", vdev->vd_refcnt);

	if ( sparse ) {
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Cached Keys \t\t: %u\n",
					 vs_info->vsi_sparse_num_keys);
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Forward Fid \t\t: %u\n",
					 vs_info->vsi_forward_fid);
		len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
					 "Backward Fid \t\t: %u\n",
					 vs_info->vsi_backward_fid);
	} else {
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Last Datafile \t\t: %s\n",
				 vs_info->vsi_last_datafile);
	}

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Data Fid \t\t: %u\n",
				 vs_info->vsi_last_datafid);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Next Fid \t\t: %u\n", vs_info->vsi_next_fid);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Data Fid Used \t\t: %u\n",
				 vs_info->vsi_next_fid_used);

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "\n");

	INM_FREE(&tmp, 200 * sizeof(char));

out:
	return len;
}

inm_32_t
proc_read_vv_context_common(char *page, void *data)
{
	inm_32_t len = 0;
	inm_vdev_t	*vdev = data;
	inm_vv_info_t *vv_info = vdev->vd_private;
	char *tmp;

	tmp = INM_ALLOC(200 * sizeof(char), GFP_KERNEL);
	if ( !tmp ) {
		ERR("No mem");
		goto out;
	}

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "VSNAP Context\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE, "-------------\n");
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Backing Sparse File \t\t: %s\n",
				 vv_info->vvi_sparse_file_path);

	proc_get_device_flags(vdev, tmp);
	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Flags \t\t\t\t: %s\n", tmp);

	len += (inm_32_t)snprintf(page + len, INM_PAGE_SIZE,
				 "Reference Count\t\t\t: %d\n",vdev->vd_refcnt);

	INM_FREE(&tmp, 200 * sizeof(char));

out:
	return len;

}

#ifdef _VSNAP_DEBUG_
char * 
get_ioctl_name(inm_32_t ioctl)
{
	char *ioctl_name = NULL;

	switch (ioctl) {
	    case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
		ioctl_name = "CREATE VSNAP";
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG:
		ioctl_name = "DELETE VSNAP";
		break;

	    case IOCTL_INMAGE_GET_VOLUME_SYMLINK_FOR_RETENTION_POINT:
		ioctl_name = "VERIFY MOUNT POINT";
		break;

	    case IOCTL_INMAGE_GET_VOLUME_DEVICE_ENTRY:
		ioctl_name = "VERIFY DEVICE PATH";
		break;

	    case IOCTL_INMAGE_UPDATE_VSNAP_VOLUME:
		ioctl_name = "UPDATE VSNAPS";
		break;

	    case IOCTL_INMAGE_GET_VOL_CONTEXT:
		ioctl_name = "NUM CONTEXTS PER RETENTION PATH";
		break;

	    case IOCTL_INMAGE_GET_VOLUME_INFORMATION:
		ioctl_name = "MANY CONTEXTS";
		break;

	    case IOCTL_INMAGE_GET_VOLUME_CONTEXT:
		ioctl_name = "ONE CONTEXT";
		break;

	    case IOCTL_INMAGE_ADD_VIRTUAL_VOLUME_FROM_FILE:
		ioctl_name = "CREATE VOLPACK";
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_FILE:
		ioctl_name = "DELETE VOLPACK";
		break;

	    case IOCTL_INMAGE_REMOVE_VIRTUAL_VOLUME_FROM_RETENTION_LOG_NOFAIL:
		ioctl_name = "FORCE DELETE VSNAP";
		break;

	    default:
		ioctl_name = "INVALID IOCTL";
		break;
	}
	return ioctl_name;
}

void
detail_ioctl_stat(inm_32_t ioctl, inm_32_t retval)
{
	inc_ioctl_stat(ioctl);

	if ( retval < 0 ) {
		inm_proc_global.is_error++;

		if ( inm_proc_global.is_failure_log ) {
			/* We start at index 0 */
			snprintf(INM_PROC_LOG(inm_proc_global.is_error),
				INM_PROC_PER_LOG_SIZE, "%s -> %d\n",
				get_ioctl_name(ioctl), -retval);
		}
	}
}
#endif

void
print_proc_data(void *data)
{
	char *page;
	inm_vdev_t *vdev = data;

	page = INM_ALLOC(INM_PAGE_SIZE, GFP_KERNEL);
	if ( !page ) {
		ERR("Cannot allocate memory");
		goto out;
	}

	if( vdev ) {
		if ( INM_TEST_BIT(VSNAP_DEVICE, &vdev->vd_flags) )
			proc_read_vs_context_common(page, vdev);
		else
			proc_read_vv_context_common(page, vdev);
	} else {
		proc_read_global_common(page);
	}

	UPRINT("%s", page);

	INM_FREE(&page, INM_PAGE_SIZE);

out:
	return;

}

/*
 * inm_vdev_init_proc
 *
 * DESC
 *	initialize the proc interface. This is just a dummy interface
 * 	as there is no proc support in solaris
 *
 */
inm_32_t
inm_vdev_init_proc_common(void)
{
	memzero(&inm_proc_global, sizeof(struct ioctl_stats_tag));

#ifdef _VSNAP_DEBUG_
	inm_proc_global.is_failure_log = INM_ALLOC(INM_PROC_LOG_SIZE,
						   GFP_KERNEL);
	if( !inm_proc_global.is_failure_log ) {
		ERR("Cannot allocate error buf");
	}

	inm_proc_global.is_error = 0;
#endif
	return 0;
}

/*
 * inm_vdev_exit_proc
 *
 * DESC
 *	clean up the proc interface
 */
void
inm_vdev_exit_proc_common(void)
{
#ifdef _VSNAP_DEBUG_
	if( inm_proc_global.is_failure_log )
		INM_FREE(&inm_proc_global.is_failure_log, INM_PROC_LOG_SIZE);
#endif
	return;
}
