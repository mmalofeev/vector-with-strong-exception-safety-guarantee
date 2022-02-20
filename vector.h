#ifndef VECTOR_H_
#define VECTOR_H_

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lab_07 {
inline std::size_t calculate_capacity(std::size_t n) {
    if (n == 0) {
        return 0;
    }
    std::size_t power_of_two = 1;
    while (power_of_two < n) {
        power_of_two *= 2;
    }
    return power_of_two;
}

template <typename T, typename Alloc = std::allocator<T>>
class vector {
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_move_assignable_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    T *data_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t size_ = 0;

    void destruct(T *data, std::size_t begin, std::size_t end) {
        for (std::size_t delete_index = begin; delete_index < end;
             delete_index++) {
            (data + delete_index)->~T();
        }
    }

    T *alloc(std::size_t capacity) {
        if (capacity == 0) {
            return nullptr;
        }
        return Alloc().allocate(capacity);
    }

    void dealloc(T *data, std::size_t capacity) {
        if (data == nullptr || capacity == 0) {
            return;
        }
        Alloc().deallocate(data, capacity);
    }

    void increase_capacity(std::size_t new_capacity) {
        T *extradata = alloc(new_capacity);
        construct_section_move(extradata, 0, size_);
        destruct(data_, 0, size_);
        dealloc(data_, capacity_);
        std::swap(data_, extradata);
    }

    template <typename InitFunc>
    void construct_section(T *data,
                           std::size_t begin,
                           std::size_t end,
                           const InitFunc &init_func) {
        std::size_t delete_index = begin;
        try {
            for (std::size_t index = begin; index < end; index++) {
                delete_index = index;
                init_func(data + index);
            }
        } catch (...) {
            destruct(data, begin, delete_index);
            throw;
        }
    }

    void construct_section_move(T *data, std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; index++) {
            new (data + index) T(std::move(*(data_ + index)));
        }
    }

    template <typename InitFunc>
    explicit vector(std::size_t n, const InitFunc &init_func)
        : data_(alloc(calculate_capacity(n))),
          capacity_(calculate_capacity(n)),
          size_(n) {
        try {
            construct_section(data_, 0, size_, init_func);
        } catch (...) {
            size_ = 0;
            dealloc(data_, capacity_);
            capacity_ = 0;
            throw;
        }
    }

    template <typename InitFunc>
    void resize(std::size_t desired_size, const InitFunc &init_func) & {
        std::size_t desired_capacity = calculate_capacity(desired_size);
        if (desired_size <= size_) {
            destruct(data_, desired_size, size_);
        } else if (desired_size <= capacity_) {
            try {
                construct_section(data_, size_, desired_size, init_func);
            } catch (...) {
                throw;
            }
        } else {
            T *extradata = alloc(desired_capacity);
            try {
                construct_section(extradata, size_, desired_size, init_func);
            } catch (...) {
                dealloc(extradata, desired_capacity);
                throw;
            }
            construct_section_move(extradata, 0, size_);
            destruct(data_, 0, size_);
            dealloc(data_, capacity_);
            std::swap(data_, extradata);
            capacity_ = desired_capacity;
        }
        size_ = desired_size;
    }

public:
    vector() noexcept = default;

    explicit vector(std::size_t n)
        : vector(n, [](T *object_pointer) { new (object_pointer) T(); }) {
    }

    vector(std::size_t n, const T &element)
        : vector(n,
                 [&](T *object_pointer) { new (object_pointer) T(element); }) {
    }

    [[nodiscard]] T &operator[](std::size_t index) &noexcept {
        return data_[index];
    }

    [[nodiscard]] const T &operator[](std::size_t index) const &noexcept {
        return data_[index];
    }

    [[nodiscard]] T &&operator[](std::size_t index) &&noexcept {
        return std::move(data_[index]);
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    ~vector() noexcept {
        for (std::size_t index = 0; index < size_; index++) {
            (data_ + index)->~T();
        }
        if (data_ != nullptr) {
            dealloc(data_, capacity_);
        }
    }

    void push_back(T &&element) & {
        std::size_t new_capacity = capacity_;
        if (size_ == capacity_) {
            if (capacity_ != 0) {
                new_capacity = capacity_ * 2;
            } else {
                new_capacity = 1;
            }
            increase_capacity(new_capacity);
        }
        new (data_ + size_) T(std::move(element));
        size_++;
        capacity_ = new_capacity;
    }

    void push_back(const T &element) & {
        resize(size_ + 1, element);
    }

    void pop_back() &noexcept {
        assert(!empty());
        (data_ + size_ - 1)->~T();
        size_--;
    }

    vector(const vector &other)
        : data_(alloc(calculate_capacity(other.size_))),
          capacity_(calculate_capacity(other.size_)),
          size_(other.size_) {
        std::size_t delete_index = 0;
        try {
            for (std::size_t index = 0; index < other.size_; index++) {
                delete_index = index;
                new (data_ + index) T(other[index]);
            }
        } catch (...) {
            destruct(data_, 0, delete_index);
            size_ = 0;
            dealloc(data_, capacity_);
            capacity_ = 0;
            throw;
        }
    }

    vector &operator=(const vector &other) {
        if (this == &other) {
            return *this;
        }
        *this = vector(other);
        return *this;
    }

    vector(vector &&other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          capacity_(std::exchange(other.capacity_, 0)),
          size_(std::exchange(other.size_, 0)) {
    }

    vector &operator=(vector &&other) noexcept {
        clear();
        if (this == &other) {
            return *this;
        }
        std::swap(capacity_, other.capacity_);
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        return *this;
    }

    void clear() &noexcept {
        destruct(data_, 0, size_);
        size_ = 0;
    }

    void resize(std::size_t desired_size) & {
        resize(desired_size,
               [](T *object_pointer) { new (object_pointer) T(); });
    }

    void resize(std::size_t desired_size, const T &element) & {
        resize(desired_size,
               [&](T *object_pointer) { new (object_pointer) T(element); });
    }

    T &at(std::size_t index) & {
        if (index >= size_) {
            throw std::out_of_range("out of range");
        }
        return data_[index];
    }

    const T &at(std::size_t index) const & {
        if (index >= size_) {
            throw std::out_of_range("out of range");
        }
        return data_[index];
    }

    T &&at(std::size_t index) && {
        if (index >= size_) {
            throw std::out_of_range("out of range");
        }
        return std::move(data_[index]);
    }

    void reserve(std::size_t quantity) & {
        quantity = calculate_capacity(quantity);
        if (quantity <= capacity_) {
            return;
        }
        increase_capacity(quantity);
        capacity_ = quantity;
    }
};

}  // namespace lab_07

#endif  // VECTOR_H_
