//
// Created by unite on 26.10.2025.
//

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
  values.add_value();
}

TEST(IndexBasedContainer, RemoveValue) {
  neko::IndexBasedContainer<Value> values = {};
  auto index = values.add_value();
  values.remove(index);
}

TEST(IndexBasedContainer, GettingARemovedValueThrows) {
  neko::IndexBasedContainer<Value> values = {};
  auto index = values.add_value();
  values.remove(index);
  EXPECT_THROW(auto v = values.at(index), std::out_of_range);
}

