
#include "Nes_Blitter.h"

#include <intrin.h>
#include <xmmintrin.h>

#ifdef _M_IX86
enum {
	CPU_HAVE_MMX        = 1 << 0,
	CPU_HAVE_SSE		= 1 << 1,
	CPU_HAVE_SSE2		= 1 << 2,
};

static bool query_cpu_feature_set(unsigned p_value) {
	__try {
		if (p_value & (CPU_HAVE_MMX | CPU_HAVE_SSE | CPU_HAVE_SSE2)) {
			int buffer[4];
			__cpuid(buffer,1);
			if (p_value & CPU_HAVE_MMX) {
				if ((buffer[3]&(1<<23)) == 0) return false;
			}
			if (p_value & CPU_HAVE_SSE) {
				if ((buffer[3]&(1<<25)) == 0) return false;
			}
			if (p_value & CPU_HAVE_SSE2) {
				if ((buffer[3]&(1<<26)) == 0) return false;
			}
		}
		return true;
	} __except(1) {
		return false;
	}
}

static const bool g_have_mmx = query_cpu_feature_set(CPU_HAVE_MMX);
static const bool g_have_sse = query_cpu_feature_set(CPU_HAVE_SSE);
static const bool g_have_sse2 = query_cpu_feature_set(CPU_HAVE_SSE2);
#elif defined(_M_X64)

enum {g_have_sse2 = true, g_have_sse = true, g_have_mmx = false};

#else

enum {g_have_sse2 = false, g_have_sse = false, g_have_mmx = flase};

#endif

Nes_Blitter::Nes_Blitter() { ntsc = 0; }

Nes_Blitter::~Nes_Blitter() { _mm_free( ntsc ); }

blargg_err_t Nes_Blitter::init()
{
	assert( !ntsc );
	ntsc = (nes_ntsc_t*) _mm_malloc( sizeof *ntsc, 16 );
	if ( !ntsc )
		return "Out of memory";
	static setup_t const s = { };
	setup_ = s;
	setup_.ntsc = nes_ntsc_composite;
	return setup( setup_ );
}

blargg_err_t Nes_Blitter::setup( setup_t const& s )
{
	setup_ = s;
	width = Nes_Emu::image_width - setup_.crop.left - setup_.crop.right;
	height = Nes_Emu::image_height - setup_.crop.top - setup_.crop.bottom;
	nes_ntsc_init( ntsc, &setup_.ntsc );
	return 0;
}

#if defined(_M_IA64) || defined(_M_X64)
typedef unsigned __int64 t_pointer;
#else
typedef unsigned int t_pointer;
#endif

void Nes_Blitter::blit( Nes_Emu& emu, void* out, long out_pitch )
{
	unsigned short const* palette = (unsigned short const*) emu.frame().palette;
	int burst_phase = (setup_.ntsc.merge_fields ? 0 : emu.frame().burst_phase);
	long in_pitch = emu.frame().pitch;
	unsigned char* in = emu.frame().pixels + setup_.crop.top * in_pitch + setup_.crop.left;

	void* out_orig = out;
	void* out_aligned = (void*)((((t_pointer)out) + 15) & ~15);

	if ( out != out_aligned )
	{
		out = _mm_malloc( out_pitch * height, 16 );
	}

	if ( g_have_sse2 )
		nes_ntsc_blit( ntsc, in, in_pitch, burst_phase, width, height, out, out_pitch, palette );
	else if ( g_have_sse && g_have_mmx )
		nes_ntsc_blit_mmx2( ntsc, in, in_pitch, burst_phase, width, height, out, out_pitch, palette );
	else if ( g_have_mmx )
		nes_ntsc_blit_mmx( ntsc, in, in_pitch, burst_phase, width, height, out, out_pitch, palette );

	if ( out != out_aligned )
	{
		memcpy( out_orig, out, out_pitch * height );
		_mm_free( out );
	}
}

