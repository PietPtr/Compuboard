#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "psftools_version.h"

#define LINEBUFSIZE 1024

/* generates a text font template
 */
static int psft_generate(const char* outfile, unsigned int version, unsigned int width, unsigned int height, unsigned int nchars, int uni)
{
	if (version != 1 && version != 2) {
		fprintf(stderr, "psfc: invalid version number: %u\n", version);
		return 0;
	}
	if ((version == 1 && width != 8) || width == 0) {
		fprintf(stderr, "psfc: invalid width%s: %u\n", width ? " for version 1 files" : "", width);
		return 0;
	}
	if (height == 0) {
		fprintf(stderr, "psfc: invalid height: %u\n", height);
		return 0;
	}
	if (version ==1 && nchars != 256 && nchars != 512) {
		fprintf(stderr, "psfc: value for -n must be either 256 or 512 for version 1 psf files\n");
	}

	FILE *out = stdout;
	if (outfile) {
		out = fopen(outfile, "w");
		if (!out) {
			perror("psft");
			return 0;
		}
	}

	fprintf(out, "@psf%u\n", version);
	fprintf(out, "Width: %u\n", width);
	fprintf(out, "Height: %u\n", height);
	fprintf(out, "Pixel: #\n");

	unsigned int ch, x, y;
	for (ch = 0; ch < nchars; ++ch) {
		fprintf(out, "@%u", ch);
		if (uni) { fprintf(out, ": U+%04x", ch); }
		fputc('\n', out);

		for (y = 0; y < height; ++y) {
			for (x = 0; x < width; ++x) {
				fputc('.', out);
			}
			fputc('\n', out);
		}
	}


	if (out != stdout) { fclose(out); }
	return 1;
}

static int skipws(const char* buf, int pos)
{
	while (buf[pos] && isspace(buf[pos])) {
		++pos;
	}
	return pos;
}

/* renumbers glyphs in a text font file
 */
static int psft_renumber(const char *infile, const char *outfile)
{
	FILE *in = stdin, *out = stdout;
	if (infile) {
		in = fopen(infile, "r");
		if (!in) {
			perror("psft");
			return 0;
		}
	}
	if (outfile) {
		out = fopen(outfile, "w");
		if (!out) {
			perror("psft");
			fclose(in);
			return 0;
		}
	}

	char inbuf[LINEBUFSIZE], outbuf[LINEBUFSIZE], *line;
	unsigned int cnt = 0, lineno = 0;
	while ((line = fgets(inbuf, LINEBUFSIZE, in)) != 0) {
		++lineno;
		int pos = skipws(line, 0);
		if (line[pos] == '@' && (isdigit(line[pos+1]) || isspace(line[pos+1]) || line[pos+1] == ':')) {
			int pos1 = pos + 1;
			while (isdigit(line[pos1])) { ++pos1; }
			char oc = line[pos+1];
			line[pos+1] = '\0';
			snprintf(outbuf, LINEBUFSIZE, "%s%d", line, cnt++);
			line[pos+1] = oc;
			if (strlen(outbuf) + strlen(&line[pos1]) >= LINEBUFSIZE) {
				fprintf(stderr, "psft: output line too long for line %u\n", lineno);
				fclose(in);
				fclose(out);
				return 0;
			}
			strcat(outbuf, &line[pos1]);
			line = outbuf;
		} else if (line[pos] == '@' && strncasecmp(&line[pos], "@psf", 4) != 0) {
			fprintf(stderr, "psft: invalid glyph header in line %u\n", lineno);
				fclose(in);
				fclose(out);
			return 0;
		}
		fputs(line, out);
	}

	if (out != stdout) { fclose(out); }
	if (in != stdin) { fclose(in); }
	return 1;
}

static void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s cmd [opts]\n", cmd);
	fputs(	"cmd is one of\n"
			"  ren[umber] [infile [outfile]]\n"
			"    renumber glyphs. If infile is omitted of -, defaults to stdin.\n"
			"    If outfile is omitted, defaults to stdout.\n"
			"  gen[erate] <version> [-w <width>] [-h <height>] [-n <num>] [-u] [outfile]\n"
			"    generate a new font template. version is 1 or 2, depending on\n"
			"    the psf version you want to generate. width and height default\n"
			"    to 8, and num (the amount of chars in the font) defaults to 256.\n"
			"    Specify -u to add sample unicode values to the template.\n"
			"    If outfile is omitted, defaults to stdout.\n"
			"  -h|--help|help\n"
			"    print this help\n"
		, stderr);
	fprintf(stderr, "psftools version %s\n", PSFTOOLS_VERSION);
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0]);
	}
	if (strcmp(argv[1], "ren") == 0 || strcmp(argv[1], "renumber") == 0) {
		if (argc > 4) {
			usage(argv[0]);
		}
		const char *infile = 0, *outfile = 0;
		if (argc >= 3 && strcmp(argv[2], "-") != 0) {
			infile = argv[2];
		}
		if (argc == 4) {
			outfile = argv[3];
		}
		if (!psft_renumber(infile, outfile)) {
			exit(1);
		}
	} else if (strcmp(argv[1], "gen") == 0 || strcmp(argv[1], "generate") == 0) {
		if (argc < 3) {
			usage(argv[0]);
		}
		unsigned int version = 0, width = 8, height = 8, num = 256;
		const char* outfile = 0;
		int uni = 0, arg = 3;
		version = strtoul(argv[2], 0, 10);
		while (arg + 1 < argc) {
			if (!strcmp(argv[arg], "-w")) {
				width = (unsigned int) strtoul(argv[arg + 1], 0, 10);
				arg += 2;
			} else if (!strcmp(argv[arg], "-h")) {
				height = (unsigned int) strtoul(argv[arg + 1], 0, 10);
				arg += 2;
			} else if (!strcmp(argv[arg], "-n")) {
				num = (unsigned int) strtoul(argv[arg + 1], 0, 10);
				arg += 2;
			} else if (!strcmp(argv[arg], "-u")) {
				uni = 1;
				arg += 1;
			} else  {
				usage(argv[0]);
			}
		}
		if (arg < argc) {
			if (!strcmp(argv[arg], "-u")) {
				uni = 1;
			} else {
				outfile = argv[arg];
			}
		}
		if (!psft_generate(outfile, version, width, height, num, uni)) {
			exit(1);
		}
	} else if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
		usage(argv[0]);
	} else {
		usage(argv[0]);
	}

	exit(0);
}