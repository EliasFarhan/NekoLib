#pragma once

#include <cstdint>
#include <iterator>

namespace neko
{

template<typename T>
class Span
{
public:
	class Iterator
	{
	public:
		using iterator_category = std::random_access_iterator_tag;
		using difference_type   = std::ptrdiff_t;
		using value_type        = T;
		using pointer           = const T*;  // or also value_type*
		using reference         = const T&;  // or also value_type&

		constexpr Iterator(pointer ptr) : ptr_(ptr){}

		constexpr reference operator*() const { return *ptr_; }
		constexpr pointer operator->() { return ptr_; }

		// Prefix increment
		constexpr Iterator& operator++() { ptr_++; return *this; }

		// Postfix increment
		constexpr Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

		constexpr friend bool operator== (const Iterator& a, const Iterator& b) { return a.ptr_ == b.ptr_; };
		constexpr friend bool operator!= (const Iterator& a, const Iterator& b) { return a.ptr_ != b.ptr_; };
	private:
		pointer ptr_ = nullptr;
	};

	constexpr Span()= default;
	constexpr Span(const T* pointer, std::size_t size): ptr_(pointer), size_(size){ }
	template <typename Container>
	constexpr Span(const Container& container): ptr_(container.data()), size_(container.size())
	{

	}

	[[nodiscard]] constexpr Iterator cbegin() const noexcept { return {ptr_};}
	[[nodiscard]] constexpr Iterator begin() const noexcept { return {ptr_};}
	[[nodiscard]] constexpr Iterator cend() const noexcept { return {ptr_+size_};}
	[[nodiscard]] constexpr Iterator end() const noexcept { return {ptr_+size_};}

	[[nodiscard]] constexpr std::size_t size() const { return size_; }
	[[nodiscard]] constexpr const T& operator[](std::size_t i) const { return ptr_[i]; }
private:
	const T* ptr_ = nullptr;
	std::size_t size_ = 0;
};

}