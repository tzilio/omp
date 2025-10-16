#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <chrono>
#include <vector>

#ifdef _OPENMP
  #include <omp.h>
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

// ---------- temporização ----------
static double g_time_pairs_gen = 0.0;
static double g_time_best_scan = 0.0;

static inline double walltime() {
#ifdef _OPENMP
    return omp_get_wtime();
#else
    using clock = std::chrono::high_resolution_clock;
    static const auto t0 = clock::now();
    auto t = clock::now();
    return std::chrono::duration<double>(t - t0).count();
#endif
}

// ---------- utilitários ----------
template <typename C> inline auto size (const C& x) -> SizeType <C> { return x.size (); }
template <typename C> inline auto at_least_two_elements_in (const C& c) -> Boolean { return size (c) > SizeType <C> (1) ; }
template <typename T> inline auto first_element (const Set <T>& x) -> T { return *(x.begin ()) ; }
template <typename T> inline auto second_element (const Set <T>& x) -> T { return *(std::next (x.begin ())) ; }

template <typename T> inline auto remove (Set <T>& x, const T& e) -> Set <T>& { x.erase (e) ; return x ; }
template <typename T> inline auto push   (Set <T>& x, const T& e) -> Set <T>& { x.insert (e) ; return x ; }
template <typename C> inline auto empty  (const C& x) -> Boolean { return x.empty () ; }

Boolean is_prefix (const String& a, const String& b)
{
    if (size (a) > size (b)) return false ;
    if ( !( std::mismatch(a.begin(), a.end(), b.begin()).first == a.end() ) ) return false ;
    return true ;
}

inline auto suffix_from_position (const String& x, SizeType <String> i) -> String { return x.substr (i) ; }

inline auto remove_prefix (const String& x, SizeType <String> n) -> String
{
    if (size (x) > n) return suffix_from_position (x, n) ;
    return x ;
}

auto all_suffixes (const String& x) -> Set <String>
{
    Set <String> ss ;
    SizeType <String> n = size (x) ;
    while (-- n) { ss.insert (x.substr (n)) ; }
    return ss ;
}

auto commom_suffix_and_prefix (const String& a, const String& b) -> String
{
    if (empty (a) || empty (b)) return "" ;
    String x = "" ;
    for (const String& s : all_suffixes (a)) {
        if (is_prefix (s, b) && size (s) > size (x)) x = s ;
    }
    return x ;
}

inline auto overlap_value (const String& s, const String& t) -> SizeType <String>
{
    return size (commom_suffix_and_prefix (s, t)) ;
}

auto overlap (const String& s, const String& t) -> String
{
    String c = commom_suffix_and_prefix (s, t) ;
    return s + remove_prefix (t, size (c)) ;
}

inline auto pop_two_elements_and_push_overlap (Set <String>& ss, const Pair <String, String>& p) -> Set <String>&
{
    ss = remove (ss, p.first)  ;
    ss = remove (ss, p.second) ;
    ss = push   (ss, overlap (p.first, p.second)) ;
    return ss ;
}

// ===================== geração paralela de pares =====================
// idx determinístico (sem diagonal): idx = i*(n-1) + (j < i ? j : j-1)
auto all_distinct_pairs_parallel (const Set <String>& ss) -> std::vector<Pair<String,String>>
{
    const double t0 = walltime();

    std::vector<String> v; v.reserve(ss.size());
    for (const auto& s : ss) v.push_back(s);
    const Size n = v.size();

    if (n < 2) {
        g_time_pairs_gen += (walltime() - t0);
        return {};
    }

    std::vector<Pair<String,String>> pairs;
    pairs.resize(n * (n - 1));

#ifdef _OPENMP
    #pragma omp parallel for schedule(static)
#endif
    for (Size i = 0; i < n; ++i) {
        for (Size j = 0; j < n; ++j) {
            if (i == j) continue;
            const Size col = (j < i ? j : j - 1); // 0..(n-2)
            const Size idx = i*(n - 1) + col;     // 0..n*(n-1)-1
            pairs[idx] = Pair<String,String>{ v[i], v[j] };
        }
    }

    g_time_pairs_gen += (walltime() - t0);
    return pairs;
}

// ===================== melhor par em paralelo =====================
auto best_pair_from_pairs_parallel (const std::vector<Pair<String,String>>& pairs) -> Pair<String,String>
{
    const double t0 = walltime();

    Pair<String,String> global_pair;
    SizeType<String>    global_best = 0;
    bool                global_has  = false;

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        Pair<String,String> local_pair;
        SizeType<String>    local_best = 0;
        bool                local_has  = false;

#ifdef _OPENMP
        #pragma omp for schedule(dynamic)
#endif
        for (Size k = 0; k < pairs.size(); ++k) {
            const auto& p  = pairs[k];
            const auto ov = overlap_value(p.first, p.second);
            if (!local_has ||
                ov > local_best ||
                (ov == local_best && p < local_pair))
            {
                local_has  = true;
                local_best = ov;
                local_pair = p;
            }
        }

#ifdef _OPENMP
        #pragma omp critical
#endif
        {
            if (local_has) {
                if (!global_has ||
                    local_best > global_best ||
                    (local_best == global_best && local_pair < global_pair))
                {
                    global_has  = true;
                    global_best = local_best;
                    global_pair = local_pair;
                }
            }
        }
    }

    g_time_best_scan += (walltime() - t0);
    return global_pair;
}

// 1) gera pares; 2) escolhe melhor
auto pair_of_strings_with_highest_overlap_value_parallel_v2 (const Set <String>& ss) -> Pair <String, String>
{
    auto pairs = all_distinct_pairs_parallel(ss);
    return best_pair_from_pairs_parallel(pairs);
}

// ===================== algoritmo principal =====================
auto shortest_superstring (Set <String> t) -> String
{
    if (empty (t)) return "" ;
    while (at_least_two_elements_in (t)) {
        const auto best_pair = pair_of_strings_with_highest_overlap_value_parallel_v2(t);
        t = pop_two_elements_and_push_overlap(t, best_pair);
    }
    return first_element (t) ;
}

inline auto write_string_and_break_line (OutStream& out, String s) -> void { out << s << std::endl ; }

inline auto read_size   (InStream& in) -> Size   { Size n; in >> n; return n; }
inline auto read_string (InStream& in) -> String { String s; in >> s; return s; }

auto read_strings_from_standard_input () -> Set <String>
{
    using N = SizeType <Set <String>> ;
    Set <String> x ;
    N n = N (read_size (standard_input)) ;
    while (n --) x.insert (read_string (standard_input)) ;
    return x ;
}

inline auto write_string_to_standard_ouput (const String& s) -> void { write_string_and_break_line (standard_output, s) ; }

auto main (int /*argc*/, char const* /*argv*/[]) -> int
{
    g_time_pairs_gen = 0.0;
    g_time_best_scan = 0.0;

    Set<String> ss = read_strings_from_standard_input();

    const auto start = std::chrono::high_resolution_clock::now();
    write_string_to_standard_ouput(shortest_superstring(ss));
    const auto end   = std::chrono::high_resolution_clock::now();

    const double total = std::chrono::duration<double>(end - start).count();
    const double par   = g_time_pairs_gen + g_time_best_scan;
    const double seq   = std::max(0.0, total - par);
    const double seq_frac = (total > 0.0 ? seq/total : 0.0);

    standard_output << total << std::endl;

    std::cerr << total << " " << par << " " << seq << " " << seq_frac << "\n";
    return 0;
}