#include "Nes_Mmc5.h"

void Nes_Mmc5::write_register( nes_time_t time, nes_addr_t addr, int data )
{
	Nes_Apu::write_register( time, addr - 0x1000, data );
}