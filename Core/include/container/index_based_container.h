#ifndef NEKOLIB_INDEX_BASED_CONTAINER_H
#define NEKOLIB_INDEX_BASED_CONTAINER_H
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <memory>

namespace neko
{
template <typename T>
concept CanBeInvalid = requires(const T value)
{
    { value.IsInvalid() } -> std::same_as<bool>;
    { T::GenerateInvalidValue() } -> std::convertible_to<T>;
};

template <typename T, typename IndexType=int, typename GenerationIndexType=int>
class Index
{
public:
    using index_type = IndexType;
    using generation_index_type = GenerationIndexType;

    explicit Index(int index, int generationIndex = 0) : index_(index), generationIndex_(generationIndex)
    {
    }

    bool operator==(const Index& index) const
    {
        return index_ == index.index_ && generationIndex_ == index.generationIndex_;
    }

private:
    template <typename U, typename V>
    friend class IndexBasedContainer;
    IndexType index_ = -1;
    GenerationIndexType generationIndex_ = 0;
};

template<template <typename> class AllocatorT, typename T>
using indexed_container_allocator_type = AllocatorT<std::pair<T, typename Index<T>::generation_index_type>>;

template <typename T, typename AllocatorT=std::allocator<std::pair<T, typename Index<T>::generation_index_type>>>
class IndexBasedContainer
{
public:
    IndexBasedContainer() = default;

    explicit IndexBasedContainer(const AllocatorT& allocator) : values_(allocator)
    {
    }

    explicit IndexBasedContainer(AllocatorT&& allocator) : values_(std::move(allocator))
    {
    }

    Index<T> add(T&& new_value)
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({std::move(new_value), 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it))};
        it->first = std::move(new_value);
        return index;
    }

    Index<T> add(const T& new_value)
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({new_value, 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it))};
        it->first = new_value;
        return index;
    }

    Index<T> add()
    {
        auto it = std::find_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return v.first.IsInvalid();
        });
        if (it == values_.end())
        {
            Index<T> index{static_cast<Index<T>::index_type>(std::ssize(values_))};
            values_.push_back({{}, 0});
            return index;
        }
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it))};
        it->first = {};
        return index;
    }

    [[nodiscard]] const T& at(Index<T> index) const
    {
        const auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong Generation Index");
        }
        return pair.first;
    }

    [[nodiscard]] T& at(Index<T> index)
    {
        auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        return pair.first;
    }

    void remove(Index<T> index)
    {
        auto& pair = values_.at(index.index_);
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        pair.first = T::GenerateInvalidValue();
        ++pair.second;
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return std::count_if(values_.begin(), values_.end(), [](const auto& v)
        {
            return !v.IsInvalid();
        });
    }

    auto allocator() const noexcept{return values_.get_allocator();}
private:
    static_assert(CanBeInvalid<T>, "requires function bool IsInvalid() && GenerateInvalidValue();");
    std::vector<std::pair<T, typename Index<T>::generation_index_type>, AllocatorT> values_;
};
}

#endif //NEKOLIB_INDEX_BASED_CONTAINER_H
