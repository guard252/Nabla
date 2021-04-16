#ifndef __NBL_SYSTEM_PATH_H_INCLUDED__
#define __NBL_SYSTEM_PATH_H_INCLUDED__

#include <filesystem>

namespace nbl
{
    //TODO: Figure out where to move this
    using string = std::string;
}
namespace nbl::system
{

    using path = std::filesystem::path;
    inline nbl::string extension_wo_dot(const path& _filename)
    {
        return _filename.extension().string().substr(1u, nbl::string::npos);
    }

    inline path filename_wo_extension(const path& filename)
    {
        path ret = filename;
        return ret.replace_extension();
    }

}

#endif