/*
** Lantern Miner: audio. A software port of audio.ts (Web Audio): the same oscillator waveforms, envelopes, note
** tables, warm low-pass on the music bus and the same sound effects, mixed on the platform's audio thread.
** The title screen plays a recorded theme (assets/music/theme.wav) instead of the synth.
** 48 kHz stereo. The game thread only queues voices; the audio thread renders.
*/
#include "lm.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 48000.0
#define MAX_VOICES 72
#define TRACK_GAIN 0.4 /* the theme is mastered much louder than the synth and effects */

enum { BUS_MUSIC, BUS_SFX };
enum { W_SINE, W_SQUARE, W_TRIANGLE, W_SAW };

struct biquad {
    double b0, b1, b2, a1, a2, z1, z2;
};

struct voice {
    bool active;
    int bus, wave;
    double phase, freq, freq_mul;
    long ramp_left, delay, age, attack_n, decay_end_n, life_n;
    double peak, gain, att_step, dec_mul;
    bool lp;
    struct biquad bq;
};

static struct {
    pthread_mutex_t lock;
    struct voice v[MAX_VOICES];
    volatile bool muted; /* written by the main thread, read by the audio thread */
    double master;
    volatile double master_target;
    volatile float music_vol, sfx_vol;
    struct biquad music_lp;
    /* sequencer */
    int mode;
    bool timer_on;
    long next_in;
    int step;
    double tempo;
    uint32_t noise;
    /* recorded title theme: interleaved 48 kHz stereo */
    int16_t *track;
    size_t track_frames, track_pos;
    double track_fade; /* 0..1, ramps so leaving the title does not click */
    bool ready;
} A;

/* RBJ low-pass; `q_db` follows Web Audio (resonance in dB). */
static void
lowpass(struct biquad *b, double fc, double q_db)
{
    double w0 = LM_TAU * fc / SR, cw = lm_cos(w0), sw = lm_sin(w0);
    double q = lm_pow(10.0, q_db / 20.0);
    double alpha = sw / (2.0 * q);
    double a0 = 1.0 + alpha;
    b->b0 = ((1.0 - cw) / 2.0) / a0;
    b->b1 = (1.0 - cw) / a0;
    b->b2 = ((1.0 - cw) / 2.0) / a0;
    b->a1 = (-2.0 * cw) / a0;
    b->a2 = (1.0 - alpha) / a0;
}

static inline double
bq_run(struct biquad *b, double x)
{
    double y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

static double
midi_hz(double m)
{
    return 440.0 * lm_pow(2.0, (m - 69.0) / 12.0);
}

static double
noise01(void)
{
    uint32_t x = A.noise;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    A.noise = x ? x : 0x9e3779b9u;
    return (double)(A.noise >> 8) / 16777216.0;
}

void
lm_audio_init(void)
{
    memset(&A, 0, sizeof(A));
    pthread_mutex_init(&A.lock, NULL);
    A.master = A.master_target = 0.72;
    A.music_vol = A.sfx_vol = 1.0f;
    A.tempo = 1.0;
    A.noise = 0x2545F491u;
    lowpass(&A.music_lp, 1320.0, 1.1);
    A.ready = true;
}

/* ------------------------------------------------------------- title theme */

static uint32_t
le32(unsigned char const *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Loads a 48 kHz 16-bit stereo WAV. Call after lm_audio_init() and before the audio thread starts.
   On any failure the title keeps the synth tune. */
bool
lm_audio_load_title_track(char const *path)
{
    unsigned char h[24];
    bool fmt_ok = false;
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
        goto fail;
    while (fread(h, 1, 8, f) == 8) {
        uint32_t size = le32(h + 4);
        long skip = (long)size + (long)(size & 1);
        if (!memcmp(h, "fmt ", 4)) {
            if (size < 16 || fread(h + 8, 1, 16, f) != 16)
                goto fail;
            /* PCM, 2 channels, 48000 Hz, 16 bits */
            fmt_ok = h[8] == 1 && h[9] == 0 && h[10] == 2 && le32(h + 12) == 48000 && h[22] == 16;
            skip -= 16;
        } else if (!memcmp(h, "data", 4)) {
            size_t frames = size / 4;
            if (!fmt_ok || !frames)
                goto fail;
            int16_t *pcm = malloc(frames * 4);
            if (!pcm || fread(pcm, 4, frames, f) != frames) {
                free(pcm);
                goto fail;
            }
            fclose(f);
            A.track = pcm;
            A.track_frames = frames;
            return true;
        }
        if (fseek(f, skip, SEEK_CUR) != 0)
            goto fail;
    }
fail:
    fclose(f);
    return false;
}

/* ------------------------------------------------------------------ voices */

static void
add_voice(int bus, int wave, double f0, double f1, double ramp_s, double delay_s, double attack_s, double peak,
          double decay_end_s, double stop_s, double detune_cents, bool lp640)
{
    if (!A.ready)
        return;
    struct voice *vo = NULL;
    pthread_mutex_lock(&A.lock);
    for (int i = 0; i < MAX_VOICES; i++)
        if (!A.v[i].active) { vo = &A.v[i]; break; }
    if (!vo) {
        long best = -1;
        for (int i = 0; i < MAX_VOICES; i++)
            if (A.v[i].age > best) { best = A.v[i].age; vo = &A.v[i]; }
    }
    memset(vo, 0, sizeof(*vo));
    vo->active = true;
    vo->bus = bus;
    vo->wave = wave;
    vo->freq = f0 * (detune_cents != 0 ? lm_pow(2.0, detune_cents / 1200.0) : 1.0);
    if (ramp_s > 0 && f1 > 0) {
        vo->ramp_left = (long)(ramp_s * SR);
        vo->freq_mul = lm_pow(f1 / f0, 1.0 / (double)vo->ramp_left);
    } else {
        vo->freq_mul = 1.0;
    }
    vo->delay = (long)(delay_s * SR);
    vo->attack_n = (long)(attack_s * SR);
    vo->decay_end_n = (long)(decay_end_s * SR);
    vo->life_n = (long)(stop_s * SR);
    vo->peak = peak;
    if (vo->attack_n > 0) {
        vo->gain = 0.0001;
        vo->att_step = (peak - 0.0001) / (double)vo->attack_n;
    } else {
        vo->gain = peak;
    }
    {
        long n = vo->decay_end_n - vo->attack_n;
        vo->dec_mul = n > 0 ? lm_pow(0.0001 / peak, 1.0 / (double)n) : 1.0;
    }
    vo->lp = lp640;
    if (lp640)
        lowpass(&vo->bq, 640.0, 1.0);
    pthread_mutex_unlock(&A.lock);
}

static void
music_tone(double freq, int wave, double dur, double peak, double attack)
{
    add_voice(BUS_MUSIC, wave, freq, 0, 0, 0, attack, peak, dur, dur + 0.02, (noise01() - 0.5) * 6.0, false);
}

static void
sfx_note(double freq, int wave, double dur, double peak, double delay)
{
    add_voice(BUS_SFX, wave, freq, 0, 0, delay, 0.012, peak, dur, dur + 0.02, 0, false);
}

static inline double
polyblep(double t, double dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

static inline double
osc(struct voice *v)
{
    double dt = v->freq / SR, s;
    switch (v->wave) {
    case W_SINE:
        s = lm_sin(v->phase * LM_TAU);
        break;
    case W_SQUARE:
        s = (v->phase < 0.5 ? 1.0 : -1.0) + polyblep(v->phase, dt);
        s -= polyblep(v->phase + 0.5 >= 1.0 ? v->phase - 0.5 : v->phase + 0.5, dt);
        break;
    case W_TRIANGLE:
        s = 4.0 * (v->phase < 0.5 ? v->phase : 1.0 - v->phase) - 1.0;
        break;
    default:
        s = (2.0 * v->phase - 1.0) - polyblep(v->phase, dt);
        break;
    }
    v->phase += dt;
    if (v->phase >= 1.0)
        v->phase -= 1.0;
    return s;
}

/* ---------------------------------------------------------------- sequencer */

static double const CAVERN_MELODY[64] = {
    59, 61, 62, 64, 66, 62, 66, 0, 65, 61, 65, 0, 64, 60, 64, 0,
    59, 61, 62, 64, 66, 62, 66, 71, 69, 66, 62, 66, 69, 0, 0, 0,
    66, 67, 69, 71, 73, 69, 73, 0, 71, 67, 71, 0, 69, 66, 69, 0,
    66, 64, 62, 61, 59, 62, 66, 69, 66, 62, 61, 59, 59, 0, 0, 0};
static double const CAVERN_BASS[64] = {
    47, 0, 54, 0, 47, 0, 54, 0, 49, 0, 53, 0, 48, 0, 52, 0,
    47, 0, 54, 0, 47, 0, 50, 0, 45, 0, 50, 0, 45, 0, 50, 0,
    42, 0, 49, 0, 45, 0, 52, 0, 43, 0, 50, 0, 42, 0, 49, 0,
    47, 0, 50, 0, 45, 0, 49, 0, 47, 0, 42, 0, 47, 0, 0, 0};
static double const CAMPFIRE_NOTES[32] = {
    62, 66, 69, 74, 0, 74, 71, 69, 62, 66, 69, 73, 0, 73, 69, 67,
    61, 64, 67, 73, 0, 73, 71, 67, 62, 66, 69, 74, 0, 69, 66, 0};

static long
step_samples(void)
{
    double ms = A.mode == MUSIC_CAVERN ? (double)(long)lm_round(195.0 / A.tempo) : 270.0;
    return (long)(ms * SR / 1000.0);
}

/* called with the lock NOT held (add_voice takes it); mode/step were read under the lock */
static void
tick_music(int mode, int step)
{
    if (A.muted || (mode == MUSIC_TITLE && A.track))
        return;
    if (mode == MUSIC_CAVERN) {
        int idx = step % 64;
        if (CAVERN_MELODY[idx] != 0)
            music_tone(midi_hz(CAVERN_MELODY[idx]), W_SQUARE, 0.14, 0.11, 0.008);
        if (CAVERN_BASS[idx] != 0)
            music_tone(midi_hz(CAVERN_BASS[idx]), W_TRIANGLE, 0.2, 0.22, 0.01);
        if (idx % 2 == 1)
            add_voice(BUS_MUSIC, W_TRIANGLE, 920, 340, 0.025, 0, 0, 0.022, 0.028, 0.035, 0, false);
    } else if (mode == MUSIC_TITLE || mode == MUSIC_CAMPFIRE) {
        int idx = step % 32;
        double note = CAMPFIRE_NOTES[idx];
        if (note != 0) {
            music_tone(midi_hz(note), W_TRIANGLE, 0.26, 0.13, 0.02);
            if (idx % 4 == 0)
                music_tone(midi_hz(note - 12), W_SINE, 0.42, 0.12, 0.03);
        }
    }
}

void
lm_audio_set_music_mode(int mode, double tempo)
{
    if (!A.ready)
        return;
    pthread_mutex_lock(&A.lock);
    A.tempo = lm_clamp(tempo, 0.75, 1.22);
    if (A.mode == mode && A.timer_on) {
        pthread_mutex_unlock(&A.lock);
        return;
    }
    A.mode = mode;
    A.timer_on = false;
    if (mode == MUSIC_TITLE)
        A.track_pos = 0; /* the theme starts from the top each time you return to the title */
    if (mode != MUSIC_NONE) {
        A.step = 0;
        A.timer_on = true;
        A.next_in = step_samples();
    }
    pthread_mutex_unlock(&A.lock);
}

void
lm_audio_update_tempo(double tempo)
{
    if (!A.ready)
        return;
    pthread_mutex_lock(&A.lock);
    double clamped = lm_clamp(tempo, 0.75, 1.25);
    if (lm_fabs(clamped - A.tempo) >= 0.08) {
        A.tempo = clamped;
        if (A.mode == MUSIC_CAVERN && A.timer_on)
            A.next_in = step_samples();
    }
    pthread_mutex_unlock(&A.lock);
}

void lm_audio_set_muted(bool muted) { A.muted = muted; A.master_target = muted ? 0.0 : 0.72; }
bool lm_audio_is_muted(void) { return A.muted; }
void lm_audio_set_volumes(float music, float sfx) { A.music_vol = music; A.sfx_vol = sfx; }

/* ----------------------------------------------------------------- render */

void
lm_audio_pull(int16_t *lr, size_t frames)
{
    double smooth = 1.0 - lm_exp(-1.0 / (0.03 * SR));
    if (!A.ready) {
        memset(lr, 0, frames * 4);
        return;
    }
    /* The voice list is locked once per chunk (not twice per sample): a chunk runs up to the next sequencer step
       or 256 samples, whichever is first. */
    double mus[256], sfxb[256], trk[256][2];
    double const fade_step = 1.0 / (0.08 * SR);
    size_t f = 0;
    while (f < frames) {
        size_t n = frames - f;
        if (n > 256)
            n = 256;
        pthread_mutex_lock(&A.lock);
        if (A.timer_on && A.next_in <= 1) {
            int mode = A.mode, step = A.step++;
            A.next_in = step_samples();
            pthread_mutex_unlock(&A.lock);
            tick_music(mode, step);
            pthread_mutex_lock(&A.lock);
        }
        if (A.timer_on) {
            if (A.next_in - 1 < (long)n && A.next_in > 1)
                n = (size_t)(A.next_in - 1);
            A.next_in -= (long)n;
            if (A.next_in < 1)
                A.next_in = 1;
        }
        for (size_t k = 0; k < n; k++) {
            double music = 0, sfx = 0;
            for (int i = 0; i < MAX_VOICES; i++) {
                struct voice *v = &A.v[i];
                if (!v->active)
                    continue;
                if (v->delay > 0) {
                    v->delay--;
                    continue;
                }
                double sm = osc(v) * v->gain;
                if (v->lp)
                    sm = bq_run(&v->bq, sm);
                if (v->bus == BUS_MUSIC) music += sm; else sfx += sm;
                /* envelope + pitch */
                v->age++;
                if (v->ramp_left > 0) {
                    v->freq *= v->freq_mul;
                    v->ramp_left--;
                }
                if (v->age < v->attack_n)
                    v->gain += v->att_step;
                else if (v->age == v->attack_n)
                    v->gain = v->peak;
                else if (v->age < v->decay_end_n)
                    v->gain *= v->dec_mul;
                else
                    v->gain = 0.0001;
                if (v->age >= v->life_n)
                    v->active = false;
            }
            mus[k] = music;
            sfxb[k] = sfx;
            trk[k][0] = trk[k][1] = 0;
            if (A.track) {
                A.track_fade = lm_clamp(A.track_fade + (A.mode == MUSIC_TITLE ? fade_step : -fade_step), 0.0, 1.0);
                if (A.track_fade > 0) {
                    double g = A.track_fade * (TRACK_GAIN / 32768.0);
                    trk[k][0] = A.track[A.track_pos * 2] * g;
                    trk[k][1] = A.track[A.track_pos * 2 + 1] * g;
                    if (++A.track_pos >= A.track_frames)
                        A.track_pos = 0;
                }
            }
        }
        pthread_mutex_unlock(&A.lock);

        for (size_t k = 0; k < n; k++) {
            double music = bq_run(&A.music_lp, mus[k]) * 0.28 * A.music_vol;
            double sfx = sfxb[k] * 0.42 * A.sfx_vol;
            A.master += (A.master_target - A.master) * smooth;
            double out = (music + sfx) * A.master * 2.2; /* console loudness: the web version leaves volume to the browser */
            double level = A.music_vol * (A.master / 0.72); /* the theme follows the music slider and mute */
            for (int c = 0; c < 2; c++)
                lr[(f + k) * 2 + c] = (int16_t)(lm_clamp(out + trk[k][c] * level, -1.0, 1.0) * 32000.0);
        }
        f += n;
    }
}

/* -------------------------------------------------------------------- sfx */

#define SFX_GUARD() do { if (A.muted) return; } while (0)

void
lm_audio_jump(bool dbl)
{
    SFX_GUARD();
    double s = dbl ? 390 : 250, e = dbl ? 740 : 520;
    add_voice(BUS_SFX, dbl ? W_SINE : W_TRIANGLE, s, e, 0.11, 0, 0.012, 0.24, 0.13, 0.14, 0, false);
}

void
lm_audio_land(double intensity)
{
    SFX_GUARD();
    double vol = lm_min(0.2, 0.08 + intensity * 0.12);
    add_voice(BUS_SFX, W_SINE, 135, 58, 0.065, 0, 0, vol, 0.075, 0.085, 0, false);
}

void
lm_audio_key(int combo)
{
    static int const scale[7] = {62, 66, 69, 71, 74, 78, 81};
    SFX_GUARD();
    int base = scale[combo < 6 ? combo : 6];
    int notes[3] = {base, base + 4, base + 7};
    for (int i = 0; i < 3; i++)
        sfx_note(midi_hz(notes[i]), W_TRIANGLE, 0.16, 0.2, i * 0.045);
}

void
lm_audio_relic(void)
{
    static int const n[3] = {74, 78, 81};
    SFX_GUARD();
    for (int i = 0; i < 3; i++)
        sfx_note(midi_hz(n[i]), W_SINE, 0.15, 0.18, i * 0.04);
}

void
lm_audio_powerup(void)
{
    static int const n[5] = {60, 64, 67, 72, 76};
    SFX_GUARD();
    for (int i = 0; i < 5; i++)
        sfx_note(midi_hz(n[i]), W_TRIANGLE, 0.18, 0.22, i * 0.05);
}

void
lm_audio_portal_open(void)
{
    static int const n[5] = {59, 66, 71, 74, 78};
    SFX_GUARD();
    for (int i = 0; i < 5; i++)
        sfx_note(midi_hz(n[i]), W_SINE, 0.36, 0.2, i * 0.06);
}

void
lm_audio_portal_enter(void)
{
    static int const n[7] = {62, 66, 69, 74, 73, 74, 78};
    SFX_GUARD();
    for (int i = 0; i < 7; i++)
        sfx_note(midi_hz(n[i]), W_TRIANGLE, 0.22, 0.22, i * 0.065);
}

void
lm_audio_crumble(void)
{
    SFX_GUARD();
    sfx_note(160, W_SAW, 0.08, 0.09, 0);
    sfx_note(118, W_TRIANGLE, 0.1, 0.11, 0.04);
}

void
lm_audio_hurt(bool shield_broken)
{
    SFX_GUARD();
    if (shield_broken) {
        sfx_note(520, W_TRIANGLE, 0.14, 0.22, 0);
        sfx_note(340, W_SINE, 0.18, 0.2, 0.06);
        return;
    }
    add_voice(BUS_SFX, W_SAW, 210, 72, 0.22, 0, 0, 0.24, 0.24, 0.26, 0, true);
}

void
lm_audio_perk_select(void)
{
    static int const n[4] = {62, 69, 74, 78};
    SFX_GUARD();
    for (int i = 0; i < 4; i++)
        sfx_note(midi_hz(n[i]), W_SINE, 0.28, 0.2, i * 0.055);
}

void
lm_audio_shop_buy(void)
{
    static int const n[4] = {69, 73, 76, 81};
    SFX_GUARD();
    for (int i = 0; i < 4; i++)
        sfx_note(midi_hz(n[i]), W_TRIANGLE, 0.16, 0.22, i * 0.04);
}

void
lm_audio_spark(void)
{
    SFX_GUARD();
    sfx_note(640, W_TRIANGLE, 0.07, 0.09, 0);
    sfx_note(440, W_SINE, 0.09, 0.08, 0.035);
}

void
lm_audio_game_over(void)
{
    static int const n[5] = {66, 64, 62, 59, 54};
    SFX_GUARD();
    for (int i = 0; i < 5; i++)
        sfx_note(midi_hz(n[i]), W_TRIANGLE, 0.3, 0.2, i * 0.11);
}

/* menu feedback (not in the web version, which had no menus to navigate) */
void
lm_audio_ui_move(void)
{
    SFX_GUARD();
    add_voice(BUS_SFX, W_TRIANGLE, 660, 0, 0, 0, 0.004, 0.1, 0.05, 0.07, 0, false);
}

void
lm_audio_ui_select(void)
{
    SFX_GUARD();
    sfx_note(midi_hz(74), W_TRIANGLE, 0.1, 0.16, 0);
    sfx_note(midi_hz(81), W_TRIANGLE, 0.14, 0.16, 0.045);
}

void
lm_audio_ui_back(void)
{
    SFX_GUARD();
    sfx_note(midi_hz(66), W_TRIANGLE, 0.1, 0.14, 0);
    sfx_note(midi_hz(59), W_TRIANGLE, 0.12, 0.14, 0.04);
}
