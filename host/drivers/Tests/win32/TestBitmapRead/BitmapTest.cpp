#include "global.h"

#define SEGMENT_SIZE  (4096)
#define FILE_SEGMENTS_COUNT (2) //Number of segments to be filled in file
#define TEST1TOTALBITMAP  (ULONG64)SEGMENT_SIZE * 8 * (ULONG64)FILE_SEGMENTS_COUNT
#define TEST2TOTALBITMAP  (ULONG64)((SEGMENT_SIZE + 1020)* 8)
#define TEST3TOTALBITMAP  (ULONG64)(((SEGMENT_SIZE + 1020)* 8) + 6)
#define TEST4TOTALBITMAP  (ULONG64)(6)
ULONG64 nextSearchOffset, bitsInBitmap;
UCHAR filename[13] = "Bitmap  .bin";
UCHAR ReadMeName[13] = "Readme  .txt";
UCHAR BufferCache[FILE_SEGMENTS_COUNT][SEGMENT_SIZE];
int Cases[]={0x00000000,//0000_0000
             0xFFFFFFFF,//1111_1111
             0x55555555,//0101_0101
             0xAAAAAAAA,//1010_1010
             0x33333333,//0011_0011
             0xCCCCCCCC,//1100_1100
             0xEEEEEEEE,//1110_1110
             0x77777777,//0111_0111
             0xF0F0F0F0,//1111_0000
             0x0F0F0F0F,//0000_1111
             0xF8F8F8F8,//1111_1000
             0x8F8F8F8F,//1000_1111
             0xFCFCFCFC,//1111_1100
             0xCFCFCFCF,//1100_1111
             0xFEFEFEFE,//1111_1110
             0xEFEFEFEF,//1110_1111
            };
             
int main(int argc,char *argv[])
{
    FILE *TestResult;
    NTSTATUS status = 1;
    if(argc > 1){
        switch (*argv[1]){

        case 'w':
        case 'W':
            if(CreateAndWriteFile() != STATUS_SUCCESS){
                printf("Creation of the data files failed");
                return 0;
            }
            else{
                printf("\nCreation of the data files successful \n");
            }
            break;
        case 'h':
        case 'H':
            printf("This program test the bitmap code \n ");
            printf("To create data files use option TestBitmapRead.exe w \n ");
            break;
        default:
            printf("Wrong option try again \n");
            printf("For Help use Option TestBitmapRead.exe h \n");
        }
    }

	//TestCase 1: In this the total bitmap size is the
    //multiple of the segment size
    bitsInBitmap = TEST1TOTALBITMAP;
    if((TestResult = fopen("TestResultCase01.log","w+t")) == NULL){
        printf("Fail to open the Test Result Log file");
        return STATUS_FAIL;
    }
    fprintf(TestResult,"\n**********************************************************\n");
    fprintf(TestResult,"TestCase 1: In this the total bitmap size is the multiple of the segment size\n");
	fprintf(TestResult,"The total bitmap size is equal to %d\n",bitsInBitmap);
    fprintf(TestResult,"\n**********************************************************\n");
    status = RunTest(TestResult);
	if(status == STATUS_FAIL)
		return 1;

    //TestCase 2: In this the total bitmap size is NOT
    //the multiple of the segment size but is the multiple of 8
    bitsInBitmap = TEST2TOTALBITMAP;
    if((TestResult = fopen("TestResultCase02.log","w+t")) == NULL){
        printf("Fail to open the Test Result Log file");
        return STATUS_FAIL;
    }
    fprintf(TestResult,"\n**********************************************************\n");
    fprintf(TestResult,"TestCase 2: In this the total bitmap size is NOT the multiple of the segment size but is the multiple of 8\n");
	fprintf(TestResult,"The total bitmap size is equal to %d\n",bitsInBitmap);
    fprintf(TestResult,"\n**********************************************************\n");
    status = RunTest(TestResult);
	if(status == STATUS_FAIL)
		return STATUS_FAIL;

	//TestCase 3: In this the total bitmap size is NOT the 
    //multiple of the segment size and is NOT the multiple of 8
    bitsInBitmap = TEST3TOTALBITMAP;
    if((TestResult = fopen("TestResultCase03.log","w+t")) == NULL){
        printf("Fail to open the Test Result Log file");
        return STATUS_FAIL;
    }
    fprintf(TestResult,"\n**********************************************************\n");
    fprintf(TestResult,"TestCase 3: TestCase 3: In this the total bitmap size is NOT the multiple of the segment size and is NOT the multiple of 8\n");
	fprintf(TestResult,"The total bitmap size is equal to %d\n",bitsInBitmap);
    fprintf(TestResult,"\n**********************************************************\n");
    status = RunTest(TestResult);
	if(status == STATUS_FAIL)
		return STATUS_FAIL;
	
	//TestCase 4: In this the total bitmap size is Less then 8 bits
    bitsInBitmap = TEST4TOTALBITMAP;
    if((TestResult = fopen("TestResultCase04.log","w+t")) == NULL){
        printf("Fail to open the Test Result Log file");
        return STATUS_FAIL;
    }
    fprintf(TestResult,"\n**********************************************************\n");
    fprintf(TestResult,"TestCase 4: In this the total bitmap size is Less then 8 bits\n");
	fprintf(TestResult,"The total bitmap size is equal to %d\n",bitsInBitmap);
    fprintf(TestResult,"\n**********************************************************\n");
    status = RunTest(TestResult);
	if(status == STATUS_FAIL)
		return STATUS_FAIL;

    _fcloseall( );
    return STATUS_SUCCESS;
}

/*This Function has the logic to test the Bitmap code.
 *It makes use of all the bimap.bin files and test for all 
 *the test cases. It prints the log into the file TestResult.log
 *It first creates the BufferCache by reading from the file in the
 *segments equal to the segment length.
 *It then runs the all the possible test on the particular file.
 */ 
NTSTATUS
RunTest(FILE *TestResult){
    int i,j,Index=0;
    FILE *FileRead;
    NTSTATUS status = 1;
    ULONG32 bitsInRun;
    ULONG64 bitOffset;
    //The Main loop to parse all the bitmap files
    for(i=0;i< (sizeof(Cases)/sizeof(int)); i++){
		nextSearchOffset = 0;
        filename[6] = '0'+ i/10;
        filename[7] = '0'+ (i%10);
        if((FileRead = fopen((const char*)filename,"r")) == NULL){
            printf("\nFailed to open the file %s \n",filename);
            printf("Run the <TestBitmapRead.exe w> to create the data files\n");
            fprintf(TestResult,"Failed to open the file %s \n",filename);
            fprintf(TestResult,"Run the <TestBitmapRead.exe w> to create the data files\n");
            _fcloseall( );
            return STATUS_FAIL;
        }
        //read from the file into the buffer
        for(j=0;j<FILE_SEGMENTS_COUNT;j++){
            fread(BufferCache[j],sizeof(char),SEGMENT_SIZE,FileRead);
        }
        fprintf(TestResult,"\n---------------------------------- \n");
        fprintf(TestResult,"For the file %s (Data = 0x%x) the output follows\n",filename,Cases[i]);
        fprintf(TestResult,"---------------------------------- \n");
        while(1){
            status = GetNextBitRun(&bitsInRun, &bitOffset);
            if(status != STATUS_SUCCESS)
                break;
            fprintf(TestResult,"Index=%d    bitsInRun=%d    bitOffset=%d  \n",Index++,bitsInRun,bitOffset);
        }
    }
    _fcloseall( );
    return 1;
}


/*This Function creates the bitmap files and the
 *coonsecutive readme files. These files are used
 *later on.
 */
NTSTATUS 
CreateAndWriteFile(void){
    int i,j,status=STATUS_SUCCESS;
    FILE *BitmapFile, *ReadMeFile;
    int NumberOfBytes = 4;
    int binary = 0x80;//in binary 1000_0000
    UCHAR BitmapSegment[SEGMENT_SIZE];
    for(i=0;i< (sizeof(Cases)/sizeof(int)); i++){
        filename[6] = '0'+ i/10;
        filename[7] = '0'+ (i%10);
        ReadMeName[6] = '0'+ i/10;
        ReadMeName[7] = '0'+ (i%10);
        if(((BitmapFile = fopen((const char *)filename,"w")) == NULL)||
            ((ReadMeFile = fopen((const char *)ReadMeName,"w+t")) == NULL)){
                if(BitmapFile == NULL)
                    printf("Error to Create bitmap File %s",filename);
                else
                    printf("Error to Create Readme File %s",ReadMeName);
                return STATUS_FAIL;
        }
        else{
            //writing the information in the readme files
            fprintf(ReadMeFile,"This is the Information file for %s \n",filename);
            fprintf(ReadMeFile,"%s contains the data in the format  0x%x \n",filename,Cases[i]);
            fprintf(ReadMeFile,"In binary one byte of the data can be shown as  ");
            while(binary){
                if(Cases[i] & binary)
                    fprintf(ReadMeFile,"%d",1);
                else
                    fprintf(ReadMeFile,"%d",0);

                binary >>=1;
            }
            fprintf(ReadMeFile,"\nThis file is filled with %d segments of size %d or %d bytes of the data shown above\n"
                ,FILE_SEGMENTS_COUNT,SEGMENT_SIZE,FILE_SEGMENTS_COUNT * SEGMENT_SIZE);

            //writing data in the files in the format depending
            //upon cases
            memcpy(BitmapSegment, &Cases[i], sizeof(int));
            while(1){   
                if(NumberOfBytes >= SEGMENT_SIZE)
                    break;
                memcpy(&BitmapSegment[NumberOfBytes], BitmapSegment, NumberOfBytes);
                NumberOfBytes = NumberOfBytes * 2;
            }
            for(j = 0;j < FILE_SEGMENTS_COUNT;j ++){
            if(fwrite(BitmapSegment,1,SEGMENT_SIZE,BitmapFile) < SEGMENT_SIZE)
                return STATUS_FAIL;
            }
        }
        NumberOfBytes = 4;
        binary = 0x80;
    }
    _fcloseall( );
    return status;
}


RETURNCODE GetNextBitRun(ULONG32 * bitsInRun, ULONG64 *bitOffset)
{
TRC
NTSTATUS status;
UCHAR * bitBuffer = NULL;
ULONG32 bitBufferByteSize = 0;
ULONG32 adjustedBufferSize;
ULONG64 byteOffset;
ULONG32 searchBitOffset;
ULONG32 runOffset;
ULONG32 runLength;

    // this is passive level synchronous code currently
    *bitsInRun = 0;
    *bitOffset = 0;
    runLength = 0;
    runOffset = 0;
    status = STATUS_SUCCESS;

    while (nextSearchOffset < bitsInBitmap) {
        byteOffset = nextSearchOffset / 8;
        status = ReadAndLock(byteOffset, &bitBuffer, &bitBufferByteSize);
        if (NT_SUCCESS(status)) {
            searchBitOffset = (ULONG32)(nextSearchOffset % 8);
            adjustedBufferSize = (ULONG32)min((bitBufferByteSize * 8),
                                              (bitsInBitmap - (byteOffset * 8)));

            status = GetNextBitmapBitRun(bitBuffer, 
                            adjustedBufferSize, 
                            &searchBitOffset, // in and out parameter, set search start and is updated
                            &runLength,
                            &runOffset);
//            /* status = */ segmentMapper->Unlock(byteOffset);
            if (!NT_SUCCESS(status)) {
                break;
            }
            // searchBitOffset and runOffset will return relative to start of bitBuffer not old nextSearchOffset
            nextSearchOffset = (byteOffset * 8) + searchBitOffset;
            if (runLength > 0) {
                *bitOffset = (byteOffset * 8) + runOffset;
                break;
            } 
        } else {
            // segment mapper could not map buffer
            break;
        }
    }

    if (NT_SUCCESS(status)) {
        if (runLength > 0) {
            *bitsInRun = runLength;
            status = STATUS_SUCCESS;
        } else {
            *bitsInRun = 0;
            *bitOffset = nextSearchOffset;
            status = STATUS_ARRAY_BOUNDS_EXCEEDED;
        }
    }

    return status;
}


NTSTATUS ReadAndLock(ULONG64 offset, UCHAR **returnBufferPointer, ULONG32 *partialBufferSize)
{
    ULONG32 BufferIndex;
    BufferIndex = (ULONG32)(offset / (ULONG64)SEGMENT_SIZE);
    *partialBufferSize = (ULONG32)(SEGMENT_SIZE - (offset % (ULONG64)SEGMENT_SIZE));
    *returnBufferPointer = &BufferCache[BufferIndex][(ULONG32)(offset % (ULONG64)SEGMENT_SIZE)];
    if(*partialBufferSize < 0)
    return STATUS_ARRAY_BOUNDS_EXCEEDED;
    else
    return STATUS_SUCCESS;
}
