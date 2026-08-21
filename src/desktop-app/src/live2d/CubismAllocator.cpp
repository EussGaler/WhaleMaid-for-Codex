#include "live2d/CubismAllocator.hpp"

#include <cstdlib>
#include <cstdint>

void* CubismAllocator::Allocate(const Csm::csmSizeType size)
{
    return std::malloc(size);
}

void CubismAllocator::Deallocate(void* memory)
{
    std::free(memory);
}

void* CubismAllocator::AllocateAligned(
    const Csm::csmSizeType size,
    const Csm::csmUint32 alignment)
{
    const std::size_t offset = alignment - 1U + sizeof(void*);
    void* allocation = Allocate(size + offset);
    if (!allocation)
    {
        return nullptr;
    }

    std::uintptr_t alignedAddress =
        reinterpret_cast<std::uintptr_t>(allocation) + sizeof(void*);
    const std::uintptr_t shift = alignedAddress % alignment;
    if (shift != 0U)
    {
        alignedAddress += alignment - shift;
    }

    auto** preamble = reinterpret_cast<void**>(alignedAddress);
    preamble[-1] = allocation;
    return reinterpret_cast<void*>(alignedAddress);
}

void CubismAllocator::DeallocateAligned(void* alignedMemory)
{
    if (!alignedMemory)
    {
        return;
    }

    auto** preamble = static_cast<void**>(alignedMemory);
    Deallocate(preamble[-1]);
}
