#!/usr/bin/env bash
set -euo pipefail

# Uso: ./with_cpu_stability.sh <comando...>
# Ex.: ./with_cpu_stability.sh ./quick_test.sh
#     ./with_cpu_stability.sh ./run_experiments.sh

need_root() {
  if ! sudo -n true 2>/dev/null; then
    echo "[info] Será solicitado sudo para ajustar CPU..."
  fi
}

STATE_DIR="$(mktemp -d)"
cleanup() {
  # Restaurar governors
  if [[ -d "$STATE_DIR" ]]; then
    for f in "$STATE_DIR"/gov_cpu*.txt; do
      [[ -f "$f" ]] || continue
      cpu="$(basename "$f" | sed -E 's/.*cpu([0-9]+).*/\1/')"
      gov="$(cat "$f")"
      sudo bash -c "echo '$gov' > /sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor" || true
    done
    # Restaurar turbo
    if [[ -f "$STATE_DIR/no_turbo.txt" && -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
      val="$(cat "$STATE_DIR/no_turbo.txt")"
      echo "$val" | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null || true
    fi
    if [[ -f "$STATE_DIR/boost.txt" && -f /sys/devices/system/cpu/cpufreq/boost ]]; then
      val="$(cat "$STATE_DIR/boost.txt")"
      echo "$val" | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null || true
    fi
    rm -rf "$STATE_DIR"
  fi
}
trap cleanup EXIT

need_root

# Salvar estado atual
for g in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor; do
  [[ -f "$g" ]] || continue
  cpu="$(echo "$g" | sed -E 's#.*/cpu([0-9]+)/.*#\1#')"
  sudo cat "$g" > "$STATE_DIR/gov_cpu${cpu}.txt" || true
done
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
  sudo cat /sys/devices/system/cpu/intel_pstate/no_turbo > "$STATE_DIR/no_turbo.txt" || true
fi
if [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
  sudo cat /sys/devices/system/cpu/cpufreq/boost > "$STATE_DIR/boost.txt" || true
fi

# Desativar Turbo Boost / Precision Boost
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
  echo "[set] Intel no_turbo=1"
  echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null
elif [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
  echo "[set] cpufreq boost=0"
  echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost >/dev/null
else
  echo "[warn] Não encontrei knobs de turbo (intel_pstate/cpufreq)."
fi

# Travar em governor=performance (para todos os cores)
for gov in /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor; do
  [[ -f "$gov" ]] || continue
  echo performance | sudo tee "$gov" >/dev/null || true
done

# (Opcional) se cpupower existir, reforça o governor:
if command -v cpupower >/dev/null 2>&1; then
  sudo cpupower frequency-set -g performance >/dev/null 2>&1 || true
fi

# Mostrar estado resumido
echo "[info] Estado após ajuste:"
if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
  echo -n "  no_turbo="; cat /sys/devices/system/cpu/intel_pstate/no_turbo
fi
if [[ -f /sys/devices/system/cpu/cpufreq/boost ]]; then
  echo -n "  boost="; cat /sys/devices/system/cpu/cpufreq/boost
fi
echo -n "  governors: "
grep -h . /sys/devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor 2>/dev/null | sort -u || echo "N/A"

# Bind/places recomendados p/ OMP (pode sobrescrever no comando se quiser)
export OMP_PROC_BIND="${OMP_PROC_BIND:-close}"
export OMP_PLACES="${OMP_PLACES:-cores}"

# Executa o comando do usuário
echo "[run] $*"
"$@"
