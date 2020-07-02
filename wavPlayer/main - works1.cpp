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

#include <chrono>
#include <ctime>  
#include "main.h"


// Output Pins
#define WavPin           23                     // pin on the pca9865

bool debug = false;
char *wavFileNames[32];
int wavFiles = 0;
int volume = 100;


struct wavHeader
{
	char     chunkID[4];
	int32_t  chunckSize;
	char     format[4];
	char     subChunk1ID[4];
	int32_t  subChunk1Size;
	int16_t  audioFormat;
	int16_t  numChannels;
	int32_t  sampleRate;
	int32_t  byteRate;
	int16_t  blockAlign;
	int16_t  bitsPerSample;
	char     subChunk2ID[4];
	int32_t  subChunk2Size;
};



bool setup() {

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

	pinMode(WavPin, OUTPUT);

	return true;
}                                             // end of setup function


bool usage() {
	fprintf(stderr, "usage: wavPlayer [-d] [-v 0-100]\n");
	fprintf(stderr, "d = debug\n");
	fprintf(stderr, "v = volume\n");

	return false;
}

bool commandLineOptions(int argc, char **argv) {
	int c, index;

	if (argc < 2) {
		return usage();
	}

	while ((c = getopt(argc, argv, "dv:")) != -1)
		switch (c) {
		case 'd':
			debug = true;
			break;
		case 'v':
			sscanf(optarg, "%f", &volume);
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



void playFile(char *filename) {
	FILE *wav = fopen(filename, "r");
	
	if (wav == NULL) {
		printf("Cannot open %s\n", filename);
		return;
	}
	printf("Playing %s...\n", filename);

	wavHeader wavHeader;
	

	fread(&wavHeader, sizeof(wavHeader), 1, wav);

	if (debug) {
		printf("chunk Id          %4.4s\n", wavHeader.chunkID);
		printf("chunck size       %d\n", wavHeader.chunckSize);
		printf("format            %4.4s\n", wavHeader.format);
		printf("subChunk1 ID      %4.4s\n", wavHeader.subChunk1ID);
		printf("subChunk1 Size    %d\n", wavHeader.subChunk1Size);
		printf("audio format      %d\n", wavHeader.audioFormat);
		printf("num channels      %d\n", wavHeader.numChannels);
		printf("sample rate       %d\n", wavHeader.sampleRate);
		printf("byte rate         %d\n", wavHeader.byteRate);
		printf("block align       %d\n", wavHeader.blockAlign);
		printf("bits per sample   %d\n", wavHeader.bitsPerSample);
		printf("subChunk2 ID      %4.4s\n", wavHeader.subChunk2ID);
		printf("subChunk2 Size    %d\n", wavHeader.subChunk2Size);
	}

	if (wavHeader.bitsPerSample != 16) {
		printf("bits per sample must be 16, found %d\n", wavHeader.bitsPerSample);
		return;
	}

	char *data = (char *)malloc(wavHeader.subChunk2Size);

	long size=fread(data, 1, wavHeader.subChunk2Size, wav);

	if (size != wavHeader.subChunk2Size) {
		printf("Could not read wav data segment, subChunk2Size=%d, but only read %d bytes\n",wavHeader.subChunk2Size,size);
		return;
	}
	fclose(wav);

	int segmetSize = (wavHeader.bitsPerSample / 8)*wavHeader.numChannels;
	int d = 1000000/wavHeader.sampleRate;

	if (debug) {
		printf("delay=%d us\n", d); fflush(stdout);
	}

	double maxAmplitude=0;

	for (int i = 0; i < wavHeader.subChunk2Size / segmetSize; i += segmetSize) {
		int16_t sample;
		memcpy(&sample, &data[i], wavHeader.bitsPerSample / 8);

		if (maxAmplitude < sample) {
			maxAmplitude = sample;
		}
	}

	if (debug) {
		printf("maxAmplitude=%d\n", (int)maxAmplitude);
	}
	
	int k = 0;
	int lastSample = 0;
	for (int i = 0; i < wavHeader.subChunk2Size / segmetSize; i += segmetSize) {
		int16_t sample;
		memcpy(&sample, &data[i], wavHeader.bitsPerSample/8);

		double p = (sample + maxAmplitude) / maxAmplitude / 2;

		int d1 = d * p;
		int d2 = d - d1;

		digitalWrite(WavPin, 1);
		delayMicroseconds(d1-1);

		digitalWrite(WavPin, 0);		
		delayMicroseconds(d2);
	}

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

	fflush(stdout);
	//		delay(4294967295U);   // 3.27 years
	return 0;

}
