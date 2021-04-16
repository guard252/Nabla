// Copyright (C) 2019 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine" and was originally part of the "Irrlicht Engine"
// For conditions of distribution and use, see copyright notice in nabla.h
// See the original file in irrlicht source for authors

#include <list>
#include "CFileSystem.h"
#include "CReadFile.h"
#include "IWriteFile.h"
#include "CZipReader.h"
#include "CMountPointReader.h"
#include "CPakReader.h"
#include "CTarReader.h"
#include "CFileList.h"
#include "stdio.h"
#include "os.h"
#include "CMemoryFile.h"
#include "CLimitReadFile.h"


#if defined (_NBL_WINDOWS_API_)
	#if !defined ( _WIN32_WCE )
		#include <direct.h> // for _chdir
		#include <io.h> // for _access
		#include <tchar.h>
	#endif
#else
	#if (defined(_NBL_POSIX_API_) || defined(_NBL_OSX_PLATFORM_))
		#include <stdio.h>
		#include <stdlib.h>
		#include <string.h>
		#include <limits.h>
		#include <sys/types.h>
		#include <dirent.h>
		#include <sys/stat.h>
		#include <unistd.h>
	#endif
#endif

namespace nbl
{
namespace io
{

//! constructor
CFileSystem::CFileSystem(std::string&& _builtinResourceDirectory) : IFileSystem(std::move(_builtinResourceDirectory))
{
	#ifdef _NBL_DEBUG
	setDebugName("CFileSystem");
	#endif

	setFileListSystem(FILESYSTEM_NATIVE);
	//! reset current working directory
	getWorkingDirectory();

#ifdef __NBL_COMPILE_WITH_PAK_ARCHIVE_LOADER_
	ArchiveLoader.push_back(new CArchiveLoaderPAK(this));
#endif

#ifdef __NBL_COMPILE_WITH_TAR_ARCHIVE_LOADER_
	ArchiveLoader.push_back(new CArchiveLoaderTAR(this));
#endif

#ifdef __NBL_COMPILE_WITH_MOUNT_ARCHIVE_LOADER_
	ArchiveLoader.push_back(new CArchiveLoaderMount(this));
#endif

#ifdef __NBL_COMPILE_WITH_ZIP_ARCHIVE_LOADER_
	ArchiveLoader.push_back(new CArchiveLoaderZIP(this));
#endif

}


//! destructor
CFileSystem::~CFileSystem()
{
	uint32_t i;

	for ( i=0; i < FileArchives.size(); ++i)
	{
		FileArchives[i]->drop();
	}

	for ( i=0; i < ArchiveLoader.size(); ++i)
	{
		ArchiveLoader[i]->drop();
	}
}


//! opens a file for read access
IReadFile* CFileSystem::createAndOpenFile(const std::filesystem::path& filename)
{
	IReadFile* file = 0;
	uint32_t i;

	for (i=0; i< FileArchives.size(); ++i)
	{
		file = FileArchives[i]->createAndOpenFile(filename);
		if (file)
			return file;
	}

	// Create the file using an absolute path so that it matches
	// the scheme used by CNullDriver::getTexture().
    file = new CReadFile(std::filesystem::absolute(filename));
    if (static_cast<CReadFile*>(file)->isOpen())
        return file;

    file->drop();
    return 0;
}


//! Creates an IReadFile interface for treating memory like a file.
IReadFile* CFileSystem::createMemoryReadFile(const void* contents, size_t len, const std::filesystem::path& fileName)
{
	if (!contents)
		return nullptr;
	else
		return new CMemoryReadFile(contents, len, fileName);
}


//! Creates an IReadFile interface for reading files inside files
IReadFile* CFileSystem::createLimitReadFile(const std::filesystem::path& fileName,
		IReadFile* alreadyOpenedFile, const size_t& pos, const size_t& areaSize)
{
	if (!alreadyOpenedFile)
		return 0;
	else
		return new CLimitReadFile(alreadyOpenedFile, pos, areaSize, fileName);
}


//! Creates an IReadFile interface for treating memory like a file.
IWriteFile* CFileSystem::createMemoryWriteFile(size_t len, const std::filesystem::path& fileName)
{
    return new CMemoryWriteFile(len, fileName);
}


//! Opens a file for write access.
IWriteFile* CFileSystem::createAndWriteFile(const std::filesystem::path& filename, bool append)
{
	return createWriteFile(filename, append);
}


//! Adds an external archive loader to the engine.
void CFileSystem::addArchiveLoader(IArchiveLoader* loader)
{
	if (!loader)
		return;

	loader->grab();
	ArchiveLoader.push_back(loader);
}

//! Returns the total number of archive loaders added.
uint32_t CFileSystem::getArchiveLoaderCount() const
{
	return ArchiveLoader.size();
}

//! Gets the archive loader by index.
IArchiveLoader* CFileSystem::getArchiveLoader(uint32_t index) const
{
	if (index < ArchiveLoader.size())
		return ArchiveLoader[index];
	else
		return 0;
}

//! move the hirarchy of the filesystem. moves sourceIndex relative up or down
bool CFileSystem::moveFileArchive(uint32_t sourceIndex, int32_t relative)
{
	bool r = false;
	const int32_t dest = (int32_t) sourceIndex + relative;
	const int32_t dir = relative < 0 ? -1 : 1;
	const int32_t sourceEnd = ((int32_t) FileArchives.size() ) - 1;
	IFileArchive *t;

	for (int32_t s = (int32_t) sourceIndex;s != dest; s += dir)
	{
		if (s < 0 || s > sourceEnd || s + dir < 0 || s + dir > sourceEnd)
			continue;

		t = FileArchives[s + dir];
		FileArchives[s + dir] = FileArchives[s];
		FileArchives[s] = t;
		r = true;
	}
	return r;
}


//! Adds an archive to the file system.
bool CFileSystem::addFileArchive(const std::filesystem::path& filename, E_FILE_ARCHIVE_TYPE archiveType,
			  const std::string_view& password,
			  IFileArchive** retArchive)
{
	IFileArchive* archive = 0;
	bool ret = false;

	// see if archive is already added
	if (changeArchivePassword(filename, password, retArchive))
		return true;

	int32_t i;

	// do we know what type it should be?
	if (archiveType == EFAT_UNKNOWN || archiveType == EFAT_FOLDER)
	{
		// try to load archive based on file name
		for (i = ArchiveLoader.size()-1; i >=0 ; --i)
		{
			if (ArchiveLoader[i]->isALoadableFileFormat(filename))
			{
				archive = ArchiveLoader[i]->createArchive(filename);
				if (archive)
					break;
			}
		}

		// try to load archive based on content
		if (!archive)
		{
			io::IReadFile* file = createAndOpenFile(filename);
			if (file)
			{
				for (i = ArchiveLoader.size()-1; i >= 0; --i)
				{
					file->seek(0);
					if (ArchiveLoader[i]->isALoadableFileFormat(file))
					{
						file->seek(0);
						archive = ArchiveLoader[i]->createArchive(file);
						if (archive)
							break;
					}
				}
				file->drop();
			}
		}
	}
	else
	{
		// try to open archive based on archive loader type

		io::IReadFile* file = 0;

		for (i = ArchiveLoader.size()-1; i >= 0; --i)
		{
			if (ArchiveLoader[i]->isALoadableFileFormat(archiveType))
			{
				// attempt to open file
				if (!file)
					file = createAndOpenFile(filename);

				// is the file open?
				if (file)
				{
					// attempt to open archive
					file->seek(0);
					if (ArchiveLoader[i]->isALoadableFileFormat(file))
					{
						file->seek(0);
						archive = ArchiveLoader[i]->createArchive(file);
						if (archive)
							break;
					}
				}
				else
				{
					// couldn't open file
					break;
				}
			}
		}

		// if open, close the file
		if (file)
			file->drop();
	}

	if (archive)
	{
		FileArchives.push_back(archive);
		if (password.size())
			archive->Password=password;
		if (retArchive)
			*retArchive = archive;
		ret = true;
	}
	else
	{
		os::Printer::log("Could not create archive for", filename.string(), ELL_ERROR);
	}

	return ret;
}

// don't expose!
bool CFileSystem::changeArchivePassword(const std::filesystem::path& filename,
		const std::string_view& password,
		IFileArchive** archive)
{
	for (int32_t idx = 0; idx < (int32_t)FileArchives.size(); ++idx)
	{
		// TODO: This should go into a path normalization method
		// We need to check for directory names with trailing slash and without
		const std::filesystem::path absPath = std::filesystem::absolute(filename);
		const std::filesystem::path arcPath = FileArchives[idx]->getFileList()->getPath();
		if ((absPath == arcPath) || ((absPath.string() + "/") == arcPath))
		{
			if (password.size())
				FileArchives[idx]->Password=password;
			if (archive)
				*archive = FileArchives[idx];
			return true;
		}
	}

	return false;
}

bool CFileSystem::addFileArchive(IReadFile* file, E_FILE_ARCHIVE_TYPE archiveType,
		const std::string_view& password, IFileArchive** retArchive)
{
	if (!file || archiveType == EFAT_FOLDER)
		return false;

	if (file)
	{
		if (changeArchivePassword(file->getFileName(), password, retArchive))
			return true;

		IFileArchive* archive = 0;
		int32_t i;

		if (archiveType == EFAT_UNKNOWN)
		{
			// try to load archive based on file name
			for (i = ArchiveLoader.size()-1; i >=0 ; --i)
			{
				if (ArchiveLoader[i]->isALoadableFileFormat(file->getFileName()))
				{
					archive = ArchiveLoader[i]->createArchive(file);
					if (archive)
						break;
				}
			}

			// try to load archive based on content
			if (!archive)
			{
				for (i = ArchiveLoader.size()-1; i >= 0; --i)
				{
					file->seek(0);
					if (ArchiveLoader[i]->isALoadableFileFormat(file))
					{
						file->seek(0);
						archive = ArchiveLoader[i]->createArchive(file);
						if (archive)
							break;
					}
				}
			}
		}
		else
		{
			// try to open archive based on archive loader type
			for (i = ArchiveLoader.size()-1; i >= 0; --i)
			{
				if (ArchiveLoader[i]->isALoadableFileFormat(archiveType))
				{
					// attempt to open archive
					file->seek(0);
					if (ArchiveLoader[i]->isALoadableFileFormat(file))
					{
						file->seek(0);
						archive = ArchiveLoader[i]->createArchive(file);
						if (archive)
							break;
					}
				}
			}
		}

		if (archive)
		{
			FileArchives.push_back(archive);
			if (password.size())
				archive->Password=password;
			if (retArchive)
				*retArchive = archive;
			return true;
		}
		else
		{
			os::Printer::log("Could not create archive for", file->getFileName().string(), ELL_ERROR);
		}
	}

	return false;
}


//! Adds an archive to the file system.
bool CFileSystem::addFileArchive(IFileArchive* archive)
{
	for (uint32_t i=0; i < FileArchives.size(); ++i)
	{
		if (archive == FileArchives[i])
			return false;
	}
	FileArchives.push_back(archive);
	return true;
}


//! removes an archive from the file system.
bool CFileSystem::removeFileArchive(uint32_t index)
{
	bool ret = false;
	if (index < FileArchives.size())
	{
	    auto it = FileArchives.begin()+index;
		(*it)->drop();
		FileArchives.erase(it);
		ret = true;
	}

	return ret;
}


//! removes an archive from the file system.
bool CFileSystem::removeFileArchive(const std::filesystem::path& filename)
{
	const std::filesystem::path absPath = std::filesystem::absolute(filename);
	for (uint32_t i=0; i < FileArchives.size(); ++i)
	{
		if (absPath == FileArchives[i]->getFileList()->getPath())
			return removeFileArchive(i);
	}

	return false;
}


//! Removes an archive from the file system.
bool CFileSystem::removeFileArchive(const IFileArchive* archive)
{
	for (uint32_t i=0; i < FileArchives.size(); ++i)
	{
		if (archive == FileArchives[i])
			return removeFileArchive(i);
	}

	return false;
}


//! gets an archive
uint32_t CFileSystem::getFileArchiveCount() const
{
	return FileArchives.size();
}


IFileArchive* CFileSystem::getFileArchive(uint32_t index)
{
	return index < getFileArchiveCount() ? FileArchives[index] : 0;
}


//! Returns the string of the current working directory
const std::filesystem::path& CFileSystem::getWorkingDirectory()
{
	EFileSystemType type = FileSystemType;

	if (type != FILESYSTEM_NATIVE)
	{
		type = FILESYSTEM_VIRTUAL;
	}
	else
	{
		//TODO: there is probably a better way without global variables
		WorkingDirectory[FILESYSTEM_NATIVE] = std::filesystem::current_path();
	}

	return WorkingDirectory[type];
}


//! Changes the current Working Directory to the given string.
bool CFileSystem::changeWorkingDirectoryTo(const std::filesystem::path& newDirectory)
{
	bool success=false;

	if (FileSystemType != FILESYSTEM_NATIVE)
	{
		WorkingDirectory[FILESYSTEM_VIRTUAL] = newDirectory;
		// is this empty string constant really intended?
		WorkingDirectory[FILESYSTEM_VIRTUAL] = flattenFilename(WorkingDirectory[FILESYSTEM_VIRTUAL], "");
		success = true;
	}
	else
	{
		WorkingDirectory[FILESYSTEM_NATIVE] = newDirectory;

#if defined(_MSC_VER)
	#if defined(_NBL_WCHAR_FILESYSTEM)
		success = (_wchdir(newDirectory.c_str()) == 0);
	#else
		success = (_chdir(newDirectory.string().c_str()) == 0);
	#endif
#else
    #if defined(_NBL_WCHAR_FILESYSTEM)
		success = (_wchdir(newDirectory.c_str()) == 0);
    #else
        success = (chdir(newDirectory.c_str()) == 0);
    #endif
#endif
	}

	return success;
}


//! Sets the current file systen type
EFileSystemType CFileSystem::setFileListSystem(EFileSystemType listType)
{
	EFileSystemType current = FileSystemType;
	FileSystemType = listType;
	return current;
}


//! Creates a list of files and directories in the current working directory
IFileList* CFileSystem::createFileList()
{
	CFileList* r = 0;
	std::filesystem::path Path = getWorkingDirectory();
	core::handleBackslashes(&Path);
	if (*Path.string().rbegin() != '/')
		Path += '/';

	//! Construct from native filesystem
	if (FileSystemType == FILESYSTEM_NATIVE)
	{
		// --------------------------------------------
		//! Windows version
		#ifdef _NBL_WINDOWS_API_
		#if !defined ( _WIN32_WCE )

		r = new CFileList(Path);

		// TODO: Should be unified once mingw adapts the proper types
#if defined(__GNUC__)
		long hFile; //mingw return type declaration
#else
		intptr_t hFile;
#endif

		struct _tfinddata_t c_file;
		if( (hFile = _tfindfirst( _T("*"), &c_file )) != -1L )
		{
			do
			{
				r->addItem(Path.string() + c_file.name, 0, c_file.size, (_A_SUBDIR & c_file.attrib) != 0, 0);
			}
			while( _tfindnext( hFile, &c_file ) == 0 );

			_findclose( hFile );
		}
		#endif

		//TODO add drives
		//entry.Name = "E:\\";
		//entry.isDirectory = true;
		//Files.push_back(entry);
		#endif

		// --------------------------------------------
		//! Linux version
		#if (defined(_NBL_POSIX_API_) || defined(_NBL_OSX_PLATFORM_))


		r = new CFileList(Path);

		r->addItem(Path + "..", 0, 0, true, 0);

		//! We use the POSIX compliant methods instead of scandir
		DIR* dirHandle=opendir(Path.c_str());
		if (dirHandle)
		{
			struct dirent *dirEntry;
			while ((dirEntry=readdir(dirHandle)))
			{
				uint32_t size = 0;
				bool isDirectory = false;

				if((strcmp(dirEntry->d_name, ".")==0) ||
				   (strcmp(dirEntry->d_name, "..")==0))
				{
					continue;
				}
				struct stat buf;
				if (stat(dirEntry->d_name, &buf)==0)
				{
					size = buf.st_size;
					isDirectory = S_ISDIR(buf.st_mode);
				}
				#if !defined(_NBL_SOLARIS_PLATFORM_) && !defined(__CYGWIN__) && !defined(__LSB_VERSION__)
				// only available on some systems
				else
				{
					isDirectory = dirEntry->d_type == DT_DIR;
				}
				#endif

				r->addItem(Path + dirEntry->d_name, 0, size, isDirectory, 0);
			}
			closedir(dirHandle);
		}
		#endif
	}
	else
	{
		//! create file list for the virtual filesystem
		r = new CFileList(Path);

		//! add relative navigation
		SFileListEntry e2;
		SFileListEntry e3;

		//! PWD
		r->addItem(Path.string() + ".", 0, 0, true, 0);

		//! parent
		r->addItem(Path.string() + "..", 0, 0, true, 0);

		//! merge archives
		for (uint32_t i=0; i < FileArchives.size(); ++i)
		{
			const IFileList *merge = FileArchives[i]->getFileList();

			auto files = merge->getFiles();
			for (auto it=files.begin(); it!=files.end(); it++)
			{
				if (core::isInSameDirectory(Path, it->FullName) == 0)
					r->addItem(it->FullName, it->Offset, it->Size, it->IsDirectory, 0);
			}
		}
	}

	return r;
}

//! Creates an empty filelist
IFileList* CFileSystem::createEmptyFileList(const std::filesystem::path& path)
{
	return new CFileList(path);
}


//! determines if a file exists and would be able to be opened.
bool CFileSystem::existFile(const std::filesystem::path& filename) const
{
	for (uint32_t i=0; i < FileArchives.size(); ++i)
	{
        auto _list = FileArchives[i]->getFileList();
        auto files = _list->getFiles();
		if (_list->findFile(files.begin(),files.end(),filename)!=files.end())
			return true;
	}

#if defined(_MSC_VER)
    #if defined(_NBL_WCHAR_FILESYSTEM)
        return (_waccess(filename.string().c_str(), 0) != -1);
    #else
        return (_access(filename.string().c_str(), 0) != -1);
    #endif
#elif defined(F_OK)
    #if defined(_NBL_WCHAR_FILESYSTEM)
        return (_waccess(filename.string().c_str(), F_OK) != -1);
    #else
        return (access(filename.string().c_str(), F_OK) != -1);
	#endif
#else
    return (access(filename.string().c_str(), 0) != -1);
#endif
}


} // end namespace nbl
} // end namespace io

