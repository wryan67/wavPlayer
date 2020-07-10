#!/bin/bash

aplay -L | sed 's/^ *//' |
awk '{
  if (/^default/) {
    device=$0
    getline
    print
    printf("  export AUDIODEV=%c%s%c\n\n", 39, device, 39)
  }
}'

