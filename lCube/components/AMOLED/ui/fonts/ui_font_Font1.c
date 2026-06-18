/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --font C:/Users/15852/SquareLine/assets/consola-1.ttf -o C:/Users/15852/SquareLine/assets\ui_font_Font1.c --format lvgl -r 0x20-0x7f --no-compress --no-prefilter
 ******************************************************************************/

#include "../ui.h"

#ifndef UI_FONT_FONT1
#define UI_FONT_FONT1 1
#endif

#if UI_FONT_FONT1

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0x55, 0x55, 0x3c,

    /* U+0022 "\"" */
    0x99, 0x90,

    /* U+0023 "#" */
    0x22, 0x26, 0xff, 0x24, 0x24, 0x24, 0xff, 0x64,
    0x44, 0x44,

    /* U+0024 "$" */
    0x8, 0x11, 0xfe, 0xc9, 0x9a, 0x1c, 0xe, 0x36,
    0x64, 0xdf, 0xe2, 0x4, 0x0,

    /* U+0025 "%" */
    0x61, 0x92, 0x96, 0x64, 0x8, 0x18, 0x16, 0x29,
    0x69, 0x49, 0x86,

    /* U+0026 "&" */
    0x38, 0x44, 0x44, 0x44, 0x78, 0x30, 0xfa, 0x9a,
    0x8e, 0xc6, 0x7a,

    /* U+0027 "'" */
    0xe0,

    /* U+0028 "(" */
    0x3, 0x24, 0x48, 0x88, 0x88, 0x84, 0x42, 0x10,

    /* U+0029 ")" */
    0xc, 0x42, 0x21, 0x11, 0x11, 0x12, 0x24, 0x80,

    /* U+002A "*" */
    0x10, 0xa8, 0xe1, 0xc5, 0x42, 0x0,

    /* U+002B "+" */
    0x10, 0x20, 0x47, 0xf1, 0x2, 0x4, 0x0,

    /* U+002C "," */
    0x77, 0x32, 0xc0,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xfc,

    /* U+002F "/" */
    0x4, 0x8, 0x30, 0x41, 0x82, 0xc, 0x10, 0x20,
    0xc1, 0x6, 0x8, 0x0,

    /* U+0030 "0" */
    0x38, 0x8a, 0xc, 0x39, 0xb6, 0x70, 0xc1, 0x44,
    0x70,

    /* U+0031 "1" */
    0x33, 0xc9, 0x4, 0x10, 0x41, 0x4, 0x13, 0xf0,

    /* U+0032 "2" */
    0x79, 0x98, 0x10, 0x20, 0x41, 0x4, 0x10, 0x41,
    0xfc,

    /* U+0033 "3" */
    0x78, 0x8, 0x10, 0x23, 0x80, 0xc0, 0x81, 0x5,
    0xf0,

    /* U+0034 "4" */
    0xc, 0x1c, 0x14, 0x34, 0x24, 0x44, 0xc4, 0xff,
    0x4, 0x4,

    /* U+0035 "5" */
    0xfe, 0x8, 0x20, 0xf0, 0x30, 0x41, 0xb, 0xc0,

    /* U+0036 "6" */
    0x1c, 0xc1, 0x4, 0xf, 0xd0, 0xe0, 0xc1, 0x46,
    0x70,

    /* U+0037 "7" */
    0xfe, 0xc, 0x10, 0x60, 0x83, 0x4, 0x18, 0x20,
    0xc0,

    /* U+0038 "8" */
    0x7d, 0x6, 0xe, 0x37, 0xcf, 0xb1, 0xc1, 0xc6,
    0xf8,

    /* U+0039 "9" */
    0x39, 0x8a, 0xc, 0x1c, 0x2f, 0xc0, 0x82, 0xd,
    0xe0,

    /* U+003A ":" */
    0xd8, 0x0, 0x36,

    /* U+003B ";" */
    0x66, 0x0, 0x7, 0x73, 0x2c,

    /* U+003C "<" */
    0x0, 0x31, 0x88, 0xc1, 0x83, 0x6, 0x4,

    /* U+003D "=" */
    0xfe, 0x0, 0x7, 0xf0,

    /* U+003E ">" */
    0x83, 0x6, 0xc, 0x8, 0x63, 0x10, 0x80,

    /* U+003F "?" */
    0xe0, 0x82, 0x10, 0xb9, 0x8, 0x3, 0x18,

    /* U+0040 "@" */
    0x1e, 0x18, 0x98, 0x28, 0x1d, 0xec, 0xb6, 0x9b,
    0x49, 0xad, 0x5f, 0xa0, 0x18, 0x4, 0x21, 0xe0,

    /* U+0041 "A" */
    0x1c, 0xe, 0x5, 0x6, 0xc2, 0x61, 0x11, 0x8c,
    0xfe, 0x41, 0x60, 0xc0,

    /* U+0042 "B" */
    0xfa, 0x18, 0x63, 0xfa, 0x38, 0x61, 0x8f, 0xe0,

    /* U+0043 "C" */
    0x3e, 0xc1, 0x4, 0x8, 0x10, 0x20, 0x60, 0x40,
    0x7c,

    /* U+0044 "D" */
    0xf9, 0xa, 0xc, 0x18, 0x30, 0x60, 0xc3, 0x8d,
    0xf0,

    /* U+0045 "E" */
    0xfc, 0x21, 0xf, 0xc2, 0x10, 0x87, 0xc0,

    /* U+0046 "F" */
    0xfc, 0x21, 0xf, 0xc2, 0x10, 0x84, 0x0,

    /* U+0047 "G" */
    0x3e, 0xc1, 0x4, 0x8, 0xf0, 0x60, 0xe1, 0x62,
    0x7c,

    /* U+0048 "H" */
    0x83, 0x6, 0xc, 0x1f, 0xf0, 0x60, 0xc1, 0x83,
    0x4,

    /* U+0049 "I" */
    0xfc, 0x41, 0x4, 0x10, 0x41, 0x4, 0x13, 0xf0,

    /* U+004A "J" */
    0xf8, 0x42, 0x10, 0x84, 0x21, 0x1f, 0x80,

    /* U+004B "K" */
    0x8d, 0x12, 0x45, 0xe, 0x1c, 0x2c, 0x4c, 0x89,
    0x18,

    /* U+004C "L" */
    0x84, 0x21, 0x8, 0x42, 0x10, 0x87, 0xc0,

    /* U+004D "M" */
    0xc7, 0x8f, 0xbd, 0x5a, 0xb6, 0x64, 0xc1, 0x83,
    0x4,

    /* U+004E "N" */
    0xc3, 0x87, 0x8d, 0x9b, 0x33, 0x66, 0xc7, 0x87,
    0xc,

    /* U+004F "O" */
    0x3c, 0x42, 0xc1, 0x81, 0x81, 0x81, 0x81, 0x83,
    0x42, 0x3c,

    /* U+0050 "P" */
    0xfa, 0x38, 0x61, 0x8f, 0xc8, 0x20, 0x82, 0x0,

    /* U+0051 "Q" */
    0x3c, 0x42, 0xc1, 0x81, 0x81, 0x81, 0x81, 0x81,
    0x42, 0x7e, 0x18, 0x18, 0xf,

    /* U+0052 "R" */
    0xf9, 0xa, 0x14, 0x6f, 0x93, 0x22, 0x46, 0x85,
    0xc,

    /* U+0053 "S" */
    0x3f, 0x82, 0x6, 0x7, 0x3, 0x81, 0x81, 0x7,
    0xf8,

    /* U+0054 "T" */
    0xfe, 0x20, 0x40, 0x81, 0x2, 0x4, 0x8, 0x10,
    0x20,

    /* U+0055 "U" */
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1, 0xc4,
    0xf0,

    /* U+0056 "V" */
    0xc1, 0xa0, 0x90, 0xcc, 0x62, 0x21, 0x30, 0xd8,
    0x28, 0x14, 0xc, 0x0,

    /* U+0057 "W" */
    0x83, 0x6, 0xc, 0x99, 0x37, 0x6a, 0xf7, 0xcf,
    0x8c,

    /* U+0058 "X" */
    0x43, 0x31, 0xd, 0x82, 0x81, 0x80, 0xe0, 0xd0,
    0x4c, 0x63, 0x60, 0x80,

    /* U+0059 "Y" */
    0x41, 0x31, 0x88, 0x86, 0xc1, 0x40, 0xe0, 0x20,
    0x10, 0x8, 0x4, 0x0,

    /* U+005A "Z" */
    0xfe, 0xc, 0x30, 0x41, 0x86, 0x8, 0x30, 0xc1,
    0xfc,

    /* U+005B "[" */
    0xf8, 0x88, 0x88, 0x88, 0x88, 0x88, 0x8f,

    /* U+005C "\\" */
    0xc0, 0x81, 0x81, 0x2, 0x2, 0x4, 0xc, 0x8,
    0x18, 0x10, 0x20, 0x20,

    /* U+005D "]" */
    0xf1, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1f,

    /* U+005E "^" */
    0x10, 0x51, 0xa2, 0x28, 0x60,

    /* U+005F "_" */
    0xff, 0x80,

    /* U+0060 "`" */
    0x30, 0xc0,

    /* U+0061 "a" */
    0xfa, 0x10, 0x5f, 0x86, 0x18, 0xdd,

    /* U+0062 "b" */
    0x81, 0x2, 0x5, 0xec, 0x50, 0x60, 0xc1, 0x83,
    0xb, 0xe0,

    /* U+0063 "c" */
    0x3d, 0x8, 0x20, 0x82, 0x4, 0xf,

    /* U+0064 "d" */
    0x4, 0x10, 0x4f, 0x46, 0x18, 0x61, 0x87, 0x37,
    0x40,

    /* U+0065 "e" */
    0x3c, 0x8e, 0x1f, 0xf8, 0x10, 0x10, 0x1f,

    /* U+0066 "f" */
    0x7, 0x8, 0x8, 0x8, 0x7f, 0x8, 0x8, 0x8,
    0x8, 0x8, 0x8,

    /* U+0067 "g" */
    0x3e, 0x89, 0x12, 0x27, 0x88, 0x10, 0x3e, 0x83,
    0x5, 0xf0,

    /* U+0068 "h" */
    0x82, 0x8, 0x2e, 0xc6, 0x18, 0x61, 0x86, 0x18,
    0x40,

    /* U+0069 "i" */
    0x30, 0xc0, 0x3c, 0x10, 0x41, 0x4, 0x10, 0x4f,
    0xc0,

    /* U+006A "j" */
    0xc, 0x30, 0x3e, 0x8, 0x20, 0x82, 0x8, 0x20,
    0x82, 0x1b, 0xc0,

    /* U+006B "k" */
    0x81, 0x2, 0x4, 0x69, 0x96, 0x38, 0x50, 0x91,
    0x32, 0x30,

    /* U+006C "l" */
    0xf0, 0x41, 0x4, 0x10, 0x41, 0x4, 0x10, 0x4f,
    0xc0,

    /* U+006D "m" */
    0xf7, 0xb6, 0x4c, 0x99, 0x32, 0x64, 0xc9,

    /* U+006E "n" */
    0xbb, 0x18, 0x61, 0x86, 0x18, 0x61,

    /* U+006F "o" */
    0x38, 0x8a, 0xc, 0x18, 0x30, 0x51, 0x1c,

    /* U+0070 "p" */
    0xbd, 0x8a, 0xc, 0x18, 0x30, 0x61, 0x7c, 0x81,
    0x2, 0x0,

    /* U+0071 "q" */
    0x3d, 0x18, 0x61, 0x86, 0x1c, 0xdd, 0x4, 0x10,
    0x40,

    /* U+0072 "r" */
    0xbb, 0x18, 0x60, 0x82, 0x8, 0x20,

    /* U+0073 "s" */
    0x7d, 0x4, 0x1c, 0x1c, 0x10, 0x7e,

    /* U+0074 "t" */
    0x0, 0x40, 0x87, 0xf2, 0x4, 0x8, 0x10, 0x20,
    0x40, 0x78,

    /* U+0075 "u" */
    0x86, 0x18, 0x61, 0x86, 0x18, 0xdd,

    /* U+0076 "v" */
    0x83, 0x8f, 0x12, 0x66, 0xc5, 0xe, 0x18,

    /* U+0077 "w" */
    0xc1, 0x60, 0x92, 0x4b, 0x25, 0x52, 0xa9, 0xdc,
    0xc4,

    /* U+0078 "x" */
    0xc6, 0x89, 0xa1, 0xc3, 0x8d, 0x13, 0x63,

    /* U+0079 "y" */
    0xc3, 0x42, 0x66, 0x64, 0x24, 0x3c, 0x18, 0x18,
    0x10, 0x30, 0xe0,

    /* U+007A "z" */
    0xfc, 0x31, 0x84, 0x21, 0x8c, 0x3f,

    /* U+007B "{" */
    0x1c, 0x82, 0x8, 0x20, 0x8c, 0x8, 0x20, 0x82,
    0x8, 0x20, 0x70,

    /* U+007C "|" */
    0xff, 0xff,

    /* U+007D "}" */
    0xe0, 0x41, 0x4, 0x10, 0x40, 0xc4, 0x10, 0x41,
    0x4, 0x13, 0x80,

    /* U+007E "~" */
    0x71, 0x99, 0x8e
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 141, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 141, .box_w = 2, .box_h = 11, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 141, .box_w = 4, .box_h = 3, .ofs_x = 2, .ofs_y = 8},
    {.bitmap_index = 6, .adv_w = 141, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 16, .adv_w = 141, .box_w = 7, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 29, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 40, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 51, .adv_w = 141, .box_w = 1, .box_h = 3, .ofs_x = 4, .ofs_y = 8},
    {.bitmap_index = 52, .adv_w = 141, .box_w = 4, .box_h = 15, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 60, .adv_w = 141, .box_w = 4, .box_h = 15, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 68, .adv_w = 141, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 74, .adv_w = 141, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 81, .adv_w = 141, .box_w = 4, .box_h = 5, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 84, .adv_w = 141, .box_w = 5, .box_h = 1, .ofs_x = 2, .ofs_y = 4},
    {.bitmap_index = 85, .adv_w = 141, .box_w = 3, .box_h = 2, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 141, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 98, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 107, .adv_w = 141, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 133, .adv_w = 141, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 143, .adv_w = 141, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 178, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 141, .box_w = 3, .box_h = 8, .ofs_x = 3, .ofs_y = 0},
    {.bitmap_index = 190, .adv_w = 141, .box_w = 4, .box_h = 10, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 195, .adv_w = 141, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 202, .adv_w = 141, .box_w = 7, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 206, .adv_w = 141, .box_w = 6, .box_h = 9, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 213, .adv_w = 141, .box_w = 5, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 141, .box_w = 9, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 236, .adv_w = 141, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 141, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 256, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 141, .box_w = 5, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 281, .adv_w = 141, .box_w = 5, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 297, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 141, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 314, .adv_w = 141, .box_w = 5, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 321, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 141, .box_w = 5, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 337, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 346, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 355, .adv_w = 141, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 365, .adv_w = 141, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 373, .adv_w = 141, .box_w = 8, .box_h = 13, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 386, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 395, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 404, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 413, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 422, .adv_w = 141, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 434, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 141, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 455, .adv_w = 141, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 467, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 476, .adv_w = 141, .box_w = 4, .box_h = 14, .ofs_x = 3, .ofs_y = -3},
    {.bitmap_index = 483, .adv_w = 141, .box_w = 7, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 495, .adv_w = 141, .box_w = 4, .box_h = 14, .ofs_x = 2, .ofs_y = -3},
    {.bitmap_index = 502, .adv_w = 141, .box_w = 7, .box_h = 5, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 507, .adv_w = 141, .box_w = 9, .box_h = 1, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 509, .adv_w = 141, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 511, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 517, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 527, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 533, .adv_w = 141, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 542, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 549, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 560, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 570, .adv_w = 141, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 579, .adv_w = 141, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 588, .adv_w = 141, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 599, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 609, .adv_w = 141, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 625, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 631, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 638, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 648, .adv_w = 141, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 657, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 669, .adv_w = 141, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 679, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 685, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 692, .adv_w = 141, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 701, .adv_w = 141, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 708, .adv_w = 141, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 719, .adv_w = 141, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 725, .adv_w = 141, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 736, .adv_w = 141, .box_w = 1, .box_h = 16, .ofs_x = 4, .ofs_y = -3},
    {.bitmap_index = 738, .adv_w = 141, .box_w = 6, .box_h = 14, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 749, .adv_w = 141, .box_w = 8, .box_h = 3, .ofs_x = 1, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_Font1 = {
#else
lv_font_t ui_font_Font1 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_FONT1*/

