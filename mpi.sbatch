#!/bin/bash
#SBATCH -p cpu
#SBATCH --job-name=ssp_mpi
#SBATCH --nodes=2                     # limite máximo permitido no Nomade
#SBATCH --ntasks-per-node=6          # 6 cores por nó
#SBATCH --time=02:00:00
#SBATCH --output=logs/mpi_%j.out
#SBATCH --error=logs/mpi_%j.err

set -euo pipefail

BIN=./shsup_mpi
CSV=results/mpi_times.csv
REPS=${REPS:-20}

# np válidos dentro do cluster
NP_LIST=(1 2 3 4 6 8 10 12)

mkdir -p logs results

if [ ! -s "$CSV" ]; then
  echo "input,np,tempo,sd" > "$CSV"
fi

for INPUT in $(ls inputs/*.txt | sort -V); do
  INBASE=$(basename "$INPUT")

  for NP in "${NP_LIST[@]}"; do

    # tasks por nó (divisão inteira), seguindo o manual
    # exemplo: NP=12, nodes=2 => PPR=6
    PPR=$(( (NP + SLURM_NNODES - 1) / SLURM_NNODES ))

    declare -a TIMES=()

    for r in $(seq 1 "$REPS"); do
      # mpirun conforme o manual do Nomade
      T=$(mpirun -np "$NP" --map-by "ppr:${PPR}:node" "$BIN" < "$INPUT" | tail -n1)
      TIMES+=("$T")
      echo "MPI $INBASE np=$NP rep=$r tempo=${T}s"
    done

    read -r MEAN SD < <(
      printf '%s\n' "${TIMES[@]}" \
      | awk '{n++; s+=$1; ss+=$1*$1} END{
             m=s/n; sd=(n>1?sqrt((ss-n*m*m)/(n-1)):0);
             printf "%.6f %.6f", m, sd}'
    )

    echo "$INBASE,$NP,$MEAN,$SD" >> "$CSV"
    echo "MPI OK: $INBASE np=$NP média=${MEAN}s sd=${SD}"
  done
done
