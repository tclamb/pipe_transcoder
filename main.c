#include <stdlib.h>
#include <stdint.h>
#include <string.h> // memmove

#include <lame/lame.h>
#include <mad.h>

lame_t lame;
uint8_t lame_init_params_run = 0;

#define INPUT_BUFFER_SIZE (96*1024)
unsigned char input_buffer[INPUT_BUFFER_SIZE];

enum mad_flow input_func(void *user_data, struct mad_stream *stream) {
    ptrdiff_t remaining = stream->bufend - stream->next_frame;
    memmove(input_buffer, stream->next_frame, remaining);

    size_t bytes_read = fread(input_buffer + remaining,
                              sizeof(unsigned char),
                              INPUT_BUFFER_SIZE - remaining,
                              stdin);

    if (bytes_read > 0) {
        mad_stream_buffer(stream, input_buffer, remaining + bytes_read);
        return MAD_FLOW_CONTINUE;
    } else {
        return MAD_FLOW_STOP;
    }
}

enum mad_flow output_func(void *user_data, const struct mad_header *header, struct mad_pcm *pcm) {
    mad_fixed_t *buffer_l  = pcm->samples[0],
                *buffer_r = pcm->samples[1];
    unsigned short nsamples = pcm->length;

    for (int i = 0; i < nsamples; i++) {
        buffer_l[i] <<= 32 - 1 - MAD_F_FRACBITS;
        buffer_r[i] <<= 32 - 1 - MAD_F_FRACBITS;
    }

    const int output_buffer_size = (int)(1.25f * nsamples + 7200);
    unsigned char *output_buffer = (unsigned char*)malloc(output_buffer_size);

    if (!lame_init_params_run) {
        lame_init_params_run = 1;

        if (header->mode == MAD_MODE_SINGLE_CHANNEL) {
            lame_set_num_channels(lame, 1);
        } else {
            lame_set_num_channels(lame, 2);
        }
        lame_set_in_samplerate(lame, header->samplerate);
        lame_init_params(lame);
    }

    int bytes_written = lame_encode_buffer_int(lame,
                                               buffer_l,
                                               buffer_r,
                                               nsamples,
                                               output_buffer,
                                               output_buffer_size);

    size_t written_out = fwrite(output_buffer, sizeof(unsigned char), bytes_written, stdout);

    free(output_buffer);

    if (bytes_written != written_out) {
        return MAD_FLOW_BREAK;
    }

    return MAD_FLOW_CONTINUE;
}

int main() {
    lame = lame_init();
    lame_set_brate(lame, 64);

    struct mad_decoder decoder;

    mad_decoder_init(&decoder,
                     /* first parameter of all functions */ NULL,
                     input_func,
                     /* header_func */ NULL,
                     /* ???_func */ NULL,
                     output_func,
                     /* error_func */ NULL,
                     /* message_func */ NULL
                     );

    mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&decoder);

    return 0;
}
