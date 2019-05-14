#!/bin/sh
#$ -cwd
#$ -N ndt
#$ -pe mpi 10
#$ -j y
/usr/lib64/mpich/bin/mpirun -np $NSLOTS ./ndt -b r -f 3 -d 4 -s scenes/hypercube.so
