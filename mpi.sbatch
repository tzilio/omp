#!/bin/bash
#SBATCH -p cpu
#SBATCH --job-name=ssp_mpi
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=6
#SBATCH --time=02:00:00
#SBATCH --output=logs/mpi_%j.out
#SBATCH --error=logs/mpi_%j.err

set -euo pipefail

BIN=./shsup_mpi
CSV=results/mpi_times.csv
REPS=${REPS:-20}
# ordem de np:
NP_LIST=(1 2 3 4 6 8 10 12)

mkdir -p logs results

# cabeçalho
if [ ! -s "$CSV" ]; then
  echo 'input,np,tempo,sd' > "$CSV"
fi

# garante que existe pelo menos um input
shopt -s nullglob
INPUTS=(inputs/*.txt)
if [ ${#INPUTS[@]} -eq 0 ]; then
  echo 'Nenhum arquivo em inputs/*.txt'
  exit 1
fi

# inputs em ordem (o shell já expande em ordem lexicográfica)
for INPUT in "${INPUTS[@]}"; do
  INBASE=$(basename "$INPUT")

  for NP in "${NP_LIST[@]}"; do
    # processos por nó (ceil(NP/nnodes))
    PPR=$(( (NP + SLURM_NNODES - 1) / SLURM_NNODES ))

    TIMES=()

    # roda REPS vezes e guarda tempos
    for r in $(seq 1 "$REPS"); do
      OUT=$(mpirun -np "$NP" --map-by "ppr:${PPR}:node" "$BIN" < "$INPUT")
      T=$(echo "$OUT" | tail -n1)
      TIMES+=("$T")
      echo "MPI $INBASE np=$NP rep=$r tempo=${T}s"
    done

    # média e desvio-padrão (amostral) dos tempos
    read -r MEAN SD <<< "$(
      printf '%s\n' "${TIMES[@]}" | awk '
        { n++; s += $1; ss += $1*$1 }
        END {
          if (n > 0) {
            m = s / n;
            if (n > 1) {
              sd = sqrt((ss - n*m*m) / (n-1));
            } else {
              sd = 0;
            }
            printf "%.6f %.6f", m, sd;
          } else {
            printf "0.000000 0.000000";
          }
        }')"

    echo "$INBASE,$NP,$MEAN,$SD" >> "$CSV"
    echo "MPI OK: $INBASE np=$NP tempo(média)=${MEAN}s sd=${SD}"
  done
done