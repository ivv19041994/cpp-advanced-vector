#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <stdexcept>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    explicit RawMemory(size_t capacity);

    //star five:
    RawMemory(RawMemory&& other) noexcept;
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;
    RawMemory& operator=(RawMemory&&) = delete;
    ~RawMemory();

    T* operator+(size_t offset) noexcept;

    const T* operator+(size_t offset) const noexcept;

    const T& operator[](size_t index) const noexcept;
    T& operator[](size_t index) noexcept;

    void Swap(RawMemory& other) noexcept;

    const T* GetAddress() const noexcept;
    T* GetAddress() noexcept;

    size_t Capacity() const noexcept;

private:

    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n);

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator EmplaceReallocate(const_iterator pos, Args&&... args) {
        size_t new_capacity = size_ ? (size_ << 1) : 1;
        RawMemory<T> new_data{ new_capacity };
        const size_t pos_id = std::distance(cbegin(), pos);
        iterator new_data_pos = new_data.GetAddress() + pos_id;

        new (new_data_pos) T(std::forward<Args>(args)...);
        iterator throw_beg = new_data_pos;
        iterator throw_end = new_data_pos + 1;

        iterator ncpos = const_cast<iterator>(pos);
        try {
            UninitializedMoveOrCopyN(begin(), pos_id, new_data.GetAddress());
            throw_beg = new_data.GetAddress();
            UninitializedMoveOrCopyN(ncpos, size_ - pos_id, new_data_pos + 1);
        }
        catch (...) {
            std::destroy(throw_beg, throw_end);
            throw;
        }

        data_.Swap(new_data);
        capacity_ = new_capacity;
        std::destroy_n(new_data.GetAddress(), size_);
        ++size_;
        return new_data_pos;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (size_ == capacity_) {
            return EmplaceReallocate(pos, std::forward<Args>(args)...);
        }
        else {
            iterator ncpos = const_cast<iterator>(pos);
            if (ncpos != end()) {
                iterator end_but_one = end() - 1;
                std::uninitialized_move_n(end_but_one, 1, end());
                std::move_backward(ncpos, end() - 1, end());

                

                *ncpos = T(std::forward<Args>(args)...);//не проходят 86/87 тесты, когда Args == T вызывается лишний конструктор
            }
            else {
                new (ncpos) T(std::forward<Args>(args)...);
            }

            ++size_;
            return ncpos;
        }
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        iterator ncpos = const_cast<iterator>(pos);
        iterator ncend = end();
        iterator ncpos_plus_one = ncpos + 1;
        iterator del = MoveOrCopy(ncpos_plus_one, ncend, ncpos);
        Destroy(del);
        --size_;
        return ncpos;
    }

    iterator Insert(const_iterator pos, const T& value) {
        if (size_ == capacity_) {
            return EmplaceReallocate(pos, value);
        }
        else {

            iterator ncpos = const_cast<iterator>(pos);
            if (ncpos != end()) {
                T temp(value);
                iterator end_but_one = end() - 1;
                std::uninitialized_move_n(end_but_one, 1, end());
                std::move_backward(ncpos, end() - 1, end());
                *ncpos = std::move(temp);//проблема 86/87 теста в этой строке в Emplace тут *ncpos = T(std::forward<Args>(args)...);
                //вызывается лишний конструктор
            }
            else {
                new (ncpos) T(value);
            }

            ++size_;
            return ncpos;
        }
    }

    iterator InsertNew(const_iterator pos, const T& value) {
        if (size_ == capacity_) {
            return Emplace(pos, value);
        }
        T temp(value);
        return Emplace(pos, std::move(temp));
    }

    iterator InsertOld(const_iterator pos, T&& value) {
        if (size_ == capacity_) {
            size_t new_capacity = size_ ? (size_ << 1) : 1;
            RawMemory<T> new_data{ new_capacity };
            const size_t pos_id = std::distance(cbegin(), pos);
            iterator new_data_pos = new_data.GetAddress() + pos_id;

            new (new_data_pos) T(std::move(value));
            iterator throw_beg = new_data_pos;
            iterator throw_end = new_data_pos + 1;

            iterator ncpos = const_cast<iterator>(pos);
            try {
                UninitializedMoveOrCopyN(begin(), pos_id, new_data.GetAddress());
                throw_beg = new_data.GetAddress();
                UninitializedMoveOrCopyN(ncpos, size_ - pos_id, new_data_pos + 1);
            }
            catch (...) {
                std::destroy(throw_beg, throw_end);
                throw;
            }

            data_.Swap(new_data);
            capacity_ = new_capacity;
            std::destroy_n(new_data.GetAddress(), size_);
            ++size_;
            return new_data_pos;
        }
        else {
            iterator ncpos = const_cast<iterator>(pos);
            if (ncpos != end()) {
                iterator end_but_one = end() - 1;
                std::uninitialized_move_n(end_but_one, 1, end());
                std::move_backward(ncpos, end() - 1, end());
                *ncpos = T(std::move(value));
            }
            else {
                new (ncpos) T(std::move(value));
            }

            ++size_;
            return ncpos;
        }
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    Vector() noexcept {
    }

    Vector(size_t size)
        : data_{ size }
        , capacity_{ size }
        , size_{ size } {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        : data_{ other.size_ }
        , capacity_{ other.size_ }
        , size_{ other.size_ } {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_{ std::move(other.data_) }
        , capacity_{ other.capacity_ }
        , size_{ other.size_ } {
        other.capacity_ = 0;
        other.size_ = 0;
    }

    Vector& operator=(const Vector& other) {
        if (capacity_ >= other.size_) {
            const auto src = other.data_.GetAddress();
            auto dst = data_.GetAddress();
            if (size_ <= other.size_) {
                const auto src_mid = src + size_;
                auto output_it = std::copy(src, src_mid, dst);
                std::uninitialized_copy_n(src_mid, other.size_ - size_, output_it);
            }
            else {
                const auto src_end = src + other.size_;
                auto output_it = std::copy(src, src_end, dst);
                std::destroy_n(output_it, size_ - other.size_);
            }

            size_ = other.size_;
        }
        else {
            Vector copy_other{ other };
            Swap(copy_other);
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        Swap(other);
        return *this;
    }

    void Reserve(size_t capacity) {
        if (capacity_ >= capacity) {
            return;
        }
        RawMemory<T> new_data{ capacity };
        UninitializedMoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());
        data_.Swap(new_data);
        capacity_ = capacity;
        std::destroy_n(new_data.GetAddress(), size_);
    }

    ~Vector() noexcept {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {

        return capacity_;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(capacity_, other.capacity_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            if (new_size > capacity_) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBackOld(const T& value) {
        PushBackOld([&value](T* ptr) {
            new (ptr) T(value);
            });
    }
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBackOld(T&& value) {
        PushBackOld([&value](T* ptr) {
            new (ptr) T(std::move(value));
            });
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() /*noexcept*/ {
        --size_;
        auto des = data_.GetAddress() + size_;
        Destroy(des);
    }

    template <typename... Args>
    T& EmplaceBackOld(Args&&... args) {
        PushBackOld([&args...](T* ptr) {
            new (ptr) T(std::forward<Args>(args)...);
            });
        return *(data_.GetAddress() + (size_ - 1));
    }


    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == capacity_) {
            size_t new_capacity = size_ ? (size_ << 1) : 1;
            RawMemory<T> new_data{ new_capacity };
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            UninitializedMoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            capacity_ = new_capacity;
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *(data_.GetAddress() + (size_ - 1));
    }


private:
    RawMemory<T> data_;
    size_t capacity_ = 0;
    size_t size_ = 0;

    inline static void UninitializedMoveOrCopyN(T* src, size_t size, T* dst) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(src, size, dst);
        }
        else {
            std::uninitialized_copy_n(src, size, dst);
        }
    }

    inline static iterator MoveOrCopy(iterator src_beg, iterator src_end, iterator dst) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            return std::move(src_beg, src_end, dst);
        }
        else {
            return std::copy(src_beg, src_end, dst);
        }
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    template <typename MoveOrCopy>
    inline void PushBackOld(MoveOrCopy setter) {
        if (size_ == capacity_) {
            size_t new_capacity = size_ ? (size_ << 1) : 1;
            RawMemory<T> new_data{ new_capacity };
            setter(new_data.GetAddress() + size_);
            UninitializedMoveOrCopyN(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
            capacity_ = new_capacity;
            std::destroy_n(new_data.GetAddress(), size_);
        }
        else {
            setter(data_.GetAddress() + size_);
        }
        ++size_;
    }
};






template <typename T>
RawMemory<T>::RawMemory(size_t capacity)
    : buffer_(Allocate(capacity))
    , capacity_(capacity) {
}

template <typename T>
RawMemory<T>::RawMemory(RawMemory&& other) noexcept
    : buffer_{ other.buffer_ }
    , capacity_{ other.capacity_ } {
    other.buffer_ = nullptr;
    other.capacity_ = 0;
}

template <typename T>
RawMemory<T>::~RawMemory() {
    Deallocate(buffer_);
}

template <typename T>
T* RawMemory<T>::operator+(size_t offset) noexcept {
    // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
    assert(offset <= capacity_);
    return buffer_ + offset;
}

template <typename T>
const T* RawMemory<T>::operator+(size_t offset) const noexcept {
    return const_cast<RawMemory&>(*this) + offset;
}

template <typename T>
const T& RawMemory<T>::operator[](size_t index) const noexcept {
    return const_cast<RawMemory&>(*this)[index];
}

template <typename T>
T& RawMemory<T>::operator[](size_t index) noexcept {
    assert(index < capacity_);
    return buffer_[index];
}

template <typename T>
void RawMemory<T>::Swap(RawMemory& other) noexcept {
    std::swap(buffer_, other.buffer_);
    std::swap(capacity_, other.capacity_);
}

template <typename T>
const T* RawMemory<T>::GetAddress() const noexcept {
    return buffer_;
}

template <typename T>
T* RawMemory<T>::GetAddress() noexcept {
    return buffer_;
}

template <typename T>
size_t RawMemory<T>::Capacity() const noexcept {
    return capacity_;
}

template <typename T>
T* RawMemory<T>::Allocate(size_t n) {
    return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
}

template <typename T>
void RawMemory<T>::Deallocate(T* buf) noexcept {
    operator delete(buf);
}
