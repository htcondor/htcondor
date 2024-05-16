
#define ZIP_VIEW_INJECT_STD_VIEWS_NAMESPACE
#include "zip_view.hpp"

#include <cassert>

auto main() -> int 
{
    auto a = std::vector<int>{1, 2, 3, 4, 5, 6};
    auto b = std::vector<int>{1, 2, 3, 4, 5, 6, 7};
    auto c = std::vector<int>{0, 0, 0, 0, 0};
    auto const & d = b;

    for (auto && [x, y, z] : std::views::zip(a, d, c)) {
        z = x + y;
        x++;
    }

    auto a_expected = std::vector<int>{2, 3, 4, 5, 6, 6};
    auto b_expected = std::vector<int>{1, 2, 3, 4, 5, 6, 7};
    auto c_expected = std::vector<int>{2, 4, 6, 8, 10};
    assert(a == a_expected);
    assert(b == b_expected);
    assert(c == c_expected);

    auto l = std::list<int>{1, 2, 3, 4, 5, 6};

    std::list<int> res;
    for (auto && [x, y] : std::views::zip(a, l)) {
        res.push_back(x + y);
    }

    auto res_expected = std::list<int>{3, 5, 7, 9, 11, 12};

    assert(res == res_expected);

    std::vector<bool> res_even = {false, false, false, false, false, true};

    for (auto && [x, y] : std::views::zip(res, res_even)) {
        assert((x % 2 == 0) == y);
    }

    std::vector<bool> res_neg(6, false);

    for (auto && [x, y] : std::views::zip(res_even, res_neg)) {
        y = !x;
    }

    auto res_neg_expected = std::vector<bool>{true, true, true, true, true, false};
    assert(res_neg == res_neg_expected);
}
