#pragma once

#include <concepts>
#include <ranges>
#include <span>

namespace microvtk {

// Concepts
template <typename T>
concept Scalar = std::is_arithmetic_v<T>;

// Concept for a contiguous range of scalars (e.g., vector<float>)
template <typename R>
concept NumericRange =
    std::ranges::contiguous_range<R> && Scalar<std::ranges::range_value_t<R>>;

// View: Returns a span for contiguous containers
template <NumericRange Container>
auto view(const Container& c) noexcept {
  return std::span(c);
}

// Adapt: Returns a view for AoS member access
// Usage (Direct): adapt(particles, &Particle::mass)
// Usage (Pipe):   particles | adapt(&Particle::mass)

namespace detail {
template <typename MemberType, typename ClassType>
struct adapt_closure {
  MemberType ClassType::*member;

  template <std::ranges::range R>
  friend auto operator|(R&& r, const adapt_closure& closure) {
    return std::forward<R>(r) |
           std::views::transform(
               [m = closure.member](const auto& obj) -> const MemberType& {
                 return obj.*m;
               });
  }
};
}  // namespace detail

template <typename MemberType, typename ClassType>
auto adapt(MemberType ClassType::*member) noexcept {
  return detail::adapt_closure<MemberType, ClassType>{member};
}

template <std::ranges::range Container, typename MemberType, typename ClassType>
auto adapt(const Container& c, MemberType ClassType::*member) noexcept {
  return c | adapt(member);
}

}  // namespace microvtk
