#!/bin/bash
#SBATCH -p cpu
#SBATCH --job-name=ssp_seq
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --time=02:00:00
#SBATCH --output=logs/seq_%j.out
#SBATCH --error=logs/seq_%j.err

set -euo pipefail

BIN=./shsup_seq
CSV=results/seq_times.csv
REPS=${REPS:-20}

mkdir -p logs results

if [ ! -s "$CSV" ]; then
  echo "input,tempo,sd" > "$CSV"
fi

for INPUT in $(ls inputs/*.txt | sort -V); do
  INBASE=$(basename "$INPUT")

  declare -a TIMES=()

  for r in $(seq 1 "$REPS"); do
    # Apenas executa sequencialmente
    T=$("$BIN" < "$INPUT" | tail -n1)
    TIMES+=("$T")
    echo "SEQ $INBASE rep=$r tempo=${T}s"
  done

  read -r MEAN SD < <(
    printf '%s\n' "${TIMES[@]}" \
    | awk '{n++; s+=$1; ss+=$1*$1} END{
          m=s/n; sd=(n>1?sqrt((ss-n*m*m)/(n-1)):0);
          printf "%.6f %.6f", m, sd}'
  )

  echo "$INBASE,$MEAN,$SD" >> "$CSV"
  echo "SEQ OK: $INBASE média=${MEAN}s sd=${SD}"
done
