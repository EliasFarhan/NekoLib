#include "utils/resource.h"
#include <gtest/gtest.h>

class SimpleObject final {
  public:
  ~SimpleObject() {
    value_ = 0;
  }

  void set_value(int new_value) {
    value_ = new_value;
  }

  [[nodiscard]] const auto& value() const{return value_;}
private:
  int value_ = 0;
};

class SimpleObjectDestructor {
  public:
  explicit SimpleObjectDestructor(SimpleObject* objectPtr): objectPtr_(objectPtr) {

  }
  void operator()() const {
    //always check nullptr as destructor can be called when object is null
    if (objectPtr_ != nullptr) {
      objectPtr_->set_value(-1);
    }
  }
private:
  SimpleObject* objectPtr_ = nullptr;
};

TEST(Resource, RAII) {
  SimpleObject simple_object;
  simple_object.set_value(1);

  ASSERT_EQ(simple_object.value(), 1);
  {
    neko::Resource<SimpleObject*, SimpleObjectDestructor> resource(&simple_object);
    //destructor is called, so value is -1 now
  }
  ASSERT_EQ(simple_object.value(), -1);
}

TEST(Resource, MoveSemantics) {
  SimpleObject simple_object;
  simple_object.set_value(1);

  ASSERT_EQ(simple_object.value(), 1);
  {
    neko::Resource<SimpleObject*, SimpleObjectDestructor> resource_parent;
    {
      neko::Resource<SimpleObject*, SimpleObjectDestructor> resource(&simple_object);
      resource_parent = std::move(resource);
      ASSERT_EQ(resource.get() , nullptr); //resource was moved into the parent
    }
    ASSERT_EQ(resource_parent->value(), 1);
    ASSERT_EQ((*resource_parent)->value(), 1);
    ASSERT_EQ(simple_object.value(), 1);
    //destructor of parent is called, so value is -1 now
  }
  ASSERT_EQ(simple_object.value(), -1);
}

// Global counter to track resource lifecycle
struct ResourceCounter {
  static inline int destructor_calls = 0;
  static inline int active_resources = 0;

  static void reset() {
    destructor_calls = 0;
    active_resources = 0;
  }
};

// Destructor that tracks calls
class CountingDestructor {
public:
  explicit CountingDestructor(int* resource_ptr) : ptr_(resource_ptr) {}

  void operator()() const {
    if (ptr_ != nullptr && *ptr_ > 0) {
      ResourceCounter::destructor_calls++;
      ResourceCounter::active_resources--;
      *ptr_ = 0;  // Mark as destroyed
    }
  }

private:
  int* ptr_ = nullptr;
};

TEST(Resource, MoveAssignmentCleansUpOldResource) {
  ResourceCounter::reset();

  int resource_a = 1;  // Represents resource A
  int resource_b = 2;  // Represents resource B
  ResourceCounter::active_resources = 2;

  {
    neko::Resource<int*, CountingDestructor> r1(&resource_a);
    {
      neko::Resource<int*, CountingDestructor> r2(&resource_b);

      // Before move: r1 owns resource_a, r2 owns resource_b
      ASSERT_EQ(ResourceCounter::destructor_calls, 0);

      // Move assignment: After swap, r2 will own resource_a and clean it up when destroyed
      r1 = std::move(r2);

      ASSERT_EQ(r1.get(), &resource_b);  // r1 now owns resource_b
      ASSERT_EQ(r2.get(), &resource_a);  // r2 now owns resource_a (via swap)
      ASSERT_EQ(ResourceCounter::destructor_calls, 0);  // Nothing destroyed yet

      // r2 goes out of scope here
    }

    // r2's destructor should have cleaned up resource_a (the old resource from r1)
    ASSERT_EQ(ResourceCounter::destructor_calls, 1);
    ASSERT_EQ(resource_a, 0);  // resource_a was destroyed
    ASSERT_EQ(resource_b, 2);  // resource_b still alive

    // r1 goes out of scope here
  }

  // r1's destructor should have cleaned up resource_b
  ASSERT_EQ(ResourceCounter::destructor_calls, 2);
  ASSERT_EQ(resource_b, 0);  // resource_b was destroyed

  // All resources cleaned up - no leak!
  ASSERT_EQ(ResourceCounter::active_resources, 0);
}
