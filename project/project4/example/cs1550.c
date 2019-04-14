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
#define DISK_SIZE 5242880

//number of block (except bitmap)
#define BLOCK_COUNT (DISK_SIZE / BLOCK_SIZE - ((DISK_SIZE - 1) / (8 * BLOCK_SIZE * BLOCK_SIZE) + 1))
//#define BLOCK_COUNT 2044

//beginning of the bitmap
#define BITMAP_HEAD  (BLOCK_COUNT * BLOCK_SIZE - 1)

//bitmap size
#define BITMAP_SIZE  (3*BLOCK_SIZE - 3)	//-3 is for storing the bitmap

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
// Most recently allocated block index
static long last_block = 0;

//operation for disk --- function prototypes
static FILE* open_disk(void);	//open the disk
static void close_disk(FILE *f);	//close the disk
static int update_bitmap(FILE *f, long block_idx, char val);	//update the bitmap
static cs1550_root_directory read_root(FILE *disk);	//return the first block of .disk
static void write_root(FILE *disk, cs1550_root_directory* root);	//update the root block
static void* get_block(FILE *f, long block_idx);		//return the whole block
static void write_block(FILE *disk, int block_idx, void* block);		//update the block
static int get_free_block(FILE *f);			//return the index to the free blcok
static Bitmap read_bitmap(FILE *disk);		//read the bitmap and return


//implementations of function prototypes

//open the .disk
static FILE* open_disk(void){
	//create a file pointer to manage the disk
	FILE *file;
	file  = fopen(".disk", "r+b");
	// Check if disk file is able to be open
	if (file == NULL) {
		fprintf(stderr, "Error opening file\n");
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

static int get_free_block(FILE *f){
	int free_blcok_idx = -1;
	struct Bitmap bitmap;

	//char *bitmap = malloc(BITMAP_SIZE);
	bitmap = read_bitmap(f);	//read the bitmap
	fprintf(stderr, "finish reading bitmap!!!!\n");

	//loop through the bitmap and see whether we have free block
	int i;
	int found = FALSE;
	//we loop from 1 because the first block is saved for the root
	for(i=1; i < BITMAP_SIZE;i++){
		if(bitmap.bits[i] == 0){
			//we have a free block
			found = TRUE;
			fprintf(stderr, "the free block index is %d\n", i);
			break;
		}
	}

	if(!found){
		free_blcok_idx = -1;
		fprintf(stderr, "no free block now\n");
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
    fprintf(stderr, "requested block %ld does not exist\n", block_idx);
    return -1;
  }

	int success = 0;
	struct Bitmap bitmap;
	bitmap = read_bitmap(disk);	//read the bitmap
	if(bitmap.bits[1] == 0){
		fprintf(stderr, "very good!!\n");
	}
	fprintf(stderr, "ready to update the bitmap\n");
	//strcpy(bitmap.bits[block_idx],val);
	bitmap.bits[block_idx] = val;
	fprintf(stderr, "fseek now\n");
	fseek(disk, BITMAP_HEAD, SEEK_SET);
	fprintf(stderr, "ready to write the bitmap\n");
	fwrite(&bitmap,sizeof(char),BITMAP_SIZE,disk);

	return success;

}

static Bitmap read_bitmap(FILE *disk){
	//create a bitmap
	struct Bitmap bitmap;
	//get to the head of the bitmap
	fseek(disk,BITMAP_HEAD,SEEK_SET);
	//read the Bitmap
	fread(&bitmap,sizeof(char),BITMAP_SIZE,disk);
	return bitmap;
}

static void write_root(FILE *disk, cs1550_root_directory* root){
	fwrite(root, BLOCK_SIZE, 1, disk);
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

/*
 * Find the index of the next free block.
 * Return -1 when all blocks are used and -2 on error.
 */
static long next_free_block(FILE *f)
{
  long i, curr_byte_loc = -1;
  char curr_byte;
  for (i = 0; i < BLOCK_COUNT; ++i) {
    last_block = (last_block + 1) % BLOCK_COUNT;
    // Ignore root directory block
    if (!last_block) continue;
    long byte_loc = DISK_SIZE - (last_block / 8) - 1;
    if (byte_loc != curr_byte_loc) {
      if (fseek(f, byte_loc, SEEK_SET) ||
          fread(&curr_byte, 1, 1, f) != 1) return -2;
      curr_byte_loc = byte_loc;
    }
    int bit_loc = last_block % 8;
    char mask = 1 << bit_loc;
    if (!(curr_byte & mask)) {
      return last_block;
    }
  }
  return -1;
}

/*
 * Request a number of free blocks and return their indices.
 * Return NULL when all blocks are used or on error.
 */
static long* request_free_blocks(FILE *f, size_t num)
{
  long *block_indices = malloc(sizeof(long) * num);
  *block_indices = 0;
  long saved_last_block = last_block;
  int i;
  for (i = 0; i < num; ++i) {
    long block_idx = next_free_block(f);
    if (block_idx < 0 || block_idx == *block_indices) {
      free(block_indices);
      last_block = saved_last_block;
      return NULL;
    }
    *(block_indices + i) = block_idx;
  }
  return block_indices;
}

/*
 * Set a bit in the tracking bitmap. 1 indicates that the block is being used,
 * and 0 indicates that the block is free. Return -1 on error.
 */
static int set_bitmap(FILE *f, long block_idx, char val)
{
  if (block_idx >= BLOCK_COUNT) {
    fprintf(stderr, "requested block %ld does not exist\n", block_idx);
    return -1;
  }
  long byte_loc = DISK_SIZE - (block_idx / 8) - 1;
  char byte;
  int bit_loc = block_idx % 8;
  if (fseek(f, byte_loc, SEEK_SET) ||
      fread(&byte, 1, 1, f) != 1) return -1;
  char mask = 1 << bit_loc;
  if (val) {
    byte |= mask;
  } else {
    byte &= (~mask);
  }
  if (fseek(f, byte_loc, SEEK_SET) ||
      fwrite(&byte, 1, 1, f) != 1) return -1;
  return 0;
}

/*
 * Load a block at some certain block index from disk.
 * This function does not assume the type of the block.
 */
static void* load_block(FILE *f, long block_idx)
{
  if (block_idx >= BLOCK_COUNT) {
    fprintf(stderr, "requested block %ld does not exist\n", block_idx);
    return NULL;
  }
  if (fseek(f, block_idx * BLOCK_SIZE, SEEK_SET)) {
    fprintf(stderr, "failed to seek to block %ld\n", block_idx);
    return NULL;
  }
  void *block = malloc(BLOCK_SIZE);
  if (fread(block, BLOCK_SIZE, 1, f) != 1) {
    fprintf(stderr, "failed to load block %ld\n", block_idx);
    free(block);
    return NULL;
  }
  return block;
}

/*
 * Save a block at some certain block index to disk.
 * This function does not assume the type of the block.
 * Return -1 on error.
 */
static int save_block(FILE *f, long block_idx, void *block)
{
  if (block_idx >= BLOCK_COUNT) {
    fprintf(stderr, "requested block %ld does not exist\n", block_idx);
    return -1;
  }
  if (fseek(f, block_idx * BLOCK_SIZE, SEEK_SET)) {
    fprintf(stderr, "failed to seek to block %ld\n", block_idx);
    return -1;
  }
  if (fwrite(block, BLOCK_SIZE, 1, f) != 1) {
    fprintf(stderr, "failed to write to block %ld\n", block_idx);
    return -1;
  }
  return 0;
}

/*
 * Load the root directory block from disk.
 */
static cs1550_root_directory* load_root_directory(FILE *f)
{
  return (cs1550_root_directory*) load_block(f, 0);
}

/*
 * Load a subdirectory block at some certain block index from disk.
 */
static cs1550_directory_entry* load_subdirectory(FILE *f, long block_idx)
{
  return (cs1550_directory_entry*) load_block(f, block_idx);
}

/*
 * Load a file block at some certain block index from disk.
 */
static cs1550_disk_block* load_file_block(FILE *f, long block_idx)
{
  return (cs1550_disk_block*) load_block(f, block_idx);
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
							stbuf->st_size = 0; //file size - make sure you replace with real size!
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

		//Parse the path, which is in the form: root/destination/filename.extension
		int path_length = strlen(path);
		char path_copy[path_length];
		strcpy(path_copy, path);

		char* destination = strtok(path_copy, "/");
		char* filename = strtok(NULL, ".");
		char* extension = strtok(NULL, ".");
		FILE *disk = open_disk();
		//Each of these iff statements first checks if the string is null; if not, check the length.
		//If the length is longer than the 8.3 file naming convention we're using, return the error ENAMETOOLONG.
		if((destination && destination[0]) && strlen(destination) > MAX_FILENAME){
			return -ENAMETOOLONG;
		}
		if((filename && filename[0]) && strlen(filename) > MAX_FILENAME){
			return -ENAMETOOLONG;
		}
		if((extension && extension[0]) && strlen(extension) > MAX_EXTENSION){
			return -ENAMETOOLONG;
		}

		//Enter the root
		if(strcmp(path, "/") == 0){
			int i = 0;

			cs1550_root_directory root = read_root(disk);

			for(i = 0; i < MAX_DIRS_IN_ROOT; i++){ //Iterate over all of the directories in the root; if their name is non-empty, print for the user using filler()
				char* directory_name = root.directories[i].dname;
				if(strcmp(directory_name, "") != 0){
					filler(buf, directory_name, NULL, 0);
				}
			}

			return 0;
		} else{
			int i = 0;
			struct cs1550_directory dir; //Initialize a directory for file checking later on
			strcpy(dir.dname, "");
			dir.nStartBlock = -1;

			cs1550_root_directory root = read_root(disk);

			for(i = 0; i < MAX_DIRS_IN_ROOT; i++){ //Iterate over the directories in the root until we find the directory with a matching name with the path
				if(strcmp(destination, root.directories[i].dname) == 0){
					dir = root.directories[i];
					break;
				}
			}

			if(strcmp(dir.dname, "") == 0){ //No directory was found in the root, so return file not found error
				return -ENOENT;
			} else{ //The proper directory was found, so read the directory
				FILE* disk = fopen(".disk", "rb+");
				int location_on_disk = dir.nStartBlock*BLOCK_SIZE;
				fseek(disk, location_on_disk, SEEK_SET);

				cs1550_directory_entry directory;
				directory.nFiles = 0;
				memset(directory.files, 0, MAX_FILES_IN_DIR*sizeof(struct cs1550_file_directory));

				fread(&directory, BLOCK_SIZE, 1, disk); //Read the directory data from memory to iterate over its files
				fclose(disk);

				int j = 0;
				for(j = 0; j < MAX_FILES_IN_DIR; j++){ //Iterate over the non-empty filenames in this directory and print them to the user using filler()
					struct cs1550_file_directory file_dir = directory.files[j];
					char filename_copy[MAX_FILENAME+1];
					strcpy(filename_copy, file_dir.fname);
					if(strcmp(file_dir.fext, "") != 0){ //Append the file extension
						strcat(filename_copy, ".");
					}
					strcat(filename_copy, file_dir.fext); //Append file extension
					if(strcmp(file_dir.fname, "") != 0){ //If the file is not empty, add it to the filler buffer
						filler(buf, filename_copy, NULL, 0);
					}
				}
			}
		}
		close_disk(disk);
		/*
		//add the user stuff (subdirs or files)
		//the +1 skips the leading '/' on the filenames
		filler(buf, newpath + 1, NULL, 0);
		*/
		return 0;
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
		if(strlen(directory) >= MAX_FILENAME){
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
			fprintf(stderr, "%s exits, no need to create a new one\n", directory);
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
				fprintf(stderr, "purely test!!!!!!!!\n");
				//there is a space for a block
				int free_blcok_idx = get_free_block(disk);
				fprintf(stderr, "I got the free block!!!!!\n");
				if(free_blcok_idx == -1){
					fprintf(stderr, "no available free block\n");
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
				fprintf(stderr, "ready to update everything\n");
				//updathe everything
				update_bitmap(disk,free_blcok_idx, '1');
				fprintf(stderr, "ready to update the root\n");
				write_block(disk,0,&root);
				fprintf(stderr, "ready to update the directory block\n");
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
		return 0;
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

			size = 0;

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
