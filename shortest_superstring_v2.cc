#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <chrono>

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

// ======= (1) corrigido: remove tudo se n >= |x| (evita duplicação desnecessária) =======
inline auto remove_prefix (const String& x, SizeType <String> n) -> String
{
    return (size(x) > n) ? suffix_from_position (x, n) : String(); // "" quando n >= |x|
}

// ======= (2) corrigido: inclui TODOS os sufixos (inclusive o inteiro) =======
auto all_suffixes (const String& x) -> Set <String>
{
    Set <String> ss ;
    for (SizeType<String> i = 0; i < size(x); ++i) {
        ss.insert (x.substr (i)); // inclui x.substr(0) == x
    }
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

inline auto pop_two_elements_and_push_overlap
        (Set <String>& ss, const Pair <String, String>& p) -> Set <String>&
{
    ss = remove (ss, p.first)  ;
    ss = remove (ss, p.second) ;
    ss = push   (ss, overlap (p.first, p.second)) ;
    return ss ;
}

auto all_distinct_pairs (const Set <String>& ss) -> Set <Pair <String, String>>
{
    Set <Pair <String, String>> x ;
    for (const String& s1 : ss)
        for (const String& s2 : ss)
            if (s1 != s2) x.insert (std::make_pair (s1, s2)) ;
    return x ;
}

// ======= (3) corrigido: desempate por menor string pós-merge; depois ordem lex =======
auto highest_overlap_value (const Set <Pair <String, String>>& sp) -> Pair <String, String>
{
    Pair <String, String> best = first_element (sp) ;
    Size best_ov = overlap_value (best.first, best.second) ;
    Size best_len = best.first.size() + best.second.size() - best_ov ;

    for (const auto& p : sp) {
        Size ov = overlap_value (p.first, p.second) ;
        if (ov > best_ov) {
            best = p; best_ov = ov; best_len = p.first.size()+p.second.size()-ov; 
            continue;
        }
        if (ov == best_ov) {
            Size plen = p.first.size() + p.second.size() - ov;
            if (plen < best_len || (plen == best_len && p < best)) {
                best = p; best_len = plen;
            }
        }
    }
    return best ;
}

auto pair_of_strings_with_highest_overlap_value (const Set <String>& ss) -> Pair <String, String>
{
    return highest_overlap_value (all_distinct_pairs (ss)) ;
}

auto shortest_superstring (Set <String> t) -> String
{
    if (empty (t)) return "" ;
    while (at_least_two_elements_in (t)) {
        t = pop_two_elements_and_push_overlap
            ( t
            , pair_of_strings_with_highest_overlap_value (t) ) ;
    }
    return first_element (t) ;
}

inline auto read_string (InStream& in) -> String { String s ; in >>  s ; return s ; }
inline auto write_string_and_break_line (OutStream& out, String s) -> void { out << s << std::endl ; }
inline auto read_size (InStream& in) -> Size { Size n ; in >>  n ; return n ; }

auto read_strings_from_standard_input () -> Set <String>
{
    using N = SizeType <Set <String>> ;
    Set <String> x ;
    N n = N (read_size (standard_input)) ;
    while (n --) x.insert (read_string (standard_input)) ;
    return x ;
}

inline auto write_string_to_standard_ouput (const String& s) -> void { write_string_and_break_line (standard_output, s) ; }

auto main ()
{
    Set<String> ss = read_strings_from_standard_input();
    auto start = std::chrono::high_resolution_clock::now();
    write_string_to_standard_ouput(shortest_superstring(ss));
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    standard_output << elapsed.count() << std::endl;
    return 0;
}
