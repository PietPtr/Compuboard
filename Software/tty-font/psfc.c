/* psfc
 *
 * Compiles a text file into a psf file.
 * part of a simple textfile based psf font editor suite.
 *
 * Gunnar Zötl <gz@tset.de> 2016
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "psf.h"
#include "psftools_version.h"

#define LINEBUFSIZE 1024

static int skipws(const char* buf, int pos)
{
	while (buf[pos] && isspace(buf[pos])) {
		++pos;
	}
	return pos;
}

static void lowercasify(char *line)
{
	while (*line) {
		if (*line >= 'A' && *line <= 'Z') {
			*line = tolower(*line);
		}
		++line;
	}
}

static int readnum(char *line, int *ppos)
{
	int res = 0, pos = *ppos;
	while (isdigit(line[pos])) {
		res = res * 10 + (line[pos] - '0');
		++pos;
	}
	*ppos = pos;
	return res;
}

static int readunichar(char *line, int *ppos, unsigned int lineno)
{
	int res = 0, pos = *ppos;
	if (line[pos++] != 'u' || line[pos++] != '+') {
		fprintf(stderr, "psfc: invalid unicode spec in line %u\n", lineno);
		return -1;
	}
	while (isxdigit(line[pos])) {
		int dig = line[pos];
		if (dig >= '0' && dig <= '9') {
			dig -= '0';
		} else if (dig >= 'a' && dig <= 'f') {
			dig = 10 + (dig - 'a');
		}
		res = (res << 4) + dig;
		++pos;
	}
	if (res > 0x10ffff || res < 0) {
		fprintf(stderr, "psfc: invalid unicode spec in line %u\n", lineno);
		return -1;
	}
	*ppos = pos;
	return res;
}

static int psfc_compile_char(struct psf_font *psf, char pixel, char *spec, FILE *in, unsigned int *plineno)
{
	int pos = 0;
	unsigned int x, y, lineno = *plineno;
	if (spec[pos] != '@') {
		fprintf(stderr, "psfc: invalid char spec in line %u", lineno);
		return 0;
	}
	/* glyph number */
	pos = 1;
	int no = readnum(spec, &pos);
	pos = skipws(spec, pos);

	/* unicode tables */
	struct psf_glyph *glyph = psf_addglyph(psf, no);
	if (spec[pos] == ':') {
		pos = skipws(spec, pos + 1);
		while (spec[pos] && spec[pos] != '#') {
			int uc = PSF1_SEPARATOR;
			if (spec[pos] != ';') {
				uc = readunichar(spec, &pos, lineno);
				if (uc < 0) {
					return 0;
				}
			}
			psf_glyph_adducval(psf, glyph, uc);
			pos = skipws(spec, pos);
		}
	}

	if (spec[pos] != '\0' && spec[pos] != '#') {
		fprintf(stderr, "psfc: invalid char spec in line %u\n", lineno);
		return 0;
	}

	/* glyph data */
	char lnbuf[LINEBUFSIZE], *line;
	for (y = 0; y < psf_height(psf); ++y) {
		line = fgets(lnbuf, LINEBUFSIZE, in);
		if (!line) {
			fprintf(stderr, "psfc: unexpected end of file in line %u\n", lineno);
			return 0;
		}
		++lineno;
		pos = 0;
		for (x = 0; x < psf_width(psf); ++x) {
			if (line[pos] != '\0') {
				psf_glyph_setpx(psf, glyph, x, y, line[pos] == pixel);
				++pos;
			}
		}
		pos = skipws(line, pos);
		if (line[pos] != '\0') {
			fprintf(stderr, "psfc: invalid bitmap data in line %d\n", lineno);
			int i; for (i = 0; line[i]; ++i) { printf("%02x ", line[i]); }printf("Width: %d Height: %d Pos: %d\n", psf_width(psf), psf_height(psf), pos);
			return 0;
		}
	}
	*plineno = lineno;
	return 1;
}

static struct psf_font *psfc_compile(FILE *in)
{
	char lnbuf[LINEBUFSIZE], *line;
	int pos;
	unsigned int version = 0, width = 0, height = 0, lineno = 0;
	char pixel = '\0';

	/* read header */
	while ((line = fgets(lnbuf, LINEBUFSIZE, in)) != 0) {
		++lineno;
		pos = skipws(line, 0);
		if (!line[pos] || line[pos] == '#') { break; }	/* skip comments and empty lines */

		lowercasify(line);
		if (strncmp(&line[pos], "@psf", 4) == 0) {
			if (version != 0) {
				fprintf(stderr, "psfc: duplicate @psf spec in line %u\n", lineno);
				return 0;
			}
			version = line[pos+4] - '1' + 1;
			pos = skipws(line, pos + 5);
			if ((version != 1 && version != 2) || (line[pos] && line[pos] != '#')) {
				fprintf(stderr, "psfc: invalid version spec in line %u\n", lineno);
				return 0;
			}
		} else if (strncmp(&line[pos], "width:", 6) == 0) {
			if (width != 0) {
				fprintf(stderr, "psfc: duplicate width spec in line %u\n", lineno);
				return 0;
			}
			pos = skipws(line, pos + 6);
			width = readnum(line, &pos);
			pos = skipws(line, pos);
			if (width == 0 || (line[pos] && line[pos] != '#')) {
				fprintf(stderr, "psfc: invalid width spec in line %u\n", lineno);
				return 0;
			}
		} else if (strncmp(&line[pos], "height:", 7) == 0) {
			if (height != 0) {
				fprintf(stderr, "psfc: duplicate height spec in line %u\n", lineno);
				return 0;
			}
			pos = skipws(line, pos + 7);
			height = readnum(line, &pos);
			pos = skipws(line, pos);
			if (height == 0 || (line[pos] && line[pos] != '#')) {
				fprintf(stderr, "psfc: invalid height spec in line %u\n", lineno);
				return 0;
			}
		} else if (strncmp(&line[pos], "pixel:", 6) == 0) {
			if (pixel != 0) {
				fprintf(stderr, "psfc: duplicate pixel spec in line %u\n", lineno);
				return 0;
			}
			int opos = pos;
			pixel = 0;
			pos = skipws(line, opos + 6);
			if (pos > opos + 6) {
				pixel = line[pos] ? line[pos] : line[pos-1];
				if (line[pos]) { ++pos; }
				pos = skipws(line, pos);
			}
			if (pixel == 0 || (line[pos] && line[pos] != '#')) {
				fprintf(stderr, "psfc: invalid pixel spec in line %u\n", lineno);
				return 0;
			}
		} else if (line[pos] == '@' && isdigit(line[pos + 1])) {
			break;	/* first char starts here */
		} else {
			fprintf(stderr, "psfc: invalid header field in line %u\n", lineno);
			return 0;
		}
	}
	if (version == 1 && width == 0) {
		width = 8;
	}
	if (version == 0 || width == 0 || height == 0) {
		fprintf(stderr, "psfc: incomplete header\n");
		return 0;
	}
	if (!pixel) {
		pixel = '#';
	}

	struct psf_font *psf = psf_new(version, width, height);
	if (!psf) { return 0; }

	while (line) {
		lowercasify(line);
		if (!psfc_compile_char(psf, pixel, line, in, &lineno)) {
			psf_delete(psf);
			return 0;
		}
		do {
			line = fgets(lnbuf, LINEBUFSIZE, in);
			if (line) {
				pos = skipws(line, pos);
				++lineno;
			}
		} while (line && (*line == 0 || *line == '#'));
	}
	return psf;
}

int main(int argc, char **argv)
{
	if (argc < 1 || argc > 3 || (argv[1] && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
		fprintf(stderr, "%s [file.txt [file.psf]]\n", argv[0]);
		fprintf(stderr, "psftools version %s\n", PSFTOOLS_VERSION);
		exit(1);
	}
	const char* infile = argc >= 2 ? argv[1] : 0;
	const char* outfile = argc == 3 ? argv[2] : 0;

	FILE *in = (infile && strcmp(infile, "-") != 0) ? fopen(infile, "r") : stdin;
	if (!in) {
		perror("psfc: could not open input file");
		exit(1);
	}
	struct psf_font *psf = psfc_compile(in);
	if (in != stdin) { fclose(in); }

	if (!psf) { exit(1); }
	int ok = 0;
	if (outfile) {
		ok = psf_save(outfile, psf);
	} else {
		ok = psf_save_tofile(stdout, psf);
	}
	psf_delete(psf);

	exit(ok == 0);
}
