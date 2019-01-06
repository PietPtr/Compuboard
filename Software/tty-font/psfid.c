/* psfid
 *
 * Prints information about a psf file.
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

void usage()
{
	fputs(	"Usage: psfid [-v] [-w] [-h] [-n] [-u] font.psf\n"
			"  print information about a psf font:\n"
			"  -v psf version\n"
			"  -w font width\n"
			"  -h font height\n"
			"  -n number of chars in font\n"
			"  -u presence of unicode translation table in font (1 for yes, 0 for no)\n"
			"  -l list table of encoded chars\n"
			"  default if no options are specified is -v -w -h -n -u\n"
		,stderr);
	fprintf(stderr, "psftools version %s\n", PSFTOOLS_VERSION);
	exit(1);
}

int unic_cmp(const void *a, const void *b)
{
	unsigned int uca = *(unsigned int*)a;
	unsigned int ucb = *(unsigned int*)b;
	return uca < ucb ? -1 : uca > ucb ? 1 : 0;
}

void listunicodechartable(struct psf_font *psf)
{
	unsigned int nglyphs = psf_numglyphs(psf);
	unsigned int gno, uno, ucnt = 0, nuc = 0;
	
	for (gno = 0; gno < nglyphs; ++gno) {
		struct psf_glyph *glyph = psf_getglyph(psf, gno);
		nuc += glyph->nucvals ? glyph->nucvals : 1;
	}

	unsigned int ucvals[nuc];
	for (gno = 0; gno < nglyphs; ++gno) {
		struct psf_glyph *glyph = psf_getglyph(psf, gno);
		if (glyph->nucvals == 0) {
			ucvals[ucnt++] = gno;
		} else {
			for (uno = 0; uno < glyph->nucvals; ++uno) {
				ucvals[ucnt++] = glyph->ucvals[uno];
			}
		}
	}

	qsort(ucvals, nuc, sizeof(int), unic_cmp);

	printf("%d chars encoded:\n", nuc);
	for (uno = 0; uno < nuc; ++uno) {
		printf("U+%05x\n", ucvals[uno]);
	}
}

int main(int argc, char **argv)
{
	char options[8] = {0};
	int optc = 0, arg = 0;
	const char *psfn = 0;

	if (argc < 2 || argc > 7) {
		usage();
	}
	for (arg = 1; arg < argc; ++arg) {
		const char* opt = argv[arg];
		if (*opt == '-') {
			switch (opt[1]) {
				case 'v': case 'w': case 'h': case 'n': case 'u': case 'l':
					if (opt[2] == '\0' && strchr(options, opt[1]) == 0) {
						options[optc++] = opt[1];
						break;
					}
					/* fallthru on error */
				default:
					fprintf(stderr, "psfid: unknown option: %s\n", opt);
					usage();
			}
		} else if (arg + 1 == argc) {
			psfn = argv[arg];
		} else {
			fprintf(stderr, "psfid: psf file name must be the last argument.\n");
			usage();
		}
	}
	if (!psfn) {
		fprintf(stderr, "psfid: psf file missing.\n");
		usage();
	}
	if (!options[0]) {
		strcpy(options, "vwhnu");
	}
	
	struct psf_font *psf = psf_load(psfn);
	if (!psf) {
		exit(1);
	}

	const char *ptr = options;
	while (*ptr) {
		switch (*ptr) {
			case 'v': printf(" v:%d", psf->version); break;
			case 'w': printf(" w:%d", psf_width(psf)); break;
			case 'h': printf(" h:%d", psf_height(psf)); break;
			case 'n': printf(" n:%d", psf_numglyphs(psf)); break;
			case 'u': printf(" u:%d", psf_hasunicodetable(psf)); break;
			case 'l': listunicodechartable(psf); break;
		}
		++ptr;
	}

	exit(0);
}