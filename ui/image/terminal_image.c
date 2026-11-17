#include "vex.h"
#include "ui/image/terminal_image.h"
#include "format/terminal_style.h"
#include <string.h>
#include <stdio.h>

;;DEFINITION
/** Image → cells. A TerminalImage borrows decoded RGBA8 pixels and rasterises
 * them into a canvas region: each cell averages a box of source pixels into a
 * luminance, picks a glyph from a character ramp, and stamps the image's style
 * flags. Owns no memory, decodes nothing, names no terminal. An application's
 * badge or wallpaper is just one such image; the toolkit ships no art. */
;;OVERVIEW
/** ========================================================================
 * CLASS: TerminalImage (ui/image/terminal_image.c)
 * STRUCT FIELDS (Mirroring ui/image/terminal_image.h)
 * const u8 *pixels  borrowed RGBA8 row-major source
 * bytes width, height   source pixel extent
 * const char *ramp       glyph ladder low→high (default " .:-=+*#%@")
 * u8 style          style flags on drawn cells (default DIM)
 * bool color             carry the sampled colour as foreground (default true)
 * const u32 *colorRamp; bytes colorRampCount  palette treatment (nullable)
 * u8 jitter; u32 jitterSeed  deterministic per-cell shade scatter
 * bool skipTransparent   leave alpha==0 cells untouched
 * bool invert            bright = low luminance
 * FUNCTION REGISTRY (exports)
 * Constructors: TerminalImage_0(), TerminalImage_3(pixels, width, height)
 * Config: setPixels/getPixels, set/getRamp, set/getStyle,
 *   setColor/hasColor, setColorRamp/getColorRamp, setJitter/getJitter,
 *   rampPurple, setSkipTransparent/skipsTransparent, setInvert/inverted
 * Queries: bool TerminalImage_isValid, getWidth/getHeight
 * Mapping: u8 TerminalImage_luminance(r,g,b.sh)
 *   char TerminalImage_glyph(self, luminance)
 *   TerminalImageSample TerminalImage_sampleCell(self, lx, ly, outW, outH)
 * Render: TerminalImage_render(self, canvas, x, y)      (native 1:1)
 *   TerminalImage_renderFit(self, canvas)               (cover the region)
 * Projections: TerminalProjection TerminalImage_toString/toStringStruct(self)
 * CONTRACT: an invalid image (null/zero pixels or extent) draws nothing; a
 * transparent cell with skipTransparent leaves the canvas untouched; render
 * clips and never allocates or logs. Sampling is pure and deterministic.
 * ======================================================================== */


;;GETTER
static inline d64 TerminalImage_cellAspect(const TerminalImage *s) { return (*s).cellAspect; }
;;GETTER
static inline d64 *TerminalImage_cellAspectMut(TerminalImage *s) { return &(*s).cellAspect; }
;;SETTER
static inline fn TerminalImage_setCellAspect(TerminalImage *s, d64 v) { (*s).cellAspect = v; }
;;GETTER
static inline bool TerminalImage_color(const TerminalImage *s) { return (*s).color; }
;;GETTER
static inline bool *TerminalImage_colorMut(TerminalImage *s) { return &(*s).color; }
;;SETTER
static inline fn TerminalImage_setColor(TerminalImage *s, bool v) { (*s).color = v; }
;;GETTER
static inline const u32 * TerminalImage_colorRamp(const TerminalImage *s) { return (*s).colorRamp; }
;;GETTER
static inline const u32 * *TerminalImage_colorRampMut(TerminalImage *s) { return &(*s).colorRamp; }
;;SETTER
static inline fn TerminalImage_setColorRamp(TerminalImage *s, const u32 * v) { (*s).colorRamp = v; }
;;GETTER
static inline bytes TerminalImage_colorRampCount(const TerminalImage *s) { return (*s).colorRampCount; }
;;GETTER
static inline bytes *TerminalImage_colorRampCountMut(TerminalImage *s) { return &(*s).colorRampCount; }
;;SETTER
static inline fn TerminalImage_setColorRampCount(TerminalImage *s, bytes v) { (*s).colorRampCount = v; }
;;GETTER
static inline bytes TerminalImage_height(const TerminalImage *s) { return (*s).height; }
;;GETTER
static inline bytes *TerminalImage_heightMut(TerminalImage *s) { return &(*s).height; }
;;SETTER
static inline fn TerminalImage_setHeight(TerminalImage *s, bytes v) { (*s).height = v; }
;;GETTER
static inline bool TerminalImage_invert(const TerminalImage *s) { return (*s).invert; }
;;GETTER
static inline bool *TerminalImage_invertMut(TerminalImage *s) { return &(*s).invert; }
;;SETTER
static inline fn TerminalImage_setInvert(TerminalImage *s, bool v) { (*s).invert = v; }
;;GETTER
static inline u8 TerminalImage_jitter(const TerminalImage *s) { return (*s).jitter; }
;;GETTER
static inline u8 *TerminalImage_jitterMut(TerminalImage *s) { return &(*s).jitter; }
;;SETTER
static inline fn TerminalImage_setJitter(TerminalImage *s, u8 v) { (*s).jitter = v; }
;;GETTER
static inline u32 TerminalImage_jitterSeed(const TerminalImage *s) { return (*s).jitterSeed; }
;;GETTER
static inline u32 *TerminalImage_jitterSeedMut(TerminalImage *s) { return &(*s).jitterSeed; }
;;SETTER
static inline fn TerminalImage_setJitterSeed(TerminalImage *s, u32 v) { (*s).jitterSeed = v; }
;;GETTER
static inline const char * TerminalImage_noise(const TerminalImage *s) { return (*s).noise; }
;;GETTER
static inline const char * *TerminalImage_noiseMut(TerminalImage *s) { return &(*s).noise; }
;;SETTER
static inline fn TerminalImage_setNoise(TerminalImage *s, const char * v) { (*s).noise = v; }
;;GETTER
static inline u32 TerminalImage_noiseColor(const TerminalImage *s) { return (*s).noiseColor; }
;;GETTER
static inline u32 *TerminalImage_noiseColorMut(TerminalImage *s) { return &(*s).noiseColor; }
;;SETTER
static inline fn TerminalImage_setNoiseColor(TerminalImage *s, u32 v) { (*s).noiseColor = v; }
;;GETTER
static inline u8 TerminalImage_noiseDensity(const TerminalImage *s) { return (*s).noiseDensity; }
;;GETTER
static inline u8 *TerminalImage_noiseDensityMut(TerminalImage *s) { return &(*s).noiseDensity; }
;;SETTER
static inline fn TerminalImage_setNoiseDensity(TerminalImage *s, u8 v) { (*s).noiseDensity = v; }
;;GETTER
static inline u32 TerminalImage_noiseSeed(const TerminalImage *s) { return (*s).noiseSeed; }
;;GETTER
static inline u32 *TerminalImage_noiseSeedMut(TerminalImage *s) { return &(*s).noiseSeed; }
;;SETTER
static inline fn TerminalImage_setNoiseSeed(TerminalImage *s, u32 v) { (*s).noiseSeed = v; }
;;GETTER
static inline u8 TerminalImage_noiseStyle(const TerminalImage *s) { return (*s).noiseStyle; }
;;GETTER
static inline u8 *TerminalImage_noiseStyleMut(TerminalImage *s) { return &(*s).noiseStyle; }
;;SETTER
static inline fn TerminalImage_setNoiseStyle(TerminalImage *s, u8 v) { (*s).noiseStyle = v; }
;;GETTER
static inline u8 TerminalImage_paletteScatter(const TerminalImage *s) { return (*s).paletteScatter; }
;;GETTER
static inline u8 *TerminalImage_paletteScatterMut(TerminalImage *s) { return &(*s).paletteScatter; }
;;SETTER
static inline fn TerminalImage_setPaletteScatter(TerminalImage *s, u8 v) { (*s).paletteScatter = v; }
;;GETTER
static inline const u8 * TerminalImage_pixels(const TerminalImage *s) { return (*s).pixels; }
;;GETTER
static inline const u8 * *TerminalImage_pixelsMut(TerminalImage *s) { return &(*s).pixels; }
;;SETTER
static inline fn TerminalImage_setPixels(TerminalImage *s, const u8 * v) { (*s).pixels = v; }
;;GETTER
static inline const char * TerminalImage_ramp(const TerminalImage *s) { return (*s).ramp; }
;;GETTER
static inline const char * *TerminalImage_rampMut(TerminalImage *s) { return &(*s).ramp; }
;;SETTER
static inline fn TerminalImage_setRamp(TerminalImage *s, const char * v) { (*s).ramp = v; }
;;GETTER
static inline bool TerminalImage_skipTransparent(const TerminalImage *s) { return (*s).skipTransparent; }
;;GETTER
static inline bool *TerminalImage_skipTransparentMut(TerminalImage *s) { return &(*s).skipTransparent; }
;;SETTER
static inline fn TerminalImage_setSkipTransparent(TerminalImage *s, bool v) { (*s).skipTransparent = v; }
;;GETTER
static inline u8 TerminalImage_style(const TerminalImage *s) { return (*s).style; }
;;GETTER
static inline u8 *TerminalImage_styleMut(TerminalImage *s) { return &(*s).style; }
;;SETTER
static inline fn TerminalImage_setStyle(TerminalImage *s, u8 v) { (*s).style = v; }
;;GETTER
static inline bytes TerminalImage_width(const TerminalImage *s) { return (*s).width; }
;;GETTER
static inline bytes *TerminalImage_widthMut(TerminalImage *s) { return &(*s).width; }
;;SETTER
static inline fn TerminalImage_setWidth(TerminalImage *s, bytes v) { (*s).width = v; }

TerminalImage TerminalImage_0() {
    TerminalImage image = {0};
    image.ramp = TERMINAL_IMAGE_RAMP_DEFAULT;
    image.style = TERMINAL_STYLE_DIM;
    image.color = true;
    image.skipTransparent = true;
    image.cellAspect = 2.0;   // a terminal cell is ~2x taller than wide
    image.noiseDensity = 100; // full cover until the caller thins it
    return image;
}

TerminalImage TerminalImage_3(const u8 *pixels, bytes width, bytes height) {
    TerminalImage image = TerminalImage_0();
    image.pixels = (width != 0 && height != 0) ? pixels : nullptr;
    image.width = (pixels != nullptr) ? width : 0;
    image.height = (pixels != nullptr) ? height : 0;
    return image;
}

bool TerminalImage_setPixels(TerminalImage *self, const u8 *pixels, bytes width, bytes height) {
    if (self == nullptr || pixels == nullptr || width == 0 || height == 0) {
        THROW("TerminalImage pixels: null or zero extent");
        return false;
    }
    TerminalImage_setPixels(self, pixels);
    TerminalImage_setWidth(self, width);
    TerminalImage_setHeight(self, height);
    return true;
}

const u8 *TerminalImage_getPixels(const TerminalImage *self) { return self ? TerminalImage_pixels(self) : nullptr; }
bytes TerminalImage_getWidth(const TerminalImage *self) { return self ? TerminalImage_width(self) : 0; }
bytes TerminalImage_getHeight(const TerminalImage *self) { return self ? TerminalImage_height(self) : 0; }
bool TerminalImage_isValid(const TerminalImage *self) {
    return self != nullptr && TerminalImage_pixels(self) != nullptr && TerminalImage_width(self) > 0 && TerminalImage_height(self) > 0;
}

bool TerminalImage_setRamp(TerminalImage *self, const char *ramp) {
    if (self == nullptr || ramp == nullptr || ramp[0] == '\0') {
        THROW("TerminalImage ramp: null or empty");
        return false;
    }
    TerminalImage_setRamp(self, ramp);
    return true;
}
const char *TerminalImage_getRamp(const TerminalImage *self) {
    return (self && TerminalImage_ramp(self)) ? TerminalImage_ramp(self) : TERMINAL_IMAGE_RAMP_DEFAULT;
}
fn TerminalImage_setStyle(TerminalImage *self, u8 style) { if (self != nullptr) (*TerminalImage_styleMut(self)) = style; }
u8 TerminalImage_getStyle(const TerminalImage *self) { return self ? TerminalImage_style(self) : TERMINAL_STYLE_NONE; }
fn TerminalImage_setColor(TerminalImage *self, bool color) { if (self != nullptr) (*TerminalImage_colorMut(self)) = color; }
bool TerminalImage_hasColor(const TerminalImage *self) { return self && TerminalImage_color(self); }

bool TerminalImage_setColorRamp(TerminalImage *self, const u32 *ramp, bytes count) {
    if (self == nullptr || (count != 0 && ramp == nullptr)) {
        THROW("TerminalImage color ramp: null");
        return false;
    }
    TerminalImage_setColorRamp(self, (count != 0) ? ramp : nullptr);
    TerminalImage_setColorRampCount(self, (count != 0) ? count : 0);
    return true;
}
TerminalImageColorRamp TerminalImage_getColorRamp(const TerminalImage *self) {
    return self ? (TerminalImageColorRamp) { TerminalImage_colorRamp(self), TerminalImage_colorRampCount(self) }
                : (TerminalImageColorRamp) { nullptr, 0u };
}
fn TerminalImage_setJitter(TerminalImage *self, u8 jitter, u32 seed) {
    if (self == nullptr) return;
    TerminalImage_setJitter(self, jitter);
    TerminalImage_setJitterSeed(self, seed);
}
u8 TerminalImage_getJitter(const TerminalImage *self) { return self ? TerminalImage_jitter(self) : 0; }
u32 TerminalImage_getJitterSeed(const TerminalImage *self) { return self ? TerminalImage_jitterSeed(self) : 0; }
fn TerminalImage_setPaletteScatter(TerminalImage *self, u8 steps) { if (self != nullptr) (*TerminalImage_paletteScatterMut(self)) = steps; }
u8 TerminalImage_getPaletteScatter(const TerminalImage *self) { return self ? TerminalImage_paletteScatter(self) : 0; }

// A purple ladder for a light terminal: the darkest entry sits at ~30% of full
// range (0x4C ≈ 255 * 0.3) and the rest climb to near-white. Which cell takes
// which entry is the caller's choice — luminance order, or scattered (see
// paletteScatter) so light and dark letters interleave at random.
static const u32 PURPLE_RAMP[] = {
    TERMINAL_RGB(0x5A, 0x4C, 0x78),
    TERMINAL_RGB(0x6E, 0x5A, 0x90),
    TERMINAL_RGB(0x82, 0x6B, 0xA8),
    TERMINAL_RGB(0x9A, 0x7E, 0xC0),
    TERMINAL_RGB(0xB2, 0x92, 0xD4),
    TERMINAL_RGB(0xCA, 0xAA, 0xE6),
    TERMINAL_RGB(0xE0, 0xC6, 0xF4),
    TERMINAL_RGB(0xF2, 0xE6, 0xFF),
};
TerminalImageColorRamp TerminalImage_rampPurple() {
    return (TerminalImageColorRamp) { PURPLE_RAMP, sizeof(PURPLE_RAMP) / sizeof(PURPLE_RAMP[0]) };
}

static u8 shiftChannel(u8 value, i32 delta) {
    i32 shifted = (i32) value + delta;
    if (shifted < 0) shifted = 0;
    if (shifted > 255) shifted = 255;
    return (u8) shifted;
}

// The colour one cell draws with: palette (with deterministic jitter) or the
// sampled colour, or none.
static u32 imageColor(const TerminalImage *self, bytes lx, bytes ly,
                           u8 r, u8 g, u8 b) {
    if (TerminalImage_colorRamp(self) != nullptr && TerminalImage_colorRampCount(self) > 0) {
        u8 lum = TerminalImage_luminance(r, g, b);
        bytes index = (bytes) lum * TerminalImage_colorRampCount(self) / 256u;
        if (index >= TerminalImage_colorRampCount(self))
            index = TerminalImage_colorRampCount(self) - 1;
        u32 hash = (u32) lx * 73856093u ^ (u32) ly * 19349663u ^ TerminalImage_jitterSeed(self);
        if (TerminalImage_paletteScatter(self) != 0) {
            // Which shade a cell takes is randomized, not luminance-ordered, so
            // light and dark entries interleave across the picture.
            index = (index + (hash >> 8) % TerminalImage_paletteScatter(self)) % TerminalImage_colorRampCount(self);
        }
        u32 colour = TerminalImage_colorRamp(self)[index];
        if (TerminalImage_jitter(self) != 0) {
            // The jitter only ever LIGHTENS a palette colour (delta >= 0), so a
            // light ramp never dips dark on some cells.
            i32 delta = (i32) ((hash >> 8) % ((u32) TerminalImage_jitter(self) + 1u));
            return TERMINAL_RGBA(shiftChannel(TERMINAL_COLOR_RED(colour), delta),
                                 shiftChannel(TERMINAL_COLOR_GREEN(colour), delta),
                                 shiftChannel(TERMINAL_COLOR_BLUE(colour), delta),
                                 TERMINAL_COLOR_ALPHA(colour));
        }
        return colour;
    }
    return TerminalImage_color(self) ? TERMINAL_RGB(r, g, b) : TERMINAL_COLOR_DEFAULT;
}
fn TerminalImage_setSkipTransparent(TerminalImage *self, bool skip) { if (self != nullptr) (*TerminalImage_skipTransparentMut(self)) = skip; }
bool TerminalImage_skipsTransparent(const TerminalImage *self) { return self && TerminalImage_skipTransparent(self); }
fn TerminalImage_setInvert(TerminalImage *self, bool invert) { if (self != nullptr) (*TerminalImage_invertMut(self)) = invert; }
bool TerminalImage_inverted(const TerminalImage *self) { return self && TerminalImage_invert(self); }
fn TerminalImage_setCellAspect(TerminalImage *self, d64 aspect) {
    if (self != nullptr && aspect > 0.0)
        TerminalImage_setCellAspect(self, aspect);
}
d64 TerminalImage_getCellAspect(const TerminalImage *self) {
    return (self && TerminalImage_cellAspect(self) > 0.0) ? TerminalImage_cellAspect(self) : 2.0;
}

bool TerminalImage_setNoise(TerminalImage *self, const char *glyphs, u32 color) {
    if (self == nullptr || glyphs == nullptr || glyphs[0] == '\0') {
        if (self != nullptr) {
            TerminalImage_setNoise(self, nullptr);
            TerminalImage_setNoiseSeed(self, 0);
        }
        return false;
    }
    TerminalImage_setNoise(self, glyphs);
    TerminalImage_setNoiseColor(self, color);
    return true;
}
const char *TerminalImage_getNoise(const TerminalImage *self) { return self ? TerminalImage_noise(self) : nullptr; }
fn TerminalImage_setNoiseSeed(TerminalImage *self, u32 seed) { if (self != nullptr) (*TerminalImage_noiseSeedMut(self)) = seed; }
u32 TerminalImage_getNoiseSeed(const TerminalImage *self) { return self ? TerminalImage_noiseSeed(self) : 0; }
fn TerminalImage_setNoiseStyle(TerminalImage *self, u8 style) { if (self != nullptr) (*TerminalImage_noiseStyleMut(self)) = style; }
u8 TerminalImage_getNoiseStyle(const TerminalImage *self) { return self ? TerminalImage_noiseStyle(self) : 0; }
fn TerminalImage_setNoiseDensity(TerminalImage *self, u8 percent) {
    if (self != nullptr)
        TerminalImage_setNoiseDensity(self, percent > 100 ? 100 : percent);
}
u8 TerminalImage_getNoiseDensity(const TerminalImage *self) {
    return self ? TerminalImage_noiseDensity(self) : 0;
}

// The background NOISE field: fill a whole rectangle with glyphs from the
// caller's alphabet, gated by density and scattered by the seed. This is a
// DECOR FIELD sized by the region (never by the picture), so it fills the panel
// at any terminal size. Deterministic for a fixed seed.
static fn paintNoiseField(const TerminalImage *self, TerminalCanvas *canvas,
                            bytes x0, bytes y0, bytes w, bytes h) {
    const char *noise = TerminalImage_noise(self);
    if (noise == nullptr || TerminalImage_noiseDensity(self) == 0)
        return;
    bytes count = strlen(noise);
    if (count == 0)
        return;
    u32 colour = TerminalImage_noiseColor(self);
    if (colour == TERMINAL_COLOR_DEFAULT)
        colour = TerminalImage_colorRamp(self) != nullptr ? TerminalImage_colorRamp(self)[0] : TERMINAL_COLOR_DEFAULT;
    TerminalPen pen = TerminalPen_2(TerminalImage_noiseStyle(self), colour);
    for (bytes ly = 0; ly < h; ++ly) {
        for (bytes lx = 0; lx < w; ++lx) {
            u32 hash = (u32) (x0 + lx) * 2654435761u
                          ^ (u32) (y0 + ly) * 40503u
                          ^ TerminalImage_noiseSeed(self);
            hash ^= hash >> 13;
            hash *= 2654435761u;
            hash ^= hash >> 15;
            if ((hash % 100u) >= TerminalImage_noiseDensity(self))
                continue;
            char glyph = noise[(hash >> 7) % count];
            TerminalCanvas_text(canvas, x0 + lx, y0 + ly, &glyph, 1, pen);
        }
    }
}

// Paint ONE cell of the picture. When a noise field is set the blank background
// is left alone (the field underneath shows through) and only real content is
// drawn on top.
static fn paintCell(const TerminalImage *self, TerminalCanvas *canvas,
                      bytes cx, bytes cy, bytes lx, bytes ly,
                      u8 r, u8 g, u8 b) {
    char glyph = TerminalImage_glyph(self, TerminalImage_luminance(r, g, b));
    if (TerminalImage_noise(self) != nullptr) {
        const char *ramp = TerminalImage_getRamp(self);
        if (glyph == ramp[0])
            return;   // the decor field shows through
    }
    TerminalCanvas_text(canvas, cx, cy, &glyph, 1,
                        TerminalPen_2(TerminalImage_style(self), imageColor(self, lx, ly, r, g, b)));
}

u8 TerminalImage_luminance(u8 r, u8 g, u8 b) {
    // Rec. 709 luma on 8-bit channels, integer arithmetic (deterministic).
    return (u8) ((2126u * r + 7152u * g + 722u * b) / 10000u);
}

char TerminalImage_glyph(const TerminalImage *self, u8 luminance) {
    const char *ramp = TerminalImage_getRamp(self);
    bytes levels = strlen(ramp);
    if (levels == 0)
        return ' ';
    bool invert = TerminalImage_inverted(self);
    bytes index = (bytes) luminance * levels / 256u;
    if (index >= levels)
        index = levels - 1;
    if (invert)
        index = levels - 1 - index;
    return ramp[index];
}

TerminalImageSample TerminalImage_sampleCell(const TerminalImage *self, bytes lx, bytes ly,
                                             bytes outWidth, bytes outHeight) {
    TerminalImageSample sample = {0};
    if (!TerminalImage_isValid(self) || outWidth == 0 || outHeight == 0)
        return sample;
    bytes x0 = lx * TerminalImage_width(self) / outWidth;
    bytes x1 = (lx + 1) * TerminalImage_width(self) / outWidth;
    bytes y0 = ly * TerminalImage_height(self) / outHeight;
    bytes y1 = (ly + 1) * TerminalImage_height(self) / outHeight;
    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    if (x1 > TerminalImage_width(self)) x1 = TerminalImage_width(self);
    if (y1 > TerminalImage_height(self)) y1 = TerminalImage_height(self);
    u64 sr = 0, sg = 0, sb = 0;
    u8 maxA = 0;
    bytes count = 0;
    for (bytes y = y0; y < y1; ++y) {
        for (bytes x = x0; x < x1; ++x) {
            bytes index = (y * TerminalImage_width(self) + x) * 4;
            const u8 *p = TerminalImage_pixels(self) + index;
            sr += p[0]; sg += p[1]; sb += p[2];
            if (p[3] > maxA) maxA = p[3];
            ++count;
        }
    }
    if (count == 0)
        return sample;
    sample.valid = true;
    sample.red = (u8) (sr / count);
    sample.green = (u8) (sg / count);
    sample.blue = (u8) (sb / count);
    sample.alpha = maxA;
    return sample;
}

fn TerminalImage_render(const TerminalImage *self, TerminalCanvas *canvas, bytes x, bytes y) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    bytes cw = TerminalCanvas_getWidth(canvas);
    bytes chh = TerminalCanvas_getHeight(canvas);
    paintNoiseField(self, canvas, x, y, TerminalImage_width(self), TerminalImage_height(self));
    for (bytes ly = 0; ly < TerminalImage_height(self); ++ly) {
        if (y + ly >= chh)
            break;
        for (bytes lx = 0; lx < TerminalImage_width(self); ++lx) {
            if (x + lx >= cw)
                break;
            bytes index = (ly * TerminalImage_width(self) + lx) * 4;
            const u8 *p = TerminalImage_pixels(self) + index;
            if (TerminalImage_skipsTransparent(self) && p[3] == 0)
                continue;
            paintCell(self, canvas, x + lx, y + ly, x + lx, y + ly, p[0], p[1], p[2]);
        }
    }
}

fn TerminalImage_renderFit(const TerminalImage *self, TerminalCanvas *canvas) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    bytes cw = TerminalCanvas_getWidth(canvas);
    bytes chh = TerminalCanvas_getHeight(canvas);
    paintNoiseField(self, canvas, 0, 0, cw, chh);
    for (bytes ly = 0; ly < chh; ++ly) {
        for (bytes lx = 0; lx < cw; ++lx) {
            TerminalImageSample sample = TerminalImage_sampleCell(self, lx, ly, cw, chh);
            if (!sample.valid)
                continue;
            if (TerminalImage_skipsTransparent(self) && sample.alpha == 0)
                continue;
            paintCell(self, canvas, lx, ly, lx, ly, sample.red, sample.green, sample.blue);
        }
    }
}

fn TerminalImage_renderContain(const TerminalImage *self, TerminalCanvas *canvas) {
    if (!TerminalImage_isValid(self) || !TerminalCanvas_isValid(canvas))
        return;
    bytes regionW = TerminalCanvas_getWidth(canvas);
    bytes regionH = TerminalCanvas_getHeight(canvas);
    if (regionW == 0 || regionH == 0)
        return;
    // One scale for both axes, so the picture is never squashed; the cells are
    // ~2x taller than wide, so the vertical extent is corrected by cellAspect.
    d64 aspect = TerminalImage_cellAspect(self) > 0.0 ? TerminalImage_cellAspect(self) : 1.0;
    d64 widthPerHeight = (d64) TerminalImage_width(self) / (d64) TerminalImage_height(self) * aspect;
    bytes destW = regionW;
    bytes destH = (bytes) ((d64) regionW / widthPerHeight + 0.5);
    if (destH > regionH) {
        destH = regionH;
        destW = (bytes) ((d64) regionH * widthPerHeight + 0.5);
    }
    if (destW == 0 || destH == 0)
        return;
    if (destW > regionW) destW = regionW;
    if (destH > regionH) destH = regionH;
    bytes offsetX = (regionW - destW) / 2;
    bytes offsetY = (regionH - destH) / 2;
    // The decor field covers the WHOLE region, not just the picture's fit rect,
    // so the panel is filled edge to edge at any terminal size.
    paintNoiseField(self, canvas, 0, 0, regionW, regionH);
    for (bytes ly = 0; ly < destH; ++ly) {
        for (bytes lx = 0; lx < destW; ++lx) {
            TerminalImageSample sample = TerminalImage_sampleCell(self, lx, ly, destW, destH);
            if (!sample.valid)
                continue;
            if (TerminalImage_skipsTransparent(self) && sample.alpha == 0)
                continue;
            paintCell(self, canvas, offsetX + lx, offsetY + ly, lx, ly,
                      sample.red, sample.green, sample.blue);
        }
    }
}

TerminalProjection TerminalImage_toString(const TerminalImage *self) {
    return TerminalImage_toStringStruct(self);
}

TerminalProjection TerminalImage_toStringStruct(const TerminalImage *self) {
    if (self == nullptr)
        return TerminalProjection_format("nullptr");
    return TerminalProjection_format(
        "TerminalImage{%zux%zu,ramp=\"%s\",style=0x%02X,color=%d,palette=%zu,jitter=%u,skipTransparent=%d,invert=%d,valid=%d}",
        TerminalImage_width(self), TerminalImage_height(self), TerminalImage_getRamp(self), TerminalImage_style(self),
        TerminalImage_color(self), TerminalImage_colorRampCount(self), TerminalImage_jitter(self), TerminalImage_skipTransparent(self), TerminalImage_invert(self),
        TerminalImage_isValid(self));
}
