#define MULIB_IMPL_MICROAUDIO
#include "mulib-microaudio.h"
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    mulib_microaudio_pcm_t tone = {0}, decoded = {0};
    unsigned char *wav = NULL; size_t length = 0; int failed;
    failed = mulib_microaudio_create(800,1,8000,NULL,&tone) ||
        mulib_microaudio_sine(&tone,440,0.25,0) ||
        mulib_microaudio_wav_encode(&tone,NULL,&wav,&length) ||
        mulib_microaudio_wav_decode(wav,length,NULL,&decoded);
    if (!failed) printf("PCM16 WAV: %zu bytes, %zu frames at %u Hz\n",length,decoded.frames,(unsigned)decoded.sample_rate);
    free(wav); mulib_microaudio_free(&tone); mulib_microaudio_free(&decoded); return failed ? 1 : 0;
}
