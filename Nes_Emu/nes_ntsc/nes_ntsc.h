/* NES NTSC video filter, optimized for MMX/SSE/AltiVec */

/* nes_ntsc 0.2.3 */
#ifndef NES_NTSC_H
#define NES_NTSC_H

#include "nes_ntsc_config.h"

#ifdef __cplusplus
	extern "C" {
#endif

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
typedef struct nes_ntsc_setup_t
{
	/* Basic parameters */
	double hue;        /* -1 = -180 degrees     +1 = +180 degrees */
	double saturation; /* -1 = grayscale (0.0)  +1 = oversaturated colors (2.0) */
	double contrast;   /* -1 = dark (0.5)       +1 = light (1.5) */
	double brightness; /* -1 = dark (0.5)       +1 = light (1.5) */
	double sharpness;  /* edge contrast enhancement/blurring */
	
	/* Advanced parameters */
	double gamma;      /* -1 = dark (1.5)       +1 = light (0.5) */
	double resolution; /* image resolution */
	double artifacts;  /* artifacts caused by color changes */
	double fringing;   /* color artifacts caused by brightness changes */
	double bleed;      /* color bleed (color resolution reduction) */
	int merge_fields;  /* if 1, merges even and odd fields together to reduce flicker */
	float const* decoder_matrix; /* optional RGB decoder matrix, 6 elements */
	
	unsigned char* palette_out;  /* optional RGB palette out, 3 bytes per color */
	
	/* You can replace the standard NES color generation with an RGB palette. The
	first replaces all color generation, while the second replaces only the core
	64-color generation and does standard color emphasis calculations on it. */
	unsigned char const* palette;/* optional 512-entry RGB palette in, 3 bytes per color */
	unsigned char const* base_palette;/* optional 64-entry RGB palette in, 3 bytes per color */
} nes_ntsc_setup_t;

/* Video format presets */
extern nes_ntsc_setup_t const nes_ntsc_composite; /* color bleeding + artifacts */
extern nes_ntsc_setup_t const nes_ntsc_svideo;    /* color bleeding only */
extern nes_ntsc_setup_t const nes_ntsc_rgb;       /* crisp image */
extern nes_ntsc_setup_t const nes_ntsc_monochrome;/* desaturated + artifacts */

#ifdef NES_NTSC_EMPHASIS
	enum { nes_ntsc_palette_size = 64 * 8 };
#else
	enum { nes_ntsc_palette_size = 64 };
#endif

/* Initializes and adjusts parameters. Can be called multiple times on the same
nes_ntsc_t object. Can pass NULL for either parameter. */
typedef struct nes_ntsc_t nes_ntsc_t;
void nes_ntsc_init( nes_ntsc_t* ntsc, nes_ntsc_setup_t const* setup );

/* Filters one or more rows of pixels. Input pixels are 6/9-bit palette indicies.
in_row_width is the number of PIXELS to get to the next input row. Out_pitch
is the number of BYTES to get to the next output row. */
void nes_ntsc_blit( nes_ntsc_t const* ntsc, NES_NTSC_IN_T const* nes_in,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, NES_NTSC_USER_DATA_T user_data );

/* MMX version for older processors */
void nes_ntsc_blit_mmx( nes_ntsc_t const* ntsc, NES_NTSC_IN_T const* nes_in,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, NES_NTSC_USER_DATA_T user_data );

/* MMX2 version for older processors with SSE */
void nes_ntsc_blit_mmx2( nes_ntsc_t const* ntsc, NES_NTSC_IN_T const* nes_in,
		long in_row_width, int burst_phase, int in_width, int in_height,
		void* rgb_out, long out_pitch, NES_NTSC_USER_DATA_T user_data );

/* AltiVec version outputs 15-bit RGB, others 32-bit RGB */
#if __POWERPC__
	#define NES_NTSC_OUT_DEPTH 15
#else
	#define NES_NTSC_OUT_DEPTH 32
#endif

/* Number of output pixels written by blitter for given input width. Width might
be rounded down slightly; use NES_NTSC_IN_WIDTH() on result to find rounded
value. Guaranteed not to round 256 down at all. */
#define NES_NTSC_OUT_WIDTH( in_width ) \
	((((in_width) - nes_ntsc_in_extra) / nes_ntsc_in_chunk + 1) * nes_ntsc_out_chunk)

/* Number of input pixels that will fit within given output width. Might be
rounded down slightly; use NES_NTSC_OUT_WIDTH() on result to find rounded
value. */
#define NES_NTSC_IN_WIDTH( out_width ) \
	(((out_width) / nes_ntsc_out_chunk - 1) * nes_ntsc_in_chunk + nes_ntsc_in_extra)

/* Number of pixels output width should be scaled to on screen. */
#define NES_NTSC_RESCALE_WIDTH( out_width ) \
	(((out_width) * 219 + 96) >> 8)


/* private */
enum { nes_ntsc_in_extra    = 4  }; /* number of extra input pixels read per row */
enum { nes_ntsc_in_chunk    = 6  }; /* number of input pixels read per chunk */
enum { nes_ntsc_out_chunk   = 16 }; /* number of output pixels generated per chunk */
enum { nes_ntsc_burst_count = 3  }; /* burst phase cycles through 0, 1, and 2 */

enum { nes_ntsc_burst_size = 16 * nes_ntsc_burst_count };
enum { nes_ntsc_entry_size = nes_ntsc_burst_size * nes_ntsc_burst_count };
typedef unsigned nes_ntsc_rgb_t;
struct nes_ntsc_t {
	#if __GNUC__
		__attribute__ ((aligned (16)))
	#endif
	nes_ntsc_rgb_t table [nes_ntsc_palette_size] [nes_ntsc_entry_size];
};

#define nes_ntsc_rgb_builder    ((1L << 16) | (1 << 8) | (1 << 0))

#ifdef __cplusplus
	}
#endif

#endif
