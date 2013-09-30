/***************************************************************************
 *   Copyright (C) 2013 by Linuxstopmotion contributors.              *
 *   see contributors.txt for details                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "workspacefile.h"

#include "src/technical/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sstream>
#include <memory.h>

namespace {

uint32_t fileNum;
uint32_t soundNumber;

uint32_t nextFileNumber() {
	return ++fileNum;
}

class WorkspacePath {
};

WorkspacePath workspacePath;

std::ostream& operator<<(std::ostream& s, WorkspacePath) {
	s << getenv("HOME");
	s << "/.stopmotion/tmp/";
	return s;
}

/**
 * Gets a fresh filename in the workspace that doesn't clash with any other
 * file.
 * @param [out] path Will get a @code{.cpp} new char[] @endcode containing
 * the full path to the new file.
 * @param [out] namePart Will get a pointer into @c path that points to the
 * basename part of the path.
 * @param [in] extension Characters that must come at the end of the filename,
 * for example ".jpg".
 */
void getFreshFilename(char*& path, const char*& namePart,
		const char* extension) {
	std::stringstream p;
	std::stringstream::pos_type zeroOff = p.tellp();
	int indexOfName = 0;
	do {
		p.str("");
		p << workspacePath;
		indexOfName = p.tellp();
		p.fill(0);
		p.width(8);
		p <<  nextFileNumber();
		p << extension;
		// keep going until we find a filename that doesn't already exist.
	} while (0 != access(p.str().c_str(), F_OK));
	int size = (p.tellp() - zeroOff) + 1;
	path = new char[size];
	strncpy(path, p.str().c_str(), size);
	namePart = path + indexOfName;
}

}

void WorkspaceFile::clear() {
	std::stringstream ps;
	ps << workspacePath;
	const char* path = ps.str().c_str();
	std::stringstream rm;
	rm << "rm -rf " << path;
	system(rm.str().c_str());
	mkdir(path, 0755);
	//TODO what about failure? Probably can only inform the user and close
	fileNum = 0;
	soundNumber = 0;
}

uint32_t WorkspaceFile::getSoundNumber() {
	return soundNumber;
}

void WorkspaceFile::nextSoundNumber() {
	++soundNumber;
}

WorkspaceFile::WorkspaceFile()
		: fullPath(0), namePart(0) {
}

WorkspaceFile::WorkspaceFile(const char* extension, FreshFilename)
		: fullPath(0), namePart(0) {
	getFreshFilename(fullPath, namePart, extension);
}

WorkspaceFile::WorkspaceFile(TemporaryWorkspaceFile& t)
		: fullPath(t.fullPath), namePart(t.namePart) {
}

WorkspaceFile& WorkspaceFile::operator=(TemporaryWorkspaceFile& t) {
	delete[] fullPath;
	fullPath = t.fullPath;
	namePart = t.namePart;
	t.fullPath = 0;
	t.namePart = 0;
	t.toBeDeleted = false;
	return *this;
}

WorkspaceFile::~WorkspaceFile() {
	delete[] fullPath;
}

const char* WorkspaceFile::basename() const {
	return namePart;
}

const char* WorkspaceFile::path() const {
	return fullPath;
}

void WorkspaceFile::swap(WorkspaceFile& w) {
	char* t = w.fullPath;
	w.fullPath = fullPath;
	fullPath = t;
	const char* t0 = w.namePart;
	w.namePart = namePart;
	namePart = t0;
}

void TemporaryWorkspaceFile::copyToWorkspace(const char* filename) {
	const char* extension = strrchr(filename,'.');
	getFreshFilename(fullPath, namePart, extension);
	toBeDeleted = false;
	if (!Util::copyFile(fullPath, filename)) {
		throw CopyFailedException();
	}
	toBeDeleted = true;
}

TemporaryWorkspaceFile::TemporaryWorkspaceFile(const char* filename)
		: fullPath(0), namePart(0), toBeDeleted(false) {
	// not a totally fullproof test...
	if (strstr(filename, "/.stopmotion/tmp/") != NULL) {
		// Already a workspace file; no need to copy it again
		int size = strlen(filename) + 1;
		fullPath = new char[size];
		strncpy(fullPath, filename, size);
		namePart = strrchr(fullPath,'/') + 1;
	} else {
		copyToWorkspace(filename);
	}
}

TemporaryWorkspaceFile::TemporaryWorkspaceFile(const char* filename,
		ForceCopy)
		: fullPath(0), namePart(0), toBeDeleted(false) {
	copyToWorkspace(filename);
}

TemporaryWorkspaceFile::TemporaryWorkspaceFile(const char* basename,
		AlreadyAWorkspaceFile)
		: fullPath(0), namePart(0), toBeDeleted(false) {
	std::stringstream p;
	std::stringstream::pos_type zeroOff = p.tellp();
	p.str("");
	p << workspacePath;
	int indexOfName = p.tellp();
	p << basename;
	int size = (p.tellp() - zeroOff) + 1;
	fullPath = new char[size];
	const char* cp = p.str().c_str();
	strncpy(fullPath, cp, size);
	namePart = cp + indexOfName;
}

TemporaryWorkspaceFile::~TemporaryWorkspaceFile() {
	if (toBeDeleted) {
		unlink(fullPath);
	}
	delete[] fullPath;
	fullPath = 0;
	namePart = 0;
}
