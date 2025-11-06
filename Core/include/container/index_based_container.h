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

    constexpr explicit Index(int index, int generationIndex = 0) : index_(index), generationIndex_(generationIndex)
    {
    }

    [[nodiscard]] constexpr bool operator==(const Index& index) const
    {
        return index_ == index.index_ && generationIndex_ == index.generationIndex_;
    }

    auto index() const {return index_;}
    auto generationIndex() const {return generationIndex_;}

private:
    template <typename U, typename V>
    friend class IndexBasedContainer;
    IndexType index_ = -1;
    GenerationIndexType generationIndex_ = 0;
};

template <template <typename> class AllocatorT, typename T>
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
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it)), it->second};
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
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it)), it->second};
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
        Index<T> index{static_cast<Index<T>::index_type>(std::distance(values_.begin(), it)), it->second};
        it->first = {};
        return index;
    }

    [[nodiscard]] T& operator[](Index<T> index)
    {
        auto& pair = values_[index.index_];
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong generation index");
        }
        return pair.first;
    }

    [[nodiscard]] const T& operator[](Index<T> index) const
    {
        const auto& pair = values_[index.index_];
        if (index.generationIndex_ != pair.second)
        {
            throw std::out_of_range("Wrong Generation Index");
        }
        return pair.first;
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

    class Iterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        using pair_type = std::pair<T, typename Index<T>::generation_index_type>;

        explicit Iterator(pair_type* ptr) : m_ptr(ptr)
        {
        }

        reference operator*() const { return m_ptr->first; }
        pointer operator->() const { return &m_ptr->first; }

        Iterator& operator++()
        {
            ++m_ptr;
            return *this;
        }

        Iterator operator++(int)
        {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        Iterator& operator--()
        {
            --m_ptr;
            return *this;
        }

        Iterator operator--(int)
        {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        Iterator operator+(difference_type n) const { return MyIterator(m_ptr + n); }
        Iterator operator-(difference_type n) const { return MyIterator(m_ptr - n); }
        difference_type operator-(const Iterator& other) const { return m_ptr - other.m_ptr; }
        bool operator==(const Iterator& other) const { return m_ptr == other.m_ptr; }
        bool operator!=(const Iterator& other) const { return m_ptr != other.m_ptr; }
        bool operator<(const Iterator& other) const { return m_ptr < other.m_ptr; }
        bool operator>(const Iterator& other) const { return m_ptr > other.m_ptr; }
        bool operator<=(const Iterator& other) const { return m_ptr <= other.m_ptr; }
        bool operator>=(const Iterator& other) const { return m_ptr >= other.m_ptr; }

    private:
        pair_type* m_ptr;
    };

    auto begin()
    {
        return Iterator{values_.data()};
    }

    auto end()
    {
        return Iterator{values_.data() + values_.size()};
    }

    void clear()
    {
        values_.clear();
    }

    auto allocator() const noexcept { return values_.get_allocator(); }

private:
    static_assert(CanBeInvalid<T>, "requires function bool IsInvalid() && GenerateInvalidValue();");
    std::vector<std::pair<T, typename Index<T>::generation_index_type>, AllocatorT> values_;
};
}

#endif //NEKOLIB_INDEX_BASED_CONTAINER_H
