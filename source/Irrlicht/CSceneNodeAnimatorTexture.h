// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_SCENE_NODE_ANIMATOR_TEXTURE_H_INCLUDED__
#define __C_SCENE_NODE_ANIMATOR_TEXTURE_H_INCLUDED__

#include "irrArray.h"
#include "ISceneNodeAnimatorFinishing.h"

namespace irr
{
namespace scene
{
	class CSceneNodeAnimatorTexture : public ISceneNodeAnimatorFinishing
	{
	public:

		//! constructor
		CSceneNodeAnimatorTexture(const core::array<video::ITexture*>& textures,
			s32 timePerFrame, bool loop, u32 now);

		//! destructor
		virtual ~CSceneNodeAnimatorTexture();

		//! animates a scene node
		virtual void animateNode(IDummyTransformationSceneNode* node, u32 timeMs);

		//! Returns type of the scene node animator
		virtual ESCENE_NODE_ANIMATOR_TYPE getType() const { return ESNAT_TEXTURE; }

		//! Creates a clone of this animator.
		/** Please note that you will have to drop
		(IReferenceCounted::drop()) the returned pointer after calling
		this. */
		virtual ISceneNodeAnimator* createClone(IDummyTransformationSceneNode* node, ISceneManager* newManager=0);

	private:

		void clearTextures();

		core::array<video::ITexture*> Textures;
		u32 TimePerFrame;
		u32 StartTime;
		bool Loop;
	};


} // end namespace scene
} // end namespace irr

#endif

