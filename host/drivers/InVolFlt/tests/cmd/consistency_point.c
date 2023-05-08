#include "libinclude.h"
#include "involflt.h"
#include "libcp.h"
#include "liblogger.h"

#define MAX_DEVS 8

void
usage(int error)
{
    log_err("Usage:\t-b: Create Barrier");
    log_err("\t-c: Commit Tag");
    log_err("\t-d: <Device Name> [Upto %d]", MAX_DEVS);
    log_err("\t-f: Freeze");
    log_err("\t-g: [GUID]");
    log_err("\t-h: Help");
    log_err("\t-m: Timeout in ms");
    log_err("\t-p: [Tag]");
    log_err("\t-r: Revoke Tag");
    log_err("\t-s: Single Node Crash Consistency");
    log_err("\t-t: Thaw");
    log_err("\t-u: Remove Barrier");
    exit(error);
}

int
main(int argc, char *argv[])
{
    cp_ops_t op = CP_NONE;
    int ops = 0;
    char *dev = NULL;
    char *guid = NULL;
    char *tag = NULL;
    int fd = 0;
    int error = 0;
    int timeout = 0;
    int c;
    int devreqd = 0;

    while ((c = getopt(argc, argv, "bcd:fg:hm:p::rstu")) != -1) {

        switch (c) {

            case 'b':   op = CP_CREATE_BARRIER;
                        ops++;
                        break;

            case 'c':   op = CP_COMMIT_TAG;
                        ops++;
                        break;

            case 'd':   dev = optarg;
                        break;

            case 'f':   op = CP_FREEZE_FS;
                        ops++;
                        devreqd = 1;
                        break;

            case 'h':   usage(0);

            case 'g':   guid = optarg;
                        break;

            case 'p':   op = CP_TAG;
                        tag = optarg;
                        ops++;
                        break;

            case 'm':   timeout = atoi(optarg);
                        break;

            case 'r':   op = CP_REVOKE_TAG;
                        ops++;
                        break;

            case 's':   op = CP_SN_TAG;
                        ops++;
                        break;

            case 't':   op = CP_THAW_FS;
                        devreqd = 1;
                        ops++;
                        break;

            case 'u':   op = CP_REMOVE_BARRIER;
                        ops++;
                        break;

            default:    usage(1);
        }
    }

    if (!op) {
        errno = EINVAL;
        log_err_usage("No ops specified");
    }

    if (ops > 1) {
        errno = EINVAL;
        log_err_usage("Multiple ops specified");
    }

    if (devreqd && !dev) {
        errno = EINVAL;
        log_err_usage("No disks specified");
    }

    if (!guid)
        log_msg("Using default guid \"abcd\"");

    switch (op) {
        case CP_CREATE_BARRIER: error = create_barrier(guid, timeout);
                                break;

        case CP_REMOVE_BARRIER: error = remove_barrier(guid);
                                break;

        case CP_TAG:            error = tagvol(dev, guid, tag, timeout);
                                break;

        case CP_COMMIT_TAG:     error = commit_tag(guid);
                                break;

        case CP_REVOKE_TAG:     error = revoke_tag(guid);
                                break;

        case CP_FREEZE_FS:      error = freeze_fs(dev, guid, timeout);
                                break;

        case CP_THAW_FS:        error = thaw_fs(dev, guid);
                                break;

        case CP_SN_TAG:         error = single_node_crash_tag(dev, guid, 
                                                              tag, timeout);
                                break;

        default:                log_err("BUG: Invalid option"); 
    }

    if (error) {
        if ((op == CP_TAG ||
            op == CP_COMMIT_TAG ||
            op == CP_SN_TAG) && 
            error == INM_TAG_PARTIAL) {
            log_err("Partial Tag");
        } else {
            log_err("Requested op failed");
        }
    }

    return error;
}

