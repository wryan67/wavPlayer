#include "Sound.h"

void *generate_sine(wavFormatType& wavConfig, double duration, double& phase, float freq) {

    long samples = (wavConfig.sampleRate * duration) + 0.5;
    long dataSize = samples * wavConfig.blockAlign * wavConfig.channels;

    if (wavConfig.bitsPerSample != 16) {
        fprintf(stderr, "generate_sine expects 16 bit sample size\n"); fflush(stderr);
        exit(EXIT_FAILURE);
    }

    unsigned char *data = (unsigned char *)malloc(dataSize);
    memset(data, 0, dataSize);

    static double max_phase = 2.0 * M_PI;
    double step = max_phase * freq / wavConfig.sampleRate;
    
    unsigned int maxVolume = ((1 << (wavConfig.bitsPerSample - 1)) - 1)/4;  

    long wavIndex = 0;
    for (long sample = 0; sample < samples; ++sample) {



        int16_t amplitude = sin(phase) * maxVolume;
//      int16_t amplitude = (sin(phase)<0?-1:1) * maxVolume;

        for (int channel = 0; channel < wavConfig.channels; channel++) {
            
            wavIndex = (sample * wavConfig.blockAlign * wavConfig.channels) + (channel * wavConfig.blockAlign);

            memcpy(&data[wavIndex], &amplitude, sizeof(int16_t));


           /* Generate signed data in little endian format */
            /*
            for (int i = 0; i < wavConfig.blockAlign; i++) {
                //int offset = wavConfig.blockAlign - 1 - i;
                int offset = 0;
               *(data + sample + channel + offset) = (amplitude >> i * 8) & 0xff;
            }
            */
        }
        phase += step;
        if (phase >= max_phase) {
            phase -= max_phase;
        }
    }
    return data;
}


void playTone(float freq, double duration, wavFormatType &wavConfig) {
    snd_pcm_t* handle = getSoundCardHandle();
    if (handle == NULL) {
        fprintf(stderr, "soundCardHandle is null\n"); fflush(stderr);
        return;
    }

    sendWavConfig(handle, wavConfig);
    double phase = 0;
    _playTone(handle, freq, duration, wavConfig, phase);
    closeSoundCard(handle);
}


void _playTone(snd_pcm_t* handle, float freq, double duration, wavFormatType &wavConfig, double &phase) {

    void* data = generate_sine(wavConfig, duration, phase, freq);

    long frames = (wavConfig.sampleRate * duration) + 0.5;

    snd_pcm_sframes_t framesWritten = snd_pcm_writei(handle, data, frames);

    if (framesWritten < 0) {
        fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(framesWritten));
        exit(EXIT_FAILURE);
    }

    if (framesWritten > 0 && framesWritten < frames) {
        fprintf(stderr, "data frames write error (expected %ld, wrote %ld)\n", frames, (long)framesWritten);
    }

    free(data);
}

