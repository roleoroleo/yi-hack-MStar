#!/bin/bash

        IDX="0000"
        N=0
        while [ "$IDX" -eq "0000" ] && [ $N -lt 50 ]; do
            IDX="0000"
            echo $N
            N=$[$N+1]
            sleep 0.2
        done
