/* psfd
 *
 * Decompiles a psf file into a text file.
 * part of a simple textfile based psf font editor suite.
 *
 * Gunnar Zötl <gz@tset.de> 2016
 * Released under the terms of the MIT license. See file LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psf.h"
#include "psftools_version.h"

static int psfd_print_header(struct psf_font *psf, FILE *out)
{
	if (!psf) { return 0; }
	fprintf(out, "@psf%d\n", psf->version);
	if (psf->version == 1) {
		fprintf(out, "Width: %d\n", 8);
		fprintf(out, "Height: %d\n", psf->header.psf1.charsize);
	} else {
		fprintf(out, "Width: %d\n", psf->header.psf2.width);
		fprintf(out, "Height: %d\n", psf->header.psf2.height);
	}
	fputs("Pixel: #\n", out);
	return 1;
}

static int psfd_print_glyph(struct psf_font *psf, unsigned int n, FILE *out)
{
	struct psf_glyph *glyph = psf_getglyph(psf, n);
	if (!glyph) { return 0; }
	unsigned int x, y, i;
	int hasseq = 0, inseq = 0;
	fprintf(out, "@%d", n);
	if (glyph->nucvals > 0) {
		fputc(':', out);
		for (i = 0; i < glyph->nucvals; ++i) {
			if (glyph->ucvals[i] == 0xFFFE) {
				hasseq = 0;
				inseq = 0;
				fputc(',', out);
			} else {
				int delim = ' ';
				if (hasseq && (inseq == 0)) {
					delim = ';';
				}
				fprintf(out, "%cU+%04x", delim, glyph->ucvals[i]);
				++inseq;
			}
		}
	}
	fputc('\n', out);
	for (y = 0; y < psf_height(psf); ++y) {
		for (x = 0; x < psf_width(psf); ++x) {
			fputc(psf_glyph_getpx(psf, glyph, x, y) ? '.' : '#', out);
		}
		fputc('\n', out);
	}
	return 1;
}

static int psfd_print_glyphs(struct psf_font *psf, FILE *out)
{
	unsigned int i;
	for (i = 0; i < psf_numglyphs(psf); ++i) {
		if (!psfd_print_glyph(psf, i, out)) {
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 1 || argc > 3 || (argv[1] && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
		fprintf(stderr, "%s [file.psf [file.txt]]\n", argv[0]);
		fprintf(stderr, "psftools version %s\n", PSFTOOLS_VERSION);
		exit(1);
	}

	const char* infile = argc >= 2 ? argv[1] : 0;
	const char* outfile = argc == 3 ? argv[2] : 0;

	struct psf_font *psf = (infile && strcmp(infile, "-") != 0) ? psf_load(infile) : psf_load_fromfile(stdin);
	if (!psf) {
		exit(1);
	}
	FILE *out = outfile ? fopen(outfile, "w") : stdout;
	if (!out) {
		perror("psfd: could not open output file");
		psf_delete(psf);
		exit(1);
	}

	if (!psfd_print_header(psf, out)) {
		exit(1);
	}
	if (!psfd_print_glyphs(psf, out)) {
		exit(1);
	}
	if (out != stdout) { fclose(out); }
	psf_delete(psf);

	exit(0);
}

