#!/bin/bash
#PBS -N deteff
#PBS -j oe
#PBS -o /mnt/home/jbradt/jobs/ar46/deteff/deteff.o
#PBS -l walltime=00:10:00
#PBS -l nodes=1:ppn=8
#PBS -l mem=500mb
#PBS -m a
#PBS -M jbradt@msu.edu

source activate np9
source ${HOME}/setenv_gcc.sh
export OMP_NUM_THREADS=8

OUTPUT_PATH=${HOME}/jobs/ar46/deteff
DETEFF_ROOT=${HOME}/Documents/Code/ar40-aug15/monte_carlo/deteff
CONFIG_PATH=${DETEFF_ROOT}/hpcc-deteff.yml
ELOSS_PATH=${OUTPUT_PATH}/eloss.h5
DB_PATH=${OUTPUT_PATH}/deteff.db

cd ${OUTPUT_PATH}

echo "Creating parameter set and eloss file"
python ${DETEFF_ROOT}/make_params.py ${CONFIG_PATH} ${ELOSS_PATH} ${DB_PATH}

echo "Running simulation"
deteff ${CONFIG_PATH} ${ELOSS_PATH} ${DB_PATH}

if [ $PBS_JOBID ]; then
  echo "Job statistics"
  qstat -f $PBS_JOBID
fi
