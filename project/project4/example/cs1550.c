/*
FUSE: Filesystem in Userspace
Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

This program can be distributed under the terms of the GNU GPL.
See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//define boolean variables
#define TRUE 1
#define FALSE 0

//size of .disk (5 MB = 5*2^20 bytes)
#define DISK_SIZE 5242880		//bytes

//bitmap size
#define BITMAP_SIZE  (DISK_SIZE/BLOCK_SIZE)

//number of blocks of bitmap
#define BITMAP_BLOCKS	BITMAP_SIZE / BLOCK_SIZE

//number of block (except bitmap)
#define BLOCK_COUNT (DISK_SIZE / BLOCK_SIZE - BITMAP_BLOCKS)

//beginning of the bitmap
#define BITMAP_HEAD  (BLOCK_COUNT * BLOCK_SIZE)

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
	//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
	//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//bitmap strcut
struct Bitmap
{
	char bits[BITMAP_SIZE];
};

typedef struct Bitmap Bitmap;

//operation for disk --- function prototypes
static FILE* open_disk(void);	//open the disk
static void close_disk(FILE *disk);	//close the disk
static int update_bitmap(FILE *f, long block_idx, char val);	//update the bitmap
static cs1550_root_directory read_root(FILE *disk);	//return the first block of .disk
static void* get_block(FILE *f, long block_idx);		//return the whole block
static void write_block(FILE *disk, int block_idx, void* block);		//update the block
static long get_free_block(FILE *f);			//return the index to the free blcok
static void read_bitmap(FILE *disk, Bitmap *bitmap);		//read the bitmap and return


//implementations of function prototypes

//open the .disk
static FILE* open_disk(void){
	//create a file pointer to manage the disk
	FILE *file;
	file  = fopen("./.disk", "r+b");
	// Check if disk file is able to be open
	if (file == NULL) {
		return NULL;
	}

	// Check disk file size
	if (fseek(file, 0, SEEK_END) || ftell(file) != DISK_SIZE) {
		fclose(file);
		fprintf(stderr, "disk file is not valid\n");
		return NULL;
	}
	return file;
}

//close the .disk
static void close_disk(FILE *f){
	//check if whether the file is closed
	if(fclose(f) != 0){
		fprintf(stderr, "close the disk failed\n");
	}
	return;
}

//read the root
//Read root from disk
static cs1550_root_directory read_root(FILE *disk){
	fseek(disk, 0, SEEK_SET);
	cs1550_root_directory root;
	fread(&root, BLOCK_SIZE, 1, disk);
	return root;
}

static void* get_block(FILE *file, long block_idx){
	/*if (!(block_idx < BLOCK_COUNT)) {
	perror("requested block %ld does not exist\n");
	return NULL;
}*/
if (fseek(file, block_idx * BLOCK_SIZE, SEEK_SET)) {
	fprintf(stderr, "failed to seek to block\n");
	return NULL;
}
void *block = malloc(BLOCK_SIZE);
if (fread(block, BLOCK_SIZE, 1, file) != 1) {
	fprintf(stderr, "failed to load block\n");
	free(block);
	return NULL;
}
return block;
}

static long get_free_block(FILE *f){
	int free_blcok_idx = -1;
	struct Bitmap bitmap;

	//char *bitmap = malloc(BITMAP_SIZE);
	read_bitmap(f,&bitmap);	//read the bitmap

	//loop through the bitmap and see whether we have free block
	int i;
	int found = FALSE;
	//we loop from 1 because the first block is saved for the root
	for(i=1; i < BITMAP_SIZE;i++){
		if(bitmap.bits[i] == 0){
			//we have a free block
			found = TRUE;
			break;
		}
	}

	if(!found){
		free_blcok_idx = -1;		//no free block
	}
	else{
		//ready to return the index
		free_blcok_idx = i;

	}
	return free_blcok_idx;
}

/*
0 means the block is free
1 means the block is used
*/
static int update_bitmap(FILE *disk, long block_idx, char val){

	if (block_idx >= BLOCK_COUNT) {
		return -1;
	}

	int success = 0;
	struct Bitmap bitmap;
	read_bitmap(disk,&bitmap);	//read the bitmap

	bitmap.bits[block_idx] = val;		//set the bitmap value

	fseek(disk, BITMAP_HEAD, SEEK_SET);
	fwrite(&bitmap,sizeof(char),BITMAP_SIZE,disk);

	return success;

}

/*
static Bitmap read_bitmap2(FILE *disk){
	//create a bitmap
	struct Bitmap bitmap;
	//get to the head of the bitmap
	fseek(disk,BITMAP_HEAD,SEEK_SET);
	//read the Bitmap
	fread(&bitmap,sizeof(char),BITMAP_SIZE,disk);
	return bitmap;

}
*/

static void read_bitmap(FILE *disk, Bitmap *bitmap){
	//get to the head of the bitmap
	fseek(disk,BITMAP_HEAD,SEEK_SET);
	//read the Bitmap
	fread(bitmap,sizeof(char),BITMAP_SIZE,disk);
	return;

}

static void write_block(FILE *disk, int block_idx, void* block){
	//locate where we want to write
	if (fseek(disk, block_idx * BLOCK_SIZE, SEEK_SET)) {
		fprintf(stderr, "failed to seek to block\n");
		return;
	}

	fwrite(block,BLOCK_SIZE,1,disk);
	return;
}



//real functions

/*
* Called whenever the system wants to know the file attributes, including
* simply whether the file exists or not.
*
* man -s 2 stat will show the fields of a stat structure
*/
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {

		//Check if name is subdirectory
		/*
		//Might want to return a structure with these fields
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		res = 0; //no error
		*/

		//Check if name is a regular file
		/*
		//regular file, probably want to be read and write
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1; //file links
		stbuf->st_size = 0; //file size - make sure you replace with real size!
		res = 0; // no error
		*/

		//implementation
		char directory[MAX_FILENAME + 1];	// + 1 is for nul
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		//initialize variables
		strcpy(directory, "");
		strcpy(filename, "");
		strcpy(extension, "");
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
		/*
		Comment for %[^\n]:
		the %[^\n] conversion specification, which matches a string of all characters
		not equal to the new line character ('\n') and stores it (plus a terminating
		'\0' character) in str.
		*/

		//check whether the name is valid for our rule
		if(strlen(directory) > MAX_FILENAME || strlen(filename) > MAX_FILENAME || strlen(extension) > MAX_EXTENSION){
			return -ENAMETOOLONG;
		}

		//Clear the data in stbuf JUST IN CASE
		memset(stbuf, 0, sizeof(struct stat));

		//check if name is sub_directory
		if(strcmp(directory,"") != 0){
			//this means the directory exits
			FILE *disk = open_disk();		//ready to read the disk blocks

			//just sub_directory
			cs1550_root_directory root = read_root(disk);		//read the root folder

			//search for the directory
			int i;
			int result = FALSE;
			struct cs1550_directory dir;
			for(i = 0; i < root.nDirectories; i++){
				//locate the current directory
				struct cs1550_directory current = root.directories[i];
				//compare the name
				if(strcmp(current.dname,directory) == 0){
					//we found this directory, now set up the struct
					result = TRUE;
					dir = current;
					break;
				}
			}

			//if we found the directory
			if(result == TRUE){

				//then we chech whether it is a file
				if(strcmp(filename,"") != 0){
					//this means it is a file
					//now we need to locate the beginning of this directory
					struct cs1550_directory_entry *entry = get_block(disk, dir.nStartBlock);

					if(entry == NULL){
						fprintf(stderr, "some error occurs\n");
						res = -ENOENT;
						return res;
					}

					result = FALSE;		//reset the result
					int num_files = entry->nFiles;	  //numer of files current directory has
					if(num_files > MAX_FILES_IN_DIR){
						fprintf(stderr, "too many files or error of reading\n");
					}
					//search for the file
					int j;
					for(j = 0; j < num_files; j++){
						struct cs1550_file_directory cur_file = entry -> files[j];
						//check if the file name matches
						if(strcmp(cur_file.fname, filename) == 0){
							stbuf->st_mode = S_IFREG | 0666;
							stbuf->st_nlink = 1; //file links
							stbuf->st_size = entry -> files[j].fsize; //file size - make sure you replace with real size!
							res = 0;
							result = TRUE;
							break;
						}
					}

					if(result == FALSE){
						res = -ENOENT;
						return res;
					}

				}

				else{
					//just the directory
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					res = 0;
				}

			}
			else{
				//Else return that path doesn't exist
				res = -ENOENT;
				return res;
			}
			close_disk(disk);
		}
		else{
			//Else return that path doesn't exist
			res = -ENOENT;
			return res;
		}


	}
	return res;
}

/*
* Called whenever the contents of a directory are desired. Could be from an 'ls'
* or could even be when a user hits TAB to do autocompletion
*/
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
	{
		//Since we're building with -Wall (all warnings reported) we need
		//to "use" every parameter, so let's just cast them to void to
		//satisfy the compiler
		(void) offset;
		(void) fi;

		//This line assumes we have no subdirectories, need to change
		//if (strcmp(path, "/") != 0)
		//return -ENOENT;

		//the filler function allows us to add entries to the listing
		//read the fuse.h file for a description (in the ../include dir)
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		//implementation
		int res = 0;		//initialize the result to 0

		//decompose the path
		char directory[MAX_FILENAME + 1];	// + 1 is for nul
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		//initialize variables
		memset(extension, 0, MAX_EXTENSION + 1);
		memset(directory, 0, MAX_FILENAME + 1);
		memset(filename, 0, MAX_FILENAME + 1);
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

		//condition check
		if(strlen(extension) > MAX_EXTENSION || strlen(filename) > MAX_FILENAME || strlen(directory) > MAX_FILENAME){
			res = -ENAMETOOLONG;
			return res;
		}

		FILE *disk = open_disk();
		//Now we search the file and see whether it is exists
		//first read the root and locate the directory
		struct cs1550_root_directory root = read_root(disk);		//get the root

		//if want to read root
		if(strlen(directory) == 0){
			int j;
			//loop all the directories in the root
			for(j = 0; j < root.nDirectories; j ++){
				struct cs1550_directory dir = root.directories[j];
				if(strcmp(dir.dname,"") != 0){
					filler(buf,dir.dname,NULL,0);
				}
			}

			res = 0;
			return res;
		}

		int i,found = FALSE;
		struct cs1550_directory_entry *entry;
		for(i = 0; i < root.nDirectories; i++){
			//check whether the directory exists
			if(strcmp(root.directories[i].dname,directory) == 0){
				//we found the directory
				found = TRUE;
				entry = get_block(disk,root.directories[i].nStartBlock);
				break;
			}
		}

		if(!found){
			res = ENOENT;
			return res;
		}

		for(i = 0; i < entry -> nFiles; i++){
			memset(extension, 0, MAX_EXTENSION + 1);
			memset(filename, 0, MAX_FILENAME + 1);
			struct cs1550_file_directory file = entry -> files[i];
			if(strcmp(file.fname,"") != 0){
				strcpy(filename,file.fname);
				if(strcmp(file.fext,"") != 0){
					strcat(filename,".");
					strcat(filename,file.fext);
				}
				filler(buf,filename,NULL,0);
			}
		}

		res = 0;

		close_disk(disk);
		/*
		//add the user stuff (subdirs or files)
		//the +1 skips the leading '/' on the filenames
		filler(buf, newpath + 1, NULL, 0);
		*/
		return res;
	}

	/*
	* Creates a directory. We can ignore mode since we're not dealing with
	* permissions, as long as getattr returns appropriate ones for us.
	*/
	static int cs1550_mkdir(const char *path, mode_t mode)
	{
		(void) path;
		(void) mode;

		int success = 0;

		//read the root
		FILE *disk = open_disk();		//ready to raed the disk blocks
		if (!disk) return -ENXIO;		//if open the disk has error

		struct cs1550_root_directory root = read_root(disk);

		//search for the directory and check whether it is exists
		//path will be in the format of /directory/sub_directory
		char* directory; //The first directory in the 2-level file system

		//Parse the two strings
		int path_length = strlen(path);
		char path_copy[path_length];
		strcpy(path_copy, path);

		directory = strtok(path_copy, "/");
		//check the name is not too long
		if(strlen(directory) > (MAX_FILENAME+1)){
			success = ENAMETOOLONG;
			return success;
		}

		//check if the directory already exists
		int found = FALSE;
		int i;
		for(i=0;i<root.nDirectories;i++){
			struct cs1550_directory cur_dir = root.directories[i];
			if(strcmp(cur_dir.dname,directory) == 0){
				found = TRUE;
				break;
			}
		}

		if(found == TRUE){
			success = -EEXIST;
			return success;
		}
		else{
			//if not exist, we need to create a new one and update the bitmap
			//first we need to check whether there is an available block
			if(root.nDirectories == MAX_DIRS_IN_ROOT){
				//the root is full
				fprintf(stderr, "the root is full\n");
				success =  -ENOSPC;
				return success;
			}
			else{
				//there is a space for a block
				long free_blcok_idx = get_free_block(disk);
				if(free_blcok_idx == -1){
					success =  -ENOSPC;
					return success;
				}
				struct cs1550_directory_entry *entry = get_block(disk, free_blcok_idx);		//create a new struct to store
				entry -> nFiles = 0;		//initialize the number of files

				//update the root strcut
				struct cs1550_directory new_dir;
				strcpy(new_dir.dname, directory);		//assign the name
				new_dir.nStartBlock = free_blcok_idx;		//assign the start block
				root.directories[root.nDirectories] = new_dir;	//update subdirs array
				root.nDirectories += 1;		//update the number of subdirectory
				//updathe everything
				update_bitmap(disk,free_blcok_idx, 1);
				write_block(disk,0,&root);
				write_block(disk,free_blcok_idx,entry);
				success = 0;
			}
		}

		close_disk(disk);
		return success;
	}

	/*
	* Removes a directory.
	*/
	static int cs1550_rmdir(const char *path)
	{
		(void) path;
		return 0;
	}

	/*
	* Does the actual creation of a file. Mode and dev can be ignored.
	*
	*/
	static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
	{
		(void) mode;
		(void) dev;

		//implementation
		int res = 0;		//initialize the result to 0

		//decompose the path
		char directory[MAX_FILENAME + 1];	// + 1 is for nul
		char filename[MAX_FILENAME + 1];
		char extension[MAX_EXTENSION + 1];

		//initialize variables
		memset(extension, 0, MAX_EXTENSION + 1);
		memset(directory, 0, MAX_FILENAME + 1);
		memset(filename, 0, MAX_FILENAME + 1);
		sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

		//condition check
		//if the file name is too long
		if(strlen(filename) == 0){
			res = -ENAMETOOLONG;
			fprintf(stderr, "the filename is too long\n");
			return res;
		}

		//if the file has no extension
		if(strlen(extension) == 0){
			res =  -EPERM;
			return res;
		}

		//if the extension is too long
		if(strlen(extension) > MAX_EXTENSION || strlen(filename) > MAX_FILENAME || strlen(directory) > MAX_FILENAME){
			res = -ENAMETOOLONG;
			return res;
		}

		//if the file is trying to be created in the root directory
		if(strlen(directory) == 0){
			res = -EPERM;
			fprintf(stderr, "the file is trying to be created in the root dir\n");
			return res;
		}
		FILE *disk = open_disk();
		//Now we search the file and see whether it is exists
		//first read the root and locate the directory
		struct cs1550_root_directory root = read_root(disk);		//get the root
		int i,found = FALSE;
		for(i = 0; i < root.nDirectories; i++){
			//check whether the directory exists
			if(strcmp(root.directories[i].dname,directory) == 0){
				//we found the directory
				found = TRUE;
				break;
			}
		}

		if(!found){
			res = -ENOENT;
			return res;
		}
		else{
			//the directory exists, let's check the file
			struct cs1550_directory_entry *entry = get_block(disk,root.directories[i].nStartBlock);
			int dir_start = root.directories[i].nStartBlock;
			//loop through to look for the file
			found = FALSE;		//reset the found
			for(i = 0; i < entry -> nFiles; i ++){
				if(strcmp(entry->files[i].fname,filename) == 0){
					found = TRUE;
					break;
				}
			}
			//if we found the file already exits
			if(found){
				res = -EEXIST;
				return res;
			}


			//create a file structure to store the file
			long free_blcok_idx = get_free_block(disk);
			if(free_blcok_idx == -1 || entry->nFiles >= MAX_FILES_IN_DIR){
				res = -ENOSPC;
				return res;
			}

			struct cs1550_file_directory *file = get_block(disk,free_blcok_idx);
			//update the file
			file -> nStartBlock = free_blcok_idx;		//assing the index of start block
			strcpy(file -> fname,filename);		//assign the file name
			strcpy(file ->fext, extension);
			//file -> fsize = sizeof(struct cs1550_file_directory);
			file->fsize = 0;		//initialize the file size to 0
			//update the entry
			entry -> files[entry->nFiles] = *file;
			entry -> nFiles ++;

			//write everything back
			//update the bitmap
			update_bitmap(disk,free_blcok_idx,1);
			//update the directory
			write_block(disk,dir_start,entry);
			//update the file
			//write_block(disk,free_blcok_idx,file);

			close_disk(disk);
			res = 0;
		}


		return res;
	}

	/*
	* Deletes a file
	*/
	static int cs1550_unlink(const char *path)
	{
		(void) path;

		return 0;
	}

	/*
	* Read size bytes from file into buf starting from offset
	*
	*/
	static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
		{
			(void) buf;
			(void) offset;
			(void) fi;
			(void) path;

			//check to make sure path exists
			//check that size is > 0
			//check that offset is <= to the file size
			//read in data
			//set size and return, or error

			//size = 0;

			//implementation
			int res = 0;
			//decompose the path
			char directory[MAX_FILENAME + 1];	// + 1 is for nul
			char filename[MAX_FILENAME + 1];
			char extension[MAX_EXTENSION + 1];

			/*
			//if the size is not correct
			if(size < 0){
				fprintf(stderr, "the size is less than 0\n");
				res = -ENOENT;
				return res;
			}
			*/

			//initialize variables
			memset(extension, 0, MAX_EXTENSION + 1);
			memset(directory, 0, MAX_FILENAME + 1);
			memset(filename, 0, MAX_FILENAME + 1);
			sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

			if(strlen(directory) == 0){
				fprintf(stderr, "try to write under root\n");
				res = -ENOENT;
				return res;
			}

			if(strlen(filename) == 0){
				fprintf(stderr, "try to write under directory \n");
				res = -EISDIR;
				return res;
			}

			if(strlen(extension) == 0){
				fprintf(stderr, "no extension in the path\n");
				res = -ENOENT;
				return res;
			}

			//check the length of the file path
			if(strlen(extension) > MAX_EXTENSION || strlen(filename) > MAX_FILENAME || strlen(directory) > MAX_FILENAME){
				res = -ENAMETOOLONG;
				return res;
			}

			//we need to open the disk to update the file struct
			FILE *disk = open_disk();
			struct cs1550_root_directory root = read_root(disk);

			//we first search the directory
			int i,found = FALSE;
			struct cs1550_directory cur_dir;
			for(i = 0; i < root.nDirectories; i ++){
				cur_dir = root.directories[i];
				if(strcmp(cur_dir.dname,directory) == 0){
					//found the directory
					found = TRUE;
					break;
				}
			}

			if(!found){
				res = -ENOENT;
				return res;
			}

			//search for the file
			struct cs1550_directory_entry *entry = get_block(disk,cur_dir.nStartBlock);
			struct cs1550_file_directory file;

			found = FALSE;
			//int index = -1;
			for(i = 0; i< entry->nFiles; i++){
				file = entry -> files[i];
				if(strcmp(file.fname,filename) == 0){
					//we found the file
					found = TRUE;
					//index = i;
					break;
				}
			}

			if(!found){
				//the file doesn't exist
				res = -ENOENT;
				return res;
			}

			//check the extension
			if(strcmp(file.fext,extension) != 0){
				fprintf(stderr, "the extension is wrong \n");
				res = -ENOENT;
				return res;
			}

			//check the file size
			if(offset > file.fsize){
				//file is not that large
				res = -EFBIG;
				return res;
			}

			//set the available size we could read
			long available = file.fsize - offset;
			if(size > available){
				size = available;
			}


			//calculate how many blocks we need to skip
			long start_block = file.nStartBlock;
			start_block = start_block + offset / BLOCK_SIZE;		//which block do we start

			/*
			if(offset % BLOCK_SIZE > 0 && offset > BLOCK_SIZE){
				start_block++;
			}*/
			long start_byte = offset % BLOCK_SIZE;			//the first block read
			long distance = BLOCK_SIZE - start_byte;
			//calculate how many blocks we need to read
			size_t left = size;
			long blocks = 0;
			if(size > distance){
				left = left - distance;
				blocks = left / BLOCK_SIZE;
				if(left % BLOCK_SIZE > 0){
					blocks ++;
				}
			}

			//read the first block
			struct cs1550_disk_block *first = get_block(disk,start_block);
			long first_try = distance;
			if(size < distance){
				first_try = size;
			}
			char temp[first_try];
			memcpy(temp,&first->data[start_byte],first_try);
			strcat(buf,temp);
			start_block++;		//move to the 2nd block
			//now we read
			int k;
			for(k=0; k<blocks; k++){
				size_t s = BLOCK_SIZE;
				if(left < BLOCK_SIZE){
					s = left;
				}
				char str[s];
				start_block = start_block + k;
				struct cs1550_disk_block *blk = get_block(disk,start_block);
				memcpy(str,blk->data,s);
			  strcat(buf,str);
				left = left - s;
			}

			close_disk(disk);

			return size;
		}

		/*
		* Write size bytes from buf into file starting from offset
		*
		*/
		static int cs1550_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
			{
				(void) buf;
				(void) offset;
				(void) fi;
				(void) path;

				//check to make sure path exists
				//check that size is > 0
				//check that offset is <= to the file size
				//write data
				//set size (should be same as input) and return, or error

				//implementation
				int res = 0;
				//decompose the path
				char directory[MAX_FILENAME + 1];	// + 1 is for nul
				char filename[MAX_FILENAME + 1];
				char extension[MAX_EXTENSION + 1];

				/*
				//if the size is not correct
				if(size < 0){
					fprintf(stderr, "the size is less than 0\n");
					res = -ENOENT;
					return res;
				}
				*/

				//initialize variables
				memset(extension, 0, MAX_EXTENSION + 1);
				memset(directory, 0, MAX_FILENAME + 1);
				memset(filename, 0, MAX_FILENAME + 1);
				sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

				if(strlen(directory) == 0){
					fprintf(stderr, "try to write under root\n");
					res = -ENOENT;
					return res;
				}

				if(strlen(filename) == 0){
					fprintf(stderr, "try to write under directory \n");
					res = -EISDIR;
					return res;
				}

				if(strlen(extension) == 0){
					fprintf(stderr, "no extension in the path\n");
					res = -ENOENT;
					return res;
				}

				//check the length of the file path
				if(strlen(extension) > MAX_EXTENSION || strlen(filename) > MAX_FILENAME || strlen(directory) > MAX_FILENAME){
					res = -ENAMETOOLONG;
					return res;
				}

				//we need to open the disk to update the file struct
				FILE *disk = open_disk();
				struct cs1550_root_directory root = read_root(disk);

				//we first search the directory
				int i,found = FALSE;
				struct cs1550_directory cur_dir;
				for(i = 0; i < root.nDirectories; i ++){
					cur_dir = root.directories[i];
					if(strcmp(cur_dir.dname,directory) == 0){
						//found the directory
						found = TRUE;
						break;
					}
				}

				if(!found){
					res = -ENOENT;
					return res;
				}

				//search for the file
				struct cs1550_directory_entry *entry = get_block(disk,cur_dir.nStartBlock);
				struct cs1550_file_directory file;

				found = FALSE;
				int index = -1;
				for(i = 0; i< entry->nFiles; i++){
					file = entry -> files[i];
					if(strcmp(file.fname,filename) == 0){
						//we found the file
						found = TRUE;
						index = i;
						break;
					}
				}

				if(!found){
					//the file doesn't exist
					res = -ENOENT;
					return res;
				}

				//check the extension
				if(strcmp(file.fext,extension) != 0){
					fprintf(stderr, "the extension is wrong \n");
					res = -ENOENT;
					return res;
				}

				//check the file size
				if(offset > file.fsize){
					//file is not that large
					res = -EFBIG;
				}

				//ready to write the file
				long start = file.nStartBlock;		//start block index for the file
				start = start + offset / BLOCK_SIZE;
				long start_byte = offset % BLOCK_SIZE;

				//ready to write in the first block
				long distance = BLOCK_SIZE - start_byte;
				long left = size;
				long blocks = 0;
				if(size > distance){
					left = size - distance;
					blocks = left / BLOCK_SIZE;
					if(left % BLOCK_SIZE > 0){
						blocks ++;
					}
				}

				//write the first block
				struct cs1550_disk_block *first = get_block(disk,start);
				long first_try = distance;
				if(size < distance){
					first_try = size;
				}
				char temp[first_try];
				memcpy(temp,buf,first_try);
				strcat(first->data,temp);
				fseek(disk,start*BLOCK_SIZE, SEEK_SET);
				fwrite(first,BLOCK_SIZE,1,disk);
				file.fsize += first_try;		//update the file size

				long last = start;		//record the previous block
				long last_byte = first_try;
				//now we write
				int k;
				struct cs1550_disk_block *free_block;
				for(k = 0; k < blocks; k++){
					//everytime we write a whole block size
					long block_idx = get_free_block(disk);

					if(block_idx - last > 2){
						res = -ENOSPC;
						break;
					}

					//check the return block
					if(block_idx == -1){
						//no more space
						res = -ENOSPC;
						break;
					}

					if(file.fsize > BITMAP_HEAD){
						//fprintf(stderr, "overhead!!! file size is %d\n", file.fsize);
						res = -ENOSPC;
						break;
					}

					free_block = get_block(disk,block_idx);		//get the new free block
					long s = BLOCK_SIZE;
					//check whether we need a whole blcok
					if(left < BLOCK_SIZE){
						s = left;
					}

					strncpy(free_block->data,&buf[last_byte],s);		//copy the data into the struct
					fseek(disk,block_idx * BLOCK_SIZE, SEEK_SET);		//get to this block

					//fprintf(stderr, "now we write the block %d\n", block_idx);
					fwrite(free_block,BLOCK_SIZE,1,disk);		//write the struct

					//fprintf(stderr, "now we update the bitmap\n");
					update_bitmap(disk,block_idx,1);
					left = left - s;
					last = block_idx;
					file.fsize += s;
					last_byte += s;
					//fprintf(stderr, "file size increase %d\n", s);
					//fprintf(stderr, "file size is %d\n", file.fsize);
				}
				//file.fsize += size;
				entry->files[index] = file;

				//update the direcotry
				write_block(disk,cur_dir.nStartBlock,entry);

				close_disk(disk);
				if(offset > file.fsize){
					//file is not that large
					res = -EFBIG;
					return res;
				}
				return size;
			}


			/******************************************************************************
			*
			*  DO NOT MODIFY ANYTHING BELOW THIS LINE
			*
			*****************************************************************************/

			/*
			* truncate is called when a new file is created (with a 0 size) or when an
			* existing file is made shorter. We're not handling deleting files or
			* truncating existing ones, so all we need to do here is to initialize
			* the appropriate directory entry.
			*
			*/
			static int cs1550_truncate(const char *path, off_t size)
			{
				(void) path;
				(void) size;

				return 0;
			}


			/*
			* Called when we open a file
			*
			*/
			static int cs1550_open(const char *path, struct fuse_file_info *fi)
			{
				(void) path;
				(void) fi;
				/*
				//if we can't find the desired file, return an error
				return -ENOENT;
				*/

				//It's not really necessary for this project to anything in open

				/* We're not going to worry about permissions for this project, but
				if we were and we don't have them to the file we should return an error

				return -EACCES;
				*/

				return 0; //success!
			}

			/*
			* Called when close is called on a file descriptor, but because it might
			* have been dup'ed, this isn't a guarantee we won't ever need the file
			* again. For us, return success simply to avoid the unimplemented error
			* in the debug log.
			*/
			static int cs1550_flush (const char *path , struct fuse_file_info *fi)
			{
				(void) path;
				(void) fi;

				return 0; //success!
			}

			//register our new functions as the implementations of the syscalls
			static struct fuse_operations hello_oper = {
				.getattr	= cs1550_getattr,
				.readdir	= cs1550_readdir,
				.mkdir	= cs1550_mkdir,
				.rmdir = cs1550_rmdir,
				.read	= cs1550_read,
				.write	= cs1550_write,
				.mknod	= cs1550_mknod,
				.unlink = cs1550_unlink,
				.truncate = cs1550_truncate,
				.flush = cs1550_flush,
				.open	= cs1550_open,
			};

			//Don't change this.
			int main(int argc, char *argv[])
			{
				return fuse_main(argc, argv, &hello_oper, NULL);
			}
