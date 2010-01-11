/* Common aspects of vector blitters */

/* Bytes to next burst phase */
enum { burst_offset = nes_ntsc_burst_size * sizeof (nes_ntsc_rgb_t) };

enum { vecs_per_chunk = nes_ntsc_out_chunk * bytes_per_pixel / sizeof (vInt8) };

/* Pointer to kernel entry for given palette index */
#define ENTRY( i ) \
	((vInt8 const*) (ktable + nes_ntsc_entry_size * sizeof (nes_ntsc_rgb_t) * (i)))

/* Sets kernel pointer for input pixel */
#define PIXEL_IN( i ) \
	kernel = ENTRY( NES_NTSC_ADJ_IN( in [i], user_data ) )

/* Sets kernel pointer for black pixel */
#define BLACK_IN() kernel = ENTRY( 15 )

/* Calculates four vectors of output pixels and writes them */
#define PIXELS_OUT( out, a, b, c, d ) \
	PADDB(   a, a );\
	PADDB(   b, b );\
	PADDB(   c, c );\
	PADDB(   d, d );\
	PADDUSB( a, a );\
	PADDUSB( b, b );\
	PADDUSB( c, c );\
	PADDUSB( d, d );\
	STORE4( out, a, b, c, d );\

/* Default vector load and store */
#ifndef LOAD
#define LOAD( out, in ) out = (in)
#endif

#ifndef STORE
#define STORE( out, n ) *(out) = n
#endif

/* Default quad vector store */
#ifndef STORE4
#define STORE4( out, a, b, c, d ) \
	STORE( out,   a );\
	STORE( out+1, b );\
	STORE( out+2, c );\
	STORE( out+3, d );
#endif
