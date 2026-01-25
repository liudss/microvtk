#pragma once

#ifdef MICROVTK_HAS_CABANA
#include <Cabana_Core.hpp>
#include <ranges>
#include <type_traits>

namespace microvtk {

template <typename T>
struct extract_data_type;

template <typename DataType, typename MemorySpace, typename AccessType,
          int VectorLength, int Stride>
struct extract_data_type<
    Cabana::Slice<DataType, MemorySpace, AccessType, VectorLength, Stride>> {
  using type = DataType;
};

template <typename SliceType>
class CabanaFlattenedView
    : public std::ranges::view_interface<CabanaFlattenedView<SliceType>> {
public:
  using ValueType = typename extract_data_type<SliceType>::type;
  using ScalarType = std::remove_all_extents_t<ValueType>;

  static constexpr size_t get_num_components() {
    if constexpr (std::is_array_v<ValueType>) {
      return std::extent_v<ValueType>;
    } else {
      return 1;
    }
  }

  static constexpr size_t NumComponents = get_num_components();

  explicit CabanaFlattenedView(const SliceType& slice) : slice_(slice) {}

  [[nodiscard]] auto size() const { return slice_.size() * NumComponents; }

  struct Iterator {
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ScalarType;
    using reference = ScalarType;
    using pointer = void;

    const SliceType* s;
    size_t idx;

    // Tag dispatching to handle Scalar vs Array slice access

    // Scalar access: Rank 0
    template <size_t R>
      requires(R == 0)
    [[nodiscard]] reference access(std::integral_constant<size_t, R>,
                                   size_t t_idx, size_t) const {
      return (*s)(t_idx);
    }

    // Array access: Rank > 0
    template <size_t R>
      requires(R > 0)
    [[nodiscard]] reference access(std::integral_constant<size_t, R>,
                                   size_t t_idx, size_t c_idx) const {
      return (*s)(t_idx, c_idx);
    }

    [[nodiscard]] reference operator*() const {
      size_t tuple_idx = idx / NumComponents;
      size_t comp_idx = idx % NumComponents;
      return access(std::integral_constant<size_t, std::rank_v<ValueType>>{},
                    tuple_idx, comp_idx);
    }

    Iterator& operator++() {
      ++idx;
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++idx;
      return tmp;
    }
    Iterator& operator--() {
      --idx;
      return *this;
    }
    Iterator operator--(int) {
      Iterator tmp = *this;
      --idx;
      return tmp;
    }

    Iterator& operator+=(difference_type n) {
      idx += n;
      return *this;
    }
    Iterator& operator-=(difference_type n) {
      idx -= n;
      return *this;
    }

    [[nodiscard]] Iterator operator+(difference_type n) const {
      return Iterator{s, idx + n};
    }
    friend Iterator operator+(difference_type n, const Iterator& it) {
      return it + n;
    }
    [[nodiscard]] Iterator operator-(difference_type n) const {
      return Iterator{s, idx - n};
    }
    [[nodiscard]] difference_type operator-(const Iterator& other) const {
      return idx - other.idx;
    }

    [[nodiscard]] bool operator==(const Iterator& other) const {
      return idx == other.idx;
    }
    [[nodiscard]] bool operator!=(const Iterator& other) const {
      return idx != other.idx;
    }
    [[nodiscard]] bool operator<(const Iterator& other) const {
      return idx < other.idx;
    }
    [[nodiscard]] bool operator>(const Iterator& other) const {
      return idx > other.idx;
    }
    [[nodiscard]] bool operator<=(const Iterator& other) const {
      return idx <= other.idx;
    }
    [[nodiscard]] bool operator>=(const Iterator& other) const {
      return idx >= other.idx;
    }

    [[nodiscard]] reference operator[](difference_type n) const {
      return *(*this + n);
    }
  };

  [[nodiscard]] auto begin() const { return Iterator{&slice_, 0}; }
  [[nodiscard]] auto end() const { return Iterator{&slice_, size()}; }

private:
  const SliceType& slice_;
};

template <typename SliceType>
[[nodiscard]] auto adapt(const SliceType& slice) {
  return CabanaFlattenedView<SliceType>(slice);
}

}  // namespace microvtk
#endif
