/***************************************************************************
 *   Copyright (C) 2013 by Linuxstopmotion contributors;                   *
 *   see the AUTHORS file for details.                                     *
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

#include "sound.h"

#include "src/technical/audio/audioformat.h"
#include "src/technical/audio/oggvorbis.h"
#include "src/technical/audio/audiodriver.h"

#include <assert.h>
#include <string.h>
#include <memory>

Sound::Sound() : af(0), name(0) {
}

Sound::~Sound() {
	delete af;
	delete[] name;
}

/**
 *@todo check audio type (ogg, mp3, wav ...)
 */
void Sound::open(WorkspaceFile& file, ErrorHandler& e) {
	std::unique_ptr<OggVorbis> a(new OggVorbis());
	a->setFilename(file, e);
	delete af;
	af = a.release();
	af->open();
}

const char* Sound::setName(const char* n) {
	const char* r = name;
	name = n;
	return r;
}

void Sound::setName(std::string& n) {
	assert(!name);
	int size = n.size() + 1;
	char* a = new char[size];
	name = a;
	strncpy(a, n.c_str(), size);
}

const char* Sound::getName() const {
	return name;
}

const char* Sound::getSoundPath() const {
	return af->getSoundPath();
}

const char* Sound::getBasename() const {
	return af->getBasename();
}

void Sound::addToDriver(AudioDriver& ad) const {
	ad.addAudioFile(af);
}
