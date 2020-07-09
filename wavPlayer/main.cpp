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
#include <poll.h>

#include <sys/mman.h>
#include <sys/time.h>

#include <stdint.h>
#include <wiringPiSPI.h>

#include <chrono>
#include <ctime>  
#include "main.h"

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#define channel_A   0x30
#define channel_B   0x34

#define DAC_Value_MAX  65535

#define DAC_VREF  3.3

#define DEV_CS_PIN      4

void DAC8532_Out_Voltage(UBYTE Channel, float Voltage);



// Input Pins
int externalClockPin = 25;
int externalClockBCMPin = 26;
#define CLOCK_FREQ 65536


bool debug = false;
char *wavFileNames[32];
int wavFiles = 0;
int volume = 100;

static volatile bool   isPlaying = false;

static volatile double currentDuration = 999 * 1000; // us
static volatile double clockTick = 1000000.0 / CLOCK_FREQ; // us
static volatile long  sampleClockCounter = 0;
static volatile long  swipe = 0;
static volatile int   flap = 0;
static volatile int   playingNote = -1;
static volatile double dutyCycle;
static volatile long  wavIndex = -1;
static volatile int   wavSegmentSize;
char* wavData;
long maxAmplitude;

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
} wavHeader;



pthread_t threadCreate(void* (*method)(void*), const char* description) {
    pthread_t threadId;
    int status = pthread_create(&threadId, NULL, method, NULL);
    if (status != 0) {
        printf("%s::thread create failed %d--%s\n", description, status, strerror(errno));
        exit(9);
    }
    pthread_detach(threadId);
    return threadId;
}


unsigned long long currentTimeMillis() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);

    return
        (unsigned long long)(currentTime.tv_sec) * 1000 +
        (unsigned long long)(currentTime.tv_usec) / 1000;
}


void SPI_WriteByte(uint8_t value) {
    int read_data;
    read_data = wiringPiSPIDataRW(0, &value, 1);
    if (read_data < 0)
        perror("wiringPiSPIDataRW failed\r\n");
}

static void Write_DAC8532(UBYTE Channel, UWORD Data) {
    digitalWrite(DEV_CS_PIN, 1);
    digitalWrite(DEV_CS_PIN, 0);
    SPI_WriteByte(Channel);
    SPI_WriteByte((Data >> 8));
    SPI_WriteByte((Data & 0xff));
    digitalWrite(DEV_CS_PIN, 1);
}

void DAC8532_Out_Voltage(UBYTE Channel, double Voltage) {
    UWORD temp = 0;
    if ((Voltage <= DAC_VREF) && (Voltage >= 0)) {
        temp = (UWORD)(Voltage * DAC_Value_MAX / DAC_VREF);
        Write_DAC8532(Channel, temp);
    }
}

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


	return true;
}                                             // end of setup function


bool usage() {
	fprintf(stderr, "usage: wavPlayer [-b bck] [-c channel] [-d data] [-g] [-v 0-100]\n");
    fprintf(stderr, "b = bck pin\n");
    fprintf(stderr, "c = channel pin\n");
    fprintf(stderr, "d = data pin\n");
    fprintf(stderr, "g = debug\n");
	fprintf(stderr, "p = gpio pin\n");
    fprintf(stderr, "x = external clock pin");
    fprintf(stderr, "x = external clock BCM pin");
    fprintf(stderr, "v = volume\n");

	return false;
}

bool commandLineOptions(int argc, char **argv) {
	int c, index;

	if (argc < 2) {
		return usage();
	}


	while ((c = getopt(argc, argv, "c:b:d:gv:x:y:")) != -1)
		switch (c) {
        case 'g':
			debug = true;
			break;
		case 'v':
			sscanf(optarg, "%d", &volume);
			break;
        case 'x':
            sscanf(optarg, "%d", &externalClockPin);
            break;
        case 'y':
            sscanf(optarg, "%d", &externalClockBCMPin);
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

void wDelay(int delay) {
    
    for (int i = 0; i < delay; ++i) {
        
    }
    
}

void playFile(char *filename) {
	FILE *wav = fopen(filename, "r");
	
	if (wav == NULL) {
		printf("Cannot open %s\n", filename);
		return;
	}
	printf("Playing %s...\n", filename);


	

	fread(&wavHeader, sizeof(wavHeader), 1, wav);

	maxAmplitude = pow(2, wavHeader.bitsPerSample - 1);
    wavSegmentSize = (wavHeader.bitsPerSample / 8)*wavHeader.numChannels;
    dutyCycle = (1000000.0 / wavHeader.sampleRate)+0.5; // us   1mm / SPS

	if (debug || 1) {
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
		printf("maxAmplitude      %ld\n", maxAmplitude);
        printf("segment size      %d\n", wavSegmentSize);
        printf("clockTick us      %9.4lf\n", clockTick);
        printf("noteDuration us   %9.4lf\n", dutyCycle);
    }

	if (wavHeader.bitsPerSample != 16) {
		printf("bits per sample must be 16, found %d\n", wavHeader.bitsPerSample);
		return;
	}

	wavData = (char *)malloc(wavHeader.subChunk2Size);

	long wavSize=fread(wavData, 1, wavHeader.subChunk2Size, wav);

	if (wavSize != wavHeader.subChunk2Size) {
		printf("Could not read wav data segment, subChunk2Size=%d, but only read %d bytes\n",wavHeader.subChunk2Size,wavSize);
		return;
	}
	fclose(wav);

    isPlaying = true;

}

static volatile double waveVolts;

void* outputVoltage(void*) {
    DAC8532_Out_Voltage(channel_B, waveVolts);
    pthread_exit(0);
}

void play() {

    currentDuration += clockTick;
    long cd = currentDuration + 0.5;

    if (cd >= dutyCycle) {
        currentDuration = 0.0;
        if (wavIndex < 0) {
            wavIndex = 0;
        } else {
            wavIndex += wavSegmentSize;
        }
        if (wavIndex >= wavHeader.subChunk2Size) {
            exit(0);
        }

        int16_t sample;

        memcpy(&sample, &wavData[wavIndex], wavHeader.bitsPerSample / 8);

        waveVolts = DAC_VREF * (((1.0*sample + maxAmplitude) / maxAmplitude/2)) ;

        if (debug) {
            printf("wavIndex=%d sample=%-6d volts: %9.6f\n", wavIndex, sample, waveVolts);
        }
        DAC8532_Out_Voltage(channel_B, waveVolts);
        //threadCreate(outputVoltage, "outputVoltage");
    }
}

void* takeSamplePolling(void*) {
    struct pollfd pfd;
    int    fd;
    char   buf[128];

    pinMode(externalClockPin, INPUT);
    sprintf(buf, "gpio export %d in", externalClockBCMPin);
    system(buf);
    sprintf(buf, "/sys/class/gpio/gpio%d/value", externalClockBCMPin);

    if ((fd = open(buf, O_RDONLY)) < 0) {
        fprintf(stderr, "Failed, gpio %d not exported.\n", externalClockBCMPin);
        exit(1);
    }

    pfd.fd = fd;
    pfd.events = POLLPRI;

    char lastValue = 0;
    int  xread = 0;

    lseek(fd, 0, SEEK_SET);    /* consume any prior interrupt */
    read(fd, buf, sizeof buf);

    while (true) {
        //  poll(&pfd, 1, -1);         /* wait for interrupt */
        lseek(fd, 0, SEEK_SET);    /* consume interrupt */
        xread = read(fd, buf, sizeof(buf));

        if (xread > 0) {
            if (buf[0] != lastValue) {
                lastValue = buf[0];
                //              if ((0x01&lastValue)==1) play();
                if (isPlaying) {
                    play();
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
	if (!commandLineOptions(argc, argv)) {
		return 1;
	}

	if (!setup()) {
		printf("setup failed\n");
		return 1;
	}

    wiringPiSPISetupMode(0, 6200000, 1);
    pinMode(DEV_CS_PIN, OUTPUT);

    piHiPri(99);
    isPlaying = false;
    threadCreate(takeSamplePolling, "takeSamplePoling");

	playFile(wavFileNames[0]);

    while (true) {
        fflush(stdout);
        fflush(stderr);
        delay(1000);
    }
	delay(4294967295U);   // 3.27 years

	return 0;

}
