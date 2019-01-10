/* psf.h
 *
 * a simple psf handling library.
 *
 * Gunnar Zötl <gz@tset.de> 2016
 * Released under the terms of the MIT license. See file LICENSE for details.
 *
 * info about the psf font file format(s) from
 * http://www.win.tue.nl/~aeb/linux/kbd/font-formats-1.html
 */

#ifndef psf_h
#define psf_h

/* this first part is copied more or less verbatim from th above source */

#define PSF1_MAGIC0     0x36
#define PSF1_MAGIC1     0x04

#define PSF1_MODE512    0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODEHASSEQ 0x04
#define PSF1_MAXMODE    0x05

#define PSF1_SEPARATOR  0xFFFF
#define PSF1_STARTSEQ   0xFFFE

struct psf1_header {
	unsigned char magic[2];     /* Magic number */
	unsigned char mode;         /* PSF font mode */
	unsigned char charsize;     /* Character size */
};

#define PSF2_MAGIC0     0x72
#define PSF2_MAGIC1     0xb5
#define PSF2_MAGIC2     0x4a
#define PSF2_MAGIC3     0x86

/* bits used in flags */
#define PSF2_HAS_UNICODE_TABLE 0x01

/* max version recognized so far */
#define PSF2_MAXVERSION 0

/* UTF8 separators */
#define PSF2_SEPARATOR  0xFF
#define PSF2_STARTSEQ   0xFE

struct psf2_header {
	unsigned char magic[4];
	unsigned int version;
	unsigned int headersize;    /* offset of bitmaps in file */
	unsigned int flags;
	unsigned int length;        /* number of glyphs */
	unsigned int charsize;      /* number of bytes for each character */
	unsigned int height, width; /* max dimensions of glyphs */
	/* charsize = height * ((width + 7) / 8) */
};

/* representation of a single glyph, including unicode mapping information */

struct psf_glyph {
	unsigned char* data;
	unsigned int nucvals;
	unsigned int* ucvals;
};

/* representation of a complete psf font. */

struct psf_font {
	int version; /* 1 or 2 */
	union {
		struct psf1_header psf1;
		struct psf2_header psf2;
	} header;
	struct psf_glyph *glyph;
};

/* psf_width (macro)
 *
 * return the width of a psf font
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	the width of the font
 */
#define psf_width(psf) (((psf)->version == 1) ? 8 : (psf)->header.psf2.width)

/* psf_height (macro)
 *
 * return the height of a psf font
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	the height of the font
 */
#define psf_height(psf) (((psf)->version == 1) ? (psf)->header.psf1.charsize : (psf)->header.psf2.height)

/* psf_new
 *
 * allocates and intializes a new psf_font structure. Based upon the version,
 * the following applies:
 * 	if version==1:
 *		width must be 8
 *		charsize is precomputed as height
 *		space for 256 glyphs is preallocated
 *	if version==2:
 *		charsize is precomputed as ((width + 7) / 8) * height
 *		headersize and version fields are set to constant values (32 and 0)
 *		no space for glyphs is preallocated
 *
 * Arguments:
 *	version	psf version to create font file for
 *	width	width of char. Must be 8 for version 1 fonts
 *	height	height of char
 *
 * Returns:
 *	a pointer to the allocated and initialized psf_font structure, or 0 on
 *	error.
 */
struct psf_font *psf_new(unsigned int version, unsigned int width, unsigned int height);

/* psf_load_fromfile
 *
 * loads a psf font from a file handle
 *
 * Arguments:
 *	file	the file handle to load the font from
 *
 * Returns:
 *	a pointer to a psf_font structure containing the loaded font, or 0 on
 *	error.
 */
struct psf_font *psf_load_fromfile(FILE *file);

/* psf_load
 *
 * load a psf font from a file
 *
 * Arguments:
 *	filename	the name of the file to load the font from
 *
 * Returns:
 *	a pointer to a psf_font structure containing the loaded font, or 0 on
 *	error.
 */
struct psf_font *psf_load(const char *filename);

/* psf_save_tofile
 *
 * saves a psf_font structure to a psf font file handle
 *
 * Arguments:
 *	file	the file handle to save to
 *	psf		the psf_font structure to save
 *
 * Returns:
 *	1 on success, 0 on failure.
 */
int psf_save_tofile(FILE *file, struct psf_font *psf);

/* psf_save
 *
 * saves a psf_font structure to a psf font file
 *
 * Arguments:
 *	filename	the name of the file to save to
 *	psf		the psf_font structure to save
 *
 * Returns:
 *	1 on success, 0 on failure.
 */
int psf_save(const char *filename, struct psf_font *psf);

/* psf_delete
 *
 * clean up and delete a psf_font structure. This also calls free() on the
 * pointer. Use this to deallocate what you get from psf_new or the psf_load*
 * functions.
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	-
 */
void psf_delete(struct psf_font *psf);

/* psf_getglyph
 *
 * returns a pointer to a glyph within a psf font
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	a pointer to a psf_glyph structure representing the <no>th glyph in the
 *	font, or 0 on failure. You must not call free() on this pointer.
 */
struct psf_glyph *psf_getglyph(struct psf_font *psf, unsigned int no);

/* psf_addglyph
 *
 * initializes glyph number <no>, sees that it is available (if within the
 * valid range for glyph numbers for the font) and returns a pointer to it.
 * If the glyph was in use prior to this call, it will be newly initialized.
 *
 * Arguments:
 *	psf		the psf font
 *	no		the number of the glyph to initialize and return
 *
 * Returns:
 *	a pointer to a psf_glyph structure representing the <no>th glyph in the
 *	font, or 0 on failure. You must not call free() on this pointer.
 */
struct psf_glyph *psf_addglyph(struct psf_font *psf, unsigned int no);

/* psf_glyph_init
 *
 * (re-)initializes a glyph.
 *
 * Arguments:
 *	psf		the psf font
 *	glyph	a pointer to the glyph to initialize
 *
 * Returns:
 *	1 on success, 0 on failure.
 */
int psf_glyph_init(struct psf_font *psf, struct psf_glyph *glyph);

/* psf_glyph_setpx
 *
 * sets a pixel in the glyph
 *
 * Arguments:
 *	psf		the psf font
 *	glyph	a pointer to the glyph to change
 *	x, y	coordinates of the pixel to set
 *	val		value to set the pixel to, 0 or 1
 *
 * Returns:
 *	1 on success, 0 on failure.
 */
int psf_glyph_setpx(struct psf_font *psf, struct psf_glyph *glyph, unsigned int x, unsigned int y, unsigned int val);

/* psf_glyph_getpx
 *
 * gets a pixel value from a glyph
 *
 * Arguments:
 *	psf		the psf font
 *	glyph	the glyph to get the pixel value from
 *	x, y	coordinates of pixel to read
 *
 * Returns:
 *	1 if the pixel is set, 0 if not. Any pixels outside of the glyph
 *	dimensions are treated as unset.
 */
int psf_glyph_getpx(struct psf_font *psf, struct psf_glyph *glyph, unsigned int x, unsigned int y);

/* psf_glyph_adducval
 *
 * adds a unicode value to a glyph. For a sequence, add PSF1_STARTSEQ and
 * then the unicode chars that make up the sequence. Note that if you add
 * a sequence, you can add more sequences but not more single unicode values.
 *
 * Arguments:
 *	psf		the psf font
 *	glyph	glyph to add unicode value to
 *	uni		unicode value to add
 *
 * Returns:
 *	1 on success, 0 on failure.
 */
int psf_glyph_adducval(struct psf_font *psf, struct psf_glyph *glyph, unsigned int uni);

/* psf_numglyphs
 *
 * return the amount of glyphs in a psf font. Note that for psf1 fonts, this
 * number is always either 256 or 512.
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	the number of glyphs in the font.
 */
unsigned int psf_numglyphs(struct psf_font *psf);

/* psf_hasunicodetable
 *
 * checks whether a font has an unicode table
 *
 * Arguments:
 *	psf		the psf font
 *
 * Returns:
 *	1 if the font has an unicode table, 0 if not.
 */
unsigned int psf_hasunicodetable(struct psf_font *psf);

#endif /* psf_h */
