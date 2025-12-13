
#ifndef NEKOLIB_RESOURCE_H
#define NEKOLIB_RESOURCE_H

#include <utility>

namespace neko {

/**
 * @brief Concept that verifies a type can act as a custom destructor for a resource.
 *
 * A valid destructor type must be:
 * - Constructible from a resource value
 * - Callable immediately after construction: DestructorT(resource)()
 * - Callable via operator() on an existing instance
 *
 * @tparam DestructorT The destructor type to validate
 * @tparam ResourceT The resource type the destructor will manage
 *
 * Example:
 * @code
 * class ShaderReleaser {
 * public:
 *   explicit ShaderReleaser(SDL_GPUShader* shader) : shader_(shader) {}
 *   void operator()() const {
 *     if (shader_) SDL_ReleaseGPUShader(GetDevice(), shader_);
 *   }
 * private:
 *   SDL_GPUShader* shader_;
 * };
 * // Satisfies: ShaderReleaser can be constructed with SDL_GPUShader* and called
 * @endcode
 */
template <typename DestructorT, typename ResourceT>
concept is_resource_destructible =
    requires(DestructorT destructor, ResourceT resource) {
      { DestructorT(resource) };
      { DestructorT(resource)() };
      { destructor() };
    };

/**
 * @brief RAII wrapper for managing resources with custom cleanup logic.
 *
 * Resource provides automatic resource management (RAII) with a user-defined destructor.
 * It ensures that resources are properly cleaned up when the Resource object goes out of
 * scope, similar to std::unique_ptr but with customizable destruction behavior.
 *
 * Key features:
 * - Custom destructor logic via the Destructor template parameter
 * - Move-only semantics (non-copyable)
 * - Swap-based move operations for exception safety
 * - Automatic cleanup on destruction
 * - Pointer-like access via operator-> and operator* for convenience
 *
 * Move semantics:
 * - Move operations use std::swap internally
 * - The moved-from object takes ownership of the original resource via swap
 * - When the moved-from object is destroyed, it cleans up the original resource
 * - This ensures no resource leaks during move assignment
 *
 * Thread safety:
 * - Not thread-safe by default
 * - Each Resource object should be owned by a single thread
 *
 * @tparam T The resource type to manage (e.g., SDL_GPUShader*, int handle, custom type)
 * @tparam Destructor A callable type that cleans up the resource when invoked
 *
 * Example usage:
 * @code
 * class ShaderReleaser {
 * public:
 *   explicit ShaderReleaser(SDL_GPUShader* shader) : shader_(shader) {}
 *   void operator()() const {
 *     if (shader_) SDL_ReleaseGPUShader(GetDevice(), shader_);
 *   }
 * private:
 *   SDL_GPUShader* shader_;
 * };
 *
 * // Usage
 * {
 *   Resource<SDL_GPUShader*, ShaderReleaser> shader(LoadShader("vertex.spv"));
 *   // Use shader...
 * } // Shader automatically released here
 *
 * // Move semantics
 * Resource<SDL_GPUShader*, ShaderReleaser> shader1(LoadShader("a.spv"));
 * Resource<SDL_GPUShader*, ShaderReleaser> shader2(LoadShader("b.spv"));
 * shader1 = std::move(shader2);  // a.spv is released when shader2 is destroyed
 *                                 // shader1 now owns b.spv
 * @endcode
 */
template <typename T, typename Destructor>
class Resource {
  static_assert(is_resource_destructible<Destructor, T>,
                "Resource requires a Destructor type");

 public:
  /**
   * @brief Default constructor. Creates a Resource with default-initialized data.
   *
   * The resource is initialized to T{}, which should represent a "null" or "empty" state.
   * When destroyed, the destructor will be called on this default value, so ensure the
   * Destructor handles default-constructed values safely.
   */
  explicit Resource() = default;

  /**
   * @brief Constructs a Resource from a const lvalue reference.
   *
   * @param data The resource to manage (copied)
   */
  explicit Resource(const T& data) : data_(data) {}

  /**
   * @brief Constructs a Resource from an rvalue reference.
   *
   * @param data The resource to manage (moved)
   */
  explicit Resource(T&& data) : data_(std::move(data)) {}

  /**
   * @brief Destructor. Automatically calls Clear() to clean up the resource.
   *
   * Note: Virtual destructor allows for potential inheritance, though this is not
   * the primary design intent.
   */
  virtual ~Resource() { Clear(); }

  /// Copy operations are deleted - Resource is move-only
  Resource(const Resource& other) = delete;
  Resource& operator=(const Resource& other) = delete;

  /**
   * @brief Move constructor. Transfers ownership via swap.
   *
   * After construction, this object owns other's resource, and other owns the
   * default-constructed value T{}.
   *
   * @param other The Resource to move from
   */
  Resource(Resource&& other) noexcept { std::swap(data_, other.data_); }

  /**
   * @brief Move assignment operator. Transfers ownership via swap.
   *
   * The old resource from this object is swapped into other, and will be cleaned up
   * when other is destroyed. This ensures no resource leaks.
   *
   * @param other The Resource to move from
   * @return Reference to this object
   */
  Resource& operator=(Resource&& other) noexcept {
    std::swap(data_, other.data_);
    return *this;
  }

  /**
   * @brief Get a const reference to the managed resource.
   *
   * @return Const reference to the resource
   */
  [[nodiscard]] const T& get() const { return data_; }

  /**
   * @brief Get a mutable reference to the managed resource.
   *
   * @return Reference to the resource
   */
  [[nodiscard]] T& get() { return data_; }

  /**
   * @brief Pointer access operator for mutable access.
   *
   * If T is a pointer type, returns the pointer itself.
   * If T is a value type, returns a pointer to the value.
   *
   * @return Pointer for member access
   */
  auto operator->() {
    if constexpr (std::is_pointer_v<T>) {
      return data_;
    } else {
      return &data_;
    }
  }

  /**
   * @brief Pointer access operator for const access.
   *
   * If T is a pointer type, returns the pointer itself.
   * If T is a value type, returns a pointer to the value.
   *
   * @return Const pointer for member access
   */
  auto operator->() const {
    if constexpr (std::is_pointer_v<T>) {
      return data_;
    } else {
      return &data_;
    }
  }

  /**
   * @brief Dereference operator for mutable access.
   *
   * If T is a pointer type, returns the pointer itself.
   * If T is a value type, returns a pointer to the value.
   *
   * @return Pointer or reference to the underlying resource
   */
  auto operator*() {
    if constexpr (std::is_pointer_v<T>) {
      return data_;
    } else {
      return &data_;
    }
  }

  /**
   * @brief Dereference operator for const access.
   *
   * If T is a pointer type, returns the pointer itself.
   * If T is a value type, returns a pointer to the value.
   *
   * @return Const pointer or reference to the underlying resource
   */
  auto operator*() const {
    if constexpr (std::is_pointer_v<T>) {
      return data_;
    } else {
      return &data_;
    }
  }

  /**
   * @brief Manually clean up the resource.
   *
   * Constructs a Destructor with the current resource, invokes it, and resets
   * the resource to its default state T{}. This is automatically called by the
   * destructor, but can also be called manually to release the resource early.
   *
   * After calling Clear(), the Resource contains T{} (typically a "null" state).
   * The destructor should handle T{} gracefully as a no-op.
   */
  void Clear() {
    Destructor destructor(data_);
    destructor();
    data_ = {};
  }

 private:
  T data_{};
};
}  // namespace neko

#endif  // NEKOLIB_RESOURCE_H
