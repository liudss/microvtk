#pragma once

#include <concepts>
#include <ranges>
#include <span>
#include <vector>

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
// Usage: adapt(particles, &Particle::mass)
template <std::ranges::range Container, typename MemberType, typename ClassType>
auto adapt(const Container& c, MemberType ClassType::* member) noexcept {
  return c |
         std::views::transform([member](const auto& obj) -> const MemberType& {
           return obj.*member;
         });
}

}  // namespace microvtk
