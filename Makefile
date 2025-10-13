# -----------------------------
# Compiladores
# -----------------------------
CC   = gcc
CXX  = g++

# -----------------------------
# Flags de compilação
# -----------------------------
CFLAGS   = -O3 -march=native -Wall -Wextra
CXXFLAGS = -O3 -std=c++11 -Wall -Wextra
OMPFLAGS = -fopenmp

# -----------------------------
# Alvos principais
# -----------------------------
all: shsup_seq shsup_omp shsup_seq_v2 shsup_omp_v2 input_gen

# Versão sequencial (original / compat)
shsup_seq: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# Versão paralela OpenMP (compat)
shsup_omp: shortest_superstring_omp.cc
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $< -o $@

# Versão sequencial corrigida (v2)
shsup_seq_v2: shortest_superstring_v2.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# Versão paralela corrigida (v2)
shsup_omp_v2: shortest_superstring_omp_v2.cc
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $< -o $@

# Gerador de entradas (se você tiver esse arquivo)
input_gen: input-generator.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# -----------------------------
# Limpeza
# -----------------------------
clean:
	rm -f shsup_seq shsup_omp shsup_seq_v2 shsup_omp_v2 input_gen
