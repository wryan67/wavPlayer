wavPlayer
---------
Read standard .wav file and write PCM data to ALSA driver. 

## Prerquisite

To compile, you will need to install the ALSA dev library:

    $ sudo apt install libasound2-dev
    
## Compiling

    make


## Device discovery
Use aplay to find your default devices:

    $ aplay -L | grep ^default
    default:CARD=ALSA
    default:CARD=Device

## Environment variable

Setup your audio device like this:

    $ export AUDIODEV="default:CARD=Device"
    
## Executing
After you find your sound card and setup the AUDIODEV environment variable, you should be ready to test.

    $ ./wavPlayer -d ~/rr-crossing-bells.48k.wav
    Playing /home/wryan/rr-crossing-bells.48k.wav...
    output device     default:CARD=Device
    volume            100
    chunk Id          RIFF
    chunck size       2761084
    format            WAVE
    subChunk1 ID      fmt
    subChunk1 Size    16
    audio format      1
    num channels      1
    sample rate       48000
    byte rate         96000
    block align       2
    bits per sample   16
    maxAmplitude      32768
    segment size      2
    duty cycle        20 us
    checking overflow
    verifying write status
    flushing soundCardHandle

