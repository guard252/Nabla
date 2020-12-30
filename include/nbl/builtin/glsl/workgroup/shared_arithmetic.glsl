#ifndef _NBL_BUILTIN_GLSL_WORKGROUP_SHARED_ARITHMETIC_INCLUDED_
#define _NBL_BUILTIN_GLSL_WORKGROUP_SHARED_ARITHMETIC_INCLUDED_



#include <nbl/builtin/glsl/workgroup/shared_clustered.glsl>



#if NBL_GLSL_WORKGROUP_SCAN_SCRATCH_BOUND(_NBL_GLSL_WORKGROUP_SIZE_-1)>_NBL_GLSL_WORKGROUP_CLUSTERED_SHARED_SIZE_NEEDED_
	#define _NBL_GLSL_WORKGROUP_ARITHMETIC_SHARED_SIZE_NEEDED_  NBL_GLSL_WORKGROUP_SCAN_SCRATCH_BOUND(_NBL_GLSL_WORKGROUP_SIZE_-1)
#else
	#define _NBL_GLSL_WORKGROUP_ARITHMETIC_SHARED_SIZE_NEEDED_  _NBL_GLSL_WORKGROUP_CLUSTERED_SHARED_SIZE_NEEDED_
#endif



#endif
