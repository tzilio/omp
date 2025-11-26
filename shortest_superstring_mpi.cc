#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <chrono>
#include <vector>
#include <cstring>

#ifdef _MPI
#include <mpi.h>
#endif

#define standard_input  std::cin
#define standard_output std::cout

using Boolean = bool ;
using Size    = std::size_t ;
using String  = std::string ;

template <typename C> inline auto size (const C& x) -> typename C::size_type { return x.size(); }
template <typename C> inline auto empty(const C& x) -> bool { return x.empty(); }

// =============================================================
// Funções auxiliares
// =============================================================

Boolean is_prefix(const String& a, const String& b)
{
    if (size(a) > size(b)) return false;
    return std::mismatch(a.begin(), a.end(), b.begin()).first == a.end();
}

auto all_suffixes(const String& x) -> std::vector<String>
{
    std::vector<String> ss;
    for (int i = (int)x.size()-1; i > 0; --i)
        ss.push_back(x.substr(i));
    return ss;
}

auto commom_suffix_and_prefix(const String& a, const String& b) -> String
{
    if (empty(a) || empty(b)) return "";
    String best = "";
    for (auto& s : all_suffixes(a)) {
        if (is_prefix(s, b) && s.size() > best.size())
            best = s;
    }
    return best;
}

inline auto overlap_value (const String& s, const String& t) -> size_t
{
    return commom_suffix_and_prefix(s, t).size();
}

auto overlap (const String& s, const String& t) -> String
{
    String c = commom_suffix_and_prefix(s, t);
    return s + t.substr(c.size());
}

// =============================================================
// Leitura: SEMPRE VETOR
// =============================================================
static std::vector<String> read_vector()
{
    Size n;
    standard_input >> n;
    std::vector<String> v(n);
    for (Size i = 0; i < n; ++i)
        standard_input >> v[i];
    return v;
}

inline void write_string(const String& s)
{
    standard_output << s << "\n";
}

// ============================================================================
// ========================= MPI IMPLEMENTAÇÃO =================================
// ============================================================================

#ifdef _MPI

// ------------------------------------------------------------
// Broadcast de vetor de strings
// ------------------------------------------------------------
static void bcast_vector_strings(std::vector<String>& v, int root, MPI_Comm comm)
{
    int rank;
    MPI_Comm_rank(comm, &rank);

    int n = (int)v.size();
    MPI_Bcast(&n, 1, MPI_INT, root, comm);
    if (rank != root) v.resize(n);

    std::vector<int> lens(n);

    if (rank == root)
        for (int i = 0; i < n; ++i)
            lens[i] = (int)v[i].size();

    MPI_Bcast(lens.data(), n, MPI_INT, root, comm);

    size_t total = 0;
    for (int i = 0; i < n; ++i)
        total += lens[i];

    std::vector<char> buf(total);

    if (rank == root) {
        size_t off = 0;
        for (int i = 0; i < n; ++i) {
            memcpy(buf.data()+off, v[i].data(), lens[i]);
            off += lens[i];
        }
    }

    MPI_Bcast(buf.data(), (int)buf.size(), MPI_CHAR, root, comm);

    if (rank != root) {
        size_t off = 0;
        for (int i = 0; i < n; ++i) {
            v[i].assign(buf.data()+off, lens[i]);
            off += lens[i];
        }
    }
}

// ------------------------------------------------------------
// mapeamento linear k -> par (i,j)
// ------------------------------------------------------------
static inline void linear_to_pair(long long k, int n, int& i, int& j)
{
    i = k / (n - 1);
    int r = k % (n - 1);
    j = (r < i ? r : r + 1);
}

// ------------------------------------------------------------
// Estrutura usada na redução MPI
// ------------------------------------------------------------
struct BestPair {
    int ov;
    int i;
    int j;
};

static MPI_Datatype MPI_BEST_TYPE;
static MPI_Op       MPI_BEST_OP;

static void best_reduce(void* invec, void* inout, int* len, MPI_Datatype*)
{
    auto* in  = (BestPair*) invec;
    auto* out = (BestPair*) inout;

    for (int k = 0; k < *len; ++k) {
        if (in[k].ov > out[k].ov ||
           (in[k].ov == out[k].ov &&
            (in[k].i < out[k].i ||
             (in[k].i == out[k].i && in[k].j < out[k].j))))
        {
            out[k] = in[k];
        }
    }
}

// ------------------------------------------------------------
// STRIDED : k = rank, rank+np, rank+2np ...
// ------------------------------------------------------------
static BestPair mpi_find_best_pair(const std::vector<String>& v, MPI_Comm comm)
{
    int rank, np;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &np);

    int n = (int)v.size();
    long long total = 1LL*n*(n-1);

    BestPair local{-1,0,1};

    for (long long k = rank; k < total; k += np) {
        int i, j;
        linear_to_pair(k, n, i, j);
        int ov = overlap_value(v[i], v[j]);

        if (ov > local.ov ||
           (ov == local.ov && (i < local.i ||
           (i == local.i && j < local.j))))
        {
            local.ov = ov;
            local.i  = i;
            local.j  = j;
        }
    }

    BestPair global;
    MPI_Reduce(&local, &global, 1, MPI_BEST_TYPE, MPI_BEST_OP, 0, comm);
    MPI_Bcast(&global, 3, MPI_INT, 0, comm);
    return global;
}

// ------------------------------------------------------------
// root faz merge e broadcast do novo vetor
// ------------------------------------------------------------
static void mpi_apply_merge(std::vector<String>& v, BestPair bp, MPI_Comm comm)
{
    int rank;
    MPI_Comm_rank(comm, &rank);

    int i = bp.i;
    int j = bp.j;

    if (rank == 0) {
        String merged = overlap(v[i], v[j]);
        v[i] = merged;
        v.erase(v.begin()+j);
    }

    bcast_vector_strings(v, 0, comm);
}

// ------------------------------------------------------------
// Guloso MPI
// ------------------------------------------------------------
static String shortest_superstring_mpi(std::vector<String> v, MPI_Comm comm)
{
    while (v.size() > 1) {
        BestPair bp = mpi_find_best_pair(v, comm);
        mpi_apply_merge(v, bp, comm);
    }
    return v.empty() ? "" : v[0];
}

// ------------------------------------------------------------
// MAIN MPI
// ------------------------------------------------------------
int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    // cria tipo MPI_BEST_TYPE
    {
        BestPair dummy;
        MPI_Aint base, disp[3];
        MPI_Get_address(&dummy,    &base);
        MPI_Get_address(&dummy.ov, &disp[0]);
        MPI_Get_address(&dummy.i,  &disp[1]);
        MPI_Get_address(&dummy.j,  &disp[2]);
        int block[3] = {1,1,1};
        MPI_Datatype types[3] = {MPI_INT, MPI_INT, MPI_INT};
        for (int k=0;k<3;++k) disp[k] -= base;
        MPI_Type_create_struct(3, block, disp, types, &MPI_BEST_TYPE);
        MPI_Type_commit(&MPI_BEST_TYPE);
    }

    MPI_Op_create(best_reduce, 1, &MPI_BEST_OP);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    std::vector<String> v;

    if (rank == 0)
        v = read_vector();

    bcast_vector_strings(v, 0, MPI_COMM_WORLD);

    auto t0 = std::chrono::high_resolution_clock::now();
    String ans = shortest_superstring_mpi(v, MPI_COMM_WORLD);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1-t0).count();

    if (rank == 0) {
        standard_output << ans << "\n";
        standard_output << elapsed << "\n";
    }

    MPI_Op_free(&MPI_BEST_OP);
    MPI_Type_free(&MPI_BEST_TYPE);

    MPI_Finalize();
    return 0;
}

#else  
// =============================================================================
// ======================== VERSÃO SEQUENCIAL ==================================
// =============================================================================

struct BestPairSequential {
    int ov;
    int i;
    int j;
};

static BestPairSequential seq_find_best_pair(const std::vector<String>& v,
                                             double& par_time)
{
    auto t0 = std::chrono::high_resolution_clock::now();

    BestPairSequential best{-1,0,1};
    int n = (int)v.size();

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            if (i != j)
            {
                int ov = overlap_value(v[i], v[j]);
                if (ov > best.ov ||
                   (ov == best.ov && (i < best.i ||
                   (i == best.i && j < best.j))))
                {
                    best = {ov,i,j};
                }
            }

    auto t1 = std::chrono::high_resolution_clock::now();
    par_time += std::chrono::duration<double>(t1 - t0).count();

    return best;
}

static String shortest_superstring(std::vector<String> v,
                                   double& par_time)
{
    while (v.size() > 1)
    {
        BestPairSequential bp = seq_find_best_pair(v, par_time);

        String merged = overlap(v[bp.i], v[bp.j]);
        v[bp.i] = merged;
        v.erase(v.begin() + bp.j);
    }

    return v.empty() ? "" : v[0];
}

int main()
{
    auto v = read_vector();

    double par_time = 0.0;

    const auto start = std::chrono::high_resolution_clock::now();
    String ans = shortest_superstring(v, par_time);
    const auto end = std::chrono::high_resolution_clock::now();

    const double total = std::chrono::duration<double>(end - start).count();

    write_string(ans);
    standard_output << total << "\n";

    double seq_time = total - par_time;
    double seq_frac = (total > 0.0 ? seq_time / total : 0.0);

    std::cerr << total << " " << par_time << " " << seq_frac << "\n";

    return 0;
}

#endif
