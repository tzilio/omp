# -----------------------------
# Compiladores
# -----------------------------
CC      = gcc
CXX     = g++
MPICXX  = mpic++

# -----------------------------
# Flags de compilação
# -----------------------------
CFLAGS    = -O3 -march=native -Wall -Wextra
CXXFLAGS  = -O3 -std=c++11 -Wall -Wextra

# -----------------------------
# Alvos principais
# -----------------------------
all: shsup_seq shsup_mpi

# Versão sequencial (compilada com mpic++)
shsup_seq: shortest_superstring_mpi.cc
	$(MPICXX) $(CXXFLAGS) $< -o $@

# Versão paralela MPI
shsup_mpi: shortest_superstring_mpi.cc
	$(MPICXX) $(CXXFLAGS) $< -o $@

# Gerador de entradas
input_gen: input-generator.cc
	$(CXX) $(CXXFLAGS) $< -o $@

# -----------------------------
# Limpeza
# -----------------------------
clean:
	rm -f shsup_seq shsup_mpi input_gen
