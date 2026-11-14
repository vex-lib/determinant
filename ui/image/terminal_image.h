#ifndef DETERMINANT_TERMINAL_IMAGE_H
#define DETERMINANT_TERMINAL_IMAGE_H

#include "vex.h"
#include "ui/surface/terminal_canvas.h"
#include "ui/surface/terminal_projection.h"
#include "format/terminal_color.h"

// ui/image/terminal_image.h — turn a picture into cells.
//
// A TerminalImage borrows raw RGBA8 pixels and rasterises them into a
// TerminalCanvas region: each output cell samples a box of source pixels,
// reduces them to a luminance, and picks a glyph from a character RAMP
// (low→high, e.g. " .:-=+*#%@"). The drawn cell carries the image's style flags
// (DIM by default), so the same picture can render crisp or ghost-faint.
//
// This is the general "image → canvas" capability. It owns NO memory and does
// NO decoding: the caller supplies decoded pixels (a PNG/BMP decoder, a
// framebuffer, a graphvex Image, or a small constant art table all work). It
// never names a terminal, never writes to a device, and never allocates.
//
// An application's badge or wallpaper is exactly this: an app-owned image
// rendered through the toolkit. The toolkit ships no art of its own.

#define TERMINAL_IMAGE_RAMP_DEFAULT " .:-=+*#%@"

typedef struct TerminalImage {
    const u8 *pixels;  // borrowed RGBA8, row-major: width*height*4 bytes
    bytes width;           // source pixel columns
    bytes height;          // source pixel rows
    const char *ramp;       // glyph ladder low→high (default " .:-=+*#%@")
    u8 style;          // style flags applied to drawn cells (default DIM)
    bool color;             // carry the sampled colour as the cell foreground
    // Colour treatment: when colorRamp is set the luminance indexes a PALETTE
    // (a mono mask renders as a coloured mix); `jitter` adds a deterministic
    // per-cell shade variation so the letters do not read as one flat tone.
    const u32 *colorRamp;   // low→high packed 0xRRGGBBAA (nullable)
    bytes colorRampCount;
    u8 jitter;              // 0..255 per-channel variation (0 = flat)
    u32 jitterSeed;         // reproducible scatter
    // Palette SCATTER: instead of following luminance, each cell's palette index
    // is offset by a value drawn from [0, paletteScatter) and wrapped, so light
    // and dark shades of the palette interleave across the picture. 0 = off
    // (luminance order); the ramp's length scatters over the whole palette.
    u8 paletteScatter;
    bool skipTransparent;   // leave alpha==0 cells untouched (default true)
    bool invert;            // treat bright as low luminance (default false)
    d64 cellAspect;      // cell height / width (default 2.0): corrects a
                            // CONTAIN fit so a square source reads square in cells
    // Background NOISE (decor): a glyph field that covers the WHOLE region the
    // image draws into — edge to edge, sized by the region, never by the picture
    // — with the picture's own content painted over it. Every glyph comes from
    // `noise` and is chosen deterministically from (cell, noiseSeed), so
    // advancing the seed (e.g. on every revalidate) re-scatters the decor while a
    // fixed seed stays reproducible. NULL disables the texture.
    const char *noise;      // background glyph alphabet (nullable)
    u32 noiseSeed;     // scatter seed; caller advances it to reshuffle
    u32 noiseColor;    // packed 0xRRGGBBAA for the noise (DEFAULT = image's own)
    u8 noiseStyle;     // style flags for the noise cells
    u8 noiseDensity;   // 0..100: percent of the field that is covered
                            // (default 100). Lower reads as a texture behind the
                            // picture instead of a wall in front of it.
} TerminalImage;
typedef struct TerminalImageColorRamp { const u32 *data; bytes count; } TerminalImageColorRamp;

TerminalImage TerminalImage_0();
TerminalImage TerminalImage_3(const u8 *pixels, bytes width, bytes height);
#define TerminalImage(...) CONSTRUCTOR_DISPATCH(TerminalImage, __VA_ARGS__)
#define TerminalImage_zero() TerminalImage_0()

bool        TerminalImage_setPixels(TerminalImage *self, const u8 *pixels, bytes width, bytes height);
const u8 *TerminalImage_getPixels(const TerminalImage *self);
bytes      TerminalImage_getWidth(const TerminalImage *self);
bytes      TerminalImage_getHeight(const TerminalImage *self);
bool        TerminalImage_isValid(const TerminalImage *self);
bool        TerminalImage_setRamp(TerminalImage *self, const char *ramp);
const char *TerminalImage_getRamp(const TerminalImage *self);
fn        TerminalImage_setStyle(TerminalImage *self, u8 style);
u8     TerminalImage_getStyle(const TerminalImage *self);
// When true (default) each drawn cell carries its sampled colour as the
// foreground (packed 0xRRGGBBAA); when false the image is monochrome shading.
fn        TerminalImage_setColor(TerminalImage *self, bool color);
bool        TerminalImage_hasColor(const TerminalImage *self);
// Palette treatment: luminance indexes `ramp` (low→high). NULL clears it.
bool        TerminalImage_setColorRamp(TerminalImage *self, const u32 *ramp, bytes count);
TerminalImageColorRamp TerminalImage_getColorRamp(const TerminalImage *self);
// Deterministic per-cell shade scatter (0 = flat). Seed makes it reproducible.
fn        TerminalImage_setJitter(TerminalImage *self, u8 jitter, u32 seed);
u8     TerminalImage_getJitter(const TerminalImage *self);
u32    TerminalImage_getJitterSeed(const TerminalImage *self);
// Palette index scatter: 0 keeps luminance order; N > 0 offsets each cell's
// palette index by [0, N) (wrapped), so which parts read light and which read
// dark is randomized per cell instead of following the gradient.
fn        TerminalImage_setPaletteScatter(TerminalImage *self, u8 steps);
u8     TerminalImage_getPaletteScatter(const TerminalImage *self);
// Named ramps: a purple mix for a mono mask (the `>w<` badge look).
TerminalImageColorRamp TerminalImage_rampPurple();
fn        TerminalImage_setSkipTransparent(TerminalImage *self, bool skip);
bool        TerminalImage_skipsTransparent(const TerminalImage *self);
fn        TerminalImage_setInvert(TerminalImage *self, bool invert);
bool        TerminalImage_inverted(const TerminalImage *self);
// Cell height ÷ width used by the CONTAIN fit (default 2.0 — a terminal cell is
// about twice as tall as it is wide). Set 1.0 for a square-pixel surface.
fn        TerminalImage_setCellAspect(TerminalImage *self, d64 aspect);
d64      TerminalImage_getCellAspect(const TerminalImage *self);
// Background NOISE: a decorative glyph field where the picture is blank. The
// alphabet is the caller's (the toolkit ships no art); `color` is the packed
// 0xRRGGBBAA it draws in (TERMINAL_COLOR_DEFAULT uses the image's own colour).
// Returns false for a null/empty alphabet (which clears the noise).
bool        TerminalImage_setNoise(TerminalImage *self, const char *glyphs, u32 color);
const char *TerminalImage_getNoise(const TerminalImage *self);
// Advance this to re-scatter the texture — one new value per revalidate gives a
// fresh random look; the same seed reproduces the same scatter exactly.
fn        TerminalImage_setNoiseSeed(TerminalImage *self, u32 seed);
u32    TerminalImage_getNoiseSeed(const TerminalImage *self);
fn        TerminalImage_setNoiseStyle(TerminalImage *self, u8 style);
u8     TerminalImage_getNoiseStyle(const TerminalImage *self);
// How much of the blank area is covered, 0..100 (default 100). Below 100 the
// texture reads as a backdrop, leaving the actual picture legible.
fn        TerminalImage_setNoiseDensity(TerminalImage *self, u8 percent);
u8     TerminalImage_getNoiseDensity(const TerminalImage *self);

// Pure helpers (public so a caller/tests can reason about the mapping).
// Rec. 709 luminance from 8-bit channels, 0..255.
u8 TerminalImage_luminance(u8 r, u8 g, u8 b);
// The glyph this image maps a luminance to.
char TerminalImage_glyph(const TerminalImage *self, u8 luminance);
typedef struct TerminalImageSample {
    bool valid;
    u8 red;
    u8 green;
    u8 blue;
    u8 alpha;
} TerminalImageSample;

// Average the source block covering one output cell; alpha is the max of the
// block so a partially transparent cell still reads as opaque.
TerminalImageSample TerminalImage_sampleCell(const TerminalImage *self, bytes lx, bytes ly,
                                             bytes outWidth, bytes outHeight);

// Rasterise at native scale (1 source pixel = 1 cell) at (x, y), clipped.
fn TerminalImage_render(const TerminalImage *self, TerminalCanvas *canvas, bytes x, bytes y);
// Rasterise to FILL and COVER the whole canvas region (box-downsample when the
// image is larger than the region, nearest when smaller). Deterministic.
fn TerminalImage_renderFit(const TerminalImage *self, TerminalCanvas *canvas);
// Rasterise with a UNIFORM zoom-to-fit: the whole picture is scaled by one
// factor that makes it fit inside the region and is centred, so its aspect and
// orientation never change (a portrait stays portrait, a wide stays wide — it is
// never squashed to the panel). This is the one for a badge or a logo.
fn TerminalImage_renderContain(const TerminalImage *self, TerminalCanvas *canvas);

TerminalProjection TerminalImage_toString(const TerminalImage *self);
TerminalProjection TerminalImage_toStringStruct(const TerminalImage *self);

#endif
