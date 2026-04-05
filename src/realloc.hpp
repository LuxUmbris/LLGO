/* LLGO memory reallocator */

#pragma once
#include "alloc.hpp"
#include <cstring>

namespace llgo
{
    class Reallocator
    {
    public:
        explicit Reallocator(Arena& arena)
            : m_arena(arena)
        {
        }

        // Reallocate memory inside the arena.
        // If possible, grow in-place. Otherwise allocate new memory and copy.
        void* realloc(void* oldPtr, std::size_t oldSize, std::size_t newSize)
        {
            oldSize = align(oldSize);
            newSize = align(newSize);

            // If no old pointer → pure alloc
            if (oldPtr == nullptr)
            {
                return m_arena.alloc(newSize);
            }

            // If shrinking → no move needed
            if (newSize <= oldSize)
            {
                return oldPtr;
            }

            // Check if oldPtr is at the end of the current arena block
            if (isAtBlockEnd(oldPtr, oldSize))
            {
                // Try to grow in-place
                if (m_arena.remaining() >= (newSize - oldSize))
                {
                    m_arena.bump(newSize - oldSize);
                    return oldPtr;
                }
            }

            // Otherwise: allocate new block + copy
            void* newPtr = m_arena.alloc(newSize);
            std::memcpy(newPtr, oldPtr, oldSize);
            return newPtr;
        }

    private:
        Arena& m_arena;

        static constexpr std::size_t ALIGN = 16;

        static std::size_t align(std::size_t n)
        {
            return (n + (ALIGN - 1)) & ~(ALIGN - 1);
        }

        bool isAtBlockEnd(void* oldPtr, std::size_t oldSize) const
        {
            std::uint8_t* pOld = static_cast<std::uint8_t*>(oldPtr);
            std::uint8_t* pCur = static_cast<std::uint8_t*>(m_arena.currentPtr());
            return (pOld + oldSize == pCur);
        }
    };
} // namespace llgo
