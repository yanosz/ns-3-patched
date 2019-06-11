#!/bin/bash

for run in `seq 1 50`; do
	for staNum in 2 8 32 64; do
		for mode in minstrel OfdmRate6Mbps OfdmRate18Mbps OfdmRate36Mbps OfdmRate54Mbps HeMcs7 HeMcs9 HeMcs10 HeMcs11; do
			./expose --run=$run --wifiMode=$mode --staNum=$staNum			
		done
	done
done