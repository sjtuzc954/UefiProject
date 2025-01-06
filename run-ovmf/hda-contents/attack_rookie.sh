#!/bin/bash

VAR_PATH="/sys/firmware/efi/efivars/MyVariable-a3a56e56-1d23-06dc-24bf-1473ff54e629"

if [ -e $VAR_PATH ]; then
    chattr -i $VAR_PATH
    rm $VAR_PATH
    echo "Removed $VAR_PATH"
fi

if [ ! -x "v2p" ]; then
    gcc -o v2p mnt/v2p.c
    gcc -o smi mnt/smi.c
fi

./v2p $*

if [ $? -eq 0 ]; then
    ./smi
fi
