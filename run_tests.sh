#!/usr/bin/env bash
set -euo pipefail

make

# Cria alguns testes canônicos
mkdir -p tests
cat > tests/t1.txt << 'EOF'
3
ABCD
CDE
EFG
EOF

cat > tests/t2.txt << 'EOF'
4
ABC
BCA
CAB
AB
EOF

cat > tests/t3.txt << 'EOF'
5
ATGC
TGCAT
CATG
TGCA
GCA
EOF

# Gerador randômico simples (N, L)
gen_rand() {
  local N=$1 L=$2
  echo "$N"
  for i in $(seq 1 $N); do
    tr -dc A-Z < /dev/urandom | head -c "$L"
    echo
  done
}

gen_rand 20 10 > tests/t_rand1.txt
gen_rand 50 12 > tests/t_rand2.txt

# Programas
PROGS_SEQ=("shsup_seq" "shsup_seq_v2")
PROGS_OMP=("shsup_omp" "shsup_omp_v2")
THREADS=(1 2 4 8)

OUT=results.csv
echo "test,program,threads,len,time_s" > "$OUT"

run_one() {
  local prog=$1 in=$2 threads=$3
  if [[ $threads -gt 0 ]]; then
    OMP_NUM_THREADS=$threads out="$($prog < "$in")"
  else
    out="$($prog < "$in")"
  fi
  super=$(printf "%s\n" "$out" | sed -n '1p')
  time=$(printf "%s\n" "$out" | sed -n '2p')
  len=${#super}
  echo "$in,$prog,$threads,$len,$time" >> "$OUT"
}

for f in tests/*.txt; do
  # sequenciais
  for p in "${PROGS_SEQ[@]}"; do
    [[ -x $p ]] && run_one "./$p" "$f" 0 || true
  done
  # paralelas com vários threads
  for p in "${PROGS_OMP[@]}"; do
    if [[ -x $p ]]; then
      for t in "${THREADS[@]}"; do
        run_one "./$p" "$f" "$t"
      done
    fi
  done
done

echo "OK -> $OUT"
