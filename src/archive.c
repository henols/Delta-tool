/*
 * © 2017 henols@gmail.com
 *
 * archive.c
 *
 *  Created on: 6 apr. 2017
 *      Author: Henrik
 */

#include "archive.h"

#include <stdio.h>
#include <string.h>

int arc_create(archive_t * arc, FILE* pf) {
	int rc = 0;
	arc->pf = pf;
	arc->pos = -1;
	int magic_len = strlen(ARCHIVE_HEADER_MACIC);

	// write MAGIC
	if (fwrite(ARCHIVE_HEADER_MACIC, magic_len, 1, pf) != 1) {
		rc = -1;
	}

	return 0;
}

void fexpand(FILE* f, size_t amount, int value) {
	while (amount--) {
		fputc(value, f);
	}
}

int arc_open_entity(archive_t * arc, const char* name) {
	int rc = 0;
	if (arc->pos >= 0) {
		rc = -1;
		goto error;
	}

	arc->pos = ftell(arc->pf);
	//Fill out a new header
	fexpand(arc->pf, sizeof(struct arc_header), 0);
	memset(&arc->header, 0, sizeof(struct arc_header));
	snprintf(arc->header.name, 64, "%s", name);

	printf("name: %s, pos: %d, head: %d\n", arc->header.name, arc->pos, ftell(arc->pf));
	error: //
	return rc;
}

int arc_close_entity(archive_t * arc, const char* md5, uint8_t mode) {
	size_t index;
	int rc;
	if (arc->pos < 0) {
		rc = -1;
		goto error;
	}
	index = ftell(arc->pf);

	snprintf(arc->header.md5, 32, "%s", md5);
	arc->header.mode = mode;
	arc->header.size = index - (arc->pos + sizeof(struct arc_header));
	rc = fseek(arc->pf, arc->pos, SEEK_SET);
	rc = fwrite(&arc->header, 1, sizeof(struct arc_header), arc->pf);

	rc = fseek(arc->pf, index, SEEK_SET);
	printf("name: %s, md5: %s, size: %d, type: %d\n", arc->header.name, arc->header.md5, arc->header.size, arc->header.mode);
	arc->pos = -1;
	error: //

	return rc;
}
