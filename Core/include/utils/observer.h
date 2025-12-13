#ifndef NEKOLIB_OBSERVER_H
#define NEKOLIB_OBSERVER_H


#include <span>
#include <stdexcept>
#include <vector>

namespace neko {

/**
 * @brief Static observer pattern implementation.
 *
 * ObserverSubject provides a centralized registry of observers for a specific type T.
 * Each distinct type T gets its own static observer list, allowing for type-safe
 * notification of observers without requiring a separate subject object.
 *
 * Design characteristics:
 * - Static storage: All observers are stored in a single static vector per type
 * - Nullptr reuse: Removed observers leave nullptr slots that are reused by new observers
 * - Raw pointers: Uses raw pointers with manual lifetime management
 * - Type-specific: Each instantiation of ObserverSubject<T> has its own observer list
 * - Duplicate prevention: Cannot add the same observer twice
 * - Null safety: Cannot add nullptr observers
 *
 * Memory management:
 * - The observer vector grows but never shrinks
 * - Removed observers are replaced with nullptr (slots are reused)
 * - After many add/remove cycles, the vector may contain many nullptr entries
 * - This is a time-space tradeoff favoring add performance over memory efficiency
 *
 * Thread safety:
 * - NOT thread-safe
 * - Concurrent calls to AddObserver/RemoveObserver will cause data races
 * - All observer operations must be performed from a single thread or externally synchronized
 *
 * Lifetime management:
 * - Observers must remain valid for the duration of their registration
 * - Observers must call RemoveObserver before destruction to avoid dangling pointers
 * - No automatic cleanup is performed
 *
 * @tparam T The observer type
 *
 * Example usage:
 * @code
 * using SystemObserverSubject = neko::ObserverSubject<System>;
 * @endcode
 */
template <typename T>
class ObserverSubject {
 public:
  /**
   * @brief Adds an observer to the static observer list.
   *
   * If there are any nullptr slots (from previously removed observers), the first
   * nullptr slot is reused. Otherwise, the observer is appended to the vector.
   *
   * Behavior:
   * - Validates that observer is not nullptr
   * - Validates that observer is not already registered (no duplicates allowed)
   * - O(n) time complexity due to linear search for nullptr slots and duplicate check
   *
   * @param observer Pointer to the observer to add (must not be nullptr or already registered)
   * @throws std::invalid_argument if observer is nullptr
   * @throws std::invalid_argument if observer already exists in the list
   *
   * Example:
   * @code
   * using SystemObserverSubject = neko::ObserverSubject<System>;
   * SystemObserverSubject::AddObserver(this);
   * @endcode
   */
  static void AddObserver(T* observer) {
    if (observer == nullptr)
    {
      throw std::invalid_argument("Observer is null");
    }
    if (observers_.contains(observer))
    {
      throw std::invalid_argument("Observer already exists");
    }

    auto it = std::ranges::find(observers_, nullptr);
    if (it == observers_.end()) {
      observers_.push_back(observer);
    } else {
      *it = observer;
    }
  }

  /**
   * @brief Removes an observer from the static observer list.
   *
   * The observer slot is replaced with nullptr rather than being removed from the vector.
   * This nullptr slot will be reused by future AddObserver calls.
   *
   * @param observer Pointer to the observer to remove
   * @throws std::invalid_argument if the observer is not found in the list
   *
   * Exception safety:
   * - Throws if observer not found (strong guarantee)
   * - Consider catching this exception if removal of non-existent observers is acceptable
   *
   * @note ALWAYS call this before destroying an observer to prevent dangling pointers
   *
   * Example:
   * @code
   * using SystemObserverSubject = neko::ObserverSubject<System>;
   * SystemObserverSubject::RemoveObserver(this);
   * @endcode
   */
  static void RemoveObserver(T* observer) {
    auto it = std::ranges::find(observers_, observer);
    if (it != observers_.end()) {
      *it = nullptr;
    } else {
      throw std::invalid_argument("Observer does not exist");
    }
  }

  /**
   * @brief Gets a span of all observers (including nullptr entries).
   *
   * The returned span contains ALL registered observers, but may include nullptr entries
   * where observers have been removed. Callers MUST check for nullptr before dereferencing.
   *
   * @return std::span<T*> A view over the observer list
   *
   * Important:
   * - The span may contain nullptr entries from removed observers
   * - The span is invalidated if AddObserver causes vector reallocation
   * - Do not store the span long-term; request it each time you need it
   * - Always check for nullptr when iterating
   *
   * Example:
   * @code
   * using SystemObserverSubject = neko::ObserverSubject<System>;
   * for (auto* system : SystemObserverSubject::GetObservers()) {
   *   if (system) {  // CRITICAL: Check for nullptr
   *     system->OnUpdate(dt);
   *   }
   * }
   * @endcode
   */
  static std::span<T*> GetObservers() { return observers_; }

 private:
  /// Static storage for all observers of type T
  inline static std::vector<T*> observers_;
};
}  // namespace neko
#endif  // NEKOLIB_OBSERVER_H
