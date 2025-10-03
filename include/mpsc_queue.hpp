#pragma once

#include "types.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <utility>

namespace ngg
{

/**
 * @brief multiple producers/single consumer queue
 *
 * Implement Michael-Scott queue, is intrusive and unbound.
 * Uses dummy sentinel node for initial head and tail.
 *
 * @tparam T type of inner data, must be default-constructible
 * @tparam Allocator allocator type, used for inner nodes
 */
template <types::default_constructible T,
          types::minimal_allocator_type<T> Allocator = std::allocator<T>>
class mpsc_queue
{
    // nikgub: semantics, a must-have
  protected:
    struct node;
    using value_type       = T;
    using pointer          = node *;
    using atomic_node      = std::atomic<node *>;
    using allocator_traits = typename std::allocator_traits<Allocator>;
    using node_allocator   = allocator_traits::template rebind_alloc<node>;
    using node_allocator_traits =
        typename std::allocator_traits<node_allocator>;

  public:
    /**
     * @brief Constructs the queue.
     *
     * Inits the head and tail nodes with a dummy node.
     */
    mpsc_queue () : m_node_alloc()
    {
        pointer dummy = node_allocator_traits::allocate(m_node_alloc, 1);
        node_allocator_traits::construct(m_node_alloc, dummy, nullptr);
        // nikgub: relaxed memory since we do not contest anything yet
        m_head.store(dummy, std::memory_order_relaxed);
        m_tail.store(dummy, std::memory_order_relaxed);
    }

    /**
     * @brief Destroys the queue.
     *
     * Is tested and proven to not leave any leaks
     */
    ~mpsc_queue ()
    {
        clear();
        node_allocator_traits::destroy(m_node_alloc, m_head.load());
        node_allocator_traits::deallocate(m_node_alloc, m_tail.load(), 1);
    }

    /**
     * @brief Copies and pushes a value to the queue.
     *
     * Implemented as forwarding.
     *
     * @param value value being copied
     */
    void push (const T &value)
    {
        push_impl(T(value));
    }

    /**
     * @brief Pushes an rvalue to the queue.
     *
     * Implemented as forwarding.
     *
     * @param value value being forwarded
     */
    void push (T &&value)
    {
        push_impl(value);
    }

    /**
     * @brief Pops the first element from the queue.
     *
     * @returns value if any, nullopt otherwise
     */
    std::optional<T> pull ()
    {
        pointer tail_ptr = m_tail.load(std::memory_order_relaxed);
        // nikgub: acquire the element
        pointer next     = tail_ptr->next.load(std::memory_order_acquire);
        if (next == nullptr) // nikgub: nullopt if none
        {
            return std::nullopt;
        }
        T result = std::move(next->data);
        m_tail.store(next, std::memory_order_release);
        node_allocator_traits::destroy(m_node_alloc, tail_ptr);
        node_allocator_traits::deallocate(m_node_alloc, tail_ptr, 1);
        return result;
    }

    /**
     * @brief Clears all the elements in the queue.
     *
     * TODO: this is somewhat hacky, theoretically there can be a situation when
     * the tail_ptr removes a dummy node but this is so unlikely it is basically
     * a no-op.
     */
    void clear ()
    {
        pointer tail_ptr = m_tail.load(std::memory_order_relaxed);
        while (true)
        {
            pointer next = tail_ptr->next.load(std::memory_order_acquire);
            if (next == nullptr)
            {
                break;
            }
            m_tail.store(next, std::memory_order_release);
            node_allocator_traits::destroy(m_node_alloc, tail_ptr);
            node_allocator_traits::deallocate(m_node_alloc, tail_ptr, 1);
            tail_ptr = next;
        }
    }

  protected:
    /**
     * @brief Inner node struct of mpsc_queue.
     *
     * Owns the inner data.
     */
    struct node
    {
        /**
         * @brief Argument constructor.
         *
         * Constructs inner data from an arg, sets the next pointer.
         */
        explicit node (node *next, T value)
            : data(std::forward<T>(value)), next(next)
        {
        }

        /**
         * @brief No-arg constructor.
         *
         * Default-constructs the inner value, sets the next pointer.
         */
        explicit node (node *next) : data(), next(next)
        {
        }

        node () : data(), next(nullptr)
        {
        }

        T data;
        atomic_node next;
    };

  private:
    // nikgub: align for 64 bytes for x86_64.
    //         do this to prevent false sharing.
    //         if someone uses 32-bit system - shame on them.
    //         TODO: maybe fix this?
    alignas(64) atomic_node m_head; // nikgub: newest node
    alignas(64) atomic_node m_tail; // nikgub: oldest node
    alignas(64) node_allocator
        m_node_alloc; // nikgub: alloc instance to support stateful allocators

  private:
    /**
     * @brief Implementation of push.
     *
     * Uses forwarding to address all value types.
     * @note possible contentions in prev_head segment but does not break until
     * a miracle happens.
     *
     * @param value forwarding reference
     */
    void push_impl (T &&value)
    {
        pointer new_node = node_allocator_traits::allocate(m_node_alloc, 1);
        node_allocator_traits::construct(m_node_alloc, new_node, nullptr,
                                         std::forward<T>(value));
        // nikgub: contested but fine
        // TODO: find a test where it fails
        pointer prev_head =
            m_head.exchange(new_node, std::memory_order_acq_rel);
        prev_head->next.store(new_node, std::memory_order_release);
    }
};

} // namespace ngg
