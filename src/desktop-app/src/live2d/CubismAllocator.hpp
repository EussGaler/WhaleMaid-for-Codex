#pragma once

#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>

class CubismAllocator final : public Csm::ICubismAllocator
{
public:
    void* Allocate(Csm::csmSizeType size) override;
    void Deallocate(void* memory) override;
    void* AllocateAligned(Csm::csmSizeType size, Csm::csmUint32 alignment) override;
    void DeallocateAligned(void* alignedMemory) override;
};
