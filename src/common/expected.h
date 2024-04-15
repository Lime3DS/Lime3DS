// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This is based on the proposed implementation of std::expected (P0323)
// https://github.com/TartanLlama/expected/blob/master/include/tl/expected.hpp

#pragma once

#include <type_traits>
#include <utility>

namespace Common {

template <typename T, typename E>
class Expected;

template <typename E>
class Unexpected {
public:
    Unexpected() = delete;

    constexpr explicit Unexpected(const E& e) : m_val{e} {}

    constexpr explicit Unexpected(E&& e) : m_val{std::move(e)} {}

    constexpr E& value() & {
        return m_val;
    }

    constexpr const E& value() const& {
        return m_val;
    }

    constexpr E&& value() && {
        return std::move(m_val);
    }

    constexpr const E&& value() const&& {
        return std::move(m_val);
    }

private:
    E m_val;
};

template <typename E>
constexpr auto operator<=>(const Unexpected<E>& lhs, const Unexpected<E>& rhs) {
    return lhs.value() <=> rhs.value();
}

struct unexpect_t {
    constexpr explicit unexpect_t() = default;
};

namespace detail {

struct no_init_t {
    constexpr explicit no_init_t() = default;
};

/**
 * This specialization is for when T is not trivially destructible,
 * so the destructor must be called on destruction of `expected'
 * Additionally, this requires E to be trivially destructible
 */
template <typename T, typename E, bool = std::is_trivially_destructible_v<T>>
    requires std::is_trivially_destructible_v<E>
struct expected_storage_base {
    constexpr expected_storage_base() : m_val{T{}}, m_has_val{true} {}

    constexpr expected_storage_base(no_init_t) : m_has_val{false} {}

    template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args&&...>>* = nullptr>
    constexpr expected_storage_base(std::in_place_t, Args&&... args)
        : m_val{std::forward<Args>(args)...}, m_has_val{true} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr expected_storage_base(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        : m_val{il, std::forward<Args>(args)...}, m_has_val{true} {}

    template <typename... Args, std::enable_if_t<std::is_constructible_v<E, Args&&...>>* = nullptr>
    constexpr explicit expected_storage_base(unexpect_t, Args&&... args)
        : m_unexpect{std::forward<Args>(args)...}, m_has_val{false} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<E, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr explicit expected_storage_base(unexpect_t, std::initializer_list<U> il,
                                             Args&&... args)
        : m_unexpect{il, std::forward<Args>(args)...}, m_has_val{false} {}

    ~expected_storage_base() {
        if (m_has_val) {
            m_val.~T();
        }
    }

    union {
        T m_val;
        Unexpected<E> m_unexpect;
    };

    bool m_has_val;
};

/**
 * This specialization is for when T is trivially destructible,
 * so the destructor of `expected` can be trivial
 * Additionally, this requires E to be trivially destructible
 */
template <typename T, typename E>
    requires std::is_trivially_destructible_v<E>
struct expected_storage_base<T, E, true> {
    constexpr expected_storage_base() : m_val{T{}}, m_has_val{true} {}

    constexpr expected_storage_base(no_init_t) : m_has_val{false} {}

    template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args&&...>>* = nullptr>
    constexpr expected_storage_base(std::in_place_t, Args&&... args)
        : m_val{std::forward<Args>(args)...}, m_has_val{true} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr expected_storage_base(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        : m_val{il, std::forward<Args>(args)...}, m_has_val{true} {}

    template <typename... Args, std::enable_if_t<std::is_constructible_v<E, Args&&...>>* = nullptr>
    constexpr explicit expected_storage_base(unexpect_t, Args&&... args)
        : m_unexpect{std::forward<Args>(args)...}, m_has_val{false} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<E, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr explicit expected_storage_base(unexpect_t, std::initializer_list<U> il,
                                             Args&&... args)
        : m_unexpect{il, std::forward<Args>(args)...}, m_has_val{false} {}

    ~expected_storage_base() = default;

    union {
        T m_val;
        Unexpected<E> m_unexpect;
    };

    bool m_has_val;
};

template <typename T, typename E>
struct expected_operations_base : expected_storage_base<T, E> {
    using expected_storage_base<T, E>::expected_storage_base;

    template <typename... Args>
    void construct(Args&&... args) noexcept {
        new (std::addressof(this->m_val)) T{std::forward<Args>(args)...};
        this->m_has_val = true;
    }

    template <typename Rhs>
    void construct_with(Rhs&& rhs) noexcept {
        new (std::addressof(this->m_val)) T{std::forward<Rhs>(rhs).get()};
        this->m_has_val = true;
    }

    template <typename... Args>
    void construct_error(Args&&... args) noexcept {
        new (std::addressof(this->m_unexpect)) Unexpected<E>{std::forward<Args>(args)...};
        this->m_has_val = false;
    }

    void assign(const expected_operations_base& rhs) noexcept {
        if (!this->m_has_val && rhs.m_has_val) {
            geterr().~Unexpected<E>();
            construct(rhs.get());
        } else {
            assign_common(rhs);
        }
    }

    void assign(expected_operations_base&& rhs) noexcept {
        if (!this->m_has_val && rhs.m_has_val) {
            geterr().~Unexpected<E>();
            construct(std::move(rhs).get());
        } else {
            assign_common(rhs);
        }
    }

    template <typename Rhs>
    void assign_common(Rhs&& rhs) {
        if (this->m_has_val) {
            if (rhs.m_has_val) {
                get() = std::forward<Rhs>(rhs).get();
            } else {
                destroy_val();
                construct_error(std::forward<Rhs>(rhs).geterr());
            }
        } else {
            if (!rhs.m_has_val) {
                geterr() = std::forward<Rhs>(rhs).geterr();
            }
        }
    }

    bool has_value() const {
        return this->m_has_val;
    }

    constexpr T& get() & {
        return this->m_val;
    }

    constexpr const T& get() const& {
        return this->m_val;
    }

    constexpr T&& get() && {
        return std::move(this->m_val);
    }

    constexpr const T&& get() const&& {
        return std::move(this->m_val);
    }

    constexpr Unexpected<E>& geterr() & {
        return this->m_unexpect;
    }

    constexpr const Unexpected<E>& geterr() const& {
        return this->m_unexpect;
    }

    constexpr Unexpected<E>&& geterr() && {
        return std::move(this->m_unexpect);
    }

    constexpr const Unexpected<E>&& geterr() const&& {
        return std::move(this->m_unexpect);
    }

    constexpr void destroy_val() {
        get().~T();
    }
};

/**
 * This manages conditionally having a trivial copy constructor
 * This specialization is for when T is trivially copy constructible
 * Additionally, this requires E to be trivially copy constructible
 */
template <typename T, typename E, bool = std::is_trivially_copy_constructible_v<T>>
    requires std::is_trivially_copy_constructible_v<E>
struct expected_copy_base : expected_operations_base<T, E> {
    using expected_operations_base<T, E>::expected_operations_base;
};

/**
 * This specialization is for when T is not trivially copy constructible
 * Additionally, this requires E to be trivially copy constructible
 */
template <typename T, typename E>
    requires std::is_trivially_copy_constructible_v<E>
struct expected_copy_base<T, E, false> : expected_operations_base<T, E> {
    using expected_operations_base<T, E>::expected_operations_base;

    expected_copy_base() = default;

    expected_copy_base(const expected_copy_base& rhs)
        : expected_operations_base<T, E>{no_init_t{}} {
        if (rhs.has_value()) {
            this->construct_with(rhs);
        } else {
            this->construct_error(rhs.geterr());
        }
    }

    expected_copy_base(expected_copy_base&&) = default;

    expected_copy_base& operator=(const expected_copy_base&) = default;

    expected_copy_base& operator=(expected_copy_base&&) = default;
};

/**
 * This manages conditionally having a trivial move constructor
 * This specialization is for when T is trivially move constructible
 * Additionally, this requires E to be trivially move constructible
 */
template <typename T, typename E, bool = std::is_trivially_move_constructible_v<T>>
    requires std::is_trivially_move_constructible_v<E>
struct expected_move_base : expected_copy_base<T, E> {
    using expected_copy_base<T, E>::expected_copy_base;
};

/**
 * This specialization is for when T is not trivially move constructible
 * Additionally, this requires E to be trivially move constructible
 */
template <typename T, typename E>
    requires std::is_trivially_move_constructible_v<E>
struct expected_move_base<T, E, false> : expected_copy_base<T, E> {
    using expected_copy_base<T, E>::expected_copy_base;

    expected_move_base() = default;

    expected_move_base(const expected_move_base&) = default;

    expected_move_base(expected_move_base&& rhs) noexcept(std::is_nothrow_move_constructible_v<T>)
        : expected_copy_base<T, E>{no_init_t{}} {
        if (rhs.has_value()) {
            this->construct_with(std::move(rhs));
        } else {
            this->construct_error(std::move(rhs.geterr()));
        }
    }

    expected_move_base& operator=(const expected_move_base&) = default;

    expected_move_base& operator=(expected_move_base&&) = default;
};

/**
 * This manages conditionally having a trivial copy assignment operator
 * This specialization is for when T is trivially copy assignable
 * Additionally, this requires E to be trivially copy assignable
 */
template <typename T, typename E,
          bool = std::conjunction_v<std::is_trivially_copy_assignable<T>,
                                    std::is_trivially_copy_constructible<T>,
                                    std::is_trivially_destructible<T>>>
    requires std::conjunction_v<std::is_trivially_copy_assignable<E>,
                                std::is_trivially_copy_constructible<E>,
                                std::is_trivially_destructible<E>>
struct expected_copy_assign_base : expected_move_base<T, E> {
    using expected_move_base<T, E>::expected_move_base;
};

/**
 * This specialization is for when T is not trivially copy assignable
 * Additionally, this requires E to be trivially copy assignable
 */
template <typename T, typename E>
    requires std::conjunction_v<std::is_trivially_copy_assignable<E>,
                                std::is_trivially_copy_constructible<E>,
                                std::is_trivially_destructible<E>>
struct expected_copy_assign_base<T, E, false> : expected_move_base<T, E> {
    using expected_move_base<T, E>::expected_move_base;

    expected_copy_assign_base() = default;

    expected_copy_assign_base(const expected_copy_assign_base&) = default;

    expected_copy_assign_base(expected_copy_assign_base&&) = default;

    expected_copy_assign_base& operator=(const expected_copy_assign_base& rhs) {
        this->assign(rhs);
        return *this;
    }

    expected_copy_assign_base& operator=(expected_copy_assign_base&&) = default;
};

/**
 * This manages conditionally having a trivial move assignment operator
 * This specialization is for when T is trivially move assignable
 * Additionally, this requires E to be trivially move assignable
 */
template <typename T, typename E,
          bool = std::conjunction_v<std::is_trivially_move_assignable<T>,
                                    std::is_trivially_move_constructible<T>,
                                    std::is_trivially_destructible<T>>>
    requires std::conjunction_v<std::is_trivially_move_assignable<E>,
                                std::is_trivially_move_constructible<E>,
                                std::is_trivially_destructible<E>>
struct expected_move_assign_base : expected_copy_assign_base<T, E> {
    using expected_copy_assign_base<T, E>::expected_copy_assign_base;
};

/**
 * This specialization is for when T is not trivially move assignable
 * Additionally, this requires E to be trivially move assignable
 */
template <typename T, typename E>
    requires std::conjunction_v<std::is_trivially_move_assignable<E>,
                                std::is_trivially_move_constructible<E>,
                                std::is_trivially_destructible<E>>
struct expected_move_assign_base<T, E, false> : expected_copy_assign_base<T, E> {
    using expected_copy_assign_base<T, E>::expected_copy_assign_base;

    expected_move_assign_base() = default;

    expected_move_assign_base(const expected_move_assign_base&) = default;

    expected_move_assign_base(expected_move_assign_base&&) = default;

    expected_move_assign_base& operator=(const expected_move_assign_base&) = default;

    expected_move_assign_base& operator=(expected_move_assign_base&& rhs) noexcept(
        std::conjunction_v<std::is_nothrow_move_constructible<T>,
                           std::is_nothrow_move_assignable<T>>) {
        this->assign(std::move(rhs));
        return *this;
    }
};

/**
 * expected_delete_ctor_base will conditionally delete copy and move constructors
 * depending on whether T is copy/move constructible
 * Additionally, this requires E to be copy/move constructible
 */
template <typename T, typename E, bool EnableCopy = std::is_copy_constructible_v<T>,
          bool EnableMove = std::is_move_constructible_v<T>>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>>
struct expected_delete_ctor_base {
    expected_delete_ctor_base() = default;
    expected_delete_ctor_base(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base(expected_delete_ctor_base&&) noexcept = default;
    expected_delete_ctor_base& operator=(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base& operator=(expected_delete_ctor_base&&) noexcept = default;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>>
struct expected_delete_ctor_base<T, E, true, false> {
    expected_delete_ctor_base() = default;
    expected_delete_ctor_base(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base(expected_delete_ctor_base&&) noexcept = delete;
    expected_delete_ctor_base& operator=(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base& operator=(expected_delete_ctor_base&&) noexcept = default;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>>
struct expected_delete_ctor_base<T, E, false, true> {
    expected_delete_ctor_base() = default;
    expected_delete_ctor_base(const expected_delete_ctor_base&) = delete;
    expected_delete_ctor_base(expected_delete_ctor_base&&) noexcept = default;
    expected_delete_ctor_base& operator=(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base& operator=(expected_delete_ctor_base&&) noexcept = default;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>>
struct expected_delete_ctor_base<T, E, false, false> {
    expected_delete_ctor_base() = default;
    expected_delete_ctor_base(const expected_delete_ctor_base&) = delete;
    expected_delete_ctor_base(expected_delete_ctor_base&&) noexcept = delete;
    expected_delete_ctor_base& operator=(const expected_delete_ctor_base&) = default;
    expected_delete_ctor_base& operator=(expected_delete_ctor_base&&) noexcept = default;
};

/**
 * expected_delete_assign_base will conditionally delete copy and move assignment operators
 * depending on whether T is copy/move constructible + assignable
 * Additionally, this requires E to be copy/move constructible + assignable
 */
template <
    typename T, typename E,
    bool EnableCopy = std::conjunction_v<std::is_copy_constructible<T>, std::is_copy_assignable<T>>,
    bool EnableMove = std::conjunction_v<std::is_move_constructible<T>, std::is_move_assignable<T>>>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>,
                                std::is_copy_assignable<E>, std::is_move_assignable<E>>
struct expected_delete_assign_base {
    expected_delete_assign_base() = default;
    expected_delete_assign_base(const expected_delete_assign_base&) = default;
    expected_delete_assign_base(expected_delete_assign_base&&) noexcept = default;
    expected_delete_assign_base& operator=(const expected_delete_assign_base&) = default;
    expected_delete_assign_base& operator=(expected_delete_assign_base&&) noexcept = default;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>,
                                std::is_copy_assignable<E>, std::is_move_assignable<E>>
struct expected_delete_assign_base<T, E, true, false> {
    expected_delete_assign_base() = default;
    expected_delete_assign_base(const expected_delete_assign_base&) = default;
    expected_delete_assign_base(expected_delete_assign_base&&) noexcept = default;
    expected_delete_assign_base& operator=(const expected_delete_assign_base&) = default;
    expected_delete_assign_base& operator=(expected_delete_assign_base&&) noexcept = delete;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>,
                                std::is_copy_assignable<E>, std::is_move_assignable<E>>
struct expected_delete_assign_base<T, E, false, true> {
    expected_delete_assign_base() = default;
    expected_delete_assign_base(const expected_delete_assign_base&) = default;
    expected_delete_assign_base(expected_delete_assign_base&&) noexcept = default;
    expected_delete_assign_base& operator=(const expected_delete_assign_base&) = delete;
    expected_delete_assign_base& operator=(expected_delete_assign_base&&) noexcept = default;
};

template <typename T, typename E>
    requires std::conjunction_v<std::is_copy_constructible<E>, std::is_move_constructible<E>,
                                std::is_copy_assignable<E>, std::is_move_assignable<E>>
struct expected_delete_assign_base<T, E, false, false> {
    expected_delete_assign_base() = default;
    expected_delete_assign_base(const expected_delete_assign_base&) = default;
    expected_delete_assign_base(expected_delete_assign_base&&) noexcept = default;
    expected_delete_assign_base& operator=(const expected_delete_assign_base&) = delete;
    expected_delete_assign_base& operator=(expected_delete_assign_base&&) noexcept = delete;
};

/**
 * This is needed to be able to construct the expected_default_ctor_base which follows,
 * while still conditionally deleting the default constructor.
 */
struct default_constructor_tag {
    constexpr explicit default_constructor_tag() = default;
};

/**
 * expected_default_ctor_base will ensure that expected
 * has a deleted default constructor if T is not default constructible
 * This specialization is for when T is default constructible
 */
template <typename T, typename E, bool Enable = std::is_default_constructible_v<T>>
struct expected_default_ctor_base {
    constexpr expected_default_ctor_base() noexcept = default;
    constexpr expected_default_ctor_base(expected_default_ctor_base const&) noexcept = default;
    constexpr expected_default_ctor_base(expected_default_ctor_base&&) noexcept = default;
    expected_default_ctor_base& operator=(expected_default_ctor_base const&) noexcept = default;
    expected_default_ctor_base& operator=(expected_default_ctor_base&&) noexcept = default;

    constexpr explicit expected_default_ctor_base(default_constructor_tag) {}
};

template <typename T, typename E>
struct expected_default_ctor_base<T, E, false> {
    constexpr expected_default_ctor_base() noexcept = delete;
    constexpr expected_default_ctor_base(expected_default_ctor_base const&) noexcept = default;
    constexpr expected_default_ctor_base(expected_default_ctor_base&&) noexcept = default;
    expected_default_ctor_base& operator=(expected_default_ctor_base const&) noexcept = default;
    expected_default_ctor_base& operator=(expected_default_ctor_base&&) noexcept = default;

    constexpr explicit expected_default_ctor_base(default_constructor_tag) {}
};

template <typename T, typename E, typename U>
using expected_enable_forward_value =
    std::enable_if_t<std::is_constructible_v<T, U&&> &&
                     !std::is_same_v<std::remove_cvref_t<U>, std::in_place_t> &&
                     !std::is_same_v<Expected<T, E>, std::remove_cvref_t<U>> &&
                     !std::is_same_v<Unexpected<E>, std::remove_cvref_t<U>>>;

template <typename T, typename E, typename U, typename G, typename UR, typename GR>
using expected_enable_from_other = std::enable_if_t<
    std::is_constructible_v<T, UR> && std::is_constructible_v<E, GR> &&
    !std::is_constructible_v<T, Expected<U, G>&> && !std::is_constructible_v<T, Expected<U, G>&&> &&
    !std::is_constructible_v<T, const Expected<U, G>&> &&
    !std::is_constructible_v<T, const Expected<U, G>&&> &&
    !std::is_convertible_v<Expected<U, G>&, T> && !std::is_convertible_v<Expected<U, G>&&, T> &&
    !std::is_convertible_v<const Expected<U, G>&, T> &&
    !std::is_convertible_v<const Expected<U, G>&&, T>>;

} // namespace detail

template <typename T, typename E>
class Expected : private detail::expected_move_assign_base<T, E>,
                 private detail::expected_delete_ctor_base<T, E>,
                 private detail::expected_delete_assign_base<T, E>,
                 private detail::expected_default_ctor_base<T, E> {
public:
    using value_type = T;
    using error_type = E;
    using unexpected_type = Unexpected<E>;

    constexpr Expected() = default;
    constexpr Expected(const Expected&) = default;
    constexpr Expected(Expected&&) = default;
    Expected& operator=(const Expected&) = default;
    Expected& operator=(Expected&&) = default;

    template <typename... Args, std::enable_if_t<std::is_constructible_v<T, Args&&...>>* = nullptr>
    constexpr Expected(std::in_place_t, Args&&... args)
        : impl_base{std::in_place, std::forward<Args>(args)...},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr Expected(std::in_place_t, std::initializer_list<U> il, Args&&... args)
        : impl_base{std::in_place, il, std::forward<Args>(args)...},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
              std::enable_if_t<!std::is_convertible_v<const G&, E>>* = nullptr>
    constexpr explicit Expected(const Unexpected<G>& e)
        : impl_base{unexpect_t{}, e.value()}, ctor_base{detail::default_constructor_tag{}} {}

    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, const G&>>* = nullptr,
              std::enable_if_t<std::is_convertible_v<const G&, E>>* = nullptr>
    constexpr Expected(Unexpected<G> const& e)
        : impl_base{unexpect_t{}, e.value()}, ctor_base{detail::default_constructor_tag{}} {}

    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, G&&>>* = nullptr,
              std::enable_if_t<!std::is_convertible_v<G&&, E>>* = nullptr>
    constexpr explicit Expected(Unexpected<G>&& e) noexcept(std::is_nothrow_constructible_v<E, G&&>)
        : impl_base{unexpect_t{}, std::move(e.value())},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, G&&>>* = nullptr,
              std::enable_if_t<std::is_convertible_v<G&&, E>>* = nullptr>
    constexpr Expected(Unexpected<G>&& e) noexcept(std::is_nothrow_constructible_v<E, G&&>)
        : impl_base{unexpect_t{}, std::move(e.value())},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename... Args, std::enable_if_t<std::is_constructible_v<E, Args&&...>>* = nullptr>
    constexpr explicit Expected(unexpect_t, Args&&... args)
        : impl_base{unexpect_t{}, std::forward<Args>(args)...},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename U, typename... Args,
              std::enable_if_t<std::is_constructible_v<E, std::initializer_list<U>&, Args&&...>>* =
                  nullptr>
    constexpr explicit Expected(unexpect_t, std::initializer_list<U> il, Args&&... args)
        : impl_base{unexpect_t{}, il, std::forward<Args>(args)...},
          ctor_base{detail::default_constructor_tag{}} {}

    template <typename U, typename G,
              std::enable_if_t<!(std::is_convertible_v<U const&, T> &&
                                 std::is_convertible_v<G const&, E>)>* = nullptr,
              detail::expected_enable_from_other<T, E, U, G, const U&, const G&>* = nullptr>
    constexpr explicit Expected(const Expected<U, G>& rhs)
        : ctor_base{detail::default_constructor_tag{}} {
        if (rhs.has_value()) {
            this->construct(*rhs);
        } else {
            this->construct_error(rhs.error());
        }
    }

    template <typename U, typename G,
              std::enable_if_t<(std::is_convertible_v<U const&, T> &&
                                std::is_convertible_v<G const&, E>)>* = nullptr,
              detail::expected_enable_from_other<T, E, U, G, const U&, const G&>* = nullptr>
    constexpr Expected(const Expected<U, G>& rhs) : ctor_base{detail::default_constructor_tag{}} {
        if (rhs.has_value()) {
            this->construct(*rhs);
        } else {
            this->construct_error(rhs.error());
        }
    }

    template <typename U, typename G,
              std::enable_if_t<!(std::is_convertible_v<U&&, T> && std::is_convertible_v<G&&, E>)>* =
                  nullptr,
              detail::expected_enable_from_other<T, E, U, G, U&&, G&&>* = nullptr>
    constexpr explicit Expected(Expected<U, G>&& rhs)
        : ctor_base{detail::default_constructor_tag{}} {
        if (rhs.has_value()) {
            this->construct(std::move(*rhs));
        } else {
            this->construct_error(std::move(rhs.error()));
        }
    }

    template <typename U, typename G,
              std::enable_if_t<(std::is_convertible_v<U&&, T> && std::is_convertible_v<G&&, E>)>* =
                  nullptr,
              detail::expected_enable_from_other<T, E, U, G, U&&, G&&>* = nullptr>
    constexpr Expected(Expected<U, G>&& rhs) : ctor_base{detail::default_constructor_tag{}} {
        if (rhs.has_value()) {
            this->construct(std::move(*rhs));
        } else {
            this->construct_error(std::move(rhs.error()));
        }
    }

    template <typename U = T, std::enable_if_t<!std::is_convertible_v<U&&, T>>* = nullptr,
              detail::expected_enable_forward_value<T, E, U>* = nullptr>
    constexpr explicit Expected(U&& v) : Expected{std::in_place, std::forward<U>(v)} {}

    template <typename U = T, std::enable_if_t<std::is_convertible_v<U&&, T>>* = nullptr,
              detail::expected_enable_forward_value<T, E, U>* = nullptr>
    constexpr Expected(U&& v) : Expected{std::in_place, std::forward<U>(v)} {}

    template <typename U = T, typename G = T,
              std::enable_if_t<std::is_nothrow_constructible_v<T, U&&>>* = nullptr,
              std::enable_if_t<(
                  !std::is_same_v<Expected<T, E>, std::remove_cvref_t<U>> &&
                  !std::conjunction_v<std::is_scalar<T>, std::is_same<T, std::remove_cvref_t<U>>> &&
                  std::is_constructible_v<T, U> && std::is_assignable_v<G&, U> &&
                  std::is_nothrow_move_constructible_v<E>)>* = nullptr>
    Expected& operator=(U&& v) {
        if (has_value()) {
            val() = std::forward<U>(v);
        } else {
            err().~Unexpected<E>();
            new (valptr()) T{std::forward<U>(v)};
            this->m_has_val = true;
        }

        return *this;
    }

    template <typename U = T, typename G = T,
              std::enable_if_t<!std::is_nothrow_constructible_v<T, U&&>>* = nullptr,
              std::enable_if_t<(
                  !std::is_same_v<Expected<T, E>, std::remove_cvref_t<U>> &&
                  !std::conjunction_v<std::is_scalar<T>, std::is_same<T, std::remove_cvref_t<U>>> &&
                  std::is_constructible_v<T, U> && std::is_assignable_v<G&, U> &&
                  std::is_nothrow_move_constructible_v<E>)>* = nullptr>
    Expected& operator=(U&& v) {
        if (has_value()) {
            val() = std::forward<U>(v);
        } else {
            auto tmp = std::move(err());
            err().~Unexpected<E>();
            new (valptr()) T{std::forward<U>(v)};
            this->m_has_val = true;
        }

        return *this;
    }

    template <typename G = E, std::enable_if_t<std::is_nothrow_copy_constructible_v<G> &&
                                               std::is_assignable_v<G&, G>>* = nullptr>
    Expected& operator=(const Unexpected<G>& rhs) {
        if (!has_value()) {
            err() = rhs;
        } else {
            this->destroy_val();
            new (errptr()) Unexpected<E>{rhs};
            this->m_has_val = false;
        }

        return *this;
    }

    template <typename G = E, std::enable_if_t<std::is_nothrow_move_constructible_v<G> &&
                                               std::is_move_assignable_v<G>>* = nullptr>
    Expected& operator=(Unexpected<G>&& rhs) noexcept {
        if (!has_value()) {
            err() = std::move(rhs);
        } else {
            this->destroy_val();
            new (errptr()) Unexpected<E>{std::move(rhs)};
            this->m_has_val = false;
        }

        return *this;
    }

    template <typename... Args,
              std::enable_if_t<std::is_nothrow_constructible_v<T, Args&&...>>* = nullptr>
    void emplace(Args&&... args) {
        if (has_value()) {
            val() = T{std::forward<Args>(args)...};
        } else {
            err().~Unexpected<E>();
            new (valptr()) T{std::forward<Args>(args)...};
            this->m_has_val = true;
        }
    }

    template <typename... Args,
              std::enable_if_t<!std::is_nothrow_constructible_v<T, Args&&...>>* = nullptr>
    void emplace(Args&&... args) {
        if (has_value()) {
            val() = T{std::forward<Args>(args)...};
        } else {
            auto tmp = std::move(err());
            err().~Unexpected<E>();
            new (valptr()) T{std::forward<Args>(args)...};
            this->m_has_val = true;
        }
    }

    template <typename U, typename... Args,
              std::enable_if_t<std::is_nothrow_constructible_v<T, std::initializer_list<U>&,
                                                               Args&&...>>* = nullptr>
    void emplace(std::initializer_list<U> il, Args&&... args) {
        if (has_value()) {
            T t{il, std::forward<Args>(args)...};
            val() = std::move(t);
        } else {
            err().~Unexpected<E>();
            new (valptr()) T{il, std::forward<Args>(args)...};
            this->m_has_val = true;
        }
    }

    template <typename U, typename... Args,
              std::enable_if_t<!std::is_nothrow_constructible_v<T, std::initializer_list<U>&,
                                                                Args&&...>>* = nullptr>
    void emplace(std::initializer_list<U> il, Args&&... args) {
        if (has_value()) {
            T t{il, std::forward<Args>(args)...};
            val() = std::move(t);
        } else {
            auto tmp = std::move(err());
            err().~Unexpected<E>();
            new (valptr()) T{il, std::forward<Args>(args)...};
            this->m_has_val = true;
        }
    }

    constexpr T* operator->() {
        return valptr();
    }

    constexpr const T* operator->() const {
        return valptr();
    }

    template <typename U = T>
    constexpr U& operator*() & {
        return val();
    }

    template <typename U = T>
    constexpr const U& operator*() const& {
        return val();
    }

    template <typename U = T>
    constexpr U&& operator*() && {
        return std::move(val());
    }

    template <typename U = T>
    constexpr const U&& operator*() const&& {
        return std::move(val());
    }

    constexpr bool has_value() const noexcept {
        return this->m_has_val;
    }

    constexpr explicit operator bool() const noexcept {
        return this->m_has_val;
    }

    template <typename U = T>
    constexpr U& value() & {
        return val();
    }

    template <typename U = T>
    constexpr const U& value() const& {
        return val();
    }

    template <typename U = T>
    constexpr U&& value() && {
        return std::move(val());
    }

    template <typename U = T>
    constexpr const U&& value() const&& {
        return std::move(val());
    }

    constexpr E& error() & {
        return err().value();
    }

    constexpr const E& error() const& {
        return err().value();
    }

    constexpr E&& error() && {
        return std::move(err().value());
    }

    constexpr const E&& error() const&& {
        return std::move(err().value());
    }

    template <typename U>
    constexpr T value_or(U&& v) const& {
        static_assert(std::is_copy_constructible_v<T> && std::is_convertible_v<U&&, T>,
                      "T must be copy-constructible and convertible from U&&");
        return bool(*this) ? **this : static_cast<T>(std::forward<U>(v));
    }

    template <typename U>
    constexpr T value_or(U&& v) && {
        static_assert(std::is_move_constructible_v<T> && std::is_convertible_v<U&&, T>,
                      "T must be move-constructible and convertible from U&&");
        return bool(*this) ? std::move(**this) : static_cast<T>(std::forward<U>(v));
    }

private:
    static_assert(!std::is_reference_v<T>, "T must not be a reference");
    static_assert(!std::is_same_v<T, std::remove_cv_t<std::in_place_t>>,
                  "T must not be std::in_place_t");
    static_assert(!std::is_same_v<T, std::remove_cv_t<unexpect_t>>, "T must not be unexpect_t");
    static_assert(!std::is_same_v<T, std::remove_cv_t<Unexpected<E>>>,
                  "T must not be Unexpected<E>");
    static_assert(!std::is_reference_v<E>, "E must not be a reference");

    T* valptr() {
        return std::addressof(this->m_val);
    }

    const T* valptr() const {
        return std::addressof(this->m_val);
    }

    Unexpected<E>* errptr() {
        return std::addressof(this->m_unexpect);
    }

    const Unexpected<E>* errptr() const {
        return std::addressof(this->m_unexpect);
    }

    template <typename U = T>
    constexpr U& val() {
        return this->m_val;
    }

    template <typename U = T>
    constexpr const U& val() const {
        return this->m_val;
    }

    constexpr Unexpected<E>& err() {
        return this->m_unexpect;
    }

    constexpr const Unexpected<E>& err() const {
        return this->m_unexpect;
    }

    using impl_base = detail::expected_move_assign_base<T, E>;
    using ctor_base = detail::expected_default_ctor_base<T, E>;
};

template <typename T, typename E, typename U, typename F>
constexpr bool operator==(const Expected<T, E>& lhs, const Expected<U, F>& rhs) {
    return (lhs.has_value() != rhs.has_value())
               ? false
               : (!lhs.has_value() ? lhs.error() == rhs.error() : *lhs == *rhs);
}

template <typename T, typename E, typename U, typename F>
constexpr bool operator!=(const Expected<T, E>& lhs, const Expected<U, F>& rhs) {
    return !operator==(lhs, rhs);
}

template <typename T, typename E, typename U>
constexpr bool operator==(const Expected<T, E>& x, const U& v) {
    return x.has_value() ? *x == v : false;
}

template <typename T, typename E, typename U>
constexpr bool operator==(const U& v, const Expected<T, E>& x) {
    return x.has_value() ? *x == v : false;
}

template <typename T, typename E, typename U>
constexpr bool operator!=(const Expected<T, E>& x, const U& v) {
    return !operator==(x, v);
}

template <typename T, typename E, typename U>
constexpr bool operator!=(const U& v, const Expected<T, E>& x) {
    return !operator==(v, x);
}

template <typename T, typename E>
constexpr bool operator==(const Expected<T, E>& x, const Unexpected<E>& e) {
    return x.has_value() ? false : x.error() == e.value();
}

template <typename T, typename E>
constexpr bool operator==(const Unexpected<E>& e, const Expected<T, E>& x) {
    return x.has_value() ? false : x.error() == e.value();
}

template <typename T, typename E>
constexpr bool operator!=(const Expected<T, E>& x, const Unexpected<E>& e) {
    return !operator==(x, e);
}

template <typename T, typename E>
constexpr bool operator!=(const Unexpected<E>& e, const Expected<T, E>& x) {
    return !operator==(e, x);
}

} // namespace Common
