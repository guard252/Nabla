#ifndef __IRR_MATERIAL_COMPILER_IR_H_INCLUDED__
#define __IRR_MATERIAL_COMPILER_IR_H_INCLUDED__

#include <irr/core/IReferenceCounted.h>
#include <irr/core/memory/refctd_dynamic_array.h>
#include <irr/asset/ICPUImageView.h>
#include <irr/asset/ICPUSampler.h>
#include <irr/core/alloc/LinearAddressAllocator.h>

namespace irr {
namespace asset {
namespace material_compiler
{

class IR : public core::IReferenceCounted
{
    class SBackingMemManager
    {
        _IRR_STATIC_INLINE_CONSTEXPR size_t INITIAL_MEM_SIZE = 1ull<<20;
        _IRR_STATIC_INLINE_CONSTEXPR size_t MAX_MEM_SIZE = 1ull<<20;
        _IRR_STATIC_INLINE_CONSTEXPR size_t ALIGNMENT = _IRR_SIMD_ALIGNMENT;

        uint8_t* mem;
        size_t currSz;
        core::LinearAddressAllocator<uint32_t> addrAlctr;

    public:
        SBackingMemManager() : currSz(INITIAL_MEM_SIZE), addrAlctr(nullptr, 0u, 0u, ALIGNMENT, MAX_MEM_SIZE) {
            mem = reinterpret_cast<uint8_t*>(_IRR_ALIGNED_MALLOC(currSz, ALIGNMENT));
        }
        ~SBackingMemManager() {
            _IRR_ALIGNED_FREE(mem);
        }

        uint8_t* alloc(size_t bytes)
        {
            auto addr = addrAlctr.alloc_addr(bytes, ALIGNMENT);
            //TODO reallocation will invalidate all pointers to nodes, so...
            //1) never reallocate (just have reasonably big buffer for nodes)
            //2) make some node_handle class that will work as pointer but is based on offset instead of actual address
            if (addr+bytes > currSz) {
                size_t newSz = currSz<<1;
                if (newSz > MAX_MEM_SIZE) {
                    addrAlctr.free_addr(addr, bytes);
                    return nullptr;
                }

                void* newMem = _IRR_ALIGNED_MALLOC(newSz, ALIGNMENT);
                memcpy(newMem, mem, currSz);
                _IRR_ALIGNED_FREE(mem);
                mem = reinterpret_cast<uint8_t*>(newMem);
                currSz = newSz;
            }

            return mem+addr;
        }
    };

protected:
    ~IR()
    {
        //call destructors on all nodes
        for (auto* root : roots)
        {
            core::set<decltype(root)> deinited;
            core::stack<decltype(root)> s;
            s.push(root);
            while (!s.empty())
            {
                auto* n = s.top();
                s.pop();
                for (auto* c : n->children)
                    s.push(c);

                if (deinited.find(n)==deinited.end()) {
                    n->~INode();
                    deinited.insert(n);
                }
            }
        }
    }

public:
    template <typename NodeType, typename ...Args>
    NodeType* allocNode(Args&& ...args)
    {
        uint8_t* ptr = memMgr.alloc(sizeof(NodeType));
        return new (ptr) NodeType(std::forward<Args>(args)...);
    }
    template <typename NodeType, typename ...Args>
    NodeType* allocRootNode(Args&& ...args)
    {
        auto* root = allocNode<NodeType>(std::forward<Args>(args)...);
        roots.push_back(root);
        return root;
    }

    struct INode
    {
        enum E_SYMBOL
        {
            ES_MATERIAL,
            ES_GEOM_MODIFIER,
            ES_FRONT_SURFACE,
            ES_BACK_SURFACE,
            ES_EMISSION,
            ES_BSDF,
            ES_BSDF_COMBINER
        };
        enum E_PARAM_SOURCE
        {
            EPS_CONSTANT,
            EPS_TEXTURE
        };

        struct STextureSource {
            core::smart_refctd_ptr<ICPUImageView> image;
            core::smart_refctd_ptr<ICPUSampler> sampler;
            float scale;

            bool operator==(const STextureSource& rhs) const { return image==rhs.image && sampler==rhs.sampler && scale==rhs.scale; }
        };
        //destructor of actually used variant member has to be called by hand!
        template <typename type_of_const>
        union UTextureOrConstant {
            UTextureOrConstant() : texture{nullptr,nullptr,0.f} {}
            ~UTextureOrConstant() {}

            type_of_const constant;
            STextureSource texture;
        };
        template <typename type_of_const>
        struct SParameter
        {
            ~SParameter() {
                if (source==EPS_TEXTURE)
                    value.texture.~STextureSource();
            }

            SParameter<type_of_const>& operator=(const SParameter<type_of_const>& rhs)
            {
                source = rhs.source;
                if (source == EPS_CONSTANT)
                    value.constant = rhs.value.constant;
                else
                    value.texture = rhs.value.texture;

                return *this;
            }

            bool operator==(const SParameter<type_of_const>& rhs) const {
                if (source!=rhs.source)
                    return false;
                switch (source)
                {
                case EPS_CONSTANT:
                    return value.constant==value.constant;
                case EPS_TEXTURE:
                    return value.texture==value.texture;
                default: return false;
                }
            }

            E_PARAM_SOURCE source;
            UTextureOrConstant<type_of_const> value;
        };

        _IRR_STATIC_INLINE_CONSTEXPR size_t MAX_CHILDREN = 16ull;
        struct children_array_t {
            INode* array[MAX_CHILDREN] {};
            size_t count = 0ull;

            inline bool operator!=(const children_array_t& rhs) const
            {
                if (count != rhs.count)
                    return false;

                for (uint32_t i = 0u; i < count; ++i)
                    if (array[i]!=rhs.array[i])
                        return false;
                return true;
            }
            inline bool operator==(const children_array_t& rhs) const
            {
                return !operator!=(rhs);
            }

            inline bool find(E_SYMBOL s, size_t* ix = nullptr) const 
            { 
                auto found = std::find_if(begin(),end(),[s](INode* child){return child->symbol==s;});
                if (found != (array+count))
                {
                    if (ix)
                        ix[0] = found-array;
                    return true;
                }
                return false;
            }

            operator bool() const { return count!=0ull; }

            INode** begin() { return array; }
            const INode*const* begin() const { return array; }
            INode** end() { return array+count; }
            const INode*const* end() const { return array+count; }

            inline INode*& operator[](size_t i) { assert(i<count); return array[i]; }
            inline const INode* const& operator[](size_t i) const { assert(i<count); return array[i]; }
        };
        template <typename ...Contents>
        static inline children_array_t createChildrenArray(Contents... children) { return { {children...}, sizeof...(children) }; }

        using color_t = core::vector3df_SIMD;

        explicit INode(E_SYMBOL s) : symbol(s) {}
        virtual ~INode() = default;

        children_array_t children;
        E_SYMBOL symbol;
    };

    struct CMaterialNode : public INode
    {
        CMaterialNode() : INode(ES_MATERIAL)
        {
            opacity.source = EPS_CONSTANT;
            opacity.value.constant = color_t(1.f);
        }

        SParameter<color_t> opacity;
    };

    struct CGeomModifierNode : public INode
    {
        enum E_TYPE
        {
            ET_DISPLACEMENT,
            ET_HEIGHT,
            ET_NORMAL
        };
        enum E_SOURCE
        {
            ESRC_UV_FUNCTION,
            ESRC_TEXTURE
        };

        CGeomModifierNode(E_TYPE t) : INode(ES_GEOM_MODIFIER), type(t) {}

        E_TYPE type;
        E_SOURCE source;
        //TODO some union member for when source==ESRC_UV_FUNCTION, no idea what type
        //in fact when we want to translate MDL function of (u,v) into this IR we could just create an image being a 2D plot of this function with some reasonable quantization (pixel dimensions)
        //union {
            STextureSource texture;
        //};
    };

    struct CEmissionNode : INode
    {
        CEmissionNode() : INode(ES_EMISSION) {}

        float intensity;
    };

    struct CBSDFCombinerNode : INode
    {
        enum E_TYPE
        {
            //mix of N BSDFs
            ET_MIX,
            //blend of 2 BSDFs weighted by constant or texture
            ET_WEIGHT_BLEND,
            //blend of 2 BSDFs weighted by fresnel
            ET_FRESNEL_BLEND,
            //blend of 2 BSDFs weighted by custom direction-based curve
            ET_CUSTOM_CURVE_BLEND,
            ET_DIFFUSE_AND_SPECULAR
        };

        E_TYPE type;

        CBSDFCombinerNode(E_TYPE t) : INode(ES_BSDF_COMBINER), type(t) {}
    };
    struct CBSDFBlendNode : CBSDFCombinerNode
    {
        CBSDFBlendNode() : CBSDFCombinerNode(ET_WEIGHT_BLEND) {}

        SParameter<float> weight;
    };
    struct CBSDFMixNode : CBSDFCombinerNode
    {
        CBSDFMixNode() : CBSDFCombinerNode(ET_MIX) {}

        float weights[MAX_CHILDREN];
    };

    struct CBSDFNode : INode
    {
        enum E_TYPE
        {
            ET_DIFFTRANS,
            ET_SPECULAR_DELTA,
            ET_MICROFACET_DIFFUSE,
            ET_MICROFACET_SPECULAR,
            ET_SHEEN,
            ET_COATING
        };

        CBSDFNode(E_TYPE t) :
            INode(ES_BSDF),
            type(t),
            eta(1.f,1.f,1.f),
            etaK(0.f,0.f,0.f)
        {}

        E_TYPE type;
        color_t eta, etaK;
    };
    struct CMicrofacetSpecularBSDFNode : CBSDFNode
    {
        enum E_NDF
        {
            ENDF_BECKMANN,
            ENDF_GGX,
            ENDF_ASHIKHMIN_SHIRLEY,
            ENDF_PHONG
        };
        enum E_SHADOWING_TERM
        {
            EST_SMITH,
            EST_VCAVITIES
        };
        enum E_SCATTER_MODE
        {
            ESM_REFLECT,
            ESM_TRANSMIT
        };

        CMicrofacetSpecularBSDFNode(E_TYPE t = ET_MICROFACET_SPECULAR) : CBSDFNode(t) {}

        E_NDF ndf;
        E_SHADOWING_TERM shadowing;
        E_SCATTER_MODE scatteringMode;
        SParameter<float> alpha_u;
        SParameter<float> alpha_v;
    };
    struct CMicrofacetDiffuseBSDFNode : CBSDFNode
    {
        CMicrofacetDiffuseBSDFNode() : CBSDFNode(ET_MICROFACET_DIFFUSE) {}

        SParameter<color_t> reflectance;
        SParameter<float> alpha_u;
        SParameter<float> alpha_v;
    };
    struct CDifftransBSDFNode : CBSDFNode
    {
        CDifftransBSDFNode() : CBSDFNode(ET_DIFFTRANS) {}

        SParameter<color_t> transmittance;
    };
    struct CCoatingBSDFNode : CMicrofacetSpecularBSDFNode
    {
        CCoatingBSDFNode() : CMicrofacetSpecularBSDFNode(ET_COATING) {}

        SParameter<color_t> sigmaA;
        float thickness;
    };

    SBackingMemManager memMgr;
    core::vector<INode*> roots;
};

}}}

#endif