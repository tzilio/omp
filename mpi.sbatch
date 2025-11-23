#!/bin/bash
#SBATCH -p cpu
#SBATCH --job-name=ssp_mpi
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=6
#SBATCH --time=02:00:00
#SBATCH --output=logs/mpi_%j.out
#SBATCH --error=logs/mpi_%j.err

set -euo pipefail

BIN=./ssp_mpi
CSV=results/mpi_times.csv
REPS=${REPS:-20}
# ordem de np:
NP_LIST=(1 2 3 4 6 8 10 12)

mkdir -p logs results

# cabeçalho
if [ ! -s "$CSV" ]; then
  echo "input,np,tempo,sd" > "$CSV"
fi

# inputs em ordem crescente
for INPUT in $(ls inputs/*.txt | sort -V); do
  INBASE=$(basename "$INPUT")

  for NP in "${NP_LIST[@]}"; do
    # processos por nó (ceil(NP/nnodes))
    PPR=$(( (NP + SLURM_NNODES - 1) / SLURM_NNODES ))

    # roda REPS vezes e guarda tempos
    declare -a TIMES=()
    for r in $(seq 1 "$REPS"); do
      T=$(mpirun -np "$NP" --map-by "ppr:${PPR}:node" "$BIN" < "$INPUT" | tail -n1)
      TIMES+=("$T")
      echo "MPI $INBASE np=$NP rep=$r tempo=${T}s"
    done

    # média e desvio-padrão (amostral)
    read -r MEAN SD < <(printf '%s\n' "${TIMES[@]}" \
      | awk '{n++; s+=$1; ss+=$1*$1} END{m=s/n; sd=(n>1?sqrt((ss-n*m*m)/(n-1)):0); printf "%.6f %.6f", m, sd}')

    echo "$INBASE,$NP,$MEAN,$SD" >> "$CSV"
    echo "MPI OK: $INBASE np=$NP tempo(média)=${MEAN}s sd=${SD}"
  done
done