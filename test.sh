#!/bin/bash
#./bsnes/out/bsnes.app/Contents/MacOS/bsnes --script=alttp-nmi.as "/Users/jamesd/Desktop/personal/Zelda/Legend of Zelda, The - A Link to the Past (U) [!].smc"
./bsnes/out/bsnes.app/Contents/MacOS/bsnes "--script=$1" "./asm/alttp.smc"
