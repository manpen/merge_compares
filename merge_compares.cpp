#include <iostream>
#include <algorithm>
#include <vector>
#include <random>
#include <cassert>
#include <chrono>
#include <utility>


class Item {

public:
    constexpr static bool check_dups = false;

    using value_type = uint32_t;
    explicit Item(value_type x=0) : key(x) {}

    static bool count_compares;
    static std::vector<size_t> counts;

    static bool record_compares;
    static std::vector< std::pair<value_type, value_type> > compare_pairs;

    bool operator<(const Item& o) const {
        if (count_compares) {
            counts[key]++;
            counts[o.key]++;
        }

        if (check_dups && record_compares) {
            //std::cout << key << " " << o.key << " " << compare_pairs.size() << std::endl;
            compare_pairs.emplace_back(std::min(key, o.key), std::max(key, o.key));
        }

        return key < o.key;
    }

    static void check_no_dups() {
        if (!check_dups || !record_compares)
            return;

        auto temp = compare_pairs;

        std::sort(temp.begin(), temp.end());
        auto it = std::unique(temp.begin(), temp.end());
        if (it != temp.end())
            std::terminate();

        compare_pairs.clear();
    }

protected:
    value_type key;

};

bool Item::count_compares = false;
std::vector<size_t> Item::counts {};
std::vector< std::pair<Item::value_type, Item::value_type> > Item::compare_pairs;

bool Item::record_compares = false;


using it = std::vector<Item>::iterator;

struct LinSearch {
    it operator() (it begin, it end, Item x) const {
        while(begin != end && *begin < x) begin++;
        return begin;
    }

    const std::string name() const {return "lin";}
};

struct ExpSearch {
    it operator() (it begin, it end, Item x) const {
        const size_t size = std::distance(begin, end);

        size_t i = 0;
        size_t last_i = 0;
        size_t step = 1;
        while (i < size && *std::next(begin, i) < x) {
            last_i = i;
            i += step;
            step *= 2;
        }

        if (last_i == i)
            return begin;

        if (i >= size) {
            if (last_i == size - 1 || *std::next(begin, size - 1) < x)
                return end;

            return std::lower_bound(begin + last_i + 1, begin + size - 1,
                                    x);

        } else {
            return std::lower_bound(begin + last_i + 1, begin + i, x);

        }
    }

    const std::string name() const {return "exp";}
};

template <typename Func>
void merge(it begin, it end, Func f) {
    static std::vector<Item> result;

    const auto size = std::distance(begin, end);

    if (false && size == 2) {
        if (*std::next(begin,1) < *begin) {
            std::swap(*std::next(begin,1), *begin);
            return;
        }
    }

    result.clear();
    result.reserve(size);

    const it middle = begin + size / 2;
    auto yi = middle;
    auto xi = begin;

    bool first = true;

    Item::record_compares = true;
    Item::compare_pairs.clear();

    while(xi != middle) {
        if (yi != end) {
            auto yi_smaller = f(yi + !first, end, *xi);

            result.insert(result.end(), yi, yi_smaller);
            yi = yi_smaller;
        }

        if (yi != end) {
            if (xi+1 == middle) {
                result.push_back(*xi);
                xi++;
            } else {
                auto xi_smaller = f(xi + 1, middle, *yi);
                result.insert(result.end(), xi, xi_smaller);
                xi = xi_smaller;
            }
        } else {
            result.insert(result.end(), xi, middle);
            xi = middle;
        }

        first = false;
    }

    Item::check_no_dups();
    Item::record_compares = false;

    // we do not insert a potential overhang of the second sequence into tmp,
    // since it would be copied to the same position anyways

    std::copy(result.cbegin(), result.cend(), begin);
}

template <typename T>
bool check_sorted(T begin, T end) {
    Item::count_compares = false;
    bool result = std::is_sorted(begin, end);
    Item::count_compares = true;

    return result;
}

template <typename Func>
void mergesort(it begin, it end, Func f) {
    const size_t size = std::distance(begin, end);
    if (size == 1) return;

    assert(size % 2 == 0);

    it middle = begin + size / 2;

    mergesort(begin, middle, f);
    mergesort(middle, end, f);

    merge(begin, end, f);
}

template <typename Func>
void benchmark(size_t n, int seed, Func f, const std::string& algo) {
    std::mt19937_64 prng(seed);

    std::vector<Item> data;
    Item::counts.resize(n);
    std::fill(Item::counts.begin(), Item::counts.end(), 0);
    data.reserve(n);
    for(uint32_t i=0; i<n; i++)
        data.emplace_back(i);
    std::shuffle(data.begin(), data.end(), prng);

    Item::count_compares = true;
    f(data.begin(), data.end());
    Item::count_compares = false;

    if (!check_sorted(data.begin(), data.end())) {
        std::terminate();
    }

    const double ninv = 1.0 / n;
    const auto maxv = *std::max_element(Item::counts.cbegin(), Item::counts.cend());
    const auto avgv = std::accumulate(Item::counts.cbegin(), Item::counts.cend(), 0.0,
                                      [ninv] (double s, auto x) {return s + x * ninv;});


    std::cout << algo << "," << n << "," << static_cast<int>(std::log2(n))
                          << "," << maxv << "," << avgv << std::endl;
}

int main() {
    std::cout << "algo,n,logn,maxc,avgc\n";

    uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    size_t i=123;
    while(1) {
        for(size_t n=16; n<(1u<<28); n*=2) {
            if (true)
            benchmark(n, seed*i+n, [] (auto begin, auto end) {
                mergesort(begin, end, LinSearch{});
            }, "lin");

            if (true)
            benchmark(n, seed*i+n, [] (auto begin, auto end) {
                mergesort(begin, end, ExpSearch{});
            }, "exp");

            if (true)
            benchmark(n, seed*i+n, [] (auto begin, auto end) {
                std::stable_sort(begin, end);
            }, "stab");

            if (true)
            benchmark(n, seed*i+n, [] (auto begin, auto end) {
                std::sort(begin, end);
            }, "intro");
        }
        i += std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    return 0;

}