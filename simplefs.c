#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "simplefs.h"
#include <string.h>
#define DIRECTORYINFOSIZE 128
#define FILECONTROLINFOSIZE 128
#define MAXFILECOUNT 16


int vdisk_fd;

struct superBlockInfo{
	int totalMemorySize;	
	int blockNumber;	
	char freeSpace[ (BLOCKSIZE - 2 * sizeof(int)) ];
};

struct directoryEntryBlock{
	int isUsed;
	char directoryName[110];
	int fcbIndex;
	char freeSpace[10];
};

struct fileControlBlock{
	int index;
	int isUsed;
	int fileSize;
	char freeSpace[ FILECONTROLINFOSIZE - 3 * sizeof(int) ];	
};

struct openFileTableBlock{
	int mode;
	int filePosition;
	int fcbIndex;
	int fdIndex;
	int isUsed;
};

struct openFileTableBlock oft[MAXFILECOUNT];

// read block k from disk (virtual disk) into buffer block.
// size of the block is BLOCKSIZE.
// space for block must be allocated outside of this function.
// block numbers start from 0 in the virtual disk. 
int read_block (void *block, int k)
{
	int n;
	int offset;

	offset = k * BLOCKSIZE;
	lseek(vdisk_fd, (off_t) offset, SEEK_SET);
	n = read (vdisk_fd, block, BLOCKSIZE);
	if (n != BLOCKSIZE) {
		printf ("read error\n");
		return -1;
    	}
    	return (0); 
}

// write block k into the virtual disk. 
int write_block (void *block, int k)
{
    	int n;
    	int offset;

    	offset = k * BLOCKSIZE;
    	lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    	n = write (vdisk_fd, block, BLOCKSIZE);
    	if (n != BLOCKSIZE) {
		printf ("write error\n");
		return (-1);
    	}
    	return 0; 
}

int getBitFromInteger( unsigned int fourbyteInteger, int bitIndex )
{
	int bit = ( fourbyteInteger & (1 << bitIndex ))  != 0;
	
	return bit;
}

void toogleBitInInteger( unsigned int* fourbyteInteger, int bitIndex )
{
	*fourbyteInteger ^= 1 << bitIndex;
}

int freeGivenBlock( int index)
{
	struct superBlockInfo* info = (struct superBlockInfo* ) malloc(sizeof (struct superBlockInfo));
	
	int k = 0;
	
	read_block( (void*)info, 0);
	
	int blockNumber = info->blockNumber;
	
	free(info);
	
	unsigned int* bitmaps = malloc ((BLOCKSIZE / 4)* sizeof(unsigned int));

	int counter = 0;
	
	for( int i = 0; (i < 4)&&(counter < blockNumber) ; i++)
	{
		read_block( (void*)bitmaps , i + 1);
		
		for( int j = 0; (j < BLOCKSIZE / 4) &&(counter < blockNumber); j++ )
		{
			unsigned int anInt = bitmaps[j];
			
			for( int z = 0; (z < 32)&&(counter < blockNumber); z++)
			{
				int bit = getBitFromInteger( anInt, z);
				if( bit == 1 && counter == index )
				{			
					toogleBitInInteger( &anInt, z );
					bitmaps[j] = anInt;
					write_block( (void*)bitmaps, i+1);
					free(bitmaps);
					k = 1;
					return k;				
				}
				counter++;
			}
		}
	
	}
	
	return k;
}

int* findAvailableBLockIndexes( int numberOfBlockNeeded , int* found, int* numAvailable )
{
	
	struct superBlockInfo* info = (struct superBlockInfo* ) malloc(sizeof (struct superBlockInfo));
	
	
	read_block( (void*)info, 0);
	
	int blockNumber = info->blockNumber;
	
	free(info);
	
	unsigned int* bitmaps = malloc ((BLOCKSIZE / 4)* sizeof(unsigned int));
	
	int counter1 = 0;
	int counter2 = 0;
	
	for( int i = 0; (i < 4)&&(counter2 < blockNumber) ; i++)
	{
		read_block( (void*)bitmaps , i + 1);
		
		for( int j = 0; (j < BLOCKSIZE / 4) &&(counter2 < blockNumber); j++ )
		{
			unsigned int anInt = bitmaps[j];
			
			for( int z = 0; (z < 32)&&(counter2 < blockNumber); z++)
			{
				int bit = getBitFromInteger( anInt, z);
				if( bit == 0)
				{			
					counter1++;
				}
				counter2++;
			}
		}
	
	}
	
	//printf("num available %d \n",  counter1);
	
	if( counter1 >= numberOfBlockNeeded)
	{
		int* availableIndexArr = (int*) malloc(sizeof(int) * numberOfBlockNeeded);
		
		*found = 1;
		*numAvailable = numberOfBlockNeeded;
		counter1 = 0;
		counter2 = 0;
		
		for( int i = 0; (i < 4)&&(counter2 < blockNumber) ; i++)
		{
			read_block( (void*)bitmaps  , i + 1);
			
			for( int j = 0; (j < BLOCKSIZE / 4) &&(counter2 < blockNumber); j++ )
			{
				unsigned int anInt = bitmaps[j];
				
				for( int z = 0; (z < 32)&&(counter2 < blockNumber); z++)
				{
					int bit = getBitFromInteger( anInt, z);
					if( bit == 0 && counter1<numberOfBlockNeeded )
					{
						toogleBitInInteger( &anInt, z );
						bitmaps[j] = anInt;
						availableIndexArr[counter1] = counter2;
						counter1++;
					}
					counter2++;
				}
				
			}
			
			write_block( (void*)bitmaps, i+1);
		}
		
		free(bitmaps);
		
		return availableIndexArr;
	
	}
	else if( numberOfBlockNeeded >counter1 && counter1 > 0)
	{
		int* availableIndexArr = (int*) malloc(sizeof(int) * counter1);
		*found = 1;
		*numAvailable = counter1;
		counter1 = 0;
		counter2 = 0;
		
		for( int i = 0; (i < 4)&&(counter2 < blockNumber) ; i++)
		{
			read_block( (void*)bitmaps  , i + 1);
			
			for( int j = 0; (j < BLOCKSIZE / 4) &&(counter2 < blockNumber); j++ )
			{
				unsigned int anInt = bitmaps[j];
				
				for( int z = 0; (z < 32)&&(counter2 < blockNumber); z++)
				{
					int bit = getBitFromInteger( anInt, z);
					if( bit == 0 && counter1< (*numAvailable) )
					{
						toogleBitInInteger( &anInt, z );
						bitmaps[j] = anInt;
						availableIndexArr[counter1] = counter2;
						counter1++;
					}
					counter2++;
				}
				
			}
			
			write_block( (void*)bitmaps, i+1);
		}
		
		free(bitmaps);
		
		return availableIndexArr;
	}
	else{
		free(bitmaps);
		*found = 0;
		*numAvailable = 0;
		return NULL;		
	}
}

int getFileControlBlockIndex( char* fileName)
{
	int found = 0;
	
	struct directoryEntryBlock* dirBlocks = (struct directoryEntryBlock*) malloc( sizeof (struct directoryEntryBlock)* (BLOCKSIZE / DIRECTORYINFOSIZE));
	
	for( int j = 0; j < 4; j++)
	{
		read_block( (void*)dirBlocks , 5 + j);
	
		for( int i = 0; (i < BLOCKSIZE / DIRECTORYINFOSIZE) && (found == 0); i++ )
		{
			if( dirBlocks[i].isUsed == 1 && (strcmp(dirBlocks[i].directoryName, fileName) == 0) )
			{
				int x = dirBlocks[i].fcbIndex;
				free(dirBlocks);
				return x;
			}

		}	
	}
	free(dirBlocks);
	return -1;
}




/**********************************************************************
   The following functions are to be called by applications directly. 
***********************************************************************/

// this function is partially implemented.
int create_format_vdisk (char *vdiskname, unsigned int m)
{
	char command[1000];
	int size;
	int num = 1;
	int count;
	size  = num << m;
	count = size / BLOCKSIZE;
	//printf ("%d %d", m, size);
	sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d", vdiskname, BLOCKSIZE, count);
	//printf ("executing command = %s\n", command);
	system (command);
		    
	sfs_mount(vdiskname);
		    
	struct superBlockInfo* super = (struct superBlockInfo* ) malloc( sizeof (struct superBlockInfo));
	super->totalMemorySize = size;	// null but int
	super->blockNumber= count;
		    
	write_block( (void*)super, 0 );
		    
	free( super);
		    
	unsigned int numOfInteger = BLOCKSIZE / 4;
	unsigned int* bitMaps1 = malloc ((numOfInteger)* sizeof(unsigned int));
	unsigned int* bitMaps2 = malloc ((numOfInteger)* sizeof(unsigned int));
	unsigned int* bitMaps3 = malloc ((numOfInteger)* sizeof(unsigned int));
	unsigned int* bitMaps4 = malloc ((numOfInteger)* sizeof(unsigned int));
		    
	//store bits in four blocks     
	for( int i = 0; i < numOfInteger / 4; i++)
	{
		bitMaps1[i] = 0;
		    	
		bitMaps2[i] = 0;
		    	
		bitMaps3[i] = 0;
		    	
		bitMaps4[i] = 0;
	}
		    
	for( int i = 0; i < 13; i++ )
	{
		toogleBitInInteger( &bitMaps1[0], i);
	}
		    
	write_block( (void*)bitMaps1, 1 );
	write_block( (void*)bitMaps2, 2 );
	write_block( (void*)bitMaps3, 3 );
	write_block( (void*)bitMaps4, 4 );
		    
	free( bitMaps1 );
	free( bitMaps2 );
	free( bitMaps3 );
	free( bitMaps4 );
		    
	struct directoryEntryBlock* dirInfo1 = ( struct directoryEntryBlock* )malloc (sizeof( struct directoryEntryBlock)*(BLOCKSIZE / DIRECTORYINFOSIZE));
	struct directoryEntryBlock* dirInfo2 = ( struct directoryEntryBlock* )malloc (sizeof( struct directoryEntryBlock)*(BLOCKSIZE / DIRECTORYINFOSIZE));
	struct directoryEntryBlock* dirInfo3 = ( struct directoryEntryBlock* )malloc (sizeof( struct directoryEntryBlock)*(BLOCKSIZE / DIRECTORYINFOSIZE));
	struct directoryEntryBlock* dirInfo4 = ( struct directoryEntryBlock* )malloc (sizeof( struct directoryEntryBlock)*(BLOCKSIZE / DIRECTORYINFOSIZE));
		    
	for( int i = 0; i < BLOCKSIZE / DIRECTORYINFOSIZE; i++ )
	{
		dirInfo1[i].isUsed = 0;
		dirInfo1[i].fcbIndex = -1;
		    	
		dirInfo2[i].isUsed = 0;
		dirInfo2[i].fcbIndex = -1;
		    	
		dirInfo3[i].isUsed = 0;
		dirInfo3[i].fcbIndex = -1;
		    	
		dirInfo4[i].isUsed = 0;
		dirInfo4[i].fcbIndex = -1;
	}
		    
	write_block( (void*)dirInfo1, 5);
	write_block( (void*)dirInfo2, 6);
	write_block( (void*)dirInfo3, 7);
	write_block( (void*)dirInfo4, 8);
		    
	free( dirInfo1 );
	free( dirInfo2 );
	free( dirInfo3 );
	free( dirInfo4 );
		    
	struct fileControlBlock* fcbInfo1 = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
	struct fileControlBlock* fcbInfo2 = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
	struct fileControlBlock* fcbInfo3 = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
	struct fileControlBlock* fcbInfo4 = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
		    
	for( int i = 0; i < BLOCKSIZE / DIRECTORYINFOSIZE; i++ )
	{
		fcbInfo1[i].isUsed = 0;
		fcbInfo1[i].index = -1;
		fcbInfo1[i].fileSize = 0;
		    	
		fcbInfo2[i].isUsed = 0;
		fcbInfo2[i].index = -1;
		fcbInfo2[i].fileSize = 0;
		    	
		fcbInfo3[i].isUsed = 0;
		fcbInfo3[i].index = -1;
		fcbInfo3[i].fileSize = 0;
		    	
		fcbInfo4[i].isUsed = 0;
		fcbInfo4[i].index = -1;
		fcbInfo4[i].fileSize = 0;
	}
		    
	write_block( (void*)fcbInfo1, 9);
	write_block( (void*)fcbInfo2, 10);
	write_block( (void*)fcbInfo3, 11);
	write_block( (void*)fcbInfo4, 12);
		    
	free( fcbInfo1 );
	free( fcbInfo2 );
	free( fcbInfo3 );
	free( fcbInfo4 );
		    
	sfs_umount();
	
	return 0;
     	
}


// already implemented
int sfs_mount (char *vdiskname)
{
    	// simply open the Linux file vdiskname and in this
    	// way make it ready to be used for other operations.
    	// vdisk_fd is global; hence other function can use it. 
    	vdisk_fd = open(vdiskname, O_RDWR); 
    	
    	for( int i = 0; i < MAXFILECOUNT; i++)
    	{
    		oft[i].fcbIndex = -1;
    		oft[i].fdIndex = i;
    		oft[i].isUsed = 0;
    		oft[i].filePosition = -1;
    	}
    	
    	return(0);
}


// already implemented
int sfs_umount ()
{
    	fsync (vdisk_fd); // copy everything in memory to disk
    	close (vdisk_fd);
    	return (0); 
}


int sfs_create(char *filename)
{
	struct superBlockInfo* info = (struct superBlockInfo* ) malloc(sizeof (struct superBlockInfo));
	
	read_block( (void*)info, 0);
	
	int blockNum = info->blockNumber;
	
	free(info);
	
	//printf("create in içine girdim bro \n");
    	int index = 0;
	
    	int found1 = 0;
	
	int found2 = 0;
	
	int exist = 0;
	
	
	struct directoryEntryBlock* dirInfo2 = (struct directoryEntryBlock*) malloc( sizeof (struct directoryEntryBlock) * (BLOCKSIZE / DIRECTORYINFOSIZE));
	
	for( int j = 0; j < 4; j++)
	{
		read_block( (void*)dirInfo2, 5 + j);
	
		for( int i = 0; (i < BLOCKSIZE / DIRECTORYINFOSIZE) && (found2 == 0); i++ )
		{
			if( dirInfo2[i].isUsed == 1 && (strcmp(dirInfo2[i].directoryName, filename)== 0))
			{
				exist = 1;
			}
		}
	}
	
	free(dirInfo2);
	
		
	if( exist == 0)
	{
		struct fileControlBlock* fcbBlocks = (struct fileControlBlock*) malloc( sizeof (struct fileControlBlock) * (BLOCKSIZE / FILECONTROLINFOSIZE));
			
		for( int j = 0; j < 4; j++)
		{
			read_block( (void*)fcbBlocks, 9 + j);
		
			for( int i = 0; (i < BLOCKSIZE / FILECONTROLINFOSIZE) && (found1 == 0) && ( index < blockNum) ; i++ )
			{
				if( fcbBlocks[i].isUsed == 0)
				{
					int found = 0;
					int foundNum = 0;
					int* arr = findAvailableBLockIndexes( 1 , &found , &foundNum);
					if( arr != NULL)
					{
						fcbBlocks[i].index = arr[0];
						int* indexTable = malloc( sizeof(int) * (BLOCKSIZE / 4) );
						for( int z = 0; z < (BLOCKSIZE / 4); z++)
						{
							indexTable[z] = -1;
						}
						write_block((void*)indexTable, arr[0]);
						fcbBlocks[i].isUsed = 1;
						found1 = 1;		
						write_block( (void*)fcbBlocks, 9 + j);
						//printf( "fcb index no %d \n" ,index );
						//printf( "fcb pointing block no %d \n" ,fcbBlocks[i].index);
						//printf(" verdim yerini index table ve fcb \n");
						break;
					}
					else
					{
						return -1;
					}
					
					
				}
				else
				{
					index++;
				}
			}
			
			if( found1)
				break;
		}
		
		free(fcbBlocks);
		
		if( found1 == 0 )
		{
			return -1;
		}
		
		int index2 = 0;
			
		struct directoryEntryBlock* dirInfo = (struct directoryEntryBlock*) malloc( sizeof (struct directoryEntryBlock)* (BLOCKSIZE / FILECONTROLINFOSIZE));
		
		for( int j = 0; j < 4; j++)
		{
			read_block( (void*)dirInfo, 5 + j);
		
			for( int i = 0; (i < BLOCKSIZE / DIRECTORYINFOSIZE) && (found2 == 0) && (index2 < blockNum); i++ )
			{
				if( dirInfo[i].isUsed == 0)
				{
					
					dirInfo[i].isUsed = 1;
					strcpy( dirInfo[i].directoryName, filename);
					dirInfo[i].fcbIndex = index;
					found2 = 1;
					write_block( (void*)dirInfo, 5 + j);
					//printf("i : %d j: %d \n", i , j);
					//printf(" verdim yerini dir info \n");
					break;
				}
				else
				{
					index2++;
				}
			}
					
			if( found2)
				break;
		}
		
		free(dirInfo);
	    	return (0);
	   }
	   else
	   {
	   	return -1;
	   }
	    
}


int sfs_open(char *filename, int mode)
{

	//printf("girdim sfs open a \n");
	for( int i = 0; i < MAXFILECOUNT; i++)
	{
		if( oft[i].isUsed == 0)
		{
			oft[i].mode = mode;
			oft[i].filePosition = -1;
			oft[i].fcbIndex = getFileControlBlockIndex(filename);
			oft[i].isUsed = 1;
			/*
			printf("index open file: %d \n", i);
			printf("mode open file: %d \n", oft[i].mode );
			printf("fcbIndex open file: %d \n", oft[i].fcbIndex);
			printf("isUsed open file: %d \n", oft[i].isUsed);
			*/
		
			//printf("\n");
			
			return oft[i].fdIndex;
		}
		
	}
	return -1; 
}

int sfs_close(int fd){
	
	if( oft[fd].isUsed == 1 )
	{
		oft[fd].isUsed = 0;
		//printf("fcbIndex %d \n", oft[fd].fcbIndex);
		oft[fd].fcbIndex = -1;
		
		return 0;
	}
	else
	{
		return -1;
	}	 
}

int sfs_getsize (int  fd)
{
	int numberOfFcbPerBlock = BLOCKSIZE / FILECONTROLINFOSIZE;
	int fcbIndex = oft[fd].fcbIndex;
	int blockNo = fcbIndex /  numberOfFcbPerBlock;
	blockNo += 9; 
	int blockIndex = fcbIndex % numberOfFcbPerBlock;
	
	//printf("get size malloc önü \n");
	struct fileControlBlock* fcbInfo = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
	//printf("get size malloc sonu \n");
	
	read_block( (void*)fcbInfo , blockNo );
	
	int size = fcbInfo[blockIndex].fileSize;
	
	free(fcbInfo);
	
	return size;
}

int sfs_read(int fd, void *buf, int n){
	
	if( oft[fd].mode == MODE_READ )
	{

		int sizeOfTheFile = sfs_getsize(fd);
		int pos = oft[fd].filePosition;
			
		int remainder = sizeOfTheFile % BLOCKSIZE;
		
		int usedMaxIndex;
		
		if( remainder == 0 )
		{
			usedMaxIndex = (sizeOfTheFile / BLOCKSIZE) -1;
		}
		else
		{
			usedMaxIndex = sizeOfTheFile / BLOCKSIZE;
		}
		
		int lastBlockInRead;
		
		lastBlockInRead = (pos-1)/ BLOCKSIZE ;
		
		
		int remainder2 = (pos + 1) % BLOCKSIZE  ;
		
		
		int numberOfFcbPerBlock = BLOCKSIZE / FILECONTROLINFOSIZE;
		int fcbIndex = oft[fd].fcbIndex;
		int blockNo = fcbIndex /  numberOfFcbPerBlock;
		blockNo += 9; 
		int blockIndex = fcbIndex % numberOfFcbPerBlock;
		
		struct fileControlBlock* fcbInfo = ( struct fileControlBlock* )malloc (sizeof( struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
		
		read_block( (void*)fcbInfo , blockNo );
			
		int indexTableIndex = fcbInfo[blockIndex].index;
			
		int* indexTable = malloc( sizeof(int) * (BLOCKSIZE / 4) );
			
		read_block((void*)indexTable, indexTableIndex);
		
		int numReaded = 0;

		if( remainder2 != 0 )
		{
			int firstBlockReadableIndex = pos % BLOCKSIZE + 1;
			int lastBlockReadableIndex;
			
			if( lastBlockInRead == usedMaxIndex )
			{
				lastBlockReadableIndex = remainder;
			}
			else
			{
				lastBlockReadableIndex = BLOCKSIZE;
			}
			
			char* data = malloc(sizeof(char)*BLOCKSIZE);
			read_block( (void*)data, indexTable[lastBlockInRead] );
			
			for( int i = firstBlockReadableIndex ; i < lastBlockReadableIndex && numReaded < n; i++)
			{
				
				((char*)buf)[numReaded] = data[i];	
				numReaded++;
				oft[fd].filePosition = oft[fd].filePosition + 1;
			}
			
			lastBlockInRead++;
		}
		
		int forCount = usedMaxIndex - lastBlockInRead + 1;
		
		for( int i = 0; i < forCount; i++)
		{
			if( lastBlockInRead <= usedMaxIndex )
			{
				int lastBlockReadableIndex; 
				if( (n - numReaded) - BLOCKSIZE > 0 )
				{
					lastBlockReadableIndex = BLOCKSIZE;
				}
				else
				{
					lastBlockReadableIndex =  n - numReaded;
				}
				char* data = malloc(sizeof(char)*BLOCKSIZE);
				read_block( (void*)data, indexTable[lastBlockInRead] );
				
				for( int i = 0; i < lastBlockReadableIndex && numReaded < n; i++)
				{
					((char*)buf)[numReaded] = data[i];	
					numReaded++;
					oft[fd].filePosition = oft[fd].filePosition + 1;
				}
				lastBlockInRead++;
			}		
		}
		//printf(" file pos: %d " , oft[fd].filePosition );
		return numReaded;		
	}
	
	else
	{
		return -1;
	}
}


int sfs_append(int fd, void *buf, int n)
{

	//printf("append in içi bro \n");
	if( oft[fd].mode == MODE_APPEND )
	{
		
		//printf("append in içi if içi bro \n");
		int sizeOfTheFile = sfs_getsize(fd);
		//printf("size of the file %d \n", sizeOfTheFile );
		
		//printf("önce dönmesi gerek bro in içi if içi bro \n");
		int remainder = sizeOfTheFile % BLOCKSIZE;
		
		int newBlockNumberNeeded;
		
		if( remainder != 0 )
		{
			int remainingSpace = BLOCKSIZE - remainder;
			
			if( n > remainingSpace)
			{
				newBlockNumberNeeded = ((n-remainingSpace)/BLOCKSIZE);
				
				if( (n-remainingSpace) %BLOCKSIZE>0)
					newBlockNumberNeeded++;
			}
			else
				newBlockNumberNeeded = 0;
			
		}
		else
		{
			newBlockNumberNeeded = n /BLOCKSIZE;
			
			if( n % BLOCKSIZE > 0)
				newBlockNumberNeeded++;
				
		}
		
		
		
		int found = 0;
		int foundBlockNumber = 0;
		
		int* availableBlocks = findAvailableBLockIndexes( newBlockNumberNeeded, &found , &foundBlockNumber);
		//printf("num needed %d \n",  newBlockNumberNeeded);
		
		
		
		if( found == 0 )
		{
			return -1;
		}
		
		else
		{
		
			int numberOfFcbPerBlock = BLOCKSIZE / FILECONTROLINFOSIZE;
			int fcbIndex = oft[fd].fcbIndex;
			int blockNo = fcbIndex /  numberOfFcbPerBlock;
			blockNo += 9; 
			int blockIndex = fcbIndex % numberOfFcbPerBlock;
		
			struct fileControlBlock* fcbInfo = ( struct fileControlBlock* )malloc (sizeof( struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
		
			read_block( (void*)fcbInfo , blockNo );
			
			int indexTableIndex = fcbInfo[blockIndex].index;
			
			int* indexTable = malloc( sizeof(int) * (BLOCKSIZE / 4) );
			
			read_block((void*)indexTable, indexTableIndex);
			
			int firstUsableIndex = sizeOfTheFile / BLOCKSIZE;
			
			int remainingSpaceInFirstIndex;
			
			int nHolder = 0;
			
			int numBytes = 0;
			
			if( n >= BLOCKSIZE - remainder )
			{
				numBytes = BLOCKSIZE - remainder; 
			}
			else
			{
				numBytes = n;
			}
			
			if( remainder != 0)
			{
				remainingSpaceInFirstIndex = remainder;
				int blockIndexToWrite = indexTable[firstUsableIndex];
				char* data = malloc(sizeof(char)*BLOCKSIZE);
				read_block( (void*)data,blockIndexToWrite);
				int free = BLOCKSIZE - remainder;
				for( int k = remainder; k < numBytes + remainder; k++)
				{
					((char*)data)[k] = ((char*)buf)[nHolder];
					nHolder++;
				}
				
				//strcpy( data + remainder, (char*)buf, numBytes);
				//printf( "mode : %d block ındex: %d  data: %d %d %d %d", remainder, blockIndexToWrite, data[0], data[1], data[2], data[3]);
				write_block((void*)data, blockIndexToWrite);
				n = n- numBytes;
				buf = (char*)buf + numBytes;
				firstUsableIndex++;
				//nHolder += numBytes;
				//printf("n holder ın artacağı yere girdiii \n");
			}
			

			for( int i = 0; i < foundBlockNumber; i++)
			{
				if( firstUsableIndex < (BLOCKSIZE / 4) )
				{
					//printf("n holder ın artacağı yere girdiii \n");
					indexTable[firstUsableIndex] = availableBlocks[i]; 
					char* data = malloc(sizeof(char)*BLOCKSIZE);
					read_block( (void*) data, availableBlocks[i] );
					int byteNumber;
					if( n -BLOCKSIZE >= 0 )
					{
						byteNumber = BLOCKSIZE;
						n = n-BLOCKSIZE;
					}
					else
					{
						byteNumber = n;
						n -= byteNumber;
					}
					
					for( int k = remainder; k < numBytes + remainder; k++)
					{
						((char*)data)[k] = ((char*)buf)[nHolder];
						nHolder++;
					}
					//strcpy( data + remainder, (char*)buf, byteNumber);
					//remainder = remainder + byteNumber;
					//nHolder += byteNumber;
					//printf( "mode: %d block ındex: %d  data: %d %d %d %d", remainder, availableBlocks[i], data[0], data[1], data[2], data[3]);
					write_block( (void*)data, availableBlocks[i]);
					buf = (char*)buf + byteNumber;
				}	
			 }
			 
			 
			 write_block((void*)indexTable, indexTableIndex);
				
			 fcbInfo[blockIndex].fileSize = fcbInfo[blockIndex].fileSize + nHolder;

			 write_block( (void*) fcbInfo, blockNo);
			 //printf("byes written %d \n", nHolder);
			 //printf("\n");
			
			 return nHolder;
		}
	}
	else
	{
		//printf("this file cannot be appended");
		return -1;
	}
	
}

int sfs_delete(char *filename)
{

	struct directoryEntryBlock* dirBlocks = (struct directoryEntryBlock*) malloc( sizeof (struct directoryEntryBlock) *( BLOCKSIZE / DIRECTORYINFOSIZE));
	
	for( int j = 0; j < 4; j++)
	{
		read_block( (void*)dirBlocks , 5 + j);
		
		for( int i = 0; i < BLOCKSIZE / DIRECTORYINFOSIZE; i++ )
		{
			if( strcmp(dirBlocks[i].directoryName, filename) == 0 &&  dirBlocks[i].isUsed == 1 )
			{
				strcpy(dirBlocks[i].directoryName, "");
				dirBlocks[i].isUsed = 0;
				int fcbIndex = dirBlocks[i].fcbIndex;
				dirBlocks[i].fcbIndex = -1;
				int numberOfFcbPerBlock = BLOCKSIZE / FILECONTROLINFOSIZE;
				int blockNo = fcbIndex /  numberOfFcbPerBlock;
				blockNo += 9; 
				int blockIndex = fcbIndex % numberOfFcbPerBlock;
				struct fileControlBlock* fcb = ( struct fileControlBlock* )malloc (sizeof(struct fileControlBlock)*(BLOCKSIZE / FILECONTROLINFOSIZE));
				read_block( (void*)fcb , blockNo );
				
				fcb[blockIndex].isUsed = 0;
				
				int indexTableIndex = fcb[blockIndex].index;
				
				int* indexTable = malloc( sizeof(int) * (BLOCKSIZE / 4) );
			
				read_block((void*)indexTable, indexTableIndex);
				
				int index1 = 0;
				
				for( int k = 0; k < MAXFILECOUNT ; k++)
				{
					if( oft[k].fcbIndex == fcbIndex && oft[k].isUsed == 1 )
					{
						oft[k].isUsed = 0;
					}
				}
				
				while( indexTable[index1] != -1)
				{
					char* data = malloc( sizeof(char)* BLOCKSIZE);
					read_block( (void*)data, indexTable[index1]);
					free( data );
					write_block((void*)data, indexTable[index1]);
					int isTrue =  freeGivenBlock( indexTable[index1] );
					index1++;
				}
				
				free(indexTable);
				
				write_block((void*)indexTable, indexTableIndex );
				
				
				int isTrue =  freeGivenBlock( indexTableIndex );
				
				fcb[blockIndex].index = -1;
				fcb[blockIndex].fileSize = 0;
				
				write_block((void*)fcb, blockNo  );
			
			}
		}
		write_block( (void*)dirBlocks , 5 + j);
		
	}
	
    	return (0); 
}

