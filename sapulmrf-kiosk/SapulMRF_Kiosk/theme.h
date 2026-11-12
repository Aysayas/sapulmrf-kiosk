#pragma once
// The kiosk's look: a bold public terminal for a bright MRF, read at arm's length.
// Deep leaf green and warm off-white, heavy type, and category colours that are signals, not decoration
// (one meaning everywhere: the scan tiles, the picker, the receipt and the bin to use).
#include <lvgl.h>

// ---- colours
#define T_BG        0xF6F4EE   // warm off-white
#define T_CARD      0xFFFFFF
#define T_INK       0x14201A   // near-black text
#define T_INK2      0x4A554E   // secondary text (5.9:1 on T_BG)
#define T_LINE      0xD9DED6
#define T_BRAND     0x0F3B2A   // deep leaf green
#define T_PRIMARY   0x1E6B47   // primary buttons (white text 6.6:1)
#define T_SOFT      0xDCEFE2   // selected / pressed tint
#define T_OK        0x1E6B47
#define T_WARN      0xA35F00   // amber that still carries white text (4.6:1)
#define T_WARN_BG   0xFBEBD2
#define T_ERR       0xB42318
#define T_VIDEO     0x111814

// Category signal colours, matched to the MRF's bins (see the open question in the plan)
#define T_CAT_MIXED 0xA35F00   // amber
#define T_CAT_RECYC 0x1F5FAF   // blue
#define T_CAT_BIO   0x2E7D32   // green
#define T_CAT_RESID 0x3A3F3C   // charcoal

// ---- type: Atkinson Hyperlegible Next (src/fonts, made by scripts/make_fonts.ps1)
LV_FONT_DECLARE(atk_r18);   // small labels
LV_FONT_DECLARE(atk_r24);   // body text
LV_FONT_DECLARE(atk_b24);   // buttons, emphasis
LV_FONT_DECLARE(atk_b32);   // titles
LV_FONT_DECLARE(atk_b48);   // greeting, big numbers
LV_FONT_DECLARE(atk_b72);   // weight and receipt code (digits, '.', '-', D, K, g, k only)

// ---- layout (800 x 480): a 56 px status strip on top, 24-32 px margins, touch targets at least 64-72 px
#define T_TOP     56
#define T_MARGIN  32
#define T_TAP     64
