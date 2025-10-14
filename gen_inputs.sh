#!/bin/bash
# Gera múltiplos arquivos de teste em ./inputs usando APENAS ./input_gen
# Formato final: 1ª linha = N, seguido de exatamente N strings.

TARGET_NS=(70 80 90 100 110 120 130 140 150 160 170 180 190 200)  # ajuste conforme necessário
GEN="${GEN:-./input_gen}"
OUTDIR="inputs"
TMPDIR=".tmp_gen_inputs"

# sementes (todos os caracteres distintos). 8! = 40320; 7! = 5040
SEEDS=(
  "ABCDEFGH"  # 40320
  "IJKLMNOQ"  # 40320
  "PRSTUVWX"  # 40320
  "YZabcdef"  # 40320
  "ghijklmz"  # 40320
  "ABCDEFG"   # 5040
  "HIJKLMN"   # 5040
  "PQRSTUV"   # 5040
)

mkdir -p "$OUTDIR" "$TMPDIR"

body_only() {
  # roda input_gen com a semente e remove o cabeçalho N
  local seed="$1"
  "$GEN" <<< "$seed" | tail -n +2
}

assemble_file() {
  local N="$1"
  local out="$2"
  local tmp="$TMPDIR/body.$$"
  : > "$tmp"

  local total=0
  while [ "$total" -lt "$N" ]; do
    for s in "${SEEDS[@]}"; do
      body_only "$s" >> "$tmp"
      total=$(wc -l < "$tmp")
      if [ "$total" -ge "$N" ]; then
        break
      fi
    done
  done

  {
    echo "$N"
    head -n "$N" "$tmp"
  } > "$out"

  rm -f "$tmp"
}

echo "Gerando arquivos em $OUTDIR usando $GEN..."
for N in "${TARGET_NS[@]}"; do
  assemble_file "$N" "$OUTDIR/input_${N}.txt"
  echo "OK: $OUTDIR/input_${N}.txt"
done
echo "Concluído."
