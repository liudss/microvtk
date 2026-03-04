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

/**
 * @brief A standard-compliant C++20 view for Cabana Slices.
 * Flattens multi-dimensional array slices into a scalar range.
 * This view maintains a pointer to the slice to ensure it is movable
 * (view-compliant) while avoiding slice handle copies.
 */
template <typename SliceType>
class CabanaFlattenedView
    : public std::ranges::view_interface<CabanaFlattenedView<SliceType>> {
public:
  using ValueType = typename extract_data_type<SliceType>::type;
  using ScalarType = std::remove_all_extents_t<ValueType>;

  static constexpr size_t get_num_components() {
    return sizeof(ValueType) / sizeof(ScalarType);
  }

  static constexpr size_t NumComponents = get_num_components();

  CabanaFlattenedView() = default;
  explicit CabanaFlattenedView(const SliceType& slice) : slice_ptr_(&slice) {}

  [[nodiscard]] auto size() const {
    return slice_ptr_ ? slice_ptr_->size() * NumComponents : 0;
  }

  struct Iterator {
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = ScalarType;
    using reference = ScalarType;
    using pointer = void;

    const SliceType* s = nullptr;
    size_t idx = 0;

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
      if constexpr (R == 1) {
        return (*s)(t_idx, c_idx);
      } else {
        using T = ValueType;
        size_t indices[R];
        size_t temp = c_idx;

        auto compute_idx = [&]<size_t... Is>(std::index_sequence<Is...>) {
          auto compute_one = [&](auto i_rev) {
            constexpr size_t d = R - 1 - i_rev;
            if constexpr (d > 0) {
              constexpr size_t ext = std::extent_v<T, d>;
              indices[d] = temp % ext;
              temp /= ext;
            } else {
              indices[0] = temp;
            }
          };
          (compute_one(std::integral_constant<size_t, Is>{}), ...);
        };
        compute_idx(std::make_index_sequence<R>{});

        return [&]<size_t... Is>(std::index_sequence<Is...>) {
          return (*s)(t_idx, indices[Is]...);
        }(std::make_index_sequence<R>{});
      }
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

  [[nodiscard]] auto begin() const { return Iterator{slice_ptr_, 0}; }
  [[nodiscard]] auto end() const { return Iterator{slice_ptr_, size()}; }

private:
  const SliceType* slice_ptr_ = nullptr;
};

/**
 * @brief Adapt a Cabana Slice into a scalar-flattened range.
 * The user must ensure the slice object remains valid while the view is in use.
 */
template <typename SliceType>
[[nodiscard]] auto adapt(const SliceType& slice) {
  return CabanaFlattenedView<SliceType>(slice);
}

}  // namespace microvtk
#endif
