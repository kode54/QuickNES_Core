/* Configure library by modifying this file */

#ifndef NES_NTSC_CONFIG_H
#define NES_NTSC_CONFIG_H

/* Uncomment to enable emphasis support and use a 512 color palette instead
of the base 64 color palette. */
#define NES_NTSC_EMPHASIS 1

/* Type of input pixel values. You'll probably use unsigned short
if you enable emphasis above. */
#define NES_NTSC_IN_T unsigned char

/* Type of user_data passed through to NES_NTSC_ADJ_IN */
#define NES_NTSC_USER_DATA_T const unsigned short*

/* Each raw pixel input value is passed through this. You might want to mask
the pixel index if you use the high bits as flags, pass it through a lookup
table if necessary (using user_data to access the table), etc. */
#define NES_NTSC_ADJ_IN( in, user_data ) user_data [in]

/* For each pixel, this is the basic operation:
output_color = color_palette [NES_NTSC_ADJ_IN( NES_NTSC_IN_T, user_data )] */

#endif
