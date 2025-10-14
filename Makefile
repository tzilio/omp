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
all: shsup_omp input_gen

# Versão "sequencial" sem o fopenmp
shsup_seq: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# Versão paralela OpenMP com o fopenmp
shsup_omp: shortest_superstring_omp.cc
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) $< -o $@

# Gerador de entradas
input_gen: input-generator.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# -----------------------------
# Limpeza
# -----------------------------
clean:
	rm -f shsup_seq shsup_omp input_gen
