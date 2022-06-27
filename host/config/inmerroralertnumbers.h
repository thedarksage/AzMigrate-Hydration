#ifndef INM_ERROR_ALERT_NUMBERS_H
#define INM_ERROR_ALERT_NUMBERS_H

typedef enum eErrorAlertNumber
{
    E_CACHE_DIR_LOW_ALERT = 401,
    E_CDP_LOG_DIR_CREATE_FAIL_ALERT,
    E_OUT_OF_ORDER_DIFF_ALERT,
    E_CLUS_SVC_SHUTDOWN_ALERT,
    E_CHANGE_LEN_ZERO_ALERT,
    E_CHANGE_BEYOND_SOURCE_SIZE_ALERT,
    E_INVALID_RESYNC_FILE_ALERT,
    E_FILTER_DRIVER_NOT_FOUND_ALERT,
    E_RESYNC_BITMAPS_INVALID_ALERT,
    E_VOLUME_RESIZE_ALERT,
    E_VX_TO_PS_COMMUNICATION_FAIL_ALERT,
    E_VOLUME_READ_FAILURE_ALERT,
    E_VOLUME_READ_FAILURE_EOF_ALERT,
    E_VOLUME_READ_FAILURE_AT_OFFSET_ALERT,
    E_VOLUME_FS_CLUSTERS_QUERY_FAILURE_ALERT,
    E_FILTER_DRIVER_GAVE_NO_DIFFS_ALERT,
    E_FILTER_DRIVER_FAILED_TO_GIVE_DIFFS_ALERT,
    E_VX_PERIODIC_REGISTRATION_FAILED_ALERT,
    E_DLM_FILE_UPLOAD_FAILED_ALERT,
    E_TARGET_VOLUME_UNMOUNT_FAILED_ALERT,
    E_ERRATIC_VOLUMES_GUID_ALERT,
    E_CACHE_DIR_BAD_ALERT,
    E_WMI_SERVICE_BAD_ALERT,
    E_RETENTION_FILE_READ_FAILED_ALERT,
    E_RETENTION_FILE_WRITE_FAILED_ALERT,
    E_WRITE_FAILURE_AT_OFFSET_ALERT,
	E_ALERT_ALREADY_CONSUMED_AT_CS, // EA0427 error is already defined at cs and even published to SRS, so ignoring this entry at agent end
	E_AZUREAUTHENTICATION_FAILURE_ALERT,
    E_CRASH_CONSISTENCY_FAILURE_NON_DATA_MODE,
    E_CRASH_CONSISTENCY_FAILURE,
    E_APP_CONSISTENCY_FAILURE,
    E_VX_TO_AZURE_BLOB_COMMUNICATION_FAIL_ALERT,
    E_ALERT_ALREADY_CONSUMED_AT_CS_1, // EA0433 error is already defined at cs and even published to SRS, so ignoring this entry at agent end
    E_REBOOT_ALERT,
    E_AGENT_UPGRADE_ALERT,
    E_LOG_UPLOAD_NETWORK_FAILURE_ALERT,
    E_DISK_CHURN_PEAK_ALERT,
    E_LOG_UPLOAD_NETWORK_HIGH_LATENCY_ALERT,
    E_DISK_RESIZE_ALERT, // EA0439
    E_TIME_JUMPED_FORWARD_ALERT,
    E_TIME_JUMPED_BACKWARD_ALERT,
	E_ASR_VSS_PROVIDER_MISSING,
	E_ASR_VSS_SERVICE_DISABLED,
    E_AZURE_CANCEL_OPERATION_ERROR_ALERT
} E_INM_ERROR_ALERT_NUMBER;

#endif /* INM_ERROR_ALERT_NUMBERS_H */