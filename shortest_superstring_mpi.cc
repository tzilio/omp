#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdio>   // snprintf

#include <mpi.h>

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
    if (x.empty()) return ss;
    for (Size i = size(x) - 1; i > 0; --i) {
        ss.insert(x.substr(i));
    }
    return ss;
}

auto commom_suffix_and_prefix(const String& a, const String& b) -> String {
    if (empty(a) || empty(b)) return "";
    String x = "";
    for (const String& s : all_suffixes(a)) {
        if (is_prefix(s, b) && size(s) > size(x))
            x = s;
    }
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
    std::vector<String> v; 
    v.reserve(ss.size());
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
        Size i   = idx / (n - 1);
        Size col = idx % (n - 1);
        Size j   = (col < i ? col : col + 1);
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

    for (const auto& p : pairs) {
        Size ov = overlap_value(p.first, p.second);
        if (!has ||
            ov > best_value ||
            (ov == best_value && p < best_pair))
        {
            best_pair  = p;
            best_value = ov;
            has        = true;
        }
    }

    if (!has) {
        return Pair<String,String>{String(), String()};
    }

    return best_pair;
}

// ------------------------------------------------------------
// Estrutura auxiliar para MPI_Gather / MPI_Bcast
// ------------------------------------------------------------
struct BestInfo {
    int  value;     // -1 = sem candidato, >=0 = overlap
    char a[256];
    char b[256];
};

// ------------------------------------------------------------
// MPI: redução global para encontrar o melhor par
//   (usa Gather para o rank 0 e depois Broadcast)
// ------------------------------------------------------------
auto reduce_best_pair_mpi(const Pair<String,String>& local,
                          int rank, int nprocs)
    -> Pair<String,String>
{
    BestInfo send{};
    // se local.first/local.second vazios, marcamos como "sem candidato"
    if (local.first.empty() && local.second.empty()) {
        send.value = -1;
        send.a[0]  = '\0';
        send.b[0]  = '\0';
    } else {
        send.value = static_cast<int>(overlap_value(local.first, local.second));
        std::snprintf(send.a, 256, "%s", local.first.c_str());
        std::snprintf(send.b, 256, "%s", local.second.c_str());
    }

    std::vector<BestInfo> all;
    if (rank == 0) {
        all.resize(static_cast<std::size_t>(nprocs));
    }

    const int bytes = static_cast<int>(sizeof(BestInfo));

    MPI_Gather(&send,  bytes, MPI_BYTE,
               rank == 0 ? all.data() : nullptr, bytes, MPI_BYTE,
               0, MPI_COMM_WORLD);

    BestInfo best{};
    bool found = false;

    if (rank == 0) {
        for (int p = 0; p < nprocs; ++p) {
            const BestInfo& cand = all[static_cast<std::size_t>(p)];
            if (cand.value < 0) continue; // sem candidato nesse processo

            if (!found) {
                best  = cand;
                found = true;
            } else {
                if (cand.value > best.value) {
                    best = cand;
                } else if (cand.value == best.value) {
                    Pair<String,String> pb(String(best.a), String(best.b));
                    Pair<String,String> pc(String(cand.a), String(cand.b));
                    if (pc < pb) best = cand;
                }
            }
        }
    }

    MPI_Bcast(&best, bytes, MPI_BYTE, 0, MPI_COMM_WORLD);

    if (!found) {
        return Pair<String,String>{String(), String()};
    }
    return Pair<String,String>{ String(best.a), String(best.b) };
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
    return reduce_best_pair_mpi(local_best, rank, nprocs);
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
                std::reverse(cand.begin() + static_cast<std::ptrdiff_t>(i),
                             cand.begin() + static_cast<std::ptrdiff_t>(j) + 1);

                String cstr = build_superstring_from_order(cand);
                Size clen = cstr.size();

                if (clen < best_len) {
                    best     = std::move(cand);
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
    order.reserve(t.size());
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

    int rank = 0, nprocs = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    auto start = std::chrono::high_resolution_clock::now();

    Set<String> ss;
    if (rank == 0) ss = read_strings_from_standard_input();

    // broadcast das strings
    {
        std::vector<String> all;
        if (rank == 0) {
            all.reserve(ss.size());
            for (auto& s : ss) all.push_back(s);
        }

        int count = (rank == 0 ? static_cast<int>(all.size()) : 0);
        MPI_Bcast(&count, 1, MPI_INT, 0, MPI_COMM_WORLD);

        if (rank != 0) all.resize(static_cast<std::size_t>(count));

        for (int i = 0; i < count; ++i) {
            int len = (rank == 0 ? static_cast<int>(all[i].size()) : 0);
            MPI_Bcast(&len, 1, MPI_INT, 0, MPI_COMM_WORLD);

            std::string tmp(static_cast<std::size_t>(len), ' ');
            if (rank == 0) tmp = all[i];

            if (len > 0) {
                MPI_Bcast(&tmp[0], len, MPI_CHAR, 0, MPI_COMM_WORLD);
            }

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
        double t = std::chrono::duration<double>(end - start).count();
        standard_output << t << "\n";
    }

    MPI_Finalize();
    return 0;
}
