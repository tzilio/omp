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
using Pair = std::pair <T, U> ;

template <typename T, typename C = std::less <T>>
using Set = std::set <T> ;

template <typename T>
using SizeType = typename T :: size_type ;

// ------------------------------------------------------------
// utilitários
// ------------------------------------------------------------
template <typename C> inline auto size (const C& x) -> SizeType <C> { return x.size (); }
template <typename C> inline auto empty(const C& x) -> Boolean { return x.empty (); }

Boolean is_prefix(const String& a, const String& b) {
    if (size(a) > size(b)) return false;
    return std::mismatch(a.begin(), a.end(), b.begin()).first == a.end();
}

inline auto remove_prefix(const String& x, SizeType<String> n) -> String {
    return (size(x) > n ? x.substr(n) : x);
}

auto all_suffixes(const String& x) -> Set<String> {
    Set<String> ss;
    for (Size i = size(x)-1; i > 0; --i) ss.insert(x.substr(i));
    return ss;
}

auto commom_suffix_and_prefix(const String& a, const String& b) -> String {
    if (empty(a) || empty(b)) return "";
    String x = "";
    for (const String& s : all_suffixes(a))
        if (is_prefix(s, b) && size(s) > size(x))
            x = s;
    return x;
}

inline auto overlap_value(const String& s, const String& t) -> SizeType<String> {
    return size(commom_suffix_and_prefix(s, t));
}

auto overlap(const String& s, const String& t) -> String {
    String c = commom_suffix_and_prefix(s, t);
    return s + remove_prefix(t, size(c));
}

// ------------------------------------------------------------
// MPI: geração distribuída de pares
// ------------------------------------------------------------
auto all_distinct_pairs_mpi(const Set<String>& ss, int rank, int nprocs)
    -> std::vector<Pair<String,String>>
{
    std::vector<String> v; v.reserve(ss.size());
    for (const auto& s : ss) v.push_back(s);

    Size n = v.size();
    std::vector<Pair<String,String>> local_pairs;

    if (n < 2) return local_pairs;

    Size total_pairs = n * (n - 1);
    Size chunk = total_pairs / nprocs;
    Size start = rank * chunk;
    Size end   = (rank == nprocs - 1 ? total_pairs : start + chunk);

    local_pairs.reserve(end - start);

    for (Size idx = start; idx < end; ++idx) {
        Size i = idx / (n - 1);
        Size col = idx % (n - 1);
        Size j = (col < i ? col : col + 1);
        local_pairs.emplace_back(v[i], v[j]);
    }

    return local_pairs;
}

// ------------------------------------------------------------
// MPI: melhor par local
// ------------------------------------------------------------
auto local_best_pair_mpi(const std::vector<Pair<String,String>>& pairs)
    -> Pair<String,String>
{
    Pair<String,String> best_pair;
    Size best_value = 0;
    bool has = false;

    for (auto& p : pairs) {
        Size ov = overlap_value(p.first, p.second);
        if (!has || ov > best_value ||
           (ov == best_value && p < best_pair)) {
            best_pair = p;
            best_value = ov;
            has = true;
        }
    }

    return best_pair;
}

// ------------------------------------------------------------
// Estrutura auxiliar para MPI_Allreduce
// ------------------------------------------------------------
struct BestInfo {
    Size value;
    char a[256];
    char b[256];
};

// ------------------------------------------------------------
// MPI: redução global para encontrar o melhor par
// ------------------------------------------------------------
auto reduce_best_pair_mpi(const Pair<String,String>& local)
    -> Pair<String,String>
{
    BestInfo in{}, out{};

    in.value = overlap_value(local.first, local.second);
    std::snprintf(in.a, 256, "%s", local.first.c_str());
    std::snprintf(in.b, 256, "%s", local.second.c_str());

    auto cmp = [](const BestInfo& x, const BestInfo& y) {
        if (x.value != y.value) return x.value > y.value;
        String xa(x.a), xb(x.b);
        String ya(y.a), yb(y.b);
        return Pair<String,String>{xa,xb} < Pair<String,String>{ya,yb};
    };

    MPI_Allreduce(&in, &out, sizeof(BestInfo), MPI_BYTE,
        [](void* invec, void* inoutvec, int* len, MPI_Datatype*) {
            BestInfo* IN  = (BestInfo*)invec;
            BestInfo* OUT = (BestInfo*)inoutvec;
            for (int i = 0; i < *len; ++i)
                if (IN[i].value > OUT[i].value) OUT[i] = IN[i];
        },
        MPI_COMM_WORLD);

    return { String(out.a), String(out.b) };
}

// ------------------------------------------------------------
// Função final que usa MPI para achar o melhor par
// ------------------------------------------------------------
auto pair_of_strings_with_highest_overlap_value_mpi(const Set<String>& ss,
                                                    int rank, int nprocs)
    -> Pair<String,String>
{
    auto local_pairs = all_distinct_pairs_mpi(ss, rank, nprocs);
    auto local_best  = local_best_pair_mpi(local_pairs);
    return reduce_best_pair_mpi(local_best);
}

// ------------------------------------------------------------
// Reconstrução de superstring a partir de uma ordem
// ------------------------------------------------------------
String build_superstring_from_order(const std::vector<String>& ord) {
    if (ord.empty()) return "";
    String s = ord[0];
    for (Size i = 1; i < ord.size(); ++i) {
        Size ov = overlap_value(s, ord[i]);
        s += remove_prefix(ord[i], ov);
    }
    return s;
}

// ------------------------------------------------------------
// Refinamento 2-opt
// ------------------------------------------------------------
auto two_opt(const std::vector<String>& base)
    -> std::vector<String>
{
    std::vector<String> best = base;
    String best_str = build_superstring_from_order(best);
    Size best_len = best_str.size();

    bool improved = true;
    while (improved) {
        improved = false;

        for (Size i = 1; i + 1 < best.size(); ++i) {
            for (Size j = i + 1; j < best.size(); ++j) {

                std::vector<String> cand = best;
                std::reverse(cand.begin() + i, cand.begin() + j + 1);

                String cstr = build_superstring_from_order(cand);
                Size clen = cstr.size();

                if (clen < best_len) {
                    best = std::move(cand);
                    best_len = clen;
                    improved = true;
                    goto again;
                }
            }
        }
    again:;
    }

    return best;
}

// ------------------------------------------------------------
// Greedy + MPI: retorna ordem final das fusões
// ------------------------------------------------------------
auto shortest_superstring_with_order(Set<String> t, int rank, int nprocs)
    -> std::vector<String>
{
    std::vector<String> order;
    for (const auto& s : t) order.push_back(s);

    while (order.size() > 1) {
        Set<String> tmp(order.begin(), order.end());
        auto best = pair_of_strings_with_highest_overlap_value_mpi(tmp, rank, nprocs);

        String merged = overlap(best.first, best.second);

        std::vector<String> new_order;
        new_order.reserve(order.size());

        for (auto& s : order)
            if (s != best.first && s != best.second)
                new_order.push_back(s);

        new_order.push_back(merged);
        order = std::move(new_order);
    }

    return order;
}

// ------------------------------------------------------------
// I/O
// ------------------------------------------------------------
inline auto read_size(InStream& in) -> Size { Size n; in >> n; return n; }
inline auto read_string(InStream& in) -> String { String s; in >> s; return s; }

auto read_strings_from_standard_input () -> Set<String>
{
    Set<String> x;
    Size n = read_size(standard_input);
    while (n--) x.insert(read_string(standard_input));
    return x;
}

inline auto write_string(const String& s) -> void {
    standard_output << s << "\n";
}

// ------------------------------------------------------------
// MAIN MPI
// ------------------------------------------------------------
int main(int argc, char** argv)
{
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    auto start = std::chrono::high_resolution_clock::now();

    Set<String> ss;
    if (rank == 0) ss = read_strings_from_standard_input();

    // broadcast das strings
    {
        // serializa
        std::vector<String> all;
        if (rank == 0) for (auto& s : ss) all.push_back(s);

        int count = all.size();
        MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (rank != 0) all.resize(count);

        for (int i = 0; i < count; ++i) {
            int len = (rank == 0 ? all[i].size() : 0);
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);

            std::string tmp(len, ' ');
            if (rank == 0) tmp = all[i];

            MPI_Bcast(tmp.data(), len, MPI_CHAR, 0, MPI_COMM_WORLD);

            if (rank != 0) all[i] = tmp;
        }

        if (rank != 0) {
            ss.clear();
            for (auto& s : all) ss.insert(s);
        }
    }

    // greedy MPI
    auto greedy_order = shortest_superstring_with_order(ss, rank, nprocs);

    // apenas processo 0 aplica 2-opt + imprime
    if (rank == 0) {
        auto improved = two_opt(greedy_order);
        String final_s = build_superstring_from_order(improved);
        write_string(final_s);
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (rank == 0) {
        double t = std::chrono::duration<double>(end-start).count();
        standard_output << t << "\n";
    }

    MPI_Finalize();
    return 0;
}
