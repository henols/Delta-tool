/*
 * © 2017 henols@gmail.com
 *
 * archive.h
 *
 *  Created on: 6 apr. 2017
 *      Author: Henrik
 */

#ifndef ARCHIVE_H_
#define ARCHIVE_H_

#include <stddef.h>
#include <stdio.h>
#include <sys/_stdint.h>

#define ARCHIVE_HEADER_MACIC "DeltaTool/archive"

struct arc_header {
	char name[64 + 1];
	uint8_t mode;
	char md5[32 + 1];
	uint size;
};

typedef struct archive
{
	FILE* pf;
	int pos;
	struct arc_header header;
} archive_t;

int arc_create(archive_t * arc, FILE* pf);

int arc_open_entity(archive_t * arc, const char* name) ;
int arc_close_entity(archive_t * arc, const char* md5, uint8_t mode) ;


#endif /* ARCHIVE_H_ */
