/* LLGO Memory Allocator */

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <new>
#include <utility>

// -----------------------------------------------------------------
// Arch-agnostic arena allocator.
// Alignment is always 16 bytes (works for x86_64, arm64, riscv64,
// riscv32 – no platform-specific assumptions).
// -----------------------------------------------------------------

namespace llgo
{
    class Arena
    {
    public:
        static constexpr std::size_t k_alignment        = 16;
        static constexpr std::size_t k_defaultBlockSize = 65536; // 64 KiB

        Arena()
        {
            reserveBlock(k_defaultBlockSize);
        }

        ~Arena()
        {
            for (Block& b : m_blocks)
            {
                ::operator delete(b.ptr, std::align_val_t{k_alignment});
            }
        }

        Arena(const Arena&)            = delete;
        Arena& operator=(const Arena&) = delete;

        Arena(Arena&& other) noexcept
            : m_blocks(std::move(other.m_blocks))
            , m_current(other.m_current)
            , m_blockSize(other.m_blockSize)
            , m_offset(other.m_offset)
        {
            other.m_current   = nullptr;
            other.m_blockSize = 0;
            other.m_offset    = 0;
        }

        // -------------------------------------------------------
        // Core allocation
        // -------------------------------------------------------
        void* alloc(std::size_t size)
        {
            size = alignUp(size);

            if (m_offset + size > m_blockSize)
            {
                const std::size_t next =
                    size > m_blockSize ? alignUp(size * 2) : m_blockSize * 2;
                reserveBlock(next);
            }

            std::uint8_t* pBase = static_cast<std::uint8_t*>(m_current);
            void* pMem          = pBase + m_offset;
            m_offset           += size;
            return pMem;
        }

        template<typename T, typename... Args>
        T* make(Args&&... args)
        {
            void* pMem = alloc(sizeof(T));
            return new (pMem) T(std::forward<Args>(args)...);
        }

        // -------------------------------------------------------
        // Needed by Reallocator
        // -------------------------------------------------------
        std::size_t remaining() const
        {
            return m_blockSize - m_offset;
        }

        void bump(std::size_t n)
        {
            m_offset += alignUp(n);
        }

        void* currentPtr() const
        {
            return static_cast<std::uint8_t*>(m_current) + m_offset;
        }

        // -------------------------------------------------------
        // Statistics
        // -------------------------------------------------------
        std::size_t totalAllocated() const
        {
            std::size_t total = 0;
            for (const Block& b : m_blocks)
                total += b.capacity;
            return total;
        }

        std::size_t numBlocks() const { return m_blocks.size(); }

    private:
        struct Block
        {
            void*       ptr      = nullptr;
            std::size_t capacity = 0;
        };

        std::vector<Block> m_blocks;
        void*              m_current   = nullptr;
        std::size_t        m_blockSize = 0;
        std::size_t        m_offset    = 0;

        void reserveBlock(std::size_t size)
        {
            size       = alignUp(size);
            void* p    = ::operator new(size, std::align_val_t{k_alignment});
            Block b;
            b.ptr      = p;
            b.capacity = size;
            m_blocks.push_back(b);
            m_current  = p;
            m_blockSize= size;
            m_offset   = 0;
        }

        static std::size_t alignUp(std::size_t n)
        {
            return (n + (k_alignment - 1)) & ~(k_alignment - 1);
        }
    };

} // namespace llgo
