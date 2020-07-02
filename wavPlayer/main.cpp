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


#include <chrono>
#include <ctime>  
#include "main.h"


// Output Pins
int wavPin = 23;                     // gpio pin

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

int setup2() {
    int memfd;
    unsigned int timend;
    void* gpio_map, * timer_map, * int_map;

    memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (memfd < 0) {
        printf("Mem open error\n");
        return(0);
    }

    gpio_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
        MAP_SHARED, memfd, GPIO_BASE);

    timer_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
        MAP_SHARED, memfd, TIMER_BASE);

    int_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
        MAP_SHARED, memfd, INT_BASE);

    close(memfd);

    if (gpio_map == MAP_FAILED ||
        timer_map == MAP_FAILED ||
        int_map == MAP_FAILED) {
        printf("Map failed\n");
        return(0);
    }
    // interrupt pointer
    intrupt = (volatile unsigned*)int_map;
    // timer pointer
    timer = (volatile unsigned*)timer_map;
    ++timer;    // timer lo 4 bytes
                // timer hi 4 bytes available via *(timer+1)

                // GPIO pointers
    gpio = (volatile unsigned*)gpio_map;
    gpset = gpio + 7;     // set bit register offset 28
    gpclr = gpio + 10;    // clr bit register
    gpin = gpio + 13;     // read all bits register

        // setup  GPIO 2/3 = inputs    have pull ups on board
        //        control reg = gpio + 0 = pin/10
        //        GPIO 2 shift 3 bits by 6 = (pin rem 10) * 3
        //        GPIO 3 shift 3 bits by 9 = (pin rem 10) * 3


    return(1);
}

/******************** INTERRUPTS *************

Is this safe?
Dunno, but it works

interrupts(0)   disable interrupts
interrupts(1)   re-enable interrupts

return 1 = OK
       0 = error with message print

Uses intrupt pointer set by setup()
Does not disable FIQ which seems to
cause a system crash
Avoid calling immediately after keyboard input
or key strokes will not be dealt with properly

*******************************************/

int interrupts(int flag) {
    static unsigned int sav132 = 0;
    static unsigned int sav133 = 0;
    static unsigned int sav134 = 0;

    if (flag == 0)    // disable
    {
        if (sav132 != 0) {
            // Interrupts already disabled so avoid printf
            return(0);
        }

        if ((*(intrupt + 128) | *(intrupt + 129) | *(intrupt + 130)) != 0) {
            printf("Pending interrupts\n");  // may be OK but probably
            return(0);                       // better to wait for the
        }                                // pending interrupts to
                                         // clear

        sav134 = *(intrupt + 134);
        *(intrupt + 137) = sav134;
        sav132 = *(intrupt + 132);  // save current interrupts
        *(intrupt + 135) = sav132;  // disable active interrupts
        sav133 = *(intrupt + 133);
        *(intrupt + 136) = sav133;
    }
    else            // flag = 1 enable
    {
        if (sav132 == 0) {
            printf("Interrupts not disabled\n");
            return(0);
        }

        *(intrupt + 132) = sav132;    // restore saved interrupts
        *(intrupt + 133) = sav133;
        *(intrupt + 134) = sav134;
        sav132 = 0;                 // indicates interrupts enabled
    }
    return(1);
}



bool setup() {

	if (int ret = wiringPiSetup()) {
		fprintf(stderr, "Wiring Pi setup failed, ret=%d\n", ret);
		return false;
	}


    if (piHiPri(1)) {
        printf("high priority failed\n");
        return 2;
    }



	int seed;
	FILE *fp;
	fp = fopen("/dev/urandom", "r");
	fread(&seed, sizeof(seed), 1, fp);
	fclose(fp);
	srand(seed);

	pinMode(wavPin, OUTPUT);

	return true;
}                                             // end of setup function


bool usage() {
	fprintf(stderr, "usage: wavPlayer [-p gpio] [-d] [-v 0-100]\n");
	fprintf(stderr, "d = debug\n");
	fprintf(stderr, "p = gpio pin\n");
	fprintf(stderr, "v = volume\n");

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
		case 'p':
			sscanf(optarg, "%d", &wavPin);
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

	double maxAmplitude = pow(2, wavHeader.bitsPerSample - 1);
	int segmetSize = (wavHeader.bitsPerSample / 8)*wavHeader.numChannels;
	int dutyCycle = 1000000 / wavHeader.sampleRate;

	if (debug) {

		printf("output pin        %d\n", wavPin);
		printf("volume            %d\n", volume);

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
		printf("subChunk2 size    %d\n", wavHeader.subChunk2Size);
		printf("maxAmplitude      %d\n", (int)maxAmplitude);
		printf("segment size      %d\n", segmetSize);
		printf("duty cycle        %d us\n", dutyCycle); fflush(stdout);
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

    setup2();
    printf("waiting for interrupts to clear...\n");
    delay(2000);
    interrupts(0);

	for (int i = 0; i < wavFiles; ++i) {
		playFile(wavFileNames[i]);
	}

	fflush(stdout);
	//		delay(4294967295U);   // 3.27 years

    interrupts(1);

	return 0;

}
