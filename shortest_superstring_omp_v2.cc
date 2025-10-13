#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <chrono>
#include <vector>
#include <omp.h>

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

template <typename C> inline auto size (const C& x) -> SizeType <C> { return x.size () ; }
template <typename C> inline auto at_least_two_elements_in (const C& c) -> Boolean { return size (c) > SizeType <C> (1) ; }
template <typename T> inline auto first_element (const Set <T>& x) -> T { return *(x.begin ()) ; }
template <typename T> inline auto second_element (const Set <T>& x) -> T { return *(std::next (x.begin ())) ; }
template <typename T> inline auto remove (Set <T>& x, const T& e) -> Set <T>& { x.erase (e) ; return x ; }
template <typename T> inline auto push (Set <T>& x, const T& e) -> Set <T>& { x.insert (e) ; return x ; }
template <typename C> inline auto empty (const C& x) -> Boolean { return x.empty () ; }

Boolean is_prefix (const String& a, const String& b)
{
    if (size (a) > size (b)) return false ;
    return std::mismatch(a.begin (), a.end (), b.begin ()).first == a.end () ;
}

inline auto suffix_from_position (const String& x, SizeType <String> i) -> String { return x.substr (i) ; }

// V2: se n >= |x|, retorna string vazia (remove tudo)
inline auto remove_prefix (const String& x, SizeType <String> n) -> String
{
    return (size(x) > n) ? suffix_from_position (x, n) : String();
}

// V2: inclui TODOS os sufixos (inclusive o inteiro)
auto all_suffixes (const String& x) -> Set <String>
{
    Set <String> ss ;
    for (SizeType<String> i = 0; i < size(x); ++i) ss.insert (x.substr (i));
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
{ return size (commom_suffix_and_prefix (s, t)) ; }

auto overlap (const String& s, const String& t) -> String
{
    String c = commom_suffix_and_prefix (s, t) ;
    return s + remove_prefix (t, size (c)) ;
}

// V2: maior overlap; em empate, MENOR |merge|; se empatar, menor par lex
static inline bool better_v2(size_t ov1, const String& a1, const String& b1,
                             size_t ov2, const String& a2, const String& b2)
{
    if (ov1 != ov2) return ov1 > ov2;
    Size L1 = a1.size() + b1.size() - ov1;
    Size L2 = a2.size() + b2.size() - ov2;
    if (L1 != L2) return L1 < L2;
    if (a1 != a2) return a1 < a2;
    return b1 < b2;
}

// Seleção paralela segundo os critérios v2
static Pair<String,String>
pair_of_strings_with_highest_overlap_value_parallel (const Set<String>& ss)
{
    std::vector<String> v(ss.begin(), ss.end());
    const int m = (int)v.size();

    int best_i = -1, best_j = -1;
    size_t best_ov = 0;

    #pragma omp parallel
    {
        int li = -1, lj = -1;
        size_t lov = 0;

        #pragma omp for schedule(static) nowait
        for (int idx = 0; idx < m*m; ++idx) {
            int i = idx / m, j = idx % m;
            if (i == j) continue;
            size_t ov = overlap_value(v[i], v[j]);
            if (li == -1 || better_v2(ov, v[i], v[j], lov, v[li], v[lj])) {
                lov = ov; li = i; lj = j;
            }
        }

        #pragma omp critical
        {
            if (li != -1 &&
               (best_i == -1 ||
                better_v2(lov, v[li], v[lj], best_ov, v[best_i], v[best_j])))
            {
                best_i = li; best_j = lj; best_ov = lov;
            }
        }
    }

    // fallback seguro
    if (best_i < 0 || best_j < 0 || best_i == best_j) {
        auto it1 = ss.begin(), it2 = std::next(it1);
        return std::make_pair(*it1, *it2);
    }
    return std::make_pair(v[best_i], v[best_j]);
}

inline auto pop_two_elements_and_push_overlap (Set <String>& ss, const Pair <String, String>& p) -> Set <String>&
{
    ss = remove (ss, p.first)  ;
    ss = remove (ss, p.second) ;
    ss = push   (ss, overlap (p.first, p.second)) ;
    return ss ;
}

auto shortest_superstring_parallel (Set <String> t) -> String
{
    if (empty (t)) return "" ;
    while (at_least_two_elements_in (t)) {
        Pair<String,String> best = pair_of_strings_with_highest_overlap_value_parallel(t);
        t = pop_two_elements_and_push_overlap(t, best);
    }
    return first_element (t) ;
}

inline auto write_string_and_break_line (OutStream& out, String s) -> void { out << s << std::endl ; }
inline auto read_size (InStream& in) -> Size { Size n ; in >>  n ; return n ; }
inline auto read_string (InStream& in) -> String { String s ; in >>  s ; return s ; }

auto read_strings_from_standard_input () -> Set <String>
{
    using N = SizeType <Set <String>> ;
    Set <String> x ;
    N n = N (read_size (standard_input)) ;
    while (n --) x.insert (read_string (standard_input)) ;
    return x ;
}

inline auto write_string_to_standard_ouput (const String& s) -> void { write_string_and_break_line (standard_output, s) ; }

auto main (int argc, char const* argv[]) -> int
{
    Set<String> ss = read_strings_from_standard_input();
    auto start = std::chrono::high_resolution_clock::now();
    write_string_to_standard_ouput(shortest_superstring_parallel(ss));
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    standard_output << elapsed.count() << std::endl;
    return 0;
}
