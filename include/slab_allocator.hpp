#pragma once

#include <array>
#include <cstddef>
#include <list>
#include <vector>

namespace ngg::util
{

/**
 * @brief Fixed-size stack-based slab allocator.
 *
 * TODO: stub implementation, works sometimes, make atomic and STL-less
 */
template <typename T, std::size_t SlabSize = 64,
          std::size_t TotalCapacity = 4 * 1024>
class slab_allocator
{
    static_assert(SlabSize <= TotalCapacity && TotalCapacity % SlabSize == 0 &&
                      SlabSize >= sizeof(T),
                  "Math doesnt math");

  public:
    using value_type                          = T;
    using pointer                             = T *;
    static constexpr std::size_t PoolCapacity = TotalCapacity / SlabSize;

    template <typename U> struct rebind
    {
        typedef slab_allocator<U, SlabSize, TotalCapacity> other;
    };

  public:
    slab_allocator () : m_pools(), m_free_list(), m_block_idx(0)
    {
    }

    pointer allocate (std::size_t num) noexcept
    {
        if (num == 1 && !m_free_list.empty())
        {
            pointer free = m_free_list.back();
            m_free_list.pop_back();
            return free;
        }
        if (m_pools.empty())
        {
            m_pools.emplace_back();
            m_pool_idx  = 0;
            m_block_idx = 0;
        }
        if (m_block_idx + num >= PoolCapacity)
        {
            m_pools.emplace_back();
            m_pool_idx += 1; // m_pools.size() - 1;
            m_block_idx = 0;
        }
        pointer last =
            reinterpret_cast<pointer>(m_pools.back()[m_block_idx].data());
        m_block_idx += num;
        return last;
    }

    void deallocate (pointer mem, std::size_t num) noexcept
    {
        if (mem == nullptr || num == 0)
        {
            return;
        }
        for (size_t i = 0; i < num; i++)
        {
            const pointer addr =
                reinterpret_cast<pointer>((std::byte *)mem + i * SlabSize);
            m_free_list.push_back(addr);
        }
    }

  private:
    using block_type = std::array<value_type, SlabSize>;
    using pool_type  = std::array<block_type, PoolCapacity>;

    std::list<pool_type> m_pools;
    std::vector<pointer> m_free_list;
    std::size_t m_pool_idx  = 0;
    std::size_t m_block_idx = 0;
};

} // namespace ngg::util
