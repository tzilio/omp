#!/usr/bin/env bash
set -euo pipefail

# ======================== CONFIG ========================
SRC='shortest_superstring_omp.cc'   # seu fonte com métricas em stderr
RUNS=${RUNS:-3}                     # repetições por caso (rápido)
N=${N:-60}                          # quantidade de strings
STR_LEN=${STR_LEN:-16}              # tamanho fixo das strings (evita substrings)
THREADS="${THREADS:-1 2}"           # threads a testar no OMP (e seq=1)
BIN_DIR='bin'
IN_DIR='inputs'
OUT_DIR='results'
RAW_CSV="$OUT_DIR/quick_raw.csv"
SUM_CSV="$OUT_DIR/quick_summary.csv"

# Afinidade recomendada (ajuste se quiser)
export OMP_PROC_BIND=${OMP_PROC_BIND:-close}
export OMP_PLACES=${OMP_PLACES:-cores}

say() { printf '[%s] %s\n' "$(date +%H:%M:%S)" "$*"; }

ensure_dirs() {
  mkdir -p "$BIN_DIR" "$IN_DIR" "$OUT_DIR"
}

build_bins() {
  say 'Compilando seq/omp...'
  g++ -O3 -std=c++11 -Wall -Wextra "$SRC" -o "$BIN_DIR/shsup_seq"
  g++ -O3 -std=c++11 -Wall -Wextra -fopenmp "$SRC" -o "$BIN_DIR/shsup_omp"
}

gen_input() {
  local n="$1" out="$2" seed="$3" len="$4"
  python3 - "$n" "$len" "$out" "$seed" << 'PY'
import sys, random, string
n, L, outf, seed = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], int(sys.argv[4])
random.seed(seed)
ALPH = string.ascii_uppercase
s = set()
while len(s) < n:
    s.add(''.join(random.choice(ALPH) for _ in range(L)))
with open(outf, 'w') as f:
    f.write(str(n) + '\n')
    for w in s:
        f.write(w + '\n')
PY
}

init_csv() {
  echo 'version,mode,threads,N,run_idx,total,gen,scan,seq,seq_frac,super_len' > "$RAW_CSV"
}

run_once() {
  local bin="$1" threads="$2" infile="$3"
  export OMP_NUM_THREADS="$threads"
  local out_file err_file
  out_file="$(mktemp)"; err_file="$(mktemp)"
  "$bin" < "$infile" >"$out_file" 2>"$err_file"
  # stdout: 1ª linha = superstring, última = total (fallback)
  local super total_stdout
  super="$(head -n 1 "$out_file")"
  total_stdout="$(tail -n 1 "$out_file")"
  local super_len=${#super}
  # stderr: 'total gen scan seq seq_frac'
  local total gen scan seq frac
  if read -r total gen scan seq frac <"$err_file"; then
    :
  else
    total="$total_stdout"; gen=''; scan=''; seq=''; frac=''
  fi
  rm -f "$out_file" "$err_file"
  echo "$total,$gen,$scan,$seq,$frac,$super_len"
}

check_correctness() {
  local infile="$1"
  local tmp1 tmp2
  tmp1="$(mktemp)"; tmp2="$(mktemp)"

  # roda completo; sem pipes; stderr silenciado
  "$BIN_DIR/shsup_seq" < "$infile" >"$tmp1" 2>/dev/null
  "$BIN_DIR/shsup_omp" < "$infile" >"$tmp2" 2>/dev/null

  # lê só a 1ª linha de cada arquivo
  local s1 s2
  s1="$(head -n 1 "$tmp1")"
  s2="$(head -n 1 "$tmp2")"

  rm -f "$tmp1" "$tmp2"

  if [[ "$s1" != "$s2" ]]; then
    echo 'ERRO: superstring difere entre seq e omp' >&2
    echo 'SEQ:' "$s1" >&2
    echo 'OMP:' "$s2" >&2
    exit 1
  fi
}


aggregate() {
  # resumo com média/desvio e speedup/eficiência (baseline = seq, threads=1)
  awk -F',' '
    BEGIN {
      OFS=",";
    }
    NR==1 { next }
    {
      key = $1 FS $3 FS $4;          # version,threads,N
      n[key]++; tot[key]+=$6; tot2[key]+=$6*$6;
      thr[key]=$3; NN[$4]=1;
      if ($7 != "") { gen[key]+=$7; gen2[key]+=$7*$7; ng[key]++ }
      if ($8 != "") { scn[key]+=$8; scn2[key]+=$8*$8; ns[key]++ }
    }
    END {
      print "threads,N,mean_seq_total,sd_seq_total,mean_omp_total,sd_omp_total,speedup,efficiency,mean_omp_gen,mean_omp_scan";
      for (nval in NN) {
        base = "seq" FS "1" FS nval;
        if (!(base in n)) continue;
        mseq = tot[base]/n[base]; vseq = (tot2[base]/n[base] - mseq*mseq); if (vseq<0) vseq=0; sdseq=sqrt(vseq);
        # listar omp groups daquele N
        for (k in n) {
          split(k,a,FS);
          if (a[1]!="omp" || a[3]!=nval) continue;
          m = tot[k]/n[k]; v = (tot2[k]/n[k] - m*m); if (v<0) v=0; sd=sqrt(v);
          sp = (m>0? mseq/m : "");
          eff = (sp!="" && a[2]+0>0 ? sp/(a[2]+0) : "");
          mg = (ng[k]>0? gen[k]/ng[k] : "");
          ms = (ns[k]>0? scn[k]/ns[k] : "");
          print a[2], nval, mseq, sdseq, m, sd, sp, eff, mg, ms;
        }
      }
    }
  ' "$RAW_CSV" | sort -t',' -k1,1n -k2,2n > "$SUM_CSV"
}

main() {
  ensure_dirs
  build_bins
  local infile="$IN_DIR/quick_N${N}.txt"
  [[ -f "$infile" ]] || gen_input "$N" "$infile" 424242 "$STR_LEN"

  init_csv

  say 'Checando corretude (seq vs omp)...'
  check_correctness "$infile"
  say 'OK'

  # aquecimento rápido
  "$BIN_DIR/shsup_seq" < "$infile" >/dev/null 2>/dev/null || true
  "$BIN_DIR/shsup_omp" < "$infile" >/dev/null 2>/dev/null || true

  # rodar seq (threads=1) RUNS vezes
  for r in $(seq 1 "$RUNS"); do
    IFS=, read -r total gen scan seq frac slen < <(run_once "$BIN_DIR/shsup_seq" 1 "$infile")
    echo "seq,quick,1,$N,$r,$total,$gen,$scan,$seq,$frac,$slen" >> "$RAW_CSV"
  done

  # rodar omp para cada T
  for T in $THREADS; do
    for r in $(seq 1 "$RUNS"); do
      IFS=, read -r total gen scan seq frac slen < <(run_once "$BIN_DIR/shsup_omp" "$T" "$infile")
      echo "omp,quick,$T,$N,$r,$total,$gen,$scan,$seq,$frac,$slen" >> "$RAW_CSV"
    done
  done

  aggregate

  say "Pronto!"
  say "CSV bruto: $RAW_CSV"
  say "Resumo:    $SUM_CSV"
  echo
  echo 'Resumo (primeiras linhas):'
  head -n 10 "$SUM_CSV"
}

main "$@"
