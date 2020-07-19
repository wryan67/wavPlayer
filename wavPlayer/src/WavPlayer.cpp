#include "Sound.h"

_snd_pcm_format getFormatFromWav(wavFormatType &wavConfig) {

    switch (wavConfig.bitsPerSample) {
    case 8:
    {
        return SND_PCM_FORMAT_S8;
        break;
    }
    case 16:
    {
        return SND_PCM_FORMAT_S16_LE;
        break;
    }
    case 24:
    {
        return SND_PCM_FORMAT_S24_3LE;
        break;
    }
    case 32:
    {
        return SND_PCM_FORMAT_S32_LE;
        break;
    }
    default:
        fprintf(stderr, "unknown alsa translation for bits per sample: %d\n", wavConfig.bitsPerSample);
        return SND_PCM_FORMAT_UNKNOWN;
    }

}

void playWavFile(char* filename, float volume) {

    snd_pcm_t* handle = getSoundCardHandle();
    if (handle == NULL) {
        fprintf(stderr, "soundCardHandle is null\n"); fflush(stderr);
        return;
    }
    _playWavFile(handle, filename, volume);
    closeSoundCard(handle);
}


void sendWavConfig(snd_pcm_t* soundCardHandle, wavFormatType &wavConfig) {
    int err;

    snd_pcm_drop(soundCardHandle);


    if (wavConfig.bitsPerSample < 1) {
        fprintf(stderr, "bits per sample must be greater than zero, bitsPerSample=%d\n", wavConfig.bitsPerSample);
        return;
    }

    if (wavConfig.bitsPerSample%8 != 00) {
        fprintf(stderr, "bits per sample must be a multiple of 8, bitsPerSample=%d\n", wavConfig.bitsPerSample);
        return;
    }

    if (wavConfig.audioFormat != 1) {
        fprintf(stderr, "unknown audio format, expected 1, found %d\n", wavConfig.audioFormat);
        return;

    }

    _snd_pcm_format sndFormat=getFormatFromWav(wavConfig);
    _snd_pcm_access accessType = SND_PCM_ACCESS_RW_INTERLEAVED;

    if (sndFormat == SND_PCM_FORMAT_UNKNOWN) {
        fprintf(stderr, "unknown alsa audio format\n");
        return;
    }

    if ((err = snd_pcm_set_params(soundCardHandle,
        sndFormat,
        accessType,
        wavConfig.channels,
        wavConfig.sampleRate,  // sps
        1,                     // software resample
        1000000)) < 0)         // latency 1 sec 
    {
        fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
}

void printWavConfig(wavFormatType &wavConfig) {
    int segmetSize = (wavConfig.bitsPerSample / 8) * wavConfig.channels;

    printf("audio format      %d\n", wavConfig.audioFormat);
    printf("num channels      %d\n", wavConfig.channels);
    printf("sample rate       %d\n", wavConfig.sampleRate);
    printf("byte rate         %d\n", wavConfig.byteRate);
    printf("block align       %d\n", wavConfig.blockAlign);
    printf("bits per sample   %d\n", wavConfig.bitsPerSample);
    printf("segment size      %d\n", segmetSize);

}


void _playWavFile(snd_pcm_t* soundCardHandle, char* filename, float volume) {
    fprintf(stderr,"Playing %s ...\n",filename); fflush(stdout);

    FILE* wav = fopen(filename, "r");

    if (wav == NULL) {
        fprintf(stderr,"Cannot open %s\n", filename); fflush(stderr);
        free(filename);
        return;
    }
    wavFileHeaderType  wavFileHeaderType;
    wavChunkHeaderType wavChunkHeaderType;
    wavFormatType      wavConfig;
    
    int b = fread(&wavFileHeaderType, sizeof(wavFileHeaderType), 1, wav);
    if (b != 1) {
        fprintf(stderr, "Could not read header from sound file %s\n",filename);
        free(filename);
        return;
    }

    if (strncmp(wavFileHeaderType.fileType, "RIFF", 4) != 0) {
        fprintf(stderr,"unexpected file descriptor, expected 'RIFF', actual='%4.4s'\n", wavFileHeaderType.fileType);
        return;
    }

    if (strncmp(wavFileHeaderType.fileFormat, "WAVE", 4) != 0) {
        fprintf(stderr,"unexpected file type, expected 'RIFF', actual='%4.4s'\n", wavFileHeaderType.fileFormat);
        return;
    }


    while (true) {
        int b = fread(&wavChunkHeaderType, sizeof(wavChunkHeaderType), 1, wav);
        if (b != 1) {
            fprintf(stderr, "Could not read chunk header from sound file %s\n", filename);
            free(filename);
            return;
        }
        if (strncmp(wavChunkHeaderType.chunkID, "data", 4)==0) {
            fprintf(stderr, "error: data chunk found before wav format\n");
            free(filename);
            return;
        }

        if (strncmp(wavChunkHeaderType.chunkID, "IF", 2) == 0) {
            exit(0);
        }

        if (strncmp(wavChunkHeaderType.chunkID, "fmt ", 4) == 0) {
            void* wavHeaderChunk = malloc(wavChunkHeaderType.chunkSize);
            b = fread(wavHeaderChunk, wavChunkHeaderType.chunkSize, 1, wav);
            if (b != 1) {
                fprintf(stderr, "Could not read wave header from sound file %s\n", filename);
                free(filename);
                return;
            }
            memcpy(&wavConfig, wavHeaderChunk, sizeof(wavConfig));
            free(wavHeaderChunk);
            break;
        } else {
            fseek(wav, wavChunkHeaderType.chunkSize, SEEK_CUR);
        }
    }


    int segmetSize = (wavConfig.bitsPerSample / 8) * wavConfig.channels;


    if (debug) {
        printWavConfig(wavConfig);
    }

    sendWavConfig(soundCardHandle, wavConfig);


    void* data = NULL;

    while (true) {

        long rs = fread(&wavChunkHeaderType, sizeof(wavChunkHeaderType), 1, wav);

        if (rs != 1) {
            break;
        }

        if (strncmp(wavChunkHeaderType.chunkID, "data", 4) != 0) {
            fseek(wav, wavChunkHeaderType.chunkSize, SEEK_CUR);
            continue;
        }

        long dataSize = wavChunkHeaderType.chunkSize;
        data = realloc(data, dataSize);

        rs = fread(data, dataSize, 1, wav);

        if (rs != 1) {
            fprintf(stderr, "unexpected error, %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        snd_pcm_sframes_t bytesWritten = snd_pcm_writei(soundCardHandle, data, dataSize / segmetSize);

        if (debug) {
           fprintf(stderr, "checking overflow\n"); fflush(stderr);
        }
        if (bytesWritten < 0) {
            bytesWritten = snd_pcm_recover(soundCardHandle, bytesWritten, 0);
        }

        if (debug) {
            fprintf(stderr, "verifying write status\n"); fflush(stderr);
        }
        if (bytesWritten < 0) {
            fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(bytesWritten));
            exit(EXIT_FAILURE);
        }

        if (bytesWritten > 0 && bytesWritten < dataSize / segmetSize) {
            fprintf(stderr, "data write error (expected %li, wrote %li)\n", dataSize, bytesWritten);
        }
    }

    if (debug) {
        fprintf(stderr, "flushing soundCardHandle\n"); fflush(stderr);
    }

    free(data);
    fclose(wav);
    free(filename);
}
