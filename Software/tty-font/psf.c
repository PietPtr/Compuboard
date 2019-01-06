/* psf.c
 *
 * a simple psf handling library.
 *
 * Gunnar Zötl <gz@tset.de> 2016
 * Released under the terms of the MIT license. See file LICENSE for details.
 *
 * info about the psf font file format(s) from
 * http://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psf.h"
#include "mini_utf8.h"

static int psf_reallocglyphs(struct psf_font *psf, unsigned int num);

struct psf_font *psf_new(unsigned int version, unsigned int width, unsigned int height)
{
	if (version != 1 && version != 2) {
		fprintf(stderr, "%s: invalid version\n", __func__);
		return 0;
	}
	if ((version == 1 && width != 8) || width == 0 || height == 0) {
		fprintf(stderr, "%s: invalid char size\n", __func__);
		return 0;
	}

	struct psf_font *psf = calloc(1, sizeof(struct psf_font));
	if (!psf) {
		perror(__func__);
		return 0;
	}

	psf->version = version;
	if (version == 1) {
		psf->header.psf1.charsize = height;
		psf->header.psf1.magic[0] = PSF1_MAGIC0;
		psf->header.psf1.magic[1] = PSF1_MAGIC1;
		psf_reallocglyphs(psf, 256);
	} else {
		psf->header.psf2.width = width;
		psf->header.psf2.height = height;
		psf->header.psf2.charsize = ((width + 7) / 8) * height;
		psf->header.psf2.magic[0] = PSF2_MAGIC0;
		psf->header.psf2.magic[1] = PSF2_MAGIC1;
		psf->header.psf2.magic[2] = PSF2_MAGIC2;
		psf->header.psf2.magic[3] = PSF2_MAGIC3;
		psf->header.psf2.version = 0;
		psf->header.psf2.headersize = sizeof(struct psf2_header);
	}
	return psf;
}

static int psf_read_byte(FILE *file, unsigned int *bval)
{
	int rd = fgetc(file);
	if (rd == EOF) {
		perror(__func__);
		return 0;
	}
	*bval = (unsigned int) rd & 0xff;
	return 1;
}

static int psf_write_byte(FILE *file, unsigned int bval)
{
	int wr = fputc(bval & 0xff, file);
	if (wr == EOF) {
		perror(__func__);
		return 0;
	}
	return 1;
}

static int psf_read_word(FILE *file, unsigned int *wval)
{
	unsigned int byte0, byte1;
	if (!psf_read_byte(file, &byte0) || !psf_read_byte(file, &byte1)) {
		return 0;
	}
	*wval = byte0 + (byte1 << 8);
	return 1;
}

static int psf_write_word(FILE *file, unsigned int wval)
{
	unsigned int byte0, byte1;
	byte0 = wval & 0xff;
	byte1 = (wval >> 8) & 0xff;
	return psf_write_byte(file, byte0) && psf_write_byte(file, byte1);
}

static int psf_read_int(FILE *file, unsigned int *ival)
{
	unsigned int byte0, byte1, byte2, byte3;
	if (!psf_read_byte(file, &byte0) || !psf_read_byte(file, &byte1) || !psf_read_byte(file, &byte2) || !psf_read_byte(file, &byte3)) {
		return 0;
	}
	*ival = byte0 + (byte1 << 8) + (byte2 << 16) + (byte3 << 24);
	return 1;
}

static int psf_write_int(FILE *file, unsigned int ival)
{
	unsigned int byte0, byte1, byte2, byte3;
	byte0 = ival & 0xff;
	byte1 = (ival >> 8) & 0xff;
	byte2 = (ival >> 16) & 0xff;
	byte3 = (ival >> 24) & 0xff;
	return psf_write_byte(file, byte0) && psf_write_byte(file, byte1) && psf_write_byte(file, byte2) && psf_write_byte(file, byte3);
}

static int psf_read_glyphs(FILE *file, struct psf_font *psf, unsigned int numglyphs, unsigned int glyphsize)
{
	unsigned int glyph;
	psf->glyph = calloc(numglyphs, sizeof(struct psf_glyph));
	if (!psf->glyph) { return 0; }
	for (glyph = 0; glyph < numglyphs; ++glyph) {
		psf->glyph[glyph].data = malloc(glyphsize);
		if (!psf->glyph[glyph].data) { return 0; } /* psf_delete will take care of cleanup */
		if (fread(psf->glyph[glyph].data, 1, glyphsize, file) != glyphsize) { return 0; }
	}
	return 1;
}

static int psf1_read_ucvals(FILE *file, struct psf_font *psf, unsigned int numglyphs)
{
	unsigned int i;
	for (i = 0; i < numglyphs; ++i) {
		struct psf_glyph *glyph = &psf->glyph[i];
		unsigned int ucval;
		while (1) {
			if (!psf_read_word(file, &ucval)) { return 0; }
			if (ucval == PSF1_SEPARATOR) { break; }
			psf_glyph_adducval(psf, glyph, ucval);
		}
	}
	return 1;
}

static struct psf_font *psf1_load_fromfile(FILE* file)
{
	unsigned char magic[2];
	magic[0] = PSF1_MAGIC0;
	if (fread(&magic[1], 1, 1, file) != 1) {
		perror(__func__);
		return 0;
	}
	if (magic[1] != PSF1_MAGIC1) {
		fprintf(stderr, "%s: invalid magic number", __func__);
		return 0;
	}
	unsigned int mode, height;
	if (!psf_read_byte(file, &mode)) { return 0; }
	if (!psf_read_byte(file, &height)) { return 0; }

	struct psf_font *psf = psf_new(1, 8, height);
	psf->header.psf1.mode = mode;

	int numglyphs = (mode & PSF1_MODE512) ? 512 : 256;
	if (!psf_read_glyphs(file, psf, numglyphs, height)) {
		psf_delete(psf);
		return 0;
	}

	if (!psf1_read_ucvals(file, psf, numglyphs)) {
		psf_delete(psf);
		return 0;
	}

	return psf;
}

static unsigned char *psf2_read_remaining_file(FILE *file, unsigned int *size)
{
	unsigned char buf[BUFSIZ], *ubuf = 0;
	size_t nrd = 0, total = 0;

	while ((nrd = fread(buf, 1, BUFSIZ, file)) > 0) {
		unsigned char *newubuf = realloc(ubuf, total + nrd);
		if (newubuf) {
			ubuf = newubuf;
		} else {
			perror(__func__);
			free(ubuf);
			return 0;
		}
		memcpy(&ubuf[total], buf, nrd);
		total += nrd;
	}
	if (ferror(file)) {
		perror(__func__);
		free(ubuf);
		return 0;
	}
	*size = total;
	return ubuf;
}

static int psf2_read_ucvals(FILE *file, struct psf_font *psf, unsigned int numglyphs)
{
	unsigned int size = 0, i;
	unsigned char *ubuf = psf2_read_remaining_file(file, &size);
	if (!ubuf) { return 0; }
	unsigned char *ptr = ubuf, *end = ubuf + size;
	for (i = 0; i < numglyphs; ++i) {
		struct psf_glyph *glyph = &psf->glyph[i];
		int ucval;
		while (1) {
			if (ptr >= end) {
				fprintf(stderr, "%s: unexpected end of file\n", __func__);
				return 0;
			}
			if (*ptr == PSF2_SEPARATOR) { ++ptr; break; }
			if (*ptr == PSF2_STARTSEQ) {
				++ptr;
				ucval = PSF1_STARTSEQ;
			} else {
				ucval = mini_utf8_decode((const char**)&ptr);
				if (ucval < 0) {
					fprintf(stderr, "%s: invalid utf8 char\n", __func__);
					return 0;
				}
			}
			psf_glyph_adducval(psf, glyph, (unsigned)ucval & 0x1FFFFF);
		}
	}

	return 1;
}

static struct psf_font *psf2_load_fromfile(FILE* file)
{
	unsigned char magic[4];
	magic[0] = PSF2_MAGIC0;
	if (fread(&magic[1], 1, 3, file) != 3) {
		perror(__func__);
		return 0;
	}
	if (magic[0] != PSF2_MAGIC0 || magic[1] != PSF2_MAGIC1 || magic[2] != PSF2_MAGIC2 || magic[3] != PSF2_MAGIC3) {
		fprintf(stderr, "%s: invalid magic number", __func__);
		return 0;
	}

	unsigned int version, headersize, flags, length, charsize, width, height;
	if (!psf_read_int(file, &version)) { return 0; }
	if (!psf_read_int(file, &headersize)) { return 0; }
	if (!psf_read_int(file, &flags)) { return 0; }
	if (!psf_read_int(file, &length)) { return 0; }
	if (!psf_read_int(file, &charsize)) { return 0; }
	if (!psf_read_int(file, &height)) { return 0; }
	if (!psf_read_int(file, &width)) { return 0; }

	struct psf_font *psf = psf_new(2, width, height);
	psf->header.psf2.version = version;
	psf->header.psf2.headersize = headersize;
	psf->header.psf2.flags = flags;
	psf->header.psf2.length = length;
	psf->header.psf2.charsize = charsize;
	psf->header.psf2.width = width;
	psf->header.psf2.height = height;

	if (!psf_read_glyphs(file, psf, length, charsize)) {
		psf_delete(psf);
		return 0;
	}

	if (!psf2_read_ucvals(file, psf, length)) {
		psf_delete(psf);
		return 0;
	}

	return psf;
}

struct psf_font *psf_load_fromfile(FILE* file)
{
	struct psf_font *res = 0;
	int byte = fgetc(file);
	if (byte == PSF1_MAGIC0) {
		res = psf1_load_fromfile(file);
	} else if (byte == PSF2_MAGIC0) {
		res = psf2_load_fromfile(file);
	} else {
		fprintf(stderr, "%s: invalid magic number", __func__);
	}
	return res;
}

struct psf_font *psf_load(const char* filename)
{
	FILE *file = fopen(filename, "rb");
	if (!file) {
		perror(__func__);
		return 0;
	}
	struct psf_font *res = psf_load_fromfile(file);
	fclose(file);
	return res;
}

static int psf_write_dummy(FILE *file, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; ++i) {
		if (fputc('\0', file) == EOF) { return 0; }
	}
	return 1;
}

static int psf_write_glyphs(FILE *file, struct psf_font *psf, unsigned int numglyphs, unsigned int glyphsize)
{
	unsigned int i;
	for (i = 0; i < numglyphs; ++i) {
		if (psf->glyph[i].data) {
			int nwr = fwrite(psf->glyph[i].data, 1, glyphsize, file);
			if (nwr != (int) glyphsize) {
				perror(__func__);
				return 0;
			}
		} else {
			if (!psf_write_dummy(file, glyphsize)) {
				perror(__func__);
				return 0;
			}
		}
	}
	return 1;
}

static int psf1_write_ucvals(FILE *file, struct psf_font *psf, unsigned int numglyphs)
{
	unsigned int i, ucv;
	for (i = 0; i < numglyphs; ++i) {
		for (ucv = 0; ucv < psf->glyph[i].nucvals; ++ucv) {
			if (!psf_write_word(file, psf->glyph[i].ucvals[ucv])) { return 0; }
		}
		if (!psf_write_word(file, PSF1_SEPARATOR)) { return 0; }
	}
	return 1;
}

static int psf1_save_tofile(FILE *file, struct psf_font *psf)
{
	if (!psf_write_byte(file, psf->header.psf1.magic[0])) { return 0; }
	if (!psf_write_byte(file, psf->header.psf1.magic[1])) { return 0; }
	if (!psf_write_byte(file, psf->header.psf1.mode)) { return 0; }
	if (!psf_write_byte(file, psf->header.psf1.charsize)) { return 0; }

	int numglyphs = (psf->header.psf1.mode & PSF1_MODE512) ? 512 : 256;
	if (!psf_write_glyphs(file, psf, numglyphs, psf->header.psf1.charsize)) {
		return 0;
	}

	if (psf->header.psf1.mode & (PSF1_MODEHASTAB | PSF1_MODEHASSEQ)) {
		if (!psf1_write_ucvals(file, psf, numglyphs)) {
			return 0;
		}
	}
	return 1;
}

static int psf2_write_ucvals(FILE *file, struct psf_font *psf, unsigned int numglyphs)
{
	char u8buf[8];
	unsigned int i, ucv;
	for (i = 0; i < numglyphs; ++i) {
		for (ucv = 0; ucv < psf->glyph[i].nucvals; ++ucv) {
			if (psf->glyph[i].ucvals[ucv] == PSF1_STARTSEQ) {
				if (!psf_write_byte(file, PSF2_STARTSEQ)) { return 0; }
			} else {
				unsigned int len = mini_utf8_encode(psf->glyph[i].ucvals[ucv], u8buf, 8);
				if (len <= 0) {
					fprintf(stderr, "%s: invalid unicode value\n", __func__);
					return 0;
				}
				if (fwrite(u8buf, 1, len, file) < len) {
					perror(__func__);
					return 0;
				}
			}
		}
		if (!psf_write_byte(file, PSF2_SEPARATOR)) { return 0; }
	}
	return 1;
}

static int psf2_save_tofile(FILE *file, struct psf_font *psf)
{
	int i;
	for (i = 0; i < 4; ++i) {
		if (!psf_write_byte(file, psf->header.psf2.magic[i])) { return 0; }
	}
	if (!psf_write_int(file, psf->header.psf2.version)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.headersize)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.flags)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.length)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.charsize)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.height)) { return 0; }
	if (!psf_write_int(file, psf->header.psf2.width)) { return 0; }

	if (!psf_write_glyphs(file, psf, psf->header.psf2.length, psf->header.psf2.charsize)) {
		return 0;
	}

	if (psf->header.psf2.flags & PSF2_HAS_UNICODE_TABLE) {
		if (!psf2_write_ucvals(file, psf, psf->header.psf2.length)) {
			return 0;
		}
	}

	return 1;
}

int psf_save_tofile(FILE *file, struct psf_font *psf)
{
	int res = 0;
	if (psf->version == 1) {
		res = psf1_save_tofile(file, psf);
	} else {
		res = psf2_save_tofile(file, psf);
	}
	return res;
}

int psf_save(const char *filename, struct psf_font *psf)
{
	FILE *file = fopen(filename, "wb");
	if (!file) {
		perror(__func__);
		return 0;
	}
	int res = psf_save_tofile(file, psf);
	fclose(file);
	return res;
}

void psf_delete(struct psf_font *psf)
{
	if (psf->glyph) {
		int i, nglyphs;
		if (psf->version == 1) {
			nglyphs = (psf->header.psf1.mode & PSF1_MODE512) ? 512 : 256;
		} else {
			nglyphs = psf->header.psf2.length;
		}
		for (i = 0; i < nglyphs; ++i) {
			if (psf->glyph[i].data) { free(psf->glyph[i].data); }
			if (psf->glyph[i].ucvals) { free(psf->glyph[i].ucvals); }
		}
		free(psf->glyph);
	}
	free(psf);
}

static int psf_reallocglyphs(struct psf_font *psf, unsigned int num)
{
	int alloced = (psf->glyph != 0);
	unsigned int ng = alloced ? psf_numglyphs(psf) : 0;

	if (psf->version == 1) {
		if (num > 512) {
			fprintf(stderr, "%s: no more than 512 chars for a version 1 psf font\n", __func__);
			return 0;
		}
		unsigned int nng = num <= 256 ? 256 : 512;
		if (nng > ng) {
			struct psf_glyph *newglyph = calloc(nng, sizeof(struct psf_glyph));
			if (!newglyph) {
				perror(__func__);
				return 0;
			}
			if (alloced) {
				/* this can only be true if 256 glyphs were allocated and the new size is 512 */
				memcpy(newglyph, psf->glyph, 256 * sizeof(struct psf_glyph));
				free(psf->glyph);
			}
			psf->glyph = newglyph;
			if (nng == 512) {
				psf->header.psf1.mode |= PSF1_MODE512;
			}
			return 1;
		}
	} else {
		if (num > ng) {
			struct psf_glyph *newglyph = calloc(num, sizeof(struct psf_glyph));
			if (!newglyph) {
				perror(__func__);
				return 0;
			}
			if (alloced) {
				memcpy(newglyph, psf->glyph, ng * sizeof(struct psf_glyph));
				free(psf->glyph);
			}
			psf->glyph = newglyph;
			psf->header.psf2.length = num;
			return 1;
		}
	}
	return 0;
}

struct psf_glyph *psf_getglyph(struct psf_font *psf, unsigned int no)
{
	if (no >= psf_numglyphs(psf)) {
		return 0;
	}
	return &psf->glyph[no];
}

struct psf_glyph *psf_addglyph(struct psf_font *psf, unsigned int no)
{
	if (no >= psf_numglyphs(psf)) {
		if (!psf_reallocglyphs(psf, no + 1)) {
			return 0;
		}
	}
	psf_glyph_init(psf, &psf->glyph[no]);
	return &psf->glyph[no];
}

int psf_glyph_init(struct psf_font *psf, struct psf_glyph *glyph)
{
	if (glyph->data != 0) { free(glyph->data); }
	if (glyph->ucvals != 0) { free(glyph->ucvals); }

	unsigned int charsize = (psf->version == 1) ? psf->header.psf1.charsize : psf->header.psf2.charsize;
	glyph->data = calloc(1, charsize);
	glyph->nucvals = 0;
	glyph->ucvals = 0;
	return 1;
}

int psf_glyph_setpx(struct psf_font *psf, struct psf_glyph *glyph, unsigned int x, unsigned int y, unsigned int val)
{
	if (!glyph->data) { return 0; }
	unsigned int w = psf_width(psf);
	unsigned int h = psf_height(psf);
	if (x >= w || y >= h) { return 0; }
	unsigned int byte = y * ((w + 7) >> 3) + (x >> 3);
	unsigned int mask = 0x80 >> (x & 7);
	if (val) {
		glyph->data[byte] |= mask;
	} else {
		glyph->data[byte] &= ~mask;
	}
	return 1;
}

int psf_glyph_getpx(struct psf_font *psf, struct psf_glyph *glyph, unsigned int x, unsigned int y)
{
	if (!glyph->data) { return 0; }
	unsigned int w = psf_width(psf);
	unsigned int h = psf_height(psf);
	if (x >= w || y >= h) { return 0; }
	unsigned int byte = y * ((w + 7) >> 3) + (x >> 3);
	unsigned int mask = 0x80 >> (x & 7);
	return (glyph->data[byte] & mask) == 0;
}

int psf_glyph_adducval(struct psf_font *psf, struct psf_glyph *glyph, unsigned int uni)
{
	if (psf->version == 1 && uni > 0xFFFF) {
		fprintf(stderr, "%s: unicode value too big for psf1\n", __func__);
		return 0;
	}
	unsigned int newnucvals = glyph->nucvals + 1;
	unsigned int *newucvals = calloc(newnucvals, sizeof(unsigned int));
	if (!newucvals) {
		perror(__func__);
		return 0;
	}
	if (glyph->ucvals) {
		memcpy(newucvals, glyph->ucvals, glyph->nucvals * sizeof(unsigned int));
		free(glyph->ucvals);
	}
	glyph->ucvals = newucvals;
	glyph->nucvals = newnucvals;
	glyph->ucvals[newnucvals - 1] = uni;
	if (psf->version == 1) {
		psf->header.psf1.mode |= PSF1_MODEHASTAB;
		if (uni == PSF1_STARTSEQ) {
			psf->header.psf1.mode |= PSF1_MODEHASSEQ;
		}
	} else {
		psf->header.psf2.flags |= PSF2_HAS_UNICODE_TABLE;
	}

	return 1;
}

unsigned int psf_numglyphs(struct psf_font *psf)
{
	if (psf->version == 1) {
		return psf->header.psf1.mode & PSF1_MODE512 ? 512 : 256;
	} else {
		return psf->header.psf2.length;
	}
}

unsigned int psf_hasunicodetable(struct psf_font *psf)
{
	if (psf->version == 1) {
		return (psf->header.psf1.mode & PSF1_MODEHASTAB) != 0;
	} else {
		return (psf->header.psf2.flags & PSF2_HAS_UNICODE_TABLE) != 0;
	}
}
