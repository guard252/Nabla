// Copyright (C) 2018 Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW Engine"
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_POOL_ADDRESS_ALLOCATOR_H_INCLUDED__
#define __IRR_POOL_ADDRESS_ALLOCATOR_H_INCLUDED__

#include "IrrCompileConfig.h"

#include "irr/core/math/irrMath.h"

#include "irr/core/alloc/AddressAllocatorBase.h"

namespace irr
{
namespace core
{


//! Can only allocate up to a size of a single block, no support for allocations larger than blocksize
template<typename _size_type>
class PoolAddressAllocator : public AddressAllocatorBase<PoolAddressAllocator<_size_type>,_size_type>
{
    private:
        typedef AddressAllocatorBase<PoolAddressAllocator<_size_type>,_size_type> Base;
    public:
        _IRR_DECLARE_ADDRESS_ALLOCATOR_TYPEDEFS(_size_type);

        /// C++17 inline static constexpr bool supportsNullBuffer = true;

        #define DUMMY_DEFAULT_CONSTRUCTOR PoolAddressAllocator() : blockSize(1u), blockCount(0u) {}
        GCC_CONSTRUCTOR_INHERITANCE_BUG_WORKAROUND(DUMMY_DEFAULT_CONSTRUCTOR)
        #undef DUMMY_DEFAULT_CONSTRUCTOR

        virtual ~PoolAddressAllocator() {}

        PoolAddressAllocator(void* reservedSpc, void* buffer, size_type maxAllocatableAlignment, size_type bufSz, size_type blockSz) noexcept :
                    Base(reservedSpc,buffer,std::min(size_type(1u)<<size_type(findLSB(blockSz)),maxAllocatableAlignment)), blockCount((bufSz-Base::alignOffset)/blockSz), blockSize(blockSz), freeStackCtr(0u)
        {
            reset();
        }

        PoolAddressAllocator(const PoolAddressAllocator& other, void* newReservedSpc, void* newBuffer, size_type newBuffSz) noexcept :
                    Base(other,newReservedSpc,newBuffer,newBuffSz), blockCount((newBuffSz-Base::alignOffset)/other.blockSize), blockSize(other.blockSize), freeStackCtr(0u)
        {
            for (size_type i=0; i<other.freeStackCtr; i++)
            {
                size_type freeEntry = reinterpret_cast<size_type*>(other.reservedSpace)[i];

                auto freeStack = reinterpret_cast<size_type*>(Base::reservedSpace);
                if (freeEntry<blockCount)
                    freeStack[freeStackCtr++] = freeEntry;
            }

            if (other.bufferStart&&Base::bufferStart)
            {
                memmove(reinterpret_cast<uint8_t*>(Base::bufferStart)+Base::alignOffset,
                        reinterpret_cast<uint8_t*>(other.bufferStart)+other.alignOffset,
                        std::min(blockCount,other.blockCount)*blockSize);
            }
        }

        PoolAddressAllocator& operator=(PoolAddressAllocator&& other)
        {
            Base::operator=(std::move(other));
            blockCount = other.blockCount;
            blockSize = other.blockSize;
            freeStackCtr = other.freeStackCtr;
            return *this;
        }


        inline size_type        alloc_addr( size_type bytes, size_type alignment, size_type hint=0ull) noexcept
        {
            if (freeStackCtr==0u || alignment>Base::maxRequestableAlignment || bytes==0u)
                return invalid_address;

            auto freeStack = reinterpret_cast<size_type*>(Base::reservedSpace);
            return freeStack[--freeStackCtr];
        }

        inline void             free_addr(size_type addr, size_type bytes) noexcept
        {
#ifdef _DEBUG
            assert(addr<Base::alignOffset && (addr-Base::alignOffset)%blockSize==0 && freeStackCtr<blockCount);
#endif // _DEBUG
            auto freeStack = reinterpret_cast<size_type*>(Base::reservedSpace);
            freeStack[freeStackCtr++] = addr;
        }

        inline void             reset()
        {
            auto freeStack = reinterpret_cast<size_type*>(Base::reservedSpace);
            for (freeStackCtr=0u; freeStackCtr<blockCount; freeStackCtr++)
                freeStack[freeStackCtr] = (blockCount-1u-freeStackCtr)*blockSize+Base::alignOffset;
        }

        //! conservative estimate, does not account for space lost to alignment
        inline size_type        max_size() const noexcept
        {
            return blockSize;
        }

        inline size_type        safe_shrink_size(size_type byteBound=0u, size_type newBuffAlignmentWeCanGuarantee=1u) const noexcept
        {
            size_type retval = (blockCount+1u)*blockSize;
            if (freeStackCtr==0u)
                return retval;

            auto allocSize = (blockCount-freeStackCtr)*blockSize;
            if (allocSize>byteBound)
                byteBound = allocSize;

            auto freeStack = reinterpret_cast<size_type*>(Base::reservedSpace);
            size_type* tmpStackCopy = reinterpret_cast<size_type*>(_IRR_ALIGNED_MALLOC(freeStackCtr*sizeof(size_type),_IRR_SIMD_ALIGNMENT));

            size_type boundedCount = 0;
            for (size_type i=0; i<freeStackCtr; i++)
            {
                if (freeStack[i]<byteBound+Base::alignOffset)
                    continue;

                tmpStackCopy[boundedCount++] = freeStack[i];
            }

            std::make_heap(tmpStackCopy,tmpStackCopy+boundedCount);
            std::sort_heap(tmpStackCopy,tmpStackCopy+boundedCount);
            // could do sophisticated modified binary search, but f'it
            size_type i=1u;
            for (size_type firstWrong = boundedCount-1; i>=boundedCount; firstWrong--,i++)
            {
                if (tmpStackCopy[firstWrong]!=blockCount-i)
                    break;
            }

            retval -= (i-1u)*blockSize;

            _IRR_ALIGNED_FREE(tmpStackCopy);
            return retval;
        }


        static inline size_type reserved_size(size_type bufSz, size_type maxAlignment, size_type blockSz) noexcept
        {
            size_type truncatedOffset = Base::aligned_start_offset(0x7fffFFFFffffFFFFull,maxAlignment);
            size_type probBlockCount =  (bufSz-truncatedOffset)/blockSz;
            return probBlockCount*sizeof(size_type);
        }
        static inline size_type reserved_size(size_type bufSz, const PoolAddressAllocator<_size_type>& other) noexcept
        {
            return reserved_size(bufSz,other.maxRequestableAlignment,other.blockSize);
        }

        inline size_type        get_free_size() const noexcept
        {
            return freeStackCtr*blockSize;
        }
        inline size_type        get_allocated_size() const noexcept
        {
            return (blockCount-freeStackCtr)*blockSize;
        }
        inline size_type        get_total_size() const noexcept
        {
            return blockCount*blockSize;
        }
    protected:
        size_type   blockCount;
        size_type   blockSize;

        size_type   freeStackCtr;

        inline size_type addressToBlockID(size_type addr) const noexcept
        {
            return (addr-Base::alignOffset)/blockSize;
        }
};

//! Ideas for general pool allocator (with support for arrays)
// sorted free vector - binary search insertion on free, linear search on allocate
// sorted free ranges - binary search on free, possible insertion or deletion (less stuff to search)

// how to find the correct free range size?
// sorted per-size vector of ranges (2x lower overhead)


}
}

#include "irr/core/alloc/AddressAllocatorConcurrencyAdaptors.h"

namespace irr
{
namespace core
{

// aliases
template<typename size_type>
using PoolAddressAllocatorST = PoolAddressAllocator<size_type>;

template<typename size_type, class BasicLockable>
using PoolAddressAllocatorMT = AddressAllocatorBasicConcurrencyAdaptor<PoolAddressAllocator<size_type>,BasicLockable>;

}
}

#endif // __IRR_POOL_ADDRESS_ALLOCATOR_H_INCLUDED__

