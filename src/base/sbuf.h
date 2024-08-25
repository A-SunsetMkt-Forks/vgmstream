#ifndef _SBUF_H_
#define _SBUF_H_

#include "../streamtypes.h"

/* All types are interleaved (buffer for all channels = [ch*s] = ch1 ch2 ch1 ch2 ch1 ch2 ...)
 * rather than planar (buffer per channel = [ch][s] = c1 c1 c1 c1 ...  c2 c2 c2 c2 ...) */
typedef enum {
    SFMT_NONE,
    SFMT_S16,           /* standard PCM16 */
  //SFMT_S24,
  //SFMT_S32,
    SFMT_F32,           /* pcm-like float (+-32768), for internal use (simpler pcm > f32 plus some decoders use this) */
    SFMT_FLT,           /* standard float (+-1.0), for external players */
} sfmt_t;


/* simple buffer info for internal mixing
 * meant to held existing sound buffer pointers rather than alloc'ing directly (some ops will swap/move its internals) */
typedef struct {
    void* buf;          /* current sample buffer */
    sfmt_t fmt;         /* buffer type */
    int channels;       /* interleaved step or planar buffers */
    int samples;        /* max samples */
    int filled;         /* samples in buffer */
} sbuf_t;


void sbuf_init_s16(sbuf_t* sbuf, int16_t* buf, int samples, int channels);

void sbuf_init_f32(sbuf_t* sbuf, float* buf, int samples, int channels);

//void sbuf_clamp(sbuf_t* sbuf, int samples);

//void sbuf_consume(sbuf_t* sbuf, int samples);

/* it's probably slightly faster to make those inline'd, but aren't called that often to matter (given big enough total samples) */

// TODO decide if using float 1.0 style or 32767 style (fuzzy PCM changes when doing that)
void sbuf_copy_to_f32(float* dst, sbuf_t* sbuf);

void sbuf_copy_from_f32(sbuf_t* sbuf, float* src);

void sbuf_copy_samples(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled);

void sbuf_copy_layers(sample_t* dst, int dst_channels, sample_t* src, int src_channels, int samples_to_do, int samples_filled, int dst_ch_start);

void sbuf_silence_s16(sample_t* dst, int samples, int channels, int filled);

bool sbuf_realloc(sample_t** dst, int samples, int channels);

#endif
