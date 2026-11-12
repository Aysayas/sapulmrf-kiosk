# Builds the kiosk's LVGL fonts from Atkinson Hyperlegible Next (SIL Open Font License, the same family as
# the SapulMRF website). Latin-1 covers English and Tagalog (e.g. "Niño"). LVGL symbols (Wi-Fi, battery,
# check mark...) fall back to the built-in Montserrat of a similar size.
#
# Run from the repo:   powershell -File sapulmrf-kiosk\scripts\make_fonts.ps1
# Needs Node.js (npx). Output: SapulMRF_Kiosk\src\fonts\atk_*.c (committed, so building doesn't need Node).

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root "SapulMRF_Kiosk\src\fonts"
$tmp = Join-Path ([IO.Path]::GetTempPath()) "kiosk-fonts"
New-Item -ItemType Directory -Force $out, $tmp | Out-Null

Push-Location $tmp
npm pack @fontsource/atkinson-hyperlegible-next@5.3.0 --silent | Out-Null
tar -xzf fontsource-atkinson-hyperlegible-next-5.3.0.tgz
Pop-Location
$files = Join-Path $tmp "package\files"
function Font($w) { Join-Path $files "atkinson-hyperlegible-next-latin-$w-normal.woff" }

$text = "0x20-0x7E,0xA0-0xFF"          # printable ASCII + Latin-1 (ñ, é, ·, °, ±, ×)
$digits = "0x20,0x2D-0x39,0x44,0x4B,0x67,0x6B"   # " -./0-9 D K g k": weights and receipt codes

# name, weight, size, characters, fallback (a built-in Montserrat enabled in lv_conf.h)
$fonts = @(
  @("atk_r18", 400, 18, $text,   "lv_font_montserrat_16"),
  @("atk_r24", 400, 24, $text,   "lv_font_montserrat_22"),
  @("atk_b24", 700, 24, $text,   "lv_font_montserrat_22"),
  @("atk_b32", 700, 32, $text,   "lv_font_montserrat_28"),
  @("atk_b48", 800, 48, $text,   "lv_font_montserrat_40"),
  @("atk_b72", 800, 72, $digits, "lv_font_montserrat_40")
)
foreach ($f in $fonts) {
  $name, $weight, $size, $range, $fallback = $f
  $file = Join-Path $out "$name.c"
  npx --yes lv_font_conv@1.5.3 --font (Font $weight) --range $range --size $size --bpp 4 --format lvgl `
      --lv-font-name $name --lv-fallback $fallback --no-prefilter -o $file
  # One include that works in both PlatformIO and the Arduino IDE; no local paths in the committed header
  $c = (Get-Content $file -Raw) -replace '(?s)#ifdef LV_LVGL_H_INCLUDE_SIMPLE.*?#endif', '#include <lvgl.h>'
  $c = $c.Replace($files, "@fontsource/atkinson-hyperlegible-next/files").Replace($file, "$name.c")
  [IO.File]::WriteAllText($file, $c, (New-Object Text.UTF8Encoding $false))
  Write-Host "made $name ($size px, weight $weight)"
}
