/* Renders the game's title waltz (the campfire arpeggio from the audio engine) as a loopable WAV for the console's
 * hover music (sce_sys/snd0.at9, which must be converted from this WAV with Sony's at9tool).
 *   make_theme assets/brand/lantern_theme.wav */
#include <stdio.h>
#include <stdlib.h>
#include "../ps5/game/lm.h"

int
main(int argc, char **argv)
{
    unsigned step = 270 * 48;               /* samples per music step */
    unsigned loops = 5;                      /* 32 steps per loop */
    unsigned total = step * 32 * loops;
    int16_t *buf = malloc((size_t)total * 4);
    unsigned pos = 0;
    lm_audio_init();
    lm_audio_set_music_mode(MUSIC_TITLE, 1.0);
    while (pos < total) {
        unsigned n = total - pos < 256 ? total - pos : 256;
        lm_audio_pull(buf + (size_t)pos * 2, n);
        pos += n;
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        unsigned data = total * 4, rate = 48000, br = rate * 4, fmtlen = 16, riff = 36 + data;
        unsigned short ch = 2, bps = 16, ba = 4, fmt = 1;
        if (!f) return 1;
        fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtlen, 4, 1, f);
        fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bps, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
        fwrite(buf, 4, total, f);
        fclose(f);
        printf("wrote %s (%.1f s)\n", argv[1], total / 48000.0);
    }
    return 0;
}
