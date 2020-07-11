/*
*
*  $ sudo apt install libasound2-dev
* 
*  $ aplay -L | grep ^default     << to find your sound card
*  export AUDIODEV="default:CARD=Device"   << your sound card
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>

#include <chrono>
#include <ctime>  
#include "wavPlayer.h"


snd_pcm_t* soundCard1Handle;
snd_pcm_t* soundCard2Handle;

bool debug = false;
int volume = 100;

int yankeeDoodle[99] = {
  5,5,6,7,5,7,6,0,5,5,6,7,5,0,4,0,5,5,6,7,8,7,6,5,4,2,3,4,5,0,5,0,-1
};
int yankeeDoodleBase[99] = {
  5,5,6,7,5,7,6,0,5,5,6,7,5,0,4,0,5,5,6,7,8,7,6,5,4,2,3,4,5,0,5,0,-1
};


bool demo1isRunning=false;
bool demo2isRunning=false;

float noteHz[11] = {
  0,        // rest
  195.9977, // 1  G3
  261.6256, // 3  C4
  293.6648, // 4  D4
  329.6276, // 5  E4
  349.2282, // 6  F4
  391.9954, // 7  G4
  440.0000, // 8  A4
  493.8833, // 9  B4
  523.2511, // 10 C5
  587.3295, // 11 D5
};

struct wavHeader
{
    char     chunkID[4];      // "RIFF" - literal
    int32_t  fileSize;        // size of entire file minus 8 bytes
    char     format[4];       // "WAVE" - big-endian format
    char     subChunk1ID[4];  // "fmt " - literal
    int32_t  subChunk1Size;   // size of sub-chunk1 minus 8 bytes
    int16_t  audioFormat;     // 1=PCM, uncompressed
    int16_t  channels;        // 1=Mono, 2=Stereo, etc.
    int32_t  sampleRate;      // SPS
    int32_t  byteRate;        // SampleRate * NumChannels * BitsPerSample/8
    int16_t  blockAlign;      // NumChannels * BitsPerSample/8 
                              // BlockAlign is the number of bytes for one sample, including all channels
    int16_t  bitsPerSample;   // 8, 16, etc.
};

struct chunkHeader {
	char     chunkID[4];  // if "data", then it is a data chunk
	int32_t  chunkSize;   // the size of the data chunk
};


volatile unsigned* gpio, * gpset, * gpclr, * gpin, * timer, * intrupt;


unsigned long long currentTimeMillis() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    return (unsigned long long)(currentTime.tv_sec) * 1000 +
        (unsigned long long)(currentTime.tv_usec) / 1000;
}

pthread_t threadCreate(void *(*method)(void *), char *description) {
        pthread_t threadId;
        int status= pthread_create(&threadId, NULL, method, NULL);
        if (status != 0) {
                printf("%s::thread create failed %d--%s\n", description, status, strerror(errno));
                exit(9);
        }
        pthread_detach(threadId);
        return threadId;
}



bool setup() {
   if (debug) {
     printf("intialization commencing...\n"); 
     fflush(stderr);
   }
   int seed;
   FILE *fp;
   fp = fopen("/dev/urandom", "r");
   fread(&seed, sizeof(seed), 1, fp);
   fclose(fp);
   srand(seed);

   return true;
}

snd_pcm_t* openSoundCard(const char *soundCardName) {
    int err;

    if (strlen(soundCardName)==0) {
      soundCardName="default";
    }
    if (debug) {
        printf("output device     %s\n", soundCardName);
    }
    
    snd_pcm_t* soundCardHandle;

    if (debug) {
       fprintf(stderr, "opening sound card: %s\n", soundCardName);  
       fflush(stderr);
    }
    if ((err = snd_pcm_open(&soundCardHandle, soundCardName, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    return soundCardHandle;
}                                             // end of setup function


bool usage() {
	fprintf(stderr, "usage: wavPlayer [-p gpio] [-d] [-v 0-100]\n");
	fprintf(stderr, "d = debug\n");
	fprintf(stderr, "p = gpio pin\n");
	fprintf(stderr, "v = volume\n");
    fprintf(stderr, "Environment variables:\n");
    fprintf(stderr, "    AUDIODEV='your sound card'\n");

	return false;
}

bool commandLineOptions(int argc, char **argv) {
	int c, index;


	while ((c = getopt(argc, argv, "dv:p:")) != -1)
		switch (c) {
		case 'd':
			debug = true;
			break;
		case 'v':
			sscanf(optarg, "%d", &volume);
			break;
		case '?':
			if (optopt == 'm' || optopt == 't')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf(stderr, "Unknown option character \\x%x.\n", optopt);

			return usage();
		default:
			abort();
		}




	return true;
}


wavHeader wavHeader;

void pushHeader(snd_pcm_t* soundCardHandle) {
    int err;

    strncpy(wavHeader.chunkID, "RIFF",4);
    strncpy(wavHeader.format, "WAVE",4);
    strncpy(wavHeader.subChunk1ID, "fmt ",4);
    wavHeader.fileSize=9;
    wavHeader.subChunk1Size=16;
    wavHeader.audioFormat=1;
    wavHeader.channels=1;
    wavHeader.sampleRate=128000;
    wavHeader.bitsPerSample=16;
    wavHeader.blockAlign=wavHeader.bitsPerSample/8;
    wavHeader.byteRate=wavHeader.sampleRate*wavHeader.channels*wavHeader.blockAlign;
    

    double maxAmplitude = pow(2, wavHeader.bitsPerSample - 1);
    int segmetSize = (wavHeader.bitsPerSample / 8) * wavHeader.channels;

    if (debug) {

        printf("volume            %d\n", volume);

        printf("chunk Id          %4.4s\n", wavHeader.chunkID);
        printf("chunck size       %d\n", wavHeader.fileSize);
        printf("format            %4.4s\n", wavHeader.format);
        printf("subChunk1 ID      %4.4s\n", wavHeader.subChunk1ID);
        printf("subChunk1 Size    %d\n", wavHeader.subChunk1Size);
        printf("audio format      %d\n", wavHeader.audioFormat);
        printf("num channels      %d\n", wavHeader.channels);
        printf("sample rate       %d\n", wavHeader.sampleRate);
        printf("byte rate         %d\n", wavHeader.byteRate);
        printf("block align       %d\n", wavHeader.blockAlign);
        printf("bits per sample   %d\n", wavHeader.bitsPerSample);
        printf("maxAmplitude      %d\n", (int)maxAmplitude);
        printf("segment size      %d\n", segmetSize);
    }

    if (wavHeader.bitsPerSample != 16) {
        printf("bits per sample must be 16, found %d\n", wavHeader.bitsPerSample);
        return;
    }
    if (strncmp(wavHeader.chunkID, "RIFF", 4) != 0) {
        printf("unexpected chunk descriptor, expected 'RIFF', actual='%4.4s'\n", wavHeader.chunkID);
        return;
    }

    if (strncmp(wavHeader.format, "WAVE", 4) != 0) {
        printf("unknown file format, expected 'WAVE', actual '%4.4s'\n", wavHeader.format);
        return;
    }
    if (wavHeader.audioFormat != 1) {
        printf("unknown audio format, expected 1, found %d\n", wavHeader.audioFormat);
        return;
    }


    if (debug) {
        fprintf(stderr, "sending wav header...\n"); 
        fflush(stderr);
    }


    if ((err = snd_pcm_set_params(soundCardHandle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        wavHeader.channels,  // channels
        wavHeader.sampleRate,  // sps
        1,  // software resample
        500000)) < 0) {   /* latency 0.5sec */

        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    
}


static void generate_sine(unsigned char *data, snd_pcm_format_t format, 
              struct wavHeader *wavHeader, snd_pcm_uframes_t offset, 
              int count, double *_phase, float freq)
{
    if (debug) {
      fprintf(stderr, "generate sine %f...\n",freq); 
      fflush(stderr);
    }
    static double max_phase = 2. * M_PI;
    double phase = *_phase;
    double step = max_phase*freq/(double)wavHeader->sampleRate;
    int format_bits = snd_pcm_format_width(format);
    unsigned int maxval = (1 << (format_bits - 1)) - 1;
    int bps = format_bits / 8;  /* bytes per sample */
    int phys_bps = snd_pcm_format_physical_width(format) / 8;
    int big_endian = snd_pcm_format_big_endian(format) == 1;
    int to_unsigned = snd_pcm_format_unsigned(format) == 1;
    int is_float = (format == SND_PCM_FORMAT_FLOAT_LE ||
            format == SND_PCM_FORMAT_FLOAT_BE);

    if (debug) {
      fprintf(stderr, "fill the channel areas count=%d...\n",count);
      fprintf(stderr, "format_bits=%d bigEndian=%d toUnsigned=%d\n",format_bits, big_endian, to_unsigned);
      fflush(stderr);
    }
    for (int k=0;k<count;++k) {
        union {
            float f;
            int i;
        } fval;
        int res, i;
        if (is_float) {
            fval.f = sin(phase);
            res = fval.i;
        } else {
            res = sin(phase) * maxval;
        }
        if (to_unsigned)
            res ^= 1U << (format_bits - 1);
        for (int chn = 0; chn < wavHeader->channels; chn++) {
            long wavIndex=k * wavHeader->bitsPerSample/8 + chn * wavHeader->bitsPerSample/8 ; 
            /* Generate data in native endian format */
            if (!big_endian) {
                for (i = 0; i < bps; i++)
                    *(data + k + phys_bps - 1 - i) = (res >> i * 8) & 0xff;
            } else {
                for (i = 0; i < bps; i++)
                    *(data + k + i) = (res >>  i * 8) & 0xff;
            }
        }
        phase += step;
        if (phase >= max_phase)
            phase -= max_phase;
    }
    *_phase = phase;
}

void playTone(snd_pcm_t* soundCardHandle, float freq) {
    if (debug) {
        fprintf(stderr, "palying tone %f...\n",freq); 
        fflush(stderr);
    }
    long dataSize = wavHeader.blockAlign*wavHeader.sampleRate*.5;

    void *data = malloc(dataSize);
    memset(data, 0, dataSize);

    chunkHeader chunkHeader;
    strncpy(chunkHeader.chunkID,"data",4);
    chunkHeader.chunkSize=dataSize;


    long samples = wavHeader.sampleRate*.45;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    double phase=1.0;
    if (freq>0) {
      generate_sine((unsigned char *)data, format, &wavHeader, 0, samples, &phase, freq); 
    }

     
    if (debug) {
        fprintf(stderr,"dataSize=%ld\n",dataSize);
        for (int i=0;i<20;++i) {
          int channel=0;
          for (channel=0;channel<wavHeader.channels;++channel) {
            int16_t sample;
            long wavIndex=i * wavHeader.bitsPerSample/8 + channel * wavHeader.bitsPerSample/8 ; 
            unsigned char *samples=(unsigned char*)data;
            memcpy(&sample, &samples[wavIndex], wavHeader.bitsPerSample / 8);
            fprintf(stderr,"i=%d channel=%d wavIndex=%d value=%ld ", i, channel, wavIndex, (long) sample );
          }
          fprintf(stderr,"\n");
        }
        fprintf(stderr, "write tone %f to sound card...\n",freq); 
        fflush(stderr);
    }
    int segmetSize = (wavHeader.bitsPerSample / 8) * wavHeader.channels;
    snd_pcm_sframes_t bytesWritten;
      bytesWritten = snd_pcm_writei(soundCardHandle, data, dataSize / segmetSize);

    if (debug) fprintf(stderr, "bytesWritten=%d\n",bytesWritten); fflush(stderr);
/*
    if (debug) fprintf(stderr, "checking overflow\n"); fflush(stderr);
    if (bytesWritten < 0) {
        bytesWritten = snd_pcm_recover(soundCardHandle, bytesWritten, 0);
    }
*/

    if (debug) fprintf(stderr, "verifying write status\n"); fflush(stderr);
    if (bytesWritten < 0) {
        fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(bytesWritten));
        exit(EXIT_FAILURE);
    }

    if (bytesWritten > 0 && bytesWritten < dataSize/segmetSize) {
        fprintf(stderr, "data write error (expected %li, wrote %li)\n", dataSize, bytesWritten);
    }


    free(data);
}
  
void drainSound(snd_pcm_t* soundCardHandle) {
    if (debug) fprintf(stderr, "flushing soundCardHandle\n"); fflush(stderr);
    int err = snd_pcm_drain(soundCardHandle);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_drain failed: %s\n", snd_strerror(err));
    }
}

void *demo1(void*) {
   demo1isRunning=true;

   pushHeader(soundCard1Handle);
   for (int i=0; yankeeDoodle[i]>=0; ++i) {
     playTone(soundCard1Handle, noteHz[yankeeDoodle[i]]);
   }
   drainSound(soundCard1Handle);
   demo1isRunning=false;
   pthread_exit(0);
}

void *demo2(void*) {
   demo2isRunning=true;

   pushHeader(soundCard2Handle);
   for (int i=0; yankeeDoodleBase[i]>=0; ++i) {
     playTone(soundCard2Handle, noteHz[yankeeDoodleBase[i]]/2);
   }
   drainSound(soundCard2Handle);
   demo2isRunning=false;
   pthread_exit(0);
}

int main(int argc, char **argv)
{


  if (!commandLineOptions(argc, argv)) {
    exit(EXIT_FAILURE);
  }

  if (!setup()) {
    printf("setup failed\n");
    exit(EXIT_FAILURE);
  }

   if (debug) {
     printf("intialization compelte\n"); 
     fflush(stderr);
   }

   long long start, end;

   start = currentTimeMillis();

   soundCard1Handle=openSoundCard(getenv("AUDIODEV"));
   soundCard2Handle=openSoundCard(getenv("AUDIODEV"));
   end = currentTimeMillis();
   printf("open elapsed=%lld\n",end-start);

   threadCreate(demo1, "demo1");
   threadCreate(demo2, "demo2");

   usleep(100 * 1000);

    while (demo1isRunning || demo2isRunning) {
      fflush(stdout);
      usleep(100 * 1000);
    }
//sleep(4294967295U);   // 3.27 years

    snd_pcm_close(soundCard1Handle);
    snd_pcm_close(soundCard2Handle);

	return 0;

}
