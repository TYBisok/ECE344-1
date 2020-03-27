#include "common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>


/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}


void make_file(char *src_file, char *dest_file) {

	// Open file descriptors
	int fd_src, fd_dest;
	
	fd_src = open(src_file, O_RDONLY, S_IRUSR);
	if (fd_src < 0)
		syserror(open, src_file);
	
	fd_dest = creat(dest_file, S_IRWXU);
	if (fd_dest < 0)
		syserror(creat, dest_file);


	// Begin reading src and writing onto dest
	char buf[4096];
	ssize_t read_ret, write_ret;
	int count = 4096;

	read_ret = read(fd_src, buf, count);
	if (read_ret < 0)
		syserror(read, src_file);
	
	while (read_ret !=0) {
		write_ret = write(fd_dest, buf, read_ret);
		if (write_ret < 0)
			syserror(write, dest_file);

		read_ret = read(fd_src, buf, count);
		if (read_ret < 0)
			syserror(read, src_file);
	}

	// Close File descriptors
	if (close(fd_src) < 0)
		syserror(read, src_file);
	if (close(fd_dest) < 0)
		syserror(read, dest_file);	
}



void copy_recursively(char *src, char *dst) {

	struct stat src_stat;
	if(stat(src, &src_stat) < 0)
		syserror(stat, src);	

	if(S_ISREG(src_stat.st_mode)){ // If it's a file
		make_file(src, dst);
		if(chmod(dst, src_stat.st_mode)) syserror(chmod, dst);
		return;
	}

	// Otherwise it's a directory
	char *srcdir = src;
	char *dstdir = dst;

	// Open source dir
	DIR *srcdir_ptr = opendir(srcdir);

	// make dst dir
	if(mkdir(dstdir, S_IRWXU)) syserror(mkdir, dstdir);

	struct dirent *src_entry = readdir(srcdir_ptr);

	char src_path[1000];
	char dest_path[1000];

	// Iterate through all entries in the dir
	while(src_entry != NULL) {

		// Skip these files
		if(strcmp(src_entry->d_name, ".") == 0 || 
			strcmp(src_entry->d_name,"..") == 0) {
			
			src_entry = readdir(srcdir_ptr);
			continue;
			
		}

		strcpy(dest_path, dstdir);
		strcat(dest_path, "/");
		strcat(dest_path, src_entry->d_name);


		strcpy(src_path, srcdir);
		strcat(src_path, "/");
		strcat(src_path, src_entry->d_name);
		

		copy_recursively(src_path, dest_path);

		src_entry = readdir(srcdir_ptr);
	}

	// Copy over the permissions
	if(chmod(dst, src_stat.st_mode)) syserror(chmod, dst);

	if(closedir(srcdir_ptr) < 0)
		syserror(closedir, srcdir);

	
	return;


}


int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	} 


	char *srcdir = argv[1];
	char *dstdir = argv[2];

	copy_recursively(srcdir, dstdir);
	return 0;
	
	
}
