#ifndef __IRR_C_OPENGL_DESCRIPTOR_SET_H_INCLUDED__
#define __IRR_C_OPENGL_DESCRIPTOR_SET_H_INCLUDED__

#include "irr/video/IGPUDescriptorSet.h"
#include "irr/macros.h"
#include "COpenGLExtensionHandler.h"
#include "COpenGLBuffer.h"
#include "COpenGLBufferView.h"
#include "COpenGLImage.h"
#include "COpenGLImageView.h"
#include "irr/video/COpenGLSampler.h"

#ifdef _IRR_COMPILE_WITH_OPENGL_
namespace irr
{
namespace video
{

class COpenGLDescriptorSet : public IGPUDescriptorSet, protected asset::impl::IEmulatedDescriptorSet<IGPUDescriptorSetLayout>
{
	public:
		struct SMultibindParams
		{
			struct SMultibindBuffers
			{
				GLuint* buffers = nullptr;
				GLintptr* offsets = nullptr;
				GLsizeiptr* sizes = nullptr;
				uint32_t* dynOffsetIxs = nullptr;
			};
			struct SMultibindTextures
			{
				GLuint* textures = nullptr;
				GLuint* samplers = nullptr;
			};
			struct SMultibindTextureImages
			{
				GLuint* textures = nullptr;
			};
        
			SMultibindBuffers ubos;
			SMultibindBuffers ssbos;
			SMultibindTextures textures;
			SMultibindTextureImages textureImages;
		};

	public:
		COpenGLDescriptorSet(core::smart_refctd_ptr<IGPUDescriptorSetLayout>&& _layout) : IGPUDescriptorSet(std::move(_layout)), asset::impl::IEmulatedDescriptorSet<IGPUDescriptorSetLayout>(m_layout.get())
		{
			m_flatOffsets = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<uint32_t>>(m_bindingInfo->size());
			uint32_t uboCount = 0u;//includes dynamics
			uint32_t ssboCount = 0u;//includes dynamics
			uint32_t textureCount = 0u;
			uint32_t imageCount = 0u;
			for (uint32_t i=0u; i<m_bindingInfo->size(); i++)
			{
				auto count = getDescriptorCountAtIndex(i);
				switch (m_bindingInfo->operator[](i).descriptorType)
				{
					case asset::EDT_UNIFORM_BUFFER_DYNAMIC:
						_IRR_FALLTHROUGH;
					case asset::EDT_UNIFORM_BUFFER:
						m_flatOffsets->operator[](i) = uboCount;
						uboCount += count;
						break;
					case asset::EDT_STORAGE_BUFFER_DYNAMIC:
						_IRR_FALLTHROUGH;
					case asset::EDT_STORAGE_BUFFER:
						m_flatOffsets->operator[](i) = ssboCount;
						ssboCount += count;
						break;
					case asset::EDT_UNIFORM_TEXEL_BUFFER: //GL_TEXTURE_BUFFER
						_IRR_FALLTHROUGH;
					case asset::EDT_COMBINED_IMAGE_SAMPLER:
						m_flatOffsets->operator[](i) = textureCount;
						textureCount += count;
						break;
					case asset::EDT_STORAGE_IMAGE:
						_IRR_FALLTHROUGH;
					case asset::EDT_STORAGE_TEXEL_BUFFER:
						m_flatOffsets->operator[](i) = imageCount;
						imageCount += count;
						break;
					default:
						break;
				}
			}

			m_names = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<GLuint> >(uboCount+ssboCount+(2ull*textureCount)+imageCount);
			std::fill(m_names->begin(),m_names->end(), 0u);
			m_offsets = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<GLintptr> >(uboCount+ssboCount);
			std::fill(m_offsets->begin(), m_offsets->end(), 0u);
			m_sizes = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<GLsizeiptr> >(uboCount+ssboCount);
			std::fill(m_sizes->begin(), m_sizes->end(), ~0u);
			m_dynOffsetIxs = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<uint32_t> >(m_offsets->size());
			std::fill(m_dynOffsetIxs->begin(), m_dynOffsetIxs->end(), 0u);
			
			const size_t uboNamesOffset = 0ull;
			const size_t ssboNamesOffset = uboCount;
			const size_t texNamesOffset = uboCount+ssboCount;
			const size_t imagNamesOffset = texNamesOffset + textureCount;
			const size_t samplerNamesOffset = imagNamesOffset + imageCount;

			const size_t uboBufOffset = 0ull;//buffer-specific offsets for `offsets` and `sizes` arrays
			const size_t ssboBufOffset = uboCount;

			auto setBufMultibindParams = [this](SMultibindParams::SMultibindBuffers& _params, size_t _namesOffset, size_t _offset) {
				_params.buffers = (*m_names).data() + _namesOffset;
				_params.offsets = (*m_offsets).data() + _offset;
				_params.sizes = (*m_sizes).data() + _offset;
				_params.dynOffsetIxs = (*m_dynOffsetIxs).data() + _offset;
			};
			setBufMultibindParams(m_multibindParams.ubos, uboNamesOffset, uboBufOffset);
			setBufMultibindParams(m_multibindParams.ssbos, ssboNamesOffset, ssboBufOffset);
			m_multibindParams.textures.textures = (*m_names).data() + texNamesOffset;
			m_multibindParams.textures.samplers = (*m_names).data() + samplerNamesOffset;
			m_multibindParams.textureImages.textures = (*m_names).data() + imagNamesOffset;

			// set up dynamic offset redirects
			uint32_t dyn_offset_iter = 0u;
			for (size_t i=0u; i<m_bindingInfo->size(); i++)
			{
				auto offset = m_flatOffsets->operator[](i);
				for (uint32_t j=0u; j<getDescriptorCountAtIndex(i); j++)
				switch (m_bindingInfo->operator[](i).descriptorType)
				{
					case asset::EDT_UNIFORM_BUFFER:
						m_multibindParams.ubos.dynOffsetIxs[offset+j] = ~static_cast<uint32_t>(0u);
						break;
					case asset::EDT_STORAGE_BUFFER:
						m_multibindParams.ssbos.dynOffsetIxs[offset+j] = ~static_cast<uint32_t>(0u);
						break;
					case asset::EDT_UNIFORM_BUFFER_DYNAMIC:
						m_multibindParams.ubos.dynOffsetIxs[offset+j] = dyn_offset_iter++;
						break;
					case asset::EDT_STORAGE_BUFFER_DYNAMIC:
						m_multibindParams.ssbos.dynOffsetIxs[offset+j] = dyn_offset_iter++;
						break;
					default:
						break;
				}
			}
		}

		inline void writeDescriptorSet(const SWriteDescriptorSet& _write)
		{
			assert(_write.dstSet==static_cast<decltype(_write.dstSet)>(this));
			assert(m_bindingInfo);
			assert(_write.binding<m_bindingInfo->size());
			const auto type = _write.descriptorType;
			assert(type==m_bindingInfo->operator[](_write.binding).descriptorType);
			auto* output = getDescriptors(_write.binding)+_write.arrayElement;
			for (uint32_t i=0u; i<_write.count; i++,output++)
			{
				output->assign(_write.info[i], type);
				uint32_t localIx = _write.arrayElement+i;
				updateMultibindParams(type,*output,m_flatOffsets->operator[](_write.binding)+localIx,_write.binding,localIx);
			}
		}
		inline void copyDescriptorSet(const SCopyDescriptorSet& _copy)
		{
			assert(_copy.dstSet==static_cast<decltype(_copy.dstSet)>(this));
			assert(_copy.srcSet);
			const auto* srcGLSet = static_cast<const COpenGLDescriptorSet*>(_copy.srcSet);
			assert(m_bindingInfo && srcGLSet->m_bindingInfo);
			assert(_copy.srcBinding<srcGLSet->m_bindingInfo->size());
			assert(_copy.dstBinding<m_bindingInfo->size());
			const auto type = srcGLSet->m_bindingInfo->operator[](_copy.srcBinding).descriptorType;
			assert(type==m_bindingInfo->operator[](_copy.dstBinding).descriptorType);
			const auto* input = srcGLSet->getDescriptors(_copy.srcBinding)+_copy.srcArrayElement;
			auto* output = getDescriptors(_copy.dstBinding)+_copy.dstArrayElement;
			for (uint32_t i=0u; i<_copy.count; i++,input++,output++)
			{
				output->assign(*input, type);
				uint32_t localIx = _copy.dstArrayElement+i;
				updateMultibindParams(type,*output,m_flatOffsets->operator[](_copy.dstBinding)+localIx,_copy.dstBinding,localIx);
			}
		}

		inline const SMultibindParams& getMultibindParams() const { return m_multibindParams; }

	protected:
		inline SDescriptorInfo* getDescriptors(uint32_t index) 
		{ 
			const auto& info = m_bindingInfo->operator[](index);
			return m_descriptors->begin()+info.offset;
		}
		inline const SDescriptorInfo* getDescriptors(uint32_t index) const
		{
			const auto& info = m_bindingInfo->operator[](index);
			return m_descriptors->begin() + info.offset;
		}

		inline uint32_t getDescriptorCountAtIndex(uint32_t index) const
		{
			const auto& info = m_bindingInfo->operator[](index);
			if (index+1u!=m_bindingInfo->size())
				return m_bindingInfo->operator[](index+1u).offset-info.offset;
			else
				return m_descriptors->size()-info.offset;
		}

	private:
		inline void updateMultibindParams(asset::E_DESCRIPTOR_TYPE descriptorType, const SDescriptorInfo& info, uint32_t offset, uint32_t binding, uint32_t local_iter)
		{
			if (descriptorType==asset::EDT_UNIFORM_BUFFER || descriptorType==asset::EDT_UNIFORM_BUFFER_DYNAMIC)
			{
				m_multibindParams.ubos.buffers[offset] = static_cast<COpenGLBuffer*>(info.desc.get())->getOpenGLName();
				m_multibindParams.ubos.offsets[offset] = info.buffer.offset;
				m_multibindParams.ubos.sizes[offset] = info.buffer.size;
			}
			else if (descriptorType==asset::EDT_STORAGE_BUFFER || descriptorType==asset::EDT_STORAGE_BUFFER_DYNAMIC)
			{
				m_multibindParams.ssbos.buffers[offset] = static_cast<COpenGLBuffer*>(info.desc.get())->getOpenGLName();
				m_multibindParams.ssbos.offsets[offset] = info.buffer.offset;
				m_multibindParams.ssbos.sizes[offset] = info.buffer.size;
			}
			else if (descriptorType==asset::EDT_COMBINED_IMAGE_SAMPLER)
			{
				m_multibindParams.textures.textures[offset] = static_cast<COpenGLImageView*>(info.desc.get())->getOpenGLName();
				auto layoutBinding = *(m_layout->getBindings().begin()+binding);
				m_multibindParams.textures.samplers[offset] = layoutBinding.samplers ? //take immutable sampler if present
						static_cast<COpenGLSampler*>(layoutBinding.samplers[local_iter].get())->getOpenGLName() :
						static_cast<COpenGLSampler*>(info.image.sampler.get())->getOpenGLName();
			}
			else if (descriptorType==asset::EDT_UNIFORM_TEXEL_BUFFER)
			{
				m_multibindParams.textures.textures[offset] = static_cast<COpenGLBufferView*>(info.desc.get())->getOpenGLName();
				m_multibindParams.textures.samplers[offset] = 0u;//no sampler for samplerBuffer descriptor
			}
			else if (descriptorType==asset::EDT_STORAGE_IMAGE)
				m_multibindParams.textureImages.textures[offset] = static_cast<COpenGLImageView*>(info.desc.get())->getOpenGLName();
			else if (descriptorType==asset::EDT_STORAGE_TEXEL_BUFFER)
				m_multibindParams.textureImages.textures[offset] = static_cast<COpenGLBufferView*>(info.desc.get())->getOpenGLName();
		}

		SMultibindParams m_multibindParams;
		core::smart_refctd_dynamic_array<uint32_t> m_flatOffsets;

		core::smart_refctd_dynamic_array<GLuint> m_names;
		core::smart_refctd_dynamic_array<GLintptr> m_offsets;
		core::smart_refctd_dynamic_array<GLsizeiptr> m_sizes;
		core::smart_refctd_dynamic_array<uint32_t> m_dynOffsetIxs;
};

}
}
#endif

#endif