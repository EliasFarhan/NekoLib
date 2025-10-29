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