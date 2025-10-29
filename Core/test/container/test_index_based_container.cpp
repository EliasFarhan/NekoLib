#include "container/index_based_container.h"

#include <gtest/gtest.h>


struct Value {
  int value;
  [[nodiscard]] constexpr static Value GenerateInvalidValue() noexcept {return {-1};}
  [[nodiscard]] constexpr bool IsInvalid() const noexcept {return value == -1;}
};

TEST(IndexBasedContainer, Construction) {
  neko::IndexBasedContainer<Value> values = {};
}

TEST(IndexBasedContainer, AddValue) {
  neko::IndexBasedContainer<Value> values = {};
  values.add();
}

TEST(IndexBasedContainer, RemoveValue) {
  neko::IndexBasedContainer<Value> values = {};
  auto index = values.add();
  values.remove(index);
}

TEST(IndexBasedContainer, GettingARemovedValueThrows) {
  neko::IndexBasedContainer<Value> values = {};
  auto index = values.add();
  values.remove(index);
  EXPECT_THROW(auto v = values.at(index), std::out_of_range);
}

class MoveOnlyValue {
public:
  int value = 0;
  MoveOnlyValue() = default;
  MoveOnlyValue(MoveOnlyValue&&) noexcept = default;
  MoveOnlyValue& operator=(MoveOnlyValue&&) noexcept = default;
  MoveOnlyValue(const MoveOnlyValue&) = delete;
  MoveOnlyValue& operator=(const MoveOnlyValue&) = delete;
  ~MoveOnlyValue() = default;
  [[nodiscard]] constexpr static MoveOnlyValue GenerateInvalidValue() noexcept {
    MoveOnlyValue value;
    value.value = -1;
    return value;
  }
  [[nodiscard]] constexpr bool IsInvalid() const noexcept {
    return value == -1;
  }
};
TEST(IndexBasedContainer, MoveValue) {
  neko::IndexBasedContainer<MoveOnlyValue> values = {};
  MoveOnlyValue m;
  constexpr int value = 3;
  m.value = value;
  const auto index = values.add(std::move(m));
  EXPECT_EQ(values.at(index).value, value);
  values.remove(index);
  [[maybe_unused]] const auto default_index = values.add();
}

template<typename T>
class CustomAllocator
{
public:
  using value_type = T;
  CustomAllocator() = default;
  template<typename U>
    constexpr CustomAllocator(const CustomAllocator<U>&) noexcept {}

  T* allocate(std::size_t n) {
    if (n > std::allocator_traits<CustomAllocator>::max_size(*this)) {
      throw std::bad_alloc();
    }
    const auto size = n * sizeof(T);
    total_size += size;
    return static_cast<T*>(::operator new(size));
  }

  void deallocate(T* p, std::size_t n) noexcept {
    const auto size = n * sizeof(T);
    total_size-=size;
    ::operator delete(p);
  }

  size_t total_size = 0;
};

TEST(IndexBasedContainer, WithCustomAllocator)
{
  using custom_allocator_type = neko::indexed_container_allocator_type<CustomAllocator, Value>;
  custom_allocator_type allocator;
  neko::IndexBasedContainer<Value, custom_allocator_type> values{allocator};
  values.add();
  EXPECT_EQ(values.allocator().total_size, sizeof(std::pair<Value, neko::Index<Value>::generation_index_type>));

}

TEST(IndexBasedContainer, RangeBasedFor)
{
  neko::IndexBasedContainer<Value> values = {};
  int total = 0;
  for (int i = 0; i < 10; ++i)
  {
    values.add({i});
    total += i;
  }
  int value_total = 0;
  for (const auto& v : values)
  {
    value_total += v.value;
  }
  EXPECT_EQ(total, value_total);
}