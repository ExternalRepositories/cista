#pragma once

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <ostream>
#include <type_traits>
#include <vector>

#include "cista/containers/ptr.h"
#include "cista/is_iterable.h"
#include "cista/next_power_of_2.h"

namespace cista {

template <typename T, typename Ptr = T*, bool IndexPointers = false,
          typename TemplateSizeType = uint32_t>
struct basic_vector {
  using size_type = TemplateSizeType;
  using value_type = T;
  using iterator = T*;
  using const_iterator = T const*;

  basic_vector() noexcept = default;
  explicit basic_vector(TemplateSizeType size) { resize(size); }
  basic_vector(std::initializer_list<T> init) { set(init.begin(), init.end()); }

  template <typename It>
  basic_vector(It begin_it, It end_it) {
    set(begin_it, end_it);
  }

  basic_vector(basic_vector&& arr) noexcept
      : el_(arr.el_),
        used_size_(arr.used_size_),
        allocated_size_(arr.allocated_size_),
        self_allocated_(arr.self_allocated_) {
    arr.el_ = nullptr;
    arr.used_size_ = 0;
    arr.self_allocated_ = false;
    arr.allocated_size_ = 0;
  }

  basic_vector(basic_vector const& arr) { set(arr); }

  basic_vector& operator=(basic_vector&& arr) noexcept {
    deallocate();

    el_ = arr.el_;
    used_size_ = arr.used_size_;
    self_allocated_ = arr.self_allocated_;
    allocated_size_ = arr.allocated_size_;

    arr.el_ = nullptr;
    arr.used_size_ = 0;
    arr.self_allocated_ = false;
    arr.allocated_size_ = 0;

    return *this;
  }

  basic_vector& operator=(basic_vector const& arr) {
    if (&arr != this) {
      set(arr);
    }
    return *this;
  }

  ~basic_vector() { deallocate(); }

  void deallocate() {
    if (!self_allocated_ || el_ == nullptr) {
      return;
    }

    for (auto& el : *this) {
      el.~T();
    }

    std::free(el_);  // NOLINT
    el_ = nullptr;
    used_size_ = 0;
    allocated_size_ = 0;
    self_allocated_ = 0;
  }

  T const* data() const noexcept { return begin(); }
  T* data() noexcept { return begin(); }
  T const* begin() const noexcept { return el_; }
  T const* end() const noexcept { return el_ + used_size_; }  // NOLINT
  T* begin() noexcept { return el_; }
  T* end() noexcept { return el_ + used_size_; }  // NOLINT

  std::reverse_iterator<T const*> rbegin() const {
    return std::reverse_iterator<T*>(el_ + size());  // NOLINT
  }
  std::reverse_iterator<T const*> rend() const {
    return std::reverse_iterator<T*>(el_);
  }
  std::reverse_iterator<T*> rbegin() {
    return std::reverse_iterator<T*>(el_ + size());  // NOLINT
  }
  std::reverse_iterator<T*> rend() { return std::reverse_iterator<T*>(el_); }

  friend T const* begin(basic_vector const& a) noexcept { return a.begin(); }
  friend T const* end(basic_vector const& a) noexcept { return a.end(); }

  friend T* begin(basic_vector& a) noexcept { return a.begin(); }
  friend T* end(basic_vector& a) noexcept { return a.end(); }

  inline T const& operator[](size_t index) const noexcept {
    assert(el_ != nullptr && index < used_size_);
    return el_[index];
  }
  inline T& operator[](size_t index) noexcept {
    assert(el_ != nullptr && index < used_size_);
    return el_[index];
  }

  inline T& at(size_t index) {
    if (index >= used_size_) {
      throw std::out_of_range{"vector::at(): invalid index"};
    }
    return (*this)[index];
  }

  inline T const& at(size_t index) const {
    return const_cast<basic_vector*>(this)->at(index);
  }

  T const& back() const noexcept { return ptr_cast(el_)[used_size_ - 1]; }
  T& back() noexcept { return ptr_cast(el_)[used_size_ - 1]; }

  T& front() noexcept { return ptr_cast(el_)[0]; }
  T const& front() const noexcept { return ptr_cast(el_)[0]; }

  inline TemplateSizeType size() const noexcept { return used_size_; }
  inline bool empty() const noexcept { return size() == 0; }

  template <typename It>
  void set(It begin_it, It end_it) {
    auto range_size =
        static_cast<TemplateSizeType>(std::distance(begin_it, end_it));
    assert(range_size <= std::numeric_limits<TemplateSizeType>::max() &&
           "size type overflow");
    reserve(static_cast<TemplateSizeType>(range_size));

    auto copy_source = begin_it;
    auto copy_target = el_;
    for (; copy_source != end_it; ++copy_source, ++copy_target) {
      new (copy_target) T{std::forward<decltype(*copy_source)>(*copy_source)};
    }

    used_size_ = static_cast<TemplateSizeType>(range_size);
  }

  void set(basic_vector const& arr) {
#if false
// disbled for unrelated reasons for now
// but: this code should be good!
    if constexpr (std::is_trivially_copyable_v<T>) {
      if (arr.used_size_ != 0) {
        reserve(arr.used_size_);
        std::memcpy(data(), arr.data(), arr.used_size_ * sizeof(T));
      }
      used_size_ = arr.used_size_;
    } else {
#endif
    set(std::begin(arr), std::end(arr));
#if false
    }
#endif
  }

  friend std::ostream& operator<<(std::ostream& out, basic_vector const& v) {
    out << "[\n  ";
    auto first = true;
    for (auto const& e : v) {
      if (!first) {
        out << ",\n  ";
      }
      out << e;
      first = false;
    }
    return out << "\n]";
  }

  template <typename Arg>
  void insert(T* it, Arg&& el) {
    auto const pos = it - begin();

    reserve(used_size_ + 1);
    it = begin() + pos;

    for (auto move_it = end() - 1, pred = end(); pred != it; --move_it) {
      *pred = std::move(*move_it);
      pred = move_it;
    }

    new (it) T{std::forward<Arg>(el)};
    ++used_size_;
  }

  template <class InputIt>
  T* insert(T* pos, InputIt first, InputIt last, std::input_iterator_tag) {
    auto const old_offset = std::distance(begin(), pos);
    auto const old_size = used_size_;

    for (; !(first == last); ++first) {
      reserve(used_size_ + 1);
      new (el_ + used_size_) T{std::forward<decltype(*first)>(*first)};
      ++used_size_;
    }

    return std::rotate(begin() + old_offset, begin() + old_size, end());
  }

  template <class FwdIt>
  T* insert(T* pos, FwdIt first, FwdIt last, std::forward_iterator_tag) {
    auto const pos_idx = pos - begin();
    auto const new_count =
        static_cast<TemplateSizeType>(std::distance(first, last));
    reserve(used_size_ + new_count);
    pos = begin() + pos_idx;

    for (auto src_last = end() - 1, dest_last = end() + new_count - 1;
         !(src_last == pos - 1); --src_last, --dest_last) {
      if (dest_last >= end()) {
        new (dest_last) T(std::move(*src_last));
      } else {
        *dest_last = std::move(*src_last);
      }
    }

    for (auto insert_ptr = pos; !(first == last); ++first, ++insert_ptr) {
      if (insert_ptr >= end()) {
        new (insert_ptr) T(std::forward<decltype(*first)>(*first));
      } else {
        *insert_ptr = std::forward<decltype(*first)>(*first);
      }
    }

    used_size_ += new_count;

    return pos;
  }

  template <class It>
  T* insert(T* pos, It first, It last) {
    return insert(pos, first, last,
                  typename std::iterator_traits<It>::iterator_category());
  }

  void push_back(T const& el) {
    reserve(used_size_ + 1);
    new (el_ + used_size_) T(el);
    ++used_size_;
  }

  template <typename... Args>
  T& emplace_back(Args&&... el) {
    reserve(used_size_ + 1);
    new (el_ + used_size_) T{std::forward<Args>(el)...};
    T* ptr = el_ + used_size_;
    ++used_size_;
    return *ptr;
  }

  void resize(size_type size, T init = T{}) {
    reserve(size);
    for (auto i = used_size_; i < size; ++i) {
      new (el_ + i) T{init};
    }
    used_size_ = size;
  }

  void clear() {
    used_size_ = 0;
    for (auto& el : *this) {
      el.~T();
    }
  }

  void reserve(TemplateSizeType new_size) {
    new_size = std::max(allocated_size_, new_size);

    if (allocated_size_ >= new_size) {
      return;
    }

    auto next_size = next_power_of_two(new_size);
    auto num_bytes = sizeof(T) * next_size;
    auto mem_buf = static_cast<T*>(std::malloc(num_bytes));  // NOLINT
    if (mem_buf == nullptr) {
      throw std::bad_alloc();
    }

    if (size() != 0) {
      try {
        auto move_target = mem_buf;
        for (auto& el : *this) {
          new (move_target++) T(std::move(el));
        }

        for (auto& el : *this) {
          el.~T();
        }
      } catch (...) {
        assert(0);
      }
    }

    auto free_me = el_;
    el_ = mem_buf;
    if (self_allocated_) {
      std::free(free_me);  // NOLINT
    }

    self_allocated_ = true;
    allocated_size_ = next_size;
  }

  T* erase(T* pos) {
    T* last = end() - 1;
    while (pos < last) {
      std::swap(*pos, *(pos + 1));
      pos = pos + 1;
    }
    pos->~T();
    --used_size_;
    return end();
  }

  T* erase(T* first, T* last) {
    if (first != last) {
      auto const new_end = std::move(last, end(), first);
      for (auto it = new_end; it != end(); ++it) {
        it->~T();
      }
      used_size_ -= std::distance(new_end, end());
    }
    return end();
  }

  bool contains(T const* el) const noexcept {
    return el >= begin() && el < end();
  }

  friend bool operator==(basic_vector const& a,
                         basic_vector const& b) noexcept {
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
  }
  friend bool operator!=(basic_vector const& a,
                         basic_vector const& b) noexcept {
    return !(a == b);
  }
  friend bool operator<(basic_vector const& a, basic_vector const& b) {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  }
  friend bool operator>(basic_vector const& a, basic_vector const& b) noexcept {
    return b < a;
  }
  friend bool operator<=(basic_vector const& a,
                         basic_vector const& b) noexcept {
    return !(a > b);
  }
  friend bool operator>=(basic_vector const& a,
                         basic_vector const& b) noexcept {
    return !(a < b);
  }

  Ptr el_{nullptr};
  TemplateSizeType used_size_{0};
  TemplateSizeType allocated_size_{0};
  bool self_allocated_{false};
  uint8_t __fill_0__{0};
  uint16_t __fill_1__{0};
  uint32_t __fill_2__{0};
};

#define CISTA_TO_VEC                                                        \
  template <typename It, typename UnaryOperation>                           \
  inline auto to_vec(It s, It e, UnaryOperation&& op)                       \
      ->vector<decay_t<decltype(op(*s))>> {                                 \
    vector<decay_t<decltype(op(*s))>> v;                                    \
    v.reserve(                                                              \
        static_cast<typename decltype(v)::size_type>(std::distance(s, e))); \
    std::transform(s, e, std::back_inserter(v), op);                        \
    return v;                                                               \
  }                                                                         \
                                                                            \
  template <typename Container, typename UnaryOperation>                    \
  inline auto to_vec(Container const& c, UnaryOperation&& op)               \
      ->vector<decltype(op(*std::begin(c)))> {                              \
    vector<decltype(op(*std::begin(c)))> v;                                 \
    v.reserve(static_cast<typename decltype(v)::size_type>(                 \
        std::distance(std::begin(c), std::end(c))));                        \
    std::transform(std::begin(c), std::end(c), std::back_inserter(v), op);  \
    return v;                                                               \
  }                                                                         \
                                                                            \
  template <typename Container>                                             \
  inline auto to_vec(Container const& c)                                    \
      ->vector<decay_t<decltype(*std::begin(c))>> {                         \
    vector<decay_t<decltype(*std::begin(c))>> v;                            \
    v.reserve(static_cast<typename decltype(v)::size_type>(                 \
        std::distance(std::begin(c), std::end(c))));                        \
    std::copy(std::begin(c), std::end(c), std::back_inserter(v));           \
    return v;                                                               \
  }                                                                         \
                                                                            \
  template <typename It, typename UnaryOperation>                           \
  inline auto to_indexed_vec(It s, It e, UnaryOperation&& op)               \
      ->indexed_vector<decay_t<decltype(op(*s))>> {                         \
    indexed_vector<decay_t<decltype(op(*s))>> v;                            \
    v.reserve(                                                              \
        static_cast<typename decltype(v)::size_type>(std::distance(s, e))); \
    std::transform(s, e, std::back_inserter(v), op);                        \
    return v;                                                               \
  }                                                                         \
                                                                            \
  template <typename Container, typename UnaryOperation>                    \
  inline auto to_indexed_vec(Container const& c, UnaryOperation&& op)       \
      ->indexed_vector<decay_t<decltype(op(*std::begin(c)))>> {             \
    indexed_vector<decay_t<decltype(op(*std::begin(c)))>> v;                \
    v.reserve(static_cast<typename decltype(v)::size_type>(                 \
        std::distance(std::begin(c), std::end(c))));                        \
    std::transform(std::begin(c), std::end(c), std::back_inserter(v), op);  \
    return v;                                                               \
  }                                                                         \
                                                                            \
  template <typename Container>                                             \
  inline auto to_indexed_vec(Container const& c)                            \
      ->indexed_vector<decay_t<decltype(*std::begin(c))>> {                 \
    indexed_vector<decay_t<decltype(*std::begin(c))>> v;                    \
    v.reserve(static_cast<typename decltype(v)::size_type>(                 \
        std::distance(std::begin(c), std::end(c))));                        \
    std::copy(std::begin(c), std::end(c), std::back_inserter(v));           \
    return v;                                                               \
  }

namespace raw {

template <typename T>
using vector = basic_vector<T, ptr<T>>;

template <typename T>
using indexed_vector = basic_vector<T, ptr<T>, true>;

CISTA_TO_VEC

}  // namespace raw

namespace offset {

template <typename T>
using vector = basic_vector<T, ptr<T>>;

template <typename T>
using indexed_vector = basic_vector<T, ptr<T>, true>;

CISTA_TO_VEC

}  // namespace offset

#undef CISTA_TO_VEC

}  // namespace cista
