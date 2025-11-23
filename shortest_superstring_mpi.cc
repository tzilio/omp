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

using InStream  = std::istream ;
using OutStream = std::ostream ;

template <typename T, typename U>
using Pair = std::pair<T,U>;

template <typename T, typename C = std::less<T>>
using Set = std::set<T>;

template <typename T>
using SizeType = typename T::size_type ;


template <typename C> inline auto size (const C& x) -> SizeType<C> { return x.size(); }
template <typename C> inline auto at_least_two_elements_in (const C& c) -> Boolean { return size(c) > SizeType<C>(1); }
template <typename T> inline auto first_element  (const Set<T>& x) -> T { return *(x.begin()); }
template <typename T> inline auto second_element (const Set<T>& x) -> T { return *(std::next(x.begin())); }

template <typename T> inline auto remove (Set<T>& x, const T& e) -> Set<T>& { x.erase(e); return x; }
template <typename T> inline auto push   (Set<T>& x, const T& e) -> Set<T>& { x.insert(e); return x; }
template <typename C> inline auto empty  (const C& x) -> Boolean { return x.empty(); }

Boolean is_prefix(const String& a, const String& b)
{
    if (size(a) > size(b)) return false;
    if (!(std::mismatch(a.begin(), a.end(), b.begin()).first == a.end())) return false;
    return true;
}

inline auto suffix_from_position (const String& x, SizeType<String> i) -> String { return x.substr(i); }

inline auto remove_prefix (const String& x, SizeType<String> n) -> String
{
    if (size(x) > n) return suffix_from_position(x,n);
    return x;
}

auto all_suffixes (const String& x) -> Set<String>
{
    Set<String> ss;
    SizeType<String> n = size(x);
    while (--n) ss.insert(x.substr(n));
    return ss;
}

auto commom_suffix_and_prefix (const String& a, const String& b) -> String
{
    if (empty(a) || empty(b)) return "";
    String x = "";
    for (const String& s : all_suffixes(a)) {
        if (is_prefix(s,b) && size(s) > size(x)) x = s;
    }
    return x;
}

inline auto overlap_value (const String& s, const String& t) -> SizeType<String>
{
    return size(commom_suffix_and_prefix(s,t));
}

auto overlap (const String& s, const String& t) -> String
{
    String c = commom_suffix_and_prefix(s,t);
    return s + remove_prefix(t, size(c));
}

inline auto pop_two_elements_and_push_overlap (Set<String>& ss, const Pair<String,String>& p) -> Set<String>&
{
    ss = remove(ss, p.first);
    ss = remove(ss, p.second);
    ss = push(ss, overlap(p.first, p.second));
    return ss;
}

inline auto write_string_and_break_line (OutStream& out, String s) -> void { out << s << std::endl; }

inline auto read_size (InStream& in) -> Size { Size n; in >> n; return n; }
inline auto read_string (InStream& in) -> String { String s; in >> s; return s; }

auto read_strings_from_standard_input() -> Set<String>
{
    using N = SizeType<Set<String>>;
    Set<String> x;
    N n = N(read_size(standard_input));
    while (n--) x.insert(read_string(standard_input));
    return x;
}

inline auto write_string_to_standard_ouput(const String& s) -> void { write_string_and_break_line(standard_output, s); }

// ============================================================================
// ========================= MPI IMPLEMENTAÇÃO ================================
// ============================================================================

#ifdef _MPI

// ------------------------------------------------------------------
// broadcast de vetor de strings (convertido do Set<String>)
// ------------------------------------------------------------------
static void bcast_vector_strings(std::vector<String>& v, int root, MPI_Comm comm)
{
    int rank;
    MPI_Comm_rank(comm, &rank);

    int n = (int)v.size();
    MPI_Bcast(&n,1,MPI_INT,root,comm);

    if (rank != root) v.resize(n);

    std::vector<int> lens(n);
    if (rank == root) {
        for (int i=0;i<n;i++) lens[i] = (int)v[i].size();
    }
    MPI_Bcast(lens.data(), n, MPI_INT, root, comm);

    size_t total = 0;
    for (int i=0;i<n;i++) total += lens[i];

    std::vector<char> buf(total);
    if (rank == root) {
        size_t off=0;
        for (int i=0;i<n;i++) {
            memcpy(buf.data()+off, v[i].data(), lens[i]);
            off += lens[i];
        }
    }

    MPI_Bcast(buf.data(), (int)buf.size(), MPI_CHAR, root, comm);

    if (rank != root) {
        size_t off=0;
        for (int i=0;i<n;i++) {
            v[i].assign(buf.data()+off, lens[i]);
            off += lens[i];
        }
    }
}

// ------------------------------------------------------------------
// converte Set<String> → vetor ordenado
// ------------------------------------------------------------------
static std::vector<String> set_to_vector(const Set<String>& ss)
{
    return std::vector<String>(ss.begin(), ss.end());
}

// ------------------------------------------------------------------
// converte vetor → Set<String>
// ------------------------------------------------------------------
static Set<String> vector_to_set(const std::vector<String>& v)
{
    Set<String> ss;
    for (auto& s : v) ss.insert(s);
    return ss;
}

// ------------------------------------------------------------------
// mapeamento indexado para pares i,j (sem diagonal)
// ------------------------------------------------------------------
static inline void linear_to_pair(long long k, int n, int& i, int& j)
{
    i = (int)(k / (n-1));
    int r = (int)(k % (n-1));
    j = (r < i ? r : r+1);
}

// ------------------------------------------------------------------
// Estrutura para redução MPI
// ------------------------------------------------------------------
struct BestPair {
    int ov;
    int i, j;
};

// tipo MPI custom
static MPI_Datatype MPI_BEST_TYPE;

// operador custom
static MPI_Op MPI_BEST_OP;

// função do operador de redução
static void best_reduce(void* invec, void* inout, int* len, MPI_Datatype*)
{
    BestPair* in  = (BestPair*) invec;
    BestPair* out = (BestPair*) inout;

    for (int k=0;k<*len;k++) {
        if (in[k].ov > out[k].ov) {
            out[k] = in[k];
        }
        else if (in[k].ov == out[k].ov) {
            if (in[k].i < out[k].i ||
                (in[k].i == out[k].i && in[k].j < out[k].j))
                out[k] = in[k];
        }
    }
}

// ------------------------------------------------------------------
// encontra melhor par global (MPI_Reduce)
// ------------------------------------------------------------------
static BestPair mpi_find_best_pair(const std::vector<String>& v, MPI_Comm comm)
{
    int rank,np;
    MPI_Comm_rank(comm,&rank);
    MPI_Comm_size(comm,&np);

    int n = (int)v.size();
    long long total = 1LL * n * (n-1);

    BestPair local{ -1, 0, 1 };

    for (long long k = rank; k < total; k += np) {
        int i,j;
        linear_to_pair(k,n,i,j);
        int ov = (int)overlap_value(v[i],v[j]);
        if (ov > local.ov ||
           (ov == local.ov && (i < local.i ||
            (i == local.i && j < local.j))))
        {
            local.ov = ov;
            local.i = i;
            local.j = j;
        }
    }

    BestPair global;
    if (rank == 0) global = local;

    MPI_Reduce(&local, &global, 1, MPI_BEST_TYPE, MPI_BEST_OP, 0, comm);

    // broadcast do resultado
    MPI_Bcast(&global, 3, MPI_INT, 0, comm);

    return global;
}

// ------------------------------------------------------------------
// aplica merge no root e difunde novo vetor
// ------------------------------------------------------------------
static void mpi_apply_merge(std::vector<String>& v, BestPair bp, MPI_Comm comm)
{
    int rank;
    MPI_Comm_rank(comm,&rank);

    int i = bp.i;
    int j = bp.j;

    String merged;

    if (rank == 0) {
        merged = overlap(v[i], v[j]);
        v[i] = merged;
        v.erase(v.begin()+j);
    }

    // difunde v atualizado
    bcast_vector_strings(v,0,comm);
}

// ------------------------------------------------------------------
// Algoritmo guloso MPI
// ------------------------------------------------------------------
static String shortest_superstring_mpi(Set<String> ss, MPI_Comm comm)
{
    std::vector<String> v = set_to_vector(ss);

    while ((int)v.size() > 1) {
        BestPair bp = mpi_find_best_pair(v,comm);
        mpi_apply_merge(v,bp,comm);
    }

    return v.empty() ? "" : v[0];
}

// ------------------------------------------------------------------
// MAIN MPI
// ------------------------------------------------------------------
int main(int argc, char** argv)
{
    MPI_Init(&argc,&argv);

    // cria tipo MPI_BEST_TYPE
    {
        BestPair dummy;
        MPI_Aint base, disp[3];
        int block[3] = {1,1,1};
        MPI_Datatype types[3] = { MPI_INT, MPI_INT, MPI_INT };

        MPI_Get_address(&dummy,&base);
        MPI_Get_address(&dummy.ov,&disp[0]);
        MPI_Get_address(&dummy.i ,&disp[1]);
        MPI_Get_address(&dummy.j ,&disp[2]);

        for (int k=0;k<3;k++) disp[k] -= base;

        MPI_Type_create_struct(3,block,disp,types,&MPI_BEST_TYPE);
        MPI_Type_commit(&MPI_BEST_TYPE);
    }

    MPI_Op_create(best_reduce,1,&MPI_BEST_OP);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    Set<String> ss;
    std::vector<String> v;

    if (rank == 0) {
        ss = read_strings_from_standard_input();
        v = set_to_vector(ss);
    }

    bcast_vector_strings(v,0,MPI_COMM_WORLD);
    ss = vector_to_set(v);

    auto t0 = std::chrono::high_resolution_clock::now();
    String ans = shortest_superstring_mpi(ss, MPI_COMM_WORLD);
    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    if (rank == 0) {
        std::cout << ans << "\n";
        std::cout << elapsed << "\n";
    }

    MPI_Op_free(&MPI_BEST_OP);
    MPI_Type_free(&MPI_BEST_TYPE);
    MPI_Finalize();
    return 0;
}

#else  // ======================================================================
// ==================== VERSÃO SEQUENCIAL (INTACTA) ===========================
// ============================================================================

auto all_distinct_pairs_parallel(const Set<String>&) -> std::vector<Pair<String,String>> { return {}; }
auto best_pair_from_pairs_parallel(const std::vector<Pair<String,String>>&) -> Pair<String,String> { return {"",""}; }

auto pair_of_strings_with_highest_overlap_value_parallel_v2(const Set<String>& ss)
    -> Pair<String,String>
{
    // versão seq original
    Pair<String,String> best;
    Bool global_has=false;
    SizeType<String> global_best=0;

    std::vector<String> v = set_to_vector(ss);
    int n = v.size();
    if (n < 2) return {"",""};

    for (int i=0;i<n;i++){
        for (int j=0;j<n;j++){
            if (i==j) continue;
            auto ov = overlap_value(v[i],v[j]);
            if (!global_has ||
                ov > global_best ||
                (ov==global_best && Pair<String,String>{v[i],v[j]} < best))
            {
                global_best = ov;
                best = {v[i],v[j]};
                global_has = true;
            }
        }
    }
    return best;
}

auto shortest_superstring(Set<String> t) -> String
{
    if (empty(t)) return "" ;
    while (at_least_two_elements_in(t)) {
        const auto best_pair = pair_of_strings_with_highest_overlap_value_parallel_v2(t);
        t = pop_two_elements_and_push_overlap(t, best_pair);
    }
    return first_element(t);
}

int main(int, char const**)
{
    Set<String> ss = read_strings_from_standard_input();

    auto start = std::chrono::high_resolution_clock::now();
    write_string_to_standard_ouput(shortest_superstring(ss));
    auto end   = std::chrono::high_resolution_clock::now();

    double total = std::chrono::duration<double>(end - start).count();
    standard_output << total << std::endl;
    return 0;
}

#endif
