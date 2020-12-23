// Copyright (C) 2020- Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW" engine.
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_KERNELS_H_INCLUDED__
#define __IRR_KERNELS_H_INCLUDED__


#include "irr/asset/filters/kernels/IImageFilterKernel.h"
#include "irr/asset/filters/kernels/CommonImageFilterKernels.h"
#include "irr/asset/filters/kernels/CBoxImageFilterKernel.h"
#include "irr/asset/filters/kernels/CTriangleImageFilterKernel.h"
#include "irr/asset/filters/kernels/CGaussianImageFilterKernel.h"
#include "irr/asset/filters/kernels/CKaiserImageFilterKernel.h"
#include "irr/asset/filters/kernels/CMitchellImageFilterKernel.h"
#include "irr/asset/filters/kernels/CScaledImageFilterKernel.h"
#include "irr/asset/filters/kernels/CChannelIndependentImageFilterKernel.h"
#include "irr/asset/filters/kernels/CDerivativeImageFilterKernel.h"
#include "irr/asset/filters/kernels/CConvolutionImageFilterKernel.h"

namespace irr
{
namespace asset
{
	
/*
// caches weights, also should we call it Polyphase?
template<class Kernel>
class CMultiphaseKernel : public CImageFilterKernel<CMultiphaseKernel<Kernel> >, private Kernel
{
	public:
		_IRR_STATIC_INLINE_CONSTEXPR bool is_separable = Kernel;

		CMultiphaseKernel(Kernel&& k) : Kernel(std::move(k)
		{
		}
		
	protected:
		static inline core::vectorSIMDu32 computePhases(const core::vectorSIMDu32& from, const core::vectorSIMDu32& to)
		{
			assert(!(to>from).any()); // Convolution Kernel cannot be used for upscaling!
			return from/core::gcd(to,from);
		}
		static inline uint32_t computePhaseStorage(const core::vectorSIMDu32& from, const core::vectorSIMDu32& to)
		{
			auto phases = computePhases(from,to);
			auto samplesInSupports = ceil();
			if constexpr(is_separable)
			{

			}
		}
};
*/

// to be inline this function relies on any kernel's `create_sample_functor_t` being defined
template<class CRTP, typename value_type>
template<class PreFilter, class PostFilter>
inline void CImageFilterKernel<CRTP,value_type>::evaluateImpl(
	PreFilter& preFilter,
	PostFilter& postFilter,
	value_type* windowSample,
	core::vectorSIMDf& relativePos,
	const core::vectorSIMDi32& globalTexelCoord,
	const UserData* userData
) const
{
	// static cast is because I'm calling a non-static but non-virtual function
	static_cast<const CRTP*>(this)->create_sample_functor_t(preFilter,postFilter)(windowSample,relativePos,globalTexelCoord,userData);
}

// @see CImageFilterKernel::evaluate
template<class CRTP>
template<class PreFilter, class PostFilter>
inline void CFloatingPointSeparableImageFilterKernelBase<CRTP>::sample_functor_t<PreFilter,PostFilter>::operator()(
		value_type* windowSample, core::vectorSIMDf& relativePos, const core::vectorSIMDi32& globalTexelCoord, const IImageFilterKernel::UserData* userData
)
{
	// this is programmable, but usually in the case of a convolution filter it would be loading the values from a temporary and decoded copy of the input image
	preFilter(windowSample, relativePos, globalTexelCoord, userData);

	// by default there's no optimization so operation is O(SupportExtent^3) even though the filter is separable
	// its possible that the original kernel which defines the `weight` function was stretched or modified, so a correction factor is applied
	auto* scale = IImageFilterKernel::ScaleFactorUserData::cast(userData);
	for (int32_t i=0; i<CRTP::MaxChannels; i++)
	{
		windowSample[i] *= _this->weight(relativePos.x,i)*_this->weight(relativePos.y,i)*_this->weight(relativePos.z,i);
		if (scale)
			windowSample[i] *= scale->factor[i];
	}

	// this is programmable, but usually in the case of a convolution filter it would be summing the values
	postFilter(windowSample, relativePos, globalTexelCoord, userData);
}

} // end namespace asset
} // end namespace irr


#endif