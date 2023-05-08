#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#define BMAP_HEADER_SZ 16384
#define SECTOR_SIZE 512
#define PAGESIZE (0x2000)

int main(int argc, char *argv[])
{
	int fd = 0;
	char *fname = NULL;
	char *tmpfile = NULL;	
	int file_size = 0;
	struct stat stat_buf;
	off_t offset = 0;
	size_t readsize = 0;
	size_t total_read = 0;
	size_t byte_bitmap = 0;
	int process = 0;
	int i = 0;
	int ret = 0;
	int check_bit = 1;
	int cur_char = 0;
	long long no_bit_set = 0;
	unsigned long long bmap_gran = 0;
	unsigned long long vol_size = 0;
	char *databuf = NULL;

	if(argc < 2){
		printf("pass the bitmap file name\n");
		goto out;
	}
	fname = argv[1];
	if(!(fd = open(fname, O_RDONLY))){
		printf("failed to open %s file with errno %d", fname, errno);
		ret = 1;
		goto out;
	}
	if(stat(fname, &stat_buf)){
		printf("stat on file %s is failed with err %d",fname, errno);
		ret = 1;
		goto out;
	}
	databuf = (char *)malloc(PAGESIZE);
	if(!databuf){
		printf("failed to alloc data buffer of size %llu",stat_buf.st_size);
		ret = 1;
		goto out;
	}
	memset(databuf, 0, PAGESIZE);
	if((readsize = read(fd, databuf, SECTOR_SIZE)) == -1){
		printf("failed to read file %s\n", fname);
		ret = 1;
		goto out;
	}
	bmap_gran = *((unsigned long long *)(databuf + 0x30));
	vol_size = *((unsigned long long *)(databuf + 0x38));
	byte_bitmap = (vol_size + bmap_gran - 1)/ bmap_gran;
	byte_bitmap = (byte_bitmap + 7 ) / 8;
	printf("total bitmap size is %d\n",byte_bitmap);
	if((offset = lseek(fd, BMAP_HEADER_SZ, SEEK_SET)) == -1){
		printf("lseek fail to set the curser at %d\n",BMAP_HEADER_SZ);
		ret = 1;
		goto out;
	}
	while (total_read < (byte_bitmap - 1)){
		if((readsize = read(fd, databuf, PAGESIZE)) == -1){
			printf("failed to read file %s\n", fname);
			ret = 1;
			goto out;
		}
		total_read += readsize;
		if((readsize != PAGESIZE) && (total_read != byte_bitmap)){
			printf("failed to read full file %s\n", fname);
			ret = 1;
			goto out;
		}
		process = 0;
		while (process < readsize){
			cur_char = (int)(databuf[process]);
			for (i = 1; i < 8; i++){
				if(check_bit & cur_char){
					no_bit_set++;
					printf("bit set at index %d, total read till now is %u process %d, char %dcheck_bit is %d\n",i, total_read, process, cur_char, check_bit);
				}
				check_bit <<= 1;
			}
			check_bit = 1;
			process++;
		}
	}
	printf("total no of bit set in bitmap %lld, no of byte process %u\n",no_bit_set, total_read);
	printf("total changes in bitmap %lld bytes\n",(no_bit_set * bmap_gran));
out:
	return ret;
}
