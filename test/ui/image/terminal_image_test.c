// Owner: TerminalImage — picture → cells. Luminance/glyph mapping, box sample,
// 1:1 render, region-fit render, transparency skip, invert, ramp, projections.
#include "ui/image/terminal_image.h"
#include "determinant/test/support.h"

int main(void) {
    // 2x1 image: black then white.
    static const u8 two[2 * 1 * 4] = {
        0, 0, 0, 255,
        255, 255, 255, 255,
    };
    TerminalImage img = TerminalImage(two, 2, 1);
    assert(TerminalImage_isValid(&img));
    assert(TerminalImage_getWidth(&img) == 2 && TerminalImage_getHeight(&img) == 1);
    assert(TerminalImage_luminance(0, 0, 0) == 0);
    assert(TerminalImage_luminance(255, 255, 255) == 255);

    assert(TerminalImage_glyph(&img, 0) == ' ');
    assert(TerminalImage_glyph(&img, 255) == '@');
    TerminalImage_setInvert(&img, true);
    assert(TerminalImage_glyph(&img, 0) == '@');
    assert(TerminalImage_glyph(&img, 255) == ' ');
    TerminalImage_setInvert(&img, false);

    // 1:1 render into cells.
    u32 cells[4] = {0, 0, 0, 0};
    TerminalCanvas c = TerminalCanvas(cells, nullptr, 2, 2);
    TerminalImage_render(&img, &c, 0, 0);
    assert(cells[0] == ' ' && cells[1] == '@');

    // Transparent pixels are skipped when requested.
    static const u8 clear[1 * 4] = { 255, 255, 255, 0 };
    TerminalImage ghost = TerminalImage(clear, 1, 1);
    u32 keep[1] = {'.'};
    TerminalCanvas k = TerminalCanvas(keep, nullptr, 1, 1);
    TerminalImage_render(&ghost, &k, 0, 0);
    assert(keep[0] == '.');
    TerminalImage_setSkipTransparent(&ghost, false);
    TerminalImage_render(&ghost, &k, 0, 0);
    assert(keep[0] == '@');

    // Box sampling: a 2x2 checker averaged into one cell lands mid-luminance.
    static const u8 checker[4 * 4] = {
        0, 0, 0, 255,   255, 255, 255, 255,
        255, 255, 255, 255,   0, 0, 0, 255,
    };
    TerminalImage box = TerminalImage(checker, 2, 2);
    TerminalImageSample sample = TerminalImage_sampleCell(&box, 0, 0, 1, 1);
    assert(sample.valid && sample.red == 127 && sample.green == 127 &&
           sample.blue == 127 && sample.alpha == 255);
    assert(!TerminalImage_sampleCell(&box, 5, 0, 1, 1).valid);

    // renderFit covers a whole region deterministically.
    u32 big[6 * 2];
    u16 bigStyles[6 * 2];
    TerminalCanvas region = TerminalCanvas(big, bigStyles, 6, 2);
    TerminalImage_setStyle(&img, TERMINAL_STYLE_DIM);
    TerminalImage_renderFit(&img, &region);
    assert(big[0] == ' ' && big[5] == '@');
    assert(TerminalCanvas_styleAt(&region, 0, 0) == TERMINAL_STYLE_DIM);

    // Colour plane: cells carry the sampled colour when color is on.
    static u32 cc[4];
    static u32 cf[4];
    TerminalCanvas colored = TerminalCanvas(cc, nullptr, cf, nullptr, 2, 2);
    TerminalImage_render(&img, &colored, 0, 0);
    assert(TerminalCanvas_fgAt(&colored, 0, 0) == TERMINAL_RGB(0, 0, 0));
    assert(TerminalCanvas_fgAt(&colored, 1, 0) == TERMINAL_RGB(255, 255, 255));
    assert(TerminalImage_hasColor(&img));
    TerminalImage_setColor(&img, false);
    assert(!TerminalImage_hasColor(&img));
    TerminalImage_render(&img, &colored, 0, 0);
    assert(TerminalCanvas_fgAt(&colored, 1, 0) == TERMINAL_COLOR_DEFAULT);
    TerminalImage_setColor(&img, true);

    // Background NOISE: a decor field covering the WHOLE region the picture draws
    // into — edge to edge, sized by the region rather than the picture — with the
    // picture's own content painted over it. Re-seeded per render → fresh decor.
    {
        static u32 noiseCells[6 * 4];
        static u32 noiseFg[6 * 4];
        TerminalCanvas noisyCanvas = TerminalCanvas(noiseCells, nullptr, noiseFg, nullptr, 6, 4);
        TerminalImage noisy = TerminalImage(two, 2, 1);
        TerminalImage_setInvert(&noisy, true);
        assert(!TerminalImage_setNoise(&noisy, "", 0));            // empty clears
        assert(TerminalImage_getNoise(&noisy) == nullptr);
        assert(!TerminalImage_setNoise(&noisy, nullptr, 0));
        assert(TerminalImage_setNoise(&noisy, "abcxyz", TERMINAL_RGB(0x80, 0x80, 0xA0)));
        assert(TerminalImage_getNoise(&noisy) != nullptr);
        TerminalImage_setNoiseStyle(&noisy, TERMINAL_STYLE_DIM);
        assert(TerminalImage_getNoiseStyle(&noisy) == TERMINAL_STYLE_DIM);
        TerminalImage_setNoiseDensity(&noisy, 100);
        assert(TerminalImage_getNoiseDensity(&noisy) == 100);
        TerminalImage_setNoiseSeed(&noisy, 7);
        assert(TerminalImage_getNoiseSeed(&noisy) == 7);
        TerminalCanvas_fill(&noisyCanvas, ' ', TerminalPen());
        TerminalImage_renderContain(&noisy, &noisyCanvas);
        // The field reaches every corner of the region (no letterbox gap), and the
        // ink half of the picture is painted over it.
        assert(strchr("abcxyz", (char) TerminalCanvas_glyphAt(&noisyCanvas, 0, 0)) != nullptr);
        assert(strchr("abcxyz", (char) TerminalCanvas_glyphAt(&noisyCanvas, 5, 3)) != nullptr);
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 0, 1) == '@');
        // A fixed seed reproduces the field exactly; a new seed re-scatters it.
        u32 first = TerminalCanvas_glyphAt(&noisyCanvas, 5, 3);
        TerminalImage_setNoiseSeed(&noisy, 7);
        TerminalCanvas_fill(&noisyCanvas, ' ', TerminalPen());
        TerminalImage_renderContain(&noisy, &noisyCanvas);
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 5, 3) == first);
        int rescattered = 0;
        for (u32 seed = 1; seed <= 8 && !rescattered; ++seed) {
            TerminalImage_setNoiseSeed(&noisy, seed);
            TerminalCanvas_fill(&noisyCanvas, ' ', TerminalPen());
            TerminalImage_renderContain(&noisy, &noisyCanvas);
            if (TerminalCanvas_glyphAt(&noisyCanvas, 5, 3) != first)
                rescattered = 1;
        }
        assert(rescattered);
        // The decor is the region's, not the picture's: a 1:1 render covers the
        // picture's own rect instead.
        TerminalImage_setNoiseSeed(&noisy, 3);
        TerminalCanvas_fill(&noisyCanvas, ' ', TerminalPen());
        TerminalImage_render(&noisy, &noisyCanvas, 2, 2);
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 2, 2) == '@');   // ink over the field
        assert(strchr("abcxyz", (char) TerminalCanvas_glyphAt(&noisyCanvas, 3, 2)) != nullptr);
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 4, 0) == ' ');   // outside the picture rect
        // Density 0 leaves the field empty but never the picture.
        TerminalImage_setNoiseDensity(&noisy, 0);
        TerminalCanvas_fill(&noisyCanvas, ' ', TerminalPen());
        TerminalImage_renderContain(&noisy, &noisyCanvas);
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 0, 0) == ' ');
        assert(TerminalCanvas_glyphAt(&noisyCanvas, 0, 1) == '@');
        TerminalImage_setNoiseDensity(&noisy, 250);                // clamped to 100
        assert(TerminalImage_getNoiseDensity(&noisy) == 100);
        // A clean image carries no noise.
        assert(TerminalImage_getNoise(&img) == nullptr);
    }

    // CONTAIN: uniform zoom-to-fit — one scale for both axes, centred, and the
    // cellAspect correction keeps a wide picture wide (letterboxed, never squashed).
    {
        static u32 containCells[8 * 4];
        static u32 containFg[8 * 4];
        TerminalCanvas contain = TerminalCanvas(containCells, nullptr, containFg, nullptr, 8, 4);
        TerminalCanvas_fill(&contain, ' ', TerminalPen());
        assert(TerminalImage_getCellAspect(&img) == 2.0);
        TerminalImage_renderContain(&img, &contain);
        // A 2x1 source at cellAspect 2 fits as 8x2, centred on rows 1..2.
        assert(TerminalCanvas_glyphAt(&contain, 7, 1) != ' ');   // the white half
        assert(TerminalCanvas_glyphAt(&contain, 7, 0) == ' ');   // letterbox top
        assert(TerminalCanvas_glyphAt(&contain, 7, 3) == ' ');   // letterbox bottom
        TerminalImage_setCellAspect(&img, 1.0);
        assert(TerminalImage_getCellAspect(&img) == 1.0);
        TerminalCanvas_fill(&contain, ' ', TerminalPen());
        TerminalImage_renderContain(&img, &contain);
        assert(TerminalCanvas_glyphAt(&contain, 7, 0) != ' ');   // square pixels: fills
        TerminalImage_setCellAspect(&img, 0.0);                  // rejected, keeps 1.0
        assert(TerminalImage_getCellAspect(&img) == 1.0);
        TerminalImage_renderContain(&img, nullptr);
        TerminalImage_renderContain(nullptr, &contain);
    }

    // Palette treatment: a mono mask renders in the purple ramp with a
    // deterministic per-cell scatter (the `>w<` idea).
    static u32 pc[2];
    static u32 pfg[2];
    TerminalCanvas paletteCanvas = TerminalCanvas(pc, nullptr, pfg, nullptr, 2, 1);
    TerminalImageColorRamp purple = TerminalImage_rampPurple();
    assert(purple.data != nullptr && purple.count >= 4);
    assert(TerminalImage_setColorRamp(&img, purple.data, purple.count));
    TerminalImageColorRamp imageRamp = TerminalImage_getColorRamp(&img);
    assert(imageRamp.data == purple.data && imageRamp.count >= 4);
    // The palette's darkest entry is ~30% of full range (0x4C ≈ 255 * 0.3) and
    // nothing is darker, so the ramp spans ~30% up to near-white.
    bytes darkest = 255;
    for (bytes i = 0; i < purple.count; ++i) {
        u8 r = TERMINAL_COLOR_RED(purple.data[i]);
        u8 g = TERMINAL_COLOR_GREEN(purple.data[i]);
        u8 b = TERMINAL_COLOR_BLUE(purple.data[i]);
        assert(r >= 0x4C && g >= 0x4C && b >= 0x4C);
        u8 low = r < g ? (r < b ? r : b) : (g < b ? g : b);
        if (low < darkest)
            darkest = low;
    }
    assert(darkest >= 0x4C && darkest <= 0x60);
    TerminalImage_setJitter(&img, 0, 0);
    TerminalImage_render(&img, &paletteCanvas, 0, 0);
    assert(TerminalCanvas_fgAt(&paletteCanvas, 0, 0) == purple.data[0]);                 // black -> the ramp's low end
    assert(TerminalCanvas_fgAt(&paletteCanvas, 1, 0) == purple.data[purple.count - 1]);   // white -> the high end
    TerminalImage_setJitter(&img, 60, 7);
    assert(TerminalImage_getJitter(&img) == 60 && TerminalImage_getJitterSeed(&img) == 7);
    TerminalImage_render(&img, &paletteCanvas, 0, 0);
    u32 scattered = TerminalCanvas_fgAt(&paletteCanvas, 0, 0);
    // The jitter only LIGHTENS, so a light ramp never dips dark on some cells.
    assert(TERMINAL_COLOR_RED(scattered) >= TERMINAL_COLOR_RED(purple.data[0]));
    assert(TERMINAL_COLOR_BLUE(scattered) >= TERMINAL_COLOR_BLUE(purple.data[0]));
    TerminalImage_render(&img, &paletteCanvas, 0, 0);
    assert(TerminalCanvas_fgAt(&paletteCanvas, 0, 0) == scattered);                 // deterministic for a seed

    // Palette SCATTER: every ink cell shares one luminance, yet which one reads
    // light and which reads dark is randomized per cell instead of luminance-ordered.
    {
        static u8 ink[64 * 4];
        static u32 wideCells[64];
        static u32 wideFg[64];
        for (bytes i = 0; i < 64; ++i) {
            ink[i * 4 + 0] = 0; ink[i * 4 + 1] = 0; ink[i * 4 + 2] = 0; ink[i * 4 + 3] = 255;
        }
        TerminalImage flat = TerminalImage(ink, 64, 1);
        assert(TerminalImage_setColorRamp(&flat, purple.data, purple.count));
        TerminalCanvas wide = TerminalCanvas(wideCells, nullptr, wideFg, nullptr, 64, 1);
        TerminalImage_setPaletteScatter(&flat, (u8) purple.count);
        assert(TerminalImage_getPaletteScatter(&flat) == purple.count);
        TerminalImage_render(&flat, &wide, 0, 0);
        u32 firstColour = TerminalCanvas_fgAt(&wide, 0, 0);
        int distinct = 0;
        for (bytes x = 0; x < 64; ++x) {
            u32 colour = TerminalCanvas_fgAt(&wide, x, 0);
            int inRamp = 0;
            for (bytes i = 0; i < purple.count; ++i)
                if (colour == purple.data[i])
                    inRamp = 1;
            assert(inRamp);
            if (colour != firstColour)
                distinct = 1;
        }
        assert(distinct);
        // With the scatter off the same cells all take the luminance's one entry.
        TerminalImage_setPaletteScatter(&flat, 0);
        TerminalCanvas_fill(&wide, ' ', TerminalPen());
        TerminalImage_render(&flat, &wide, 0, 0);
        for (bytes x = 0; x < 64; ++x)
            assert(TerminalCanvas_fgAt(&wide, x, 0) == firstColour);
    }
    TerminalImage_setJitter(&img, 0, 0);
    assert(TerminalImage_setColorRamp(&img, nullptr, 0));
    imageRamp = TerminalImage_getColorRamp(&img);
    assert(imageRamp.data == nullptr && imageRamp.count == 0);
    TerminalImage_render(&img, &paletteCanvas, 0, 0);
    assert(TerminalCanvas_fgAt(&paletteCanvas, 1, 0) == TERMINAL_RGB(255, 255, 255));
    REJECT(TerminalImage_setColorRamp(&img, nullptr, 4), "color ramp");

    // Ramp + config.
    assert(TerminalImage_setRamp(&img, "01") && strcmp(TerminalImage_getRamp(&img), "01") == 0);
    assert(TerminalImage_glyph(&img, 0) == '0' && TerminalImage_glyph(&img, 255) == '1');
    assert(TerminalImage_getStyle(&img) == TERMINAL_STYLE_DIM);
    assert(TerminalImage_skipsTransparent(&img));
    assert(!TerminalImage_inverted(&img));

    // Rejections + invalid image safety.
    assert(!TerminalImage_isValid(nullptr));
    assert(TerminalImage_getPixels(nullptr) == nullptr);
    TerminalImage zero = TerminalImage_0();
    assert(!TerminalImage_isValid(&zero));
    REJECT(TerminalImage_setRamp(&zero, nullptr), "null or empty");
    TerminalImage_render(&zero, &region, 0, 0);   // no-op
    TerminalImage_renderFit(&zero, &region);
    TerminalImage_renderFit(&img, nullptr);
    REJECT(TerminalImage_setPixels(&zero, nullptr, 2, 2), "extent");
    REJECT(TerminalImage_setRamp(&zero, ""), "empty");
    TerminalProjection emptyImageProjection = TerminalImage_toString(&zero);
    assert(emptyImageProjection.length > 0 && !emptyImageProjection.truncated);
    TerminalImage rejected = TerminalImage(nullptr, 2, 2);
    assert(!TerminalImage_isValid(&rejected));

    TERMINAL_PROJECT(TerminalImage, &img);
    return 0;
}
