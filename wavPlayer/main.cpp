/*
*
*  $ sudo apt install libasound2-dev
* 
*  $ aplay -L | grep ^default     << to find your sound card
*  export AUDIODEV="default:CARD=Device"   << your sound card
*/

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <lcd.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>

#include <chrono>
#include <ctime>  
#include "main.h"


char* soundCardName;
snd_pcm_t* soundCardHandle;

bool debug = false;
char *wavFileNames[32];
int wavFiles = 0;
int volume = 100;


struct wavHeader
{
    char     chunkID[4];      // "RIFF" - literal
    int32_t  fileSize;        // size of entire file minus 8 bytes
    char     format[4];       // "WAVE" - big-endian format
    char     subChunk1ID[4];  // "fmt " - literal
    int32_t  subChunk1Size;   // size of sub-chunk1 minus 8 bytes
    int16_t  audioFormat;     // 1=PCM, uncompressed
    int16_t  numChannels;     // 1=Mono, 2=Stereo, etc.
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
#define GPIO_BASE  0x20200000
#define TIMER_BASE 0x20003000
#define INT_BASE 0x2000B000

/***************** SETUP ****************
Sets up five GPIO pins as described in comments
Sets timer and interrupt pointers for future use
Does not disable interrupts
return 1 = OK
       0 = error with message print
************************************/



bool setup() {
    int err;

	if (int ret = wiringPiSetup()) {
		fprintf(stderr, "Wiring Pi setup failed, ret=%d\n", ret);
		return false;
	}



	int seed;
	FILE *fp;
	fp = fopen("/dev/urandom", "r");
	fread(&seed, sizeof(seed), 1, fp);
	fclose(fp);
	srand(seed);


    soundCardName = getenv("AUDIODEV");

    
    if ((err = snd_pcm_open(&soundCardHandle, soundCardName, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }



	return true;
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

	if (argc < 2) {
		return usage();
	}

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



	for (index = optind; index < argc; index++) {
		wavFileNames[wavFiles++] =argv[index];
	}

	if (wavFiles < 1) {
		printf("please supply at least 1 wav file to play\n");
		return false;
	}
	return true;
}



void playFile(char* filename) {
    int err;
    FILE* wav = fopen(filename, "r");

    if (wav == NULL) {
        printf("Cannot open %s\n", filename);
        return;
    }
    printf("Playing %s...\n", filename);


    wavHeader wavHeader;


    fread(&wavHeader, sizeof(wavHeader), 1, wav);

    double maxAmplitude = pow(2, wavHeader.bitsPerSample - 1);
    int segmetSize = (wavHeader.bitsPerSample / 8) * wavHeader.numChannels;
    int dutyCycle = 1000000 / wavHeader.sampleRate;

    if (debug) {

        printf("output device     %s\n", soundCardName);
        printf("volume            %d\n", volume);

        printf("chunk Id          %4.4s\n", wavHeader.chunkID);
        printf("chunck size       %d\n", wavHeader.fileSize);
        printf("format            %4.4s\n", wavHeader.format);
        printf("subChunk1 ID      %4.4s\n", wavHeader.subChunk1ID);
        printf("subChunk1 Size    %d\n", wavHeader.subChunk1Size);
        printf("audio format      %d\n", wavHeader.audioFormat);
        printf("num channels      %d\n", wavHeader.numChannels);
        printf("sample rate       %d\n", wavHeader.sampleRate);
        printf("byte rate         %d\n", wavHeader.byteRate);
        printf("block align       %d\n", wavHeader.blockAlign);
        printf("bits per sample   %d\n", wavHeader.bitsPerSample);
        printf("maxAmplitude      %d\n", (int)maxAmplitude);
        printf("segment size      %d\n", segmetSize);
        printf("duty cycle        %d us\n", dutyCycle); fflush(stdout);
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


    if ((err = snd_pcm_set_params(soundCardHandle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        wavHeader.numChannels,  // channels
        wavHeader.sampleRate,  // sps
        1,  // software resample
        500000)) < 0) {   /* latency 0.5sec */

        printf("Playback open error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }


    void *data = NULL;

    while (true) {
        chunkHeader chunkHeader;

        long rs = fread(&chunkHeader, sizeof(chunkHeader), 1, wav);

        if (rs != 1) {
            break;
        }

        if (strncmp(chunkHeader.chunkID, "data", 4) != 0) {
            fseek(wav, chunkHeader.chunkSize, SEEK_CUR);
            continue;
        }

        long dataSize = chunkHeader.chunkSize;
        data = realloc(data, dataSize);

        rs = fread(data, dataSize, 1, wav);

        if (rs != 1) {
            fprintf(stderr, "unexpected error, %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        snd_pcm_sframes_t bytesWritten = snd_pcm_writei(soundCardHandle, data, dataSize / segmetSize);

        if (debug) fprintf(stderr, "checking overflow\n"); fflush(stderr);
        if (bytesWritten < 0) {
            bytesWritten = snd_pcm_recover(soundCardHandle, bytesWritten, 0);
        }

        if (debug) fprintf(stderr, "verifying write status\n"); fflush(stderr);
        if (bytesWritten < 0) {
            fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(bytesWritten));
            exit(EXIT_FAILURE);
        }

        if (bytesWritten > 0 && bytesWritten < dataSize/segmetSize) {
            fprintf(stderr, "data write error (expected %li, wrote %li)\n", dataSize, bytesWritten);
        }
    }

    if (debug) fprintf(stderr, "flushing soundCardHandle\n"); fflush(stderr);
    err = snd_pcm_drain(soundCardHandle);
    if (err < 0) {
        fprintf(stderr, "snd_pcm_drain failed: %s\n", snd_strerror(err));
    }

    free(data);
    fclose(wav);

    
    /*
	int k = 0;
	int lastSample = 0;
	for (int32_t i = 0; i < wavHeader.subChunk2Size; i += segmetSize) {
		int16_t sample;
		memcpy(&sample, &data[i], wavHeader.bitsPerSample/8);

		double percent =   ((sample+maxAmplitude)/maxAmplitude)/2;

		int d1 = dutyCycle * percent;
		int d2 = dutyCycle - d1;

		if (debug) {
			if (d1 < 0 || d2 < 0 || d1>dutyCycle || d2>dutyCycle || percent < 0 || percent>1) {
				printf("error, s=%d, p=%6.4f d1=%d d2=%d\n", sample, percent, d1, d2);  fflush(stdout);
			}
		}

		digitalWrite(wavPin, 1);
		delayMicroseconds(d1);

		digitalWrite(wavPin, 0);		
		delayMicroseconds(d2);
	}
*/
}

int main(int argc, char **argv)
{
	setuid(0);

	if (!commandLineOptions(argc, argv)) {
		return 1;
	}

	if (!setup()) {
		printf("setup failed\n");
		return 1;
	}


	for (int i = 0; i < wavFiles; ++i) {
		playFile(wavFileNames[i]);
	}

    /*
    while (true) {
        sleep(1000);
        fflush(stdout);
    }
    */
	//		delay(4294967295U);   // 3.27 years

    snd_pcm_close(soundCardHandle);

	return 0;

}
