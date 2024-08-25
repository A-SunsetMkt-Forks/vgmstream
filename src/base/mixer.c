#include "../vgmstream.h"
#include "../util/channel_mappings.h"
#include "mixing.h"
#include "mixer_priv.h"
#include "mixer.h"
#include "sbuf.h"
#include <math.h>
#include <limits.h>

//TODO simplify
/**
 * Mixer modifies decoded sample buffer before final output. This is implemented
 * mostly with simplicity in mind rather than performance. Process:
 * - detect if mixing applies at current moment or exit (mini performance optimization)
 * - copy/upgrade buf to float mixbuf if needed
 * - do mixing ops
 * - copy/downgrade mixbuf to original buf if needed
 * 
 * Mixing may add or remove channels. input_channels is the buf's original channels,
 * and output_channels the resulting buf's channels. buf and mixbuf must be
 * as big as max channels (mixing_channels).
 * 
 * Mixing ops are added by a meta (ex. TXTP) or plugin through the API. Non-sensical
 * mixes are ignored (to avoid rechecking every time).
 * 
 * Currently, mixing must be manually enabled before starting to decode, because plugins
 * need to setup bigger bufs when upmixing. (to be changed)
 *
 * segmented/layered layouts handle mixing on their own.
 */

mixer_t* mixer_init(int channels) {
    mixer_t* mixer = calloc(1, sizeof(mixer_t));
    if (!mixer) goto fail;

    mixer->chain_size = VGMSTREAM_MAX_MIXING; /* fixed array for now */
    mixer->mixing_channels = channels;
    mixer->output_channels = channels;
    mixer->input_channels = channels;

    return mixer;

fail:
    mixer_free(mixer);
    return NULL;
}

void mixer_free(mixer_t* mixer) {
    if (!mixer) return;

    free(mixer->mixbuf);
    free(mixer);
}

void mixer_update_channel(mixer_t* mixer) {
    if (!mixer) return;

    /* lame hack for dual stereo, but dual stereo is pretty hack-ish to begin with */
    mixer->mixing_channels++;
    mixer->output_channels++;
}

bool mixer_is_active(mixer_t* mixer) {
    /* no support or not need to apply */
    if (!mixer || !mixer->active || mixer->chain_count == 0)
        return false;

    return true;
}

void mixer_process(mixer_t* mixer, sbuf_t* sbuf, int32_t current_pos) {

    /* no support or not need to apply */
    if (!mixer || !mixer->active || mixer->chain_count == 0)
        return;

    /* try to skip if no fades apply (set but does nothing yet) + only has fades 
     * (could be done in mix op but avoids upgrading bufs in some cases) */
    mixer->current_subpos = 0;
    if (mixer->has_fade) {
        //;VGM_LOG("MIX: fade test %i, %i\n", data->has_non_fade, mixer_op_fade_is_active(data, current_pos, current_pos + sample_count));
        if (!mixer->has_non_fade && !mixer_op_fade_is_active(mixer, current_pos, current_pos + sbuf->filled))
            return;

        //;VGM_LOG("MIX: fade pos=%i\n", current_pos);
        mixer->current_subpos = current_pos;
    }

    // remix to temp buf for mixing (somehow using float buf rather than int32 is faster?)
    sbuf_copy_to_f32(mixer->mixbuf, sbuf);

    // apply mixing ops in order. current_channels may increase or decrease per op
    // - 2ch w/ "1+2,1u" = ch1+ch2, ch1(add and push rest) = 3ch: ch1' ch1+ch2 ch2
    // - 2ch w/ "1u"     = downmix to 1ch (current_channels decreases once)
    mixer->current_channels = mixer->input_channels;
    for (int m = 0; m < mixer->chain_count; m++) {
        mix_op_t* mix = &mixer->chain[m];

        //TO-DO: set callback
        switch(mix->type) {
            case MIX_SWAP:      mixer_op_swap(mixer, sbuf->filled, mix); break;
            case MIX_ADD:       mixer_op_add(mixer, sbuf->filled, mix); break;
            case MIX_VOLUME:    mixer_op_volume(mixer, sbuf->filled, mix); break;
            case MIX_LIMIT:     mixer_op_limit(mixer, sbuf->filled, mix); break;
            case MIX_UPMIX:     mixer_op_upmix(mixer, sbuf->filled, mix); break;
            case MIX_DOWNMIX:   mixer_op_downmix(mixer, sbuf->filled, mix); break;
            case MIX_KILLMIX:   mixer_op_killmix(mixer, sbuf->filled, mix); break;
            case MIX_FADE:      mixer_op_fade(mixer, sbuf->filled, mix);
            default:
                break;
        }
    }

    // setup + remix to output buf (buf is expected to be big enough to handle config)
    sbuf->channels = mixer->output_channels; // new channels
    //if (force_float) sbuf->fmt = SFMT_FLT; // new format
    //if (force_pcm16) sbuf->fmt = SFMT_PCM16; // new format
    sbuf_copy_from_f32(sbuf, mixer->mixbuf);
}
