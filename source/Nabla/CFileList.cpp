// Copyright (C) 2019 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine" and was originally part of the "Irrlicht Engine"
// For conditions of distribution and use, see copyright notice in nabla.h
// See the original file in irrlicht source for authors

#include "CFileList.h"
#include "nbl/core/core.h"

#include <algorithm>

#include "os.h"

namespace nbl
{
namespace io
{

static const std::filesystem::path emptyFileListEntry;

CFileList::CFileList(const std::filesystem::path& path) : Path(path)
{
	#ifdef _NBL_DEBUG
	setDebugName("CFileList");
	#endif

	core::handleBackslashes(&Path);
}

CFileList::~CFileList()
{
	Files.clear();
}

//! adds a file or folder
void CFileList::addItem(const std::filesystem::path& fullPath, uint32_t offset, uint32_t size, bool isDirectory, uint32_t id)
{
	SFileListEntry entry;
	entry.ID   = id ? id : Files.size();
	entry.Offset = offset;
	entry.Size = size;
	entry.Name = fullPath;
	core::handleBackslashes(&entry.Name);
	entry.IsDirectory = isDirectory;

	// remove trailing slash
	if (*entry.Name.string().rbegin() == '/')
	{
		entry.IsDirectory = true;
		entry.Name.string()[entry.Name.string().size()-1] = 0;
	}

	entry.FullName = entry.Name;

	core::deletePathFromFilename(entry.Name);

	//os::Printer::log(Path.c_str(), entry.FullName);

	Files.insert(std::lower_bound(Files.begin(),Files.end(),entry),entry);
}



//! Searches for a file or folder within the list, returns the index
IFileList::ListCIterator CFileList::findFile(IFileList::ListCIterator _begin, IFileList::ListCIterator _end, const std::filesystem::path& filename, bool isDirectory) const
{
    SFileListEntry entry; 
    // we only need FullName to be set for the search
    entry.FullName = filename;
    entry.IsDirectory = isDirectory;

	entry.Name = entry.FullName.filename();

    auto retval = std::lower_bound(_begin,_end,entry);
    if (retval!=_end && entry<*retval)
        return _end;
    return retval;
}


} // end namespace nbl
} // end namespace io

