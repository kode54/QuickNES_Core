#ifndef NES_MMC5_H
#define NES_MMC5_H

#include "Nes_Apu.h"

class Nes_Mmc5 : public Nes_Apu
{
public:
	enum { start_addr = 0x5000 };
	enum { end_addr   = 0x5015 };
	void write_register( nes_time_t, nes_addr_t, int data );
};

#endif