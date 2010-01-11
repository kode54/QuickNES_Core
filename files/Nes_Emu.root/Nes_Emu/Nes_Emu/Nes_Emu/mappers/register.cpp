#include "stdafx.h"

void register_mappers();
void register_mappers()
{
	Nes_Mapper::register_optional_mappers();

	extern void register_vrc7_mapper();
	register_vrc7_mapper();
}