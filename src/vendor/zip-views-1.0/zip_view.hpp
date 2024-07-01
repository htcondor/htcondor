#ifndef ZIP_VIEW_HPP
#define ZIP_VIEW_HPP

#include <cassert>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <typeinfo>
#include <ranges>

namespace c9::detail {

template <typename ... Args, std::size_t ... Index>
auto any_match_impl(std::tuple<Args...> const & lhs,
    std::tuple<Args...> const & rhs,
    std::index_sequence<Index...>) -> bool
{
    auto result = false;
    result = (... | (std::get<Index>(lhs) == std::get<Index>(rhs)));
    return result;
}


template <typename ... Args>
auto any_match(std::tuple<Args...> const & lhs, std::tuple<Args...> const & rhs)
    -> bool
{
    return any_match_impl(lhs, rhs, std::index_sequence_for<Args...>{});
}



template <std::ranges::viewable_range ... Rng>
class zip_iterator
{
public:

    using value_type = std::tuple<
        std::ranges::range_reference_t<Rng>...
    >;

    zip_iterator() = delete;

    zip_iterator(std::ranges::iterator_t<Rng> && ... iters)
        : m_iters {std::forward<std::ranges::iterator_t<Rng>>(iters)...}
    {
    }

    auto operator++() -> zip_iterator& 
    {
        std::apply([](auto && ... args){ ((++args), ...); }, m_iters);
        return *this;
    }

    auto operator++(int) -> zip_iterator 
    {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    auto operator!=(zip_iterator const & other) const
    {
        return !(*this == other);
    }

    auto operator==(zip_iterator const & other) const
    {
        return any_match(m_iters, other.m_iters);
    }

    auto operator*() -> value_type
    {
        return std::apply([](auto && ... args){ 
                return value_type(*args...); 
            }, m_iters);
    }

private:
    std::tuple<std::ranges::iterator_t<Rng>...> m_iters;
};

template <std::ranges::viewable_range ... T>
class zipper
{
public:
    using zip_type = zip_iterator<T ...>;

    template <typename ... Args>
    zipper(Args && ... args)
        : m_args{std::forward<Args>(args)...}
    {
    }

    auto begin() -> zip_type
    {
        return std::apply([](auto && ... args){
            return zip_type(std::ranges::begin(args)...);
            }, m_args);
    }
    auto end() -> zip_type
    {
        return std::apply([](auto && ... args){
            return zip_type(std::ranges::end(args)...);
            }, m_args);
    }

private:
    std::tuple<T ...> m_args;

};

} // namespace c9::detail


#ifdef ZIP_VIEW_INJECT_STD_VIEWS_NAMESPACE
namespace std::ranges::views {
#else
namespace c9 {
#endif

template <std::ranges::viewable_range ... T>
auto zip(T && ... t)
{
    return c9::detail::zipper<T ...>{std::forward<T>(t)...};
}

}

#endif // ZIP_VIEW_HPP
