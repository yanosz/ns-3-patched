#!/bin/bash

for run in `seq 1 20`; do
	#for staNum in 2 8 32 64; do
		for mode in minstrel OfdmRate6Mbps OfdmRate18Mbps OfdmRate36Mbps OfdmRate54Mbps HeMcs6 HeMcs7 HeMcs9 HeMcs10 HeMcs11; do
			./waf --run "expose --run=$run --wifiMode=$mode --staNum=2"   
			./waf --run "expose --run=$run --wifiMode=$mode --staNum=8"	  		
			./waf --run "expose --run=$run --wifiMode=$mode --staNum=32"  			
			./waf --run "expose --run=$run --wifiMode=$mode --staNum=64"  			
			./waf --run "expose --run=$run --wifiMode=$mode --staNum=128" 			
		done
	#done
done