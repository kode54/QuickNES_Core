#include "stdafx.h"

void register_mmc5_mapper();
void register_vrc6_mapper();
void register_misc_mappers();
void register_fme07_mapper();
void register_vrc7_mapper();

void register_mappers();
void register_mappers()
{
	register_mmc5_mapper();
	register_vrc6_mapper();
	register_misc_mappers();
	register_fme07_mapper();
	register_vrc7_mapper();
}