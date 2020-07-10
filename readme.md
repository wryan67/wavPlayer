wavPlayer
---------
Read standard .wav file and write PCM data to ALSA driver.  If you're not familiar with ALSA, it's a very good and mature framework that provides an universal API to access and use sound card devices.  I'm currently using it to play music over an USB sound card on a Raspberry Pi zero.  Here are some links which describe ALSA in more detail. 

* https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture
* https://www.alsa-project.org/wiki/Main_Page
* https://www.alsa-project.org/alsa-doc/alsa-lib/

## Prerquisite

To compile, you will need to install the ALSA dev library:

    $ sudo apt install libasound2-dev
    
## Download & switching branches
After cloning the repository, you'll need to switch to the alsa branch:

    $ git clone https://github.com/wryan67/wavPlayer.git
    $ cd wavPlayer
    $ git checkout alsa
    
## Compiling

    make


## Device discovery
Use aplay to find your default devices.  I typically use the grep ^default because aplay -L by itself reads like a dictionary. 

    $ aplay -L | grep ^default
    default:CARD=ALSA
    default:CARD=Device

Or use my handy shell script which gives a little more info than the above command, and also provides the audio environment variable needed in the next step:

    $ ./listSoundCards.sh
    bcm2835 ALSA, bcm2835 ALSA
      export AUDIODEV='default:CARD=ALSA'

    Generic USB Audio Device, USB Audio
      export AUDIODEV='default:CARD=Device'


## Environment variable

Setup your audio device like this:

    $ export AUDIODEV='default:CARD=Device'

I recommend putting the export command in your .bashrc script so that it is available after each reboot.

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

