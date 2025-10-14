#!/usr/bin/env bash
set -euo pipefail

# ===========================
# Parâmetros
# ===========================
REPS=${REPS:-20}
THREADS="${THREADS:-1 2 4 8 16 32}"
INPUTS=${INPUTS:-"inputs/*.txt inputs/*.in"}

# Binaries
OMP="${OMP:-./shsup_omp_not_compat}"
OMP_SERIAL="${OMP_SERIAL:-./shsup_omp_not_compat_serial}"

# CSV de saída
CSV_FILE="${CSV_OUT:-results_pl.csv}"

# ===========================
# Utils
# ===========================
expand_and_sort() {
  local patterns=("$@"); local tmp=()
  shopt -s nullglob
  for pat in "${patterns[@]}"; do for f in $pat; do tmp+=("$f"); done; done
  printf '%s\n' "${tmp[@]}" | sort -V
}

# média e desvio (Welford)
mean_sd() {
  awk '
    function isnum(s){ return (s ~ /^-?[0-9]+([.][0-9]+)?$/) }
    {
      gsub(",","."); if (!isnum($1)) next;
      x=$1+0; n+=1; d=x-m; m+=d/n; m2+=d*(x-m)
    }
    END {
      if (n<1) { printf("NaN,NaN\n"); exit 1 }
      sd = (n>1)?sqrt(m2/(n-1)):0
      printf("%.6f,%.6f\n", m, sd)
    }'
}

# Extrai (total, parallel, seq_frac) da 1ª linha do STDERR (tempo do OMP)
omp_triplet_once() {
  local infile="$1" threads="$2" bin="$3"
  local line
  line="$(( OMP_NUM_THREADS="$threads" "$bin" < "$infile" 1>/dev/null ) 2>&1 | tr -d '\r' | sed -n '1p')"
  awk -v L="$line" '
    BEGIN{
      gsub(",",".",L);
      n=split(L, a, /[[:space:]]+/);
      for(i=1;i<=n;i++){
        if (a[i] ~ /^-?[0-9]+([.][0-9]+)?$/) nums[++c]=a[i]
      }
      if (c<3) { exit 2 }
      printf("%s,%s,%s\n", nums[1], nums[2], nums[3])
    }'
}

# ===========================
# Cabeçalho + CSV
# ===========================
printf "\n==== Config ====\n"
echo "REPS     = $REPS"
echo "THREADS  = $THREADS"
echo "INPUTS   = $INPUTS"
printf "============\n\n"

# Cabeçalho final simplificado
echo "input,threads,T_seq,T_seq_sd,T_omp,T_omp_sd,S_justo,Eficiência" > "$CSV_FILE"

printf "%-18s %-6s %-10s %-10s %-12s %-12s %-12s %-12s\n" \
       "input" "p" "T_seq" "T_seq_sd" "T_omp" "T_omp_sd" "S_justo" "Eficiência"
printf "%s\n" "----------------------------------------------------------------------------------------------------"

# ===========================
# Loop
# ===========================
mapfile -t files < <(expand_and_sort $INPUTS)
if (( ${#files[@]} == 0 )); then
  echo "Nenhum arquivo casa com padrões: $INPUTS" >&2
  exit 1
fi

for in_file in "${files[@]}"; do
  base="$(basename "$in_file")"
  echo "Base: $base"

  # Coleta tempos do Serial-Equivalente (1 thread)
  ser_list=""
  for ((i=1;i<=REPS;i++)); do
    triplet="$(omp_triplet_once "$in_file" 1 "$OMP_SERIAL" 2>/dev/null || true)"
    if [[ -z "$triplet" ]]; then
      echo "ERRO: não achei 3 números no stderr do OMP_SERIAL para '$in_file'." >&2
      exit 1
    fi
    IFS=, read -r ser_total _ _ <<<"$triplet"
    ser_list+="$ser_total"$'\n'
  done
  IFS=, read -r TSEQ TSEQ_SD <<<"$(printf "%s" "$ser_list" | mean_sd)"
  printf "  T_seq (serEQ) mean=%s  sd=%s\n" "$TSEQ" "$TSEQ_SD"

  # Mede OMP (total)
  for p in $THREADS; do
    omp_total_list=""
    for ((i=1;i<=REPS;i++)); do
      triplet="$(omp_triplet_once "$in_file" "$p" "$OMP" 2>/dev/null || true)"
      if [[ -z "$triplet" ]]; then
        echo "ERRO: não achei 3 números no stderr do OMP (p=$p) para '$in_file'." >&2
        exit 1
      fi
      IFS=, read -r tt _ _ <<<"$triplet"
      omp_total_list+="$tt"$'\n'
    done

    IFS=, read -r TOMP TOMP_SD <<<"$(printf "%s" "$omp_total_list" | mean_sd)"

    # Speedup e eficiência
    SJUSTO="$(awk -v a="$TSEQ" -v b="$TOMP" 'BEGIN{if(b>0) printf("%.6f", a/b); else print "Inf"}')"
    EFF="$(awk -v s="$SJUSTO" -v th="$p" 'BEGIN{if(th>0) printf("%.6f", s/th); else print "NaN"}')"

    printf "%-18s %-6s %-10.6f %-10.6f %-12.6f %-12.6f %-12.6f %-12.6f\n" \
      "$base" "$p" "$TSEQ" "$TSEQ_SD" "$TOMP" "$TOMP_SD" "$SJUSTO" "$EFF"

    echo "$base,$p,$TSEQ,$TSEQ_SD,$TOMP,$TOMP_SD,$SJUSTO,$EFF" >> "$CSV_FILE"
  done
done

echo
echo "Resultados exportados para: $CSV_FILE"
