#include <stdio.h>
#include "bitmap_api.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

typedef struct _bmap_exthdr {
        char devname[4096];
        unsigned int nr_blks;
        unsigned int blk_sz;
        unsigned long long pblk_addrs[32];
} bmap_exthdr_t;


int GetNextBitmapBitRun(
    unsigned char * bitBuffer,
    unsigned int totalBitsInBitmap,
    unsigned int * startingBitOffset,
    unsigned int * bitsInRun,
    unsigned int * bitOffset);

int print_bitmap(unsigned char *iob, int size) {
	int i, ind=0;
	int line_count=0;
	
	unsigned char byte[8]={0};

//	getchar();
	printf("dirty blocks \n");
	while (ind < size) {
		for(i=0; i<8; i++) {
			//byte[i]= (48 + ((iob[ind] & (1<<i)) ? 1 : 0));
			if(iob[ind] & (1<<i)) {
				printf("|%d",(ind*8)+i+1);
			} 
		}
		
/*
		if (ind % 10 == 0) {
			line_count++;
			printf("\n");
		}
		if (line_count % 50 == 0){
			line_count++;
//			getchar();
		}
//		printf("%s", byte);
*/
		ind++;
	}

}
int read_bitmap(unsigned char *bit_buffer, unsigned int starting_offset, unsigned int total_bits, unsigned int bitmap_granularity) {
	unsigned long long data_offset, data_length;
	int n=8;
	int i=0;
	unsigned char byte[8]={0};
	unsigned int bits_in_run = 0;
	unsigned int bit_offset = 0;
	while (starting_offset < total_bits) {
/*		
		for(i=0; i<8; i++) {
			byte[7-i]= (48 + ((bit_buffer[0] & (1<<i)) ? 1 : 0));
		}
		
*/

		GetNextBitmapBitRun(bit_buffer, total_bits, &starting_offset, &bits_in_run, &bit_offset);
		if (bits_in_run) {
			printf("OFF = %20d| LEN = %15d| BLK = %5d\n", bit_offset * bitmap_granularity, bits_in_run * bitmap_granularity, bits_in_run);
			data_offset = bit_offset * bitmap_granularity;
			bits_in_run = 0;
			bit_offset = 0;
		}

	}
	return 0;
}

void 
print_bmap(char *filename, int print_hdr) 
{

	unsigned int starting_offset=0;
	unsigned int total_bits = 4096 * 8;
	unsigned int bitmap_granularity = 4096;
	unsigned int data_offset = 0;
	unsigned char iobuffer[4096] = {0};
	bitmap_header_t *bh = NULL;
	
	int rc = 0;
	int k = 0;
	int x = 0;

	int fd = -1;

	fd = open(filename, O_RDONLY);

	if (fd == -1) {
		printf("unable to open %s \n", filename);
		exit(3);
	}

	bh = (bitmap_header_t *) malloc ( sizeof(bitmap_header_t));

	if (!bh) {
		printf("bitmap header is null\n");
		exit(2);
	}

	rc = read(fd, (void *)bh, sizeof(bitmap_header_t));

	if (rc != sizeof(bitmap_header_t)) {
		printf("reading non bitmap file \n");
		exit(4);
	}	
	for(x=0;x<4;x++)
	        printf("%d|",*((char *)bh + x));
	printf("\n");

#define bhdr  bh->u.header
    printf("------------------------------------------------------------------\n");
    printf("\n                  BITMAP HEADER \n");
    printf("------------------------------------------------------------------\n");

//    printf("validation check sum = %s\n", bhdr.validation_checksum);
    printf("VALIDATION CHECK SUM = ");
    for (k=0; k<16; k++)
	printf("|%d",bhdr.validation_checksum[k]);
    printf("\n");
    printf("ENDIAN               = 0x%x\n", bhdr.endian);
    printf("HEADER SIZE          = %d\n", bhdr.header_size);
    printf("VERSION              = 0x%x\n", bhdr.version);
    printf("DATA OFFSET          = %d\n", bhdr.data_offset);
    printf("BITMAP OFFSET        = %d\n", bhdr.bitmap_offset);
    printf("BITMAP SIZE          = %d\n", bhdr.bitmap_size);
    printf("BITMAP GRANULARITY   = %d KB\n", bhdr.bitmap_granularity/1024);
    printf("VOLUME SIZE          = %d MB\n", bhdr.volume_size/(1024* 1024));
    printf("RECOVERY STATE       = ");
    switch(bhdr.recovery_state) {
	case 0:
		printf("BITMAP_LOG_RECOVERY_STATE_UNINITIALIZED\n");
		break;

	case 1:
		printf("BITMAP_LOG_RECOVERY_STATE_CLEAN_SHUTDOWN\n");
		break;

	case 2:
		printf("BITMAP_LOG_RECOVERY_STATE_DIRTY_SHUTDOWN\n");
		break;

	case 3:
		printf("BITMAP_LOG_RECOVERY_STATE_LOST_SYNC\n");
		break;
	default:
		printf("UNKNOWN\n");
		break;
	}
    printf("LAST CHANCE CHANGES  = %d\n", bhdr.last_chance_changes);
    printf("BOOT CYCLES          = %d\n", bhdr.boot_cycles);
    printf("CHANGES LOST         = %d\n", bhdr.changes_lost);
    printf("RESYNC REQUIRED      = %d\n", bhdr.resync_required);
    printf("RESYNC ERROR         = %d\n", bhdr.resync_errcode);
    printf("RESYNC STATUS        = %llu\n", bhdr.resync_status);
    printf("------------------------------------------------------------------\n\n");

/*
	bhdr.last_chance_changes = 1;
	bh->change_groups[0].u.length_offset_pair[0]=0xbfffffbbbbbbbb;
*/
    
    if(bhdr.last_chance_changes || print_hdr) {
	unsigned long long scaled_size, scaled_offset, write_offset, write_size;
	unsigned long long size_offset_pair;
	int i;
	    int nlcw = 0;
        nlcw = print_hdr ? 
               MAX_WRITE_GROUPS_IN_BITMAP_HEADER * MAX_CHANGES_IN_WRITE_GROUP : 
               bhdr.last_chance_changes;
    	printf("LAST CHANCE CHANGES  \n");
        for (i = 0; i < nlcw; i++) {
                size_offset_pair = bh->change_groups[i / MAX_CHANGES_IN_WRITE_GROUP ].u.length_offset_pair[i%MAX_CHANGES_IN_WRITE_GROUP];

                scaled_size = (unsigned long) (size_offset_pair >> 48);
                scaled_offset = size_offset_pair & 0xFFFFFFFFFFFFULL;

                write_offset = scaled_offset * bhdr.bitmap_granularity;
                write_size = scaled_size * bhdr.bitmap_granularity;

	

	printf("OFF = %20d| LEN = %15d| BLK = %5d\n", write_offset, write_size, scaled_size);
	}
        printf("------------------------------------------------------------------\n\n");
    }

    if (!print_hdr) {
	bitmap_granularity = bhdr.bitmap_granularity;
	//total_bits = bhdr.bitmap_size;
	total_bits = 0;

	data_offset = bhdr.data_offset;

	while (1) {
		int adjusted_bsize = 0;
		lseek(fd, data_offset, SEEK_SET);
		rc = read(fd, (void *)iobuffer, 4096);
		data_offset += 4096;
	
		if (rc == 0)
			break;
		//print_bitmap(iobuffer, rc);
		starting_offset = total_bits * 8;
		total_bits += rc;
		printf("\nSEGMENT # %d \n", (data_offset - bhdr.data_offset)/4096);
		adjusted_bsize = (rc >= 4096) ? 4096 : rc;
		read_bitmap(iobuffer, starting_offset, adjusted_bsize , bitmap_granularity);
    //		read_bitmap(iobuffer, 0, (rc >= 4096) ? 4096 : rc), bitmap_granularity);
    //		read_bitmap(iobuffer, (4096*8), bitmap_granularity);
    printf("------------------------------------------------------------------\n\n");
	}
    }

	close(fd);
}

void
read_bitmap_file(char *filename) 
{
    print_bmap(filename, 0);
}
    
void
read_bmap_exthdr_file(char *filename) {

	unsigned int starting_offset=0;
	unsigned int total_bits = 4096 * 8;
	unsigned int bitmap_granularity = 4096;
	unsigned int data_offset = 0;
	unsigned char iobuffer[4096] = {0};
	bitmap_header_t *bh = NULL;
	bmap_exthdr_t *beh = NULL;
	
	int rc = 0;
	int k = 0;
	int x= 0;


	int fd = -1;



	fd = open(filename, O_RDONLY);

	if (fd == -1) {
		printf("unable to open %s \n", filename);
		exit(3);
	}

	beh = (bmap_exthdr_t *) malloc ( sizeof(bmap_exthdr_t));

	if (!beh) {
		printf("bitmap header is null\n");
		exit(2);
	}

	rc = read(fd, (void *)beh, sizeof(bmap_exthdr_t));

	if (rc != sizeof(bmap_exthdr_t)) {
		printf("invalid bmap ext hdr file \n");
		exit(4);
	}	

    printf("------------------------------------------------------------------\n");
    printf("\n                EXTERNAL  BITMAP HEADER \n");
    printf("------------------------------------------------------------------\n");

    printf(" device name   = %s\n", beh->devname);
    printf(" nr blks       = %d\n", beh->nr_blks);
    printf(" blk sz	   = %d\n", beh->blk_sz);
    printf(" physical block addresses :\t ");
    for (k=0; k < beh->nr_blks; k++) {
	 printf("%x ", beh->pblk_addrs[k]);
    }
    printf("\n");

    close(fd);

    //fd = open(beh->devname, O_RDONLY|O_DIRECT);
    fd = open(beh->devname, O_RDONLY);
        if (fd == -1) {
                printf("unable to open %s \n", filename);
                exit(3);
        }
     printf("buffer size = %d\n", sizeof(bitmap_header_t));
     bh = (bitmap_header_t *) malloc ( sizeof(bitmap_header_t));
     //posix_memalign((void **)&bh, sysconf(_SC_PAGESIZE), sizeof(bitmap_header_t));

        if (!bh) {
                printf("bitmap header is null\n");
                exit(2);
        }

	for(k=0; k<beh->nr_blks; k++) {
		char *buf = ((char *)bh + (k * beh->blk_sz));
		rc = lseek(fd, (beh->pblk_addrs[k]*beh->blk_sz), SEEK_SET);
//		printf("fptr moved to %x location \n", rc);
		if (rc < 0) {
			printf("can't seek devfile\n");
			exit(4);
		}
        	rc = read(fd, buf, beh->blk_sz);
//		printf("rc value after read = %d\n", rc);
//		printf("errno = %d\n", errno);
		//perror(errno);


        	if (rc != beh->blk_sz) {
                	printf("invalid bmap ext hdr file \n");
                	exit(4);
        	}
	}

	for(x=0;x<4;x++)
	        printf("%d|",*((char *)bh + x));
	printf("\n");
#define bhdr  bh->u.header
    printf("------------------------------------------------------------------\n");
    printf("\n                  EXTERNAL BITMAP HEADER \n");
    printf("------------------------------------------------------------------\n");

//    printf("validation check sum = %s\n", bhdr.validation_checksum);
    printf("VALIDATION CHECK SUM = ");
    for (k=0; k<16; k++)
	printf("|%d",bhdr.validation_checksum[k]);
    printf("\n");
    printf("ENDIAN               = 0x%x\n", bhdr.endian);
    printf("HEADER SIZE          = %d\n", bhdr.header_size);
    printf("VERSION              = 0x%x\n", bhdr.version);
    printf("DATA OFFSET          = %d\n", bhdr.data_offset);
    printf("BITMAP OFFSET        = %d\n", bhdr.bitmap_offset);
    printf("BITMAP SIZE          = %d\n", bhdr.bitmap_size);
    printf("BITMAP GRANULARITY   = %d\n", bhdr.bitmap_granularity);
    printf("VOLUME SIZE          = %d MB\n", bhdr.volume_size/(1024* 1024));
    printf("RECOVERY STATE       = ");
    switch(bhdr.recovery_state) {
	case 0:
		printf("BITMAP_LOG_RECOVERY_STATE_UNINITIALIZED\n");
		break;

	case 1:
		printf("BITMAP_LOG_RECOVERY_STATE_CLEAN_SHUTDOWN\n");
		break;

	case 2:
		printf("BITMAP_LOG_RECOVERY_STATE_DIRTY_SHUTDOWN\n");
		break;

	case 3:
		printf("BITMAP_LOG_RECOVERY_STATE_LOST_SYNC\n");
		break;
	default:
		printf("UNKNOWN\n");
		break;
	}
    printf("LAST CHANCE CHANGES  = %d\n", bhdr.last_chance_changes);
    printf("BOOT CYCLES          = %d\n", bhdr.boot_cycles);
    printf("CHANGES LOST         = %d\n", bhdr.changes_lost);
    printf("------------------------------------------------------------------\n\n");

/*
	bhdr.last_chance_changes = 1;
	bh->change_groups[0].u.length_offset_pair[0]=0xbfffffbbbbbbbb;
*/
    
    if(bhdr.last_chance_changes) {
	unsigned long long scaled_size, scaled_offset, write_offset, write_size;
	unsigned long long size_offset_pair;
	int i;
    	printf("LAST CHANCE CHANGES  \n");
        for (i = 0; i < bhdr.last_chance_changes; i++) {
                size_offset_pair = bh->change_groups[i / MAX_CHANGES_IN_WRITE_GROUP ].u.length_offset_pair[i%MAX_CHANGES_IN_WRITE_GROUP];

                scaled_size = (unsigned long) (size_offset_pair >> 48);
                scaled_offset = size_offset_pair & 0xFFFFFFFFFFFFULL;

                write_offset = scaled_offset * bhdr.bitmap_granularity;
                write_size = scaled_size * bhdr.bitmap_granularity;

	

	printf("OFF = %20d| LEN = %15d| BLK = %5d\n", write_offset, write_size, scaled_size);
	}
        printf("------------------------------------------------------------------\n\n");
    }

	close(fd);
}

int
main(int argc, char **argv)
{

	int c;
	static struct option long_options[] = {
        /* These options set a flag. */
        {"read",  required_argument, 0, 'r'},
        {"extheader",  required_argument, 0, 'd'},
        {"header",  required_argument, 0, 'h'},
        {0, 0, 0, 0} };
        int option_index = 0;

	c = getopt_long (argc, argv, ":r:d:h:", long_options, &option_index);

	switch(c) {
		case 'r':
			read_bitmap_file(optarg);
			break;

		case 'h':
			print_bmap(optarg, 1);
			break;

		case 'd':
			read_bmap_exthdr_file(argv[2]);
			break;

		case 'c':
			printf("Yet to implement clearbits bitmap functionality\n");
			break;
		case 's':
			printf("Yet to implement setbits bitmap functionality\n");
			break;
		default:
			printf("Yet to implement unknown bitmap functionality\n");
			break;
        }
	return 0;			

}
