/*
 * © 2017 henols@gmail.com
 *
 * delta_tool.c
 *
 *  Created on: 6 apr. 2017
 *      Author: Henrik
 */


#include "bzip2/bzlib.h"
#include <cygwin/stat.h>
#include <dirent.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_default_fcntl.h>
#include <sys/_stdint.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "archive.h"
#include "argparse.h"
#include "config.h"
#include "delta/bsdiff.h"
#include "delta/bspatch.h"
#include "delta/md5.h"

#define TT_DELTA_PATCH 10
#define TT_DELTA_UNCHANGED 11
#define TT_DELTA_ADD 12
#define TT_DELTA_DELETE 13

#define COMPRESS
int verbose = 0;

struct file_node {
	char name[256 + 1];
	struct file_node *next;
};

void combine(char* destination, const char* path1, const char* path2);
int get_md5_from_file(char* file_md5, const char* source);
int check_file_by_md5(const char* source, char* file_md5);

static int create_patch(char* source, char* target, FILE * pf);
static int apply_patch(char* source, char* target, char* delta);
static int copy_file(char* target, FILE * pf);

static int bz2_write(struct bsdiff_stream* stream, const void* buffer, int size) {
	int bz2err;
	BZFILE* bz2;

	bz2 = (BZFILE*) stream->opaque;
	BZ2_bzWrite(&bz2err, bz2, (void*) buffer, size);
	if (bz2err != BZ_STREAM_END && bz2err != BZ_OK)
		return -1;

	return 0;
}

static int bz2_read(const struct bspatch_stream* stream, void* buffer, int length) {
	int n;
	int bz2err;
	BZFILE* bz2;

	bz2 = (BZFILE*) stream->opaque;
	n = BZ2_bzRead(&bz2err, bz2, buffer, length);
	if (n != length)
		return -1;

	return 0;
}

struct file_node * detach_node(struct file_node ** list, char *name);

struct file_node * file_list(DIR *d) {
	struct dirent *dir;
	struct file_node * first_node = NULL;
	struct file_node * current_node = NULL;
	if (d) {
		struct stat file_info;
		while ((dir = readdir(d)) != NULL) {
			lstat(dir->d_name, &file_info);
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && (S_ISREG(file_info.st_mode) || dir->d_type == DT_REG)) {
				if (first_node == NULL) {
					first_node = malloc(sizeof(struct file_node));

					current_node = first_node;
				} else {
					current_node->next = malloc(sizeof(struct file_node));
					current_node = current_node->next;
				}
				current_node->next = NULL;
				strcpy(current_node->name, dir->d_name);
			} else {
				printf("Ignoring: '%s'\n", dir->d_name);
			}
		}
	}
	return first_node;
}

struct file_node * detach_node(struct file_node ** list, char *name) {
	struct file_node *current;
	struct file_node *parrent = NULL;
	current = *list;
	while (current != NULL && strcmp(current->name, name) != 0) {
		parrent = current;
		current = current->next;
	}
	if (current) {
		if (parrent) {
			parrent->next = current->next;
		} else {
			*list = current->next;
		}
		current->next = NULL;
		return current;
	}
	return NULL;
}

static const char * const usage[] = { "delta_tool -a|-c [options]  source_path target_path delta",
//    "test_argparse [options]",
		NULL, };

int apply_delta_package(const char* source_path, const char* target_path, const char* delta_name) {

}
int build_delta_package(const char* source_path, const char* target_path, const char* delta_name) {
	//	int len = strlen(argv[0]);
	//	if (argv[0][len - 1]) {
	//
	//	}

	int rc = 0;

	DIR* d = opendir(source_path);
	struct file_node* source_list;
	struct file_node* target_list;
	if (d) {
		source_list = file_list(d);
		closedir(d);
	} else {
		printf("Can't find source path: '%s'.\n", source_path);
		rc = -1;
		goto exit;
	}
	d = opendir(target_path);
	if (d) {
		target_list = file_list(d);
		closedir(d);
	} else {
		printf("Can't find target path: '%s'.\n", target_path);
		rc = -1;
		goto exit;
	}
	struct file_node* current = source_list;
	struct file_node* delta_list = NULL;
	struct file_node* last_delta = NULL;
	while (current) {
		struct file_node* tmp = detach_node(&target_list, current->name);
		if (tmp) {
			tmp = current->next;
			if (delta_list == NULL) {
				delta_list = detach_node(&source_list, current->name);
				last_delta = delta_list;
			} else {
				last_delta->next = detach_node(&source_list, current->name);
				last_delta = last_delta->next;
			}
			current = tmp;
		} else {
			current = current->next;
		}
	}
	printf("Deltas:\n");
	current = delta_list;
	FILE* pf_tar;
	/* Create the patch file */
	if ((pf_tar = fopen(delta_name, "w+")) == NULL) {
		err(1, " -  %s", delta_name);
	}
	archive_t arc;
	arc_create(&arc, pf_tar);
	while (current) {
		//		size_t tmp_len = strlen(source_path) + strlen(current->name) +1;
		//		char * path = malloc(tmp_len);
		char s_path[213];
		char t_path[213];
		char target_md5[32];
		//		char  		o_path[213];
		//		memset((void*)path, 0, tmp_len);
		combine(s_path, source_path, current->name);
		combine(t_path, target_path, current->name);
		//		combine(o_path, "./cccc/", current->name);
		printf("Delta, node: src:'%s' trg:'%s'\n", s_path, t_path);
		if (arc_open_entity(&arc, current->name) < 0) {
			printf("Failed to open archive entity:'%s'\n", current->name);
		}
		int type = create_patch(s_path, t_path, pf_tar) == 0 ? TT_DELTA_PATCH : TT_DELTA_UNCHANGED;
		get_md5_from_file(target_md5, t_path);
		arc_close_entity(&arc, target_md5, type);
		printf("----- \n\n");
		current = current->next;
		//		free(path);
	}
	printf("Deletion:\n");
	current = source_list;
	while (current) {
		printf("Delete, node: %s.\n", current->name);
		arc_open_entity(&arc, current->name);
		arc_close_entity(&arc, "", TT_DELTA_DELETE);
		current = current->next;
	}
	printf("Adding:\n");
	current = target_list;
	while (current) {
		char target_md5[32];
		printf("Add, node: %s.\n", current->name);
		char t_path[213];
		arc_open_entity(&arc, current->name);
		combine(t_path, target_path, current->name);
		copy_file(t_path, pf_tar);
		get_md5_from_file(target_md5, t_path);
		arc_close_entity(&arc, target_md5, TT_DELTA_ADD);
		current = current->next;
	}
	if (fclose(pf_tar))
		err(1, "fclose");

	exit:

	return rc;
}

int main(int argc, const char **argv) {
	char *apply = NULL;
	char *create = NULL;
	const char *source_path = NULL;
	const char *target_path = NULL;
	const char *delta_name = NULL;
	struct argparse_option options[] = {
	OPT_HELP(),
	OPT_GROUP("Basic options"),
//		OPT_STRING('s', "source", &source_path, "Source binaries"),
//		OPT_STRING('t', "target", &target_path, "Target binaries"),
			OPT_STRING('a', "apply", &apply, "Apply delta patch"),
			OPT_STRING('c', "create", &create, "Create delta patch"),
			OPT_BOOLEAN('o', "output", &create, "Output path to put delta"),
			OPT_BOOLEAN('v', "verbose", &verbose, "Verbose output"),
//        OPT_GROUP("Bits options"),
//        OPT_BIT(0, "read", &perms, "read perm", NULL, PERM_READ, OPT_NONEG),
//        OPT_BIT(0, "write", &perms, "write perm", NULL, PERM_WRITE),
//        OPT_BIT(0, "exec", &perms, "exec perm", NULL, PERM_EXEC),
			OPT_END(), };

	struct argparse argparse;
	argparse_init(&argparse, options, usage, 0);
	argparse_describe(&argparse, "\nA brief description of what the program does and how it works.", "\nAdditional description of the program after the description of the arguments.");
	argc = argparse_parse(&argparse, argc, argv);
	if (verbose != 0) {
		printf("verbose: %d\n", verbose);
	}

	if ((apply == NULL || strlen(apply) <= 1) && (create == NULL || strlen(create) <= 1)) {
		printf("One of create or apply options must be used.\n");
		argparse_usage(&argparse);
		exit(1);
	}
	if (strlen(apply) > 1 && strlen(create) > 1) {
		printf("Both create and apply options can't be used.\n");
		argparse_usage(&argparse);
		exit(1);
	}

	if (argc < 2) {
		printf("Missing one or more of source path, target path or delta.\n");
		argparse_usage(&argparse);
		exit(1);
	}

	if (argc > 2) {
		printf("Only source path, target path or delta name can be set.\n");
		argparse_usage(&argparse);
		exit(1);
	}

//	int len = strlen(argv[0]);
//	if (argv[0][len - 1]) {
//
//	}
	source_path = argv[0];
	target_path = argv[1];

	int rc;
	if (create) {
		rc = build_delta_package(source_path, target_path, create + 1);
	} else {
		rc = apply_delta_package(source_path, apply, target_path);
	}
	if (rc < 0) {
		argparse_usage(&argparse);
		exit(1);
	}
	return 0;
}

static int create_patch(char* source, char* target, FILE * pf) {
	int rc;
	int fd;
	int bz2err;
	uint8_t *old, *new;
	off_t oldsize, newsize;
	uint8_t buf[8];
//	FILE * pf;
	struct bsdiff_stream stream;
	BZFILE* bz2;

	memset(&bz2, 0, sizeof(bz2));
	stream.malloc = malloc;
	stream.free = free;
	stream.write = bz2_write;

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
	 that we never try to malloc(0) and get a NULL pointer */
	if (((fd = open(source, O_RDONLY, 0)) < 0) || ((oldsize = lseek(fd, 0, SEEK_END)) == -1) || ((old = malloc(oldsize + 1)) == NULL) || (lseek(fd, 0, SEEK_SET) != 0) || (read(fd, old, oldsize) != oldsize) || (close(fd) == -1)) {
		err(1, " -s- %s", source);
	}

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
	 that we never try to malloc(0) and get a NULL pointer */
	if (((fd = open(target, O_RDONLY, 0)) < 0) || ((newsize = lseek(fd, 0, SEEK_END)) == -1) || ((new = malloc(newsize + 1)) == NULL) || (lseek(fd, 0, SEEK_SET) != 0) || (read(fd, new, newsize) != newsize) || (close(fd) == -1)) {
		err(1, " -t- %s", target);
	}

	if (oldsize == newsize) {
		if (memcmp(new, old, newsize) == 0) {
			printf("Identical...\n");
			rc = 0;
			goto exit;
		}
	}

	/* Create the patch file */
//	if ((pf = fopen(delta, "w")) == NULL)
//		err(1, " delta: %s", delta);
	/* Write header (signature+newsize)*/
	offtout(newsize, buf);

	int magic_len = strlen(HEADER_MACIC);

	if (fwrite(HEADER_MACIC, magic_len, 1, pf) != 1 || fwrite(buf, sizeof(buf), 1, pf) != 1) {
		err(1, "Failed to write header");
	}

//	goto exit;

	if (NULL == (bz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 2, 0))) {
		errx(1, "BZ2_bzWriteOpen, bz2err=%d", bz2err);
	}

	stream.opaque = bz2;
	if (bsdiff(old, oldsize, new, newsize, &stream)) {
		err(1, "bsdiff");
	}

	unsigned int nbytes_in;
	unsigned int nbytes_out;

	BZ2_bzWriteClose(&bz2err, bz2, 0, &nbytes_in, &nbytes_out);

	if (bz2err != BZ_OK) {
		err(1, "BZ2_bzWriteClose, bz2err=%d", bz2err);
	} else {
		float p = ((float) nbytes_out / (float) nbytes_in) * 100.0;
		printf("Written, in: %d, out: %d compression %.2f%%\n", (int) nbytes_in, (int) nbytes_out, p);
		rc = nbytes_out;
	}

	exit:

	/* Free the memory we used */
	free(old);
	free(new);

	return rc;
}

static int copy_file(char* target, FILE * pf) {
	int fd;
	uint8_t *new;
	off_t newsize;
//

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
	 that we never try to malloc(0) and get a NULL pointer */
	if (((fd = open(target, O_RDONLY, 0)) < 0) || ((newsize = lseek(fd, 0, SEEK_END)) == -1) || ((new = malloc(newsize + 1)) == NULL) || (lseek(fd, 0, SEEK_SET) != 0) || (read(fd, new, newsize) != newsize) || (close(fd) == -1)) {
		err(1, " -t- %s", target);
	}

	fwrite(new, 1, newsize, pf);

	exit:

	/* Free the memory we used */
	free(new);

	return 0;
}

static int apply_patch(char* source, char* target, char* delta) {
	FILE * f;
	int fd;
	int bz2err;
	uint8_t *old, *new;
	int64_t oldsize, newsize;
	BZFILE* bz2;
	struct bspatch_stream stream;
	struct stat sb;

	int magic_len = strlen(HEADER_MACIC);
	int header_len = magic_len + 8;
	uint8_t header[header_len];

	/* Open patch file */
	if ((f = fopen(delta, "r")) == NULL)
		err(1, "fopen(%s)", delta);

	/* Read header */
	if (fread(header, 1, header_len, f) != header_len) {
		if (feof(f))
			errx(1, "Corrupt patch\n");
		err(1, "fread(%s)", delta);
	}

	/* Check for appropriate magic */
	if (memcmp(header, HEADER_MACIC, magic_len) != 0)
		errx(1, "Corrupt patch\n");

	/* Read lengths from header */
	newsize = offtin(header + magic_len);
	if (newsize < 0)
		errx(1, "Corrupt patch\n");

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (((fd = open(source, O_RDONLY, 0)) < 0) || ((oldsize = lseek(fd, 0, SEEK_END)) == -1) || ((old = malloc(oldsize + 1)) == NULL) || (lseek(fd, 0, SEEK_SET) != 0) || (read(fd, old, oldsize) != oldsize) || (fstat(fd, &sb)) || (close(fd) == -1))
		err(1, "source: %s", source);
	if ((new = malloc(newsize + 1)) == NULL)
		err(1, "Malloc? newsize: %d", newsize);

	if (NULL == (bz2 = BZ2_bzReadOpen(&bz2err, f, 0, 0, NULL, 0)))
		errx(1, "BZ2_bzReadOpen, bz2err=%d", bz2err);

	stream.read = bz2_read;
	stream.opaque = bz2;

	int rc = bspatch(old, oldsize, new, newsize, &stream);
	if (rc) {
		errx(1, "bspatch: %d, old size: %d, new size: %d", rc, oldsize, newsize);
	}
	/* Clean up the bzip2 reads */
	BZ2_bzReadClose(&bz2err, bz2);
	fclose(f);

	/* Write the new file */
	if (((fd = open(target, O_CREAT | O_TRUNC | O_WRONLY, sb.st_mode)) < 0) || (write(fd, new, newsize) != newsize) || (close(fd) == -1))
		err(1, "target: %s", target);

	free(new);
	free(old);

	return 0;
}

void combine(char* destination, const char* path1, const char* path2) {
	if (path1 == NULL && path2 == NULL) {
		strcpy(destination, "");
		;
	} else if (path2 == NULL || strlen(path2) == 0) {
		strcpy(destination, path1);
	} else if (path1 == NULL || strlen(path1) == 0) {
		strcpy(destination, path2);
	} else {
		char directory_separator[] = "/";
//#ifdef WIN32
//		directory_separator[0] = '\\';
//#endif
		const char* last_char = path1;
		while (*last_char != '\0') {
			last_char++;
		}
		int append_directory_separator = 0;
//		printf(" combine '%c' '%c'\n", *(last_char-1) , *directory_separator);

		if (*(last_char - 1) != *directory_separator) {
			append_directory_separator = 1;
		}
		strcpy(destination, path1);
		if (append_directory_separator)
			strcat(destination, directory_separator);
		strcat(destination, path2);
	}
}

int get_md5_from_file(char* file_md5, const char* source) {
	int rc = 0;
	int fd;
	size_t file_len = 0;
	char decrypt[16];
	char buffer[1024] = { 0 };

	fd = open(source, O_RDONLY, 0);
	if (fd > 0) {
		file_len = lseek(fd, 0, SEEK_END);
		if (file_len >= 0) {
			lseek(fd, 0, SEEK_SET);
			size_t read_len = 0;
			int i;
			MD5_CTX md5;
			MD5Init(&md5);
			while ((read_len = read(fd, buffer, 1024)) > 0) {
				MD5Update(&md5, (unsigned char*) buffer, read_len);
			}
			MD5Final(&md5, (unsigned char*) decrypt);
			memset(buffer, 0, 1024);
			for (i = 0; i < 16; i++) {
				sprintf((char*) (file_md5 + i * 2), "%02x", decrypt[i]);
			}
		}
		close(fd);
		rc = 1;
	}
	return rc;
}

int check_file_by_md5(const char* source, char* file_md5) {
	int rc = 0;
	char decrypt[32];

	if (get_md5_from_file(decrypt, source) > 0) {
		rc = strcasecmp((const char*) decrypt, (const char*) file_md5) == 0;
	}
	return rc;
}

