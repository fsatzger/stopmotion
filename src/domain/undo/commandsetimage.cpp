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

#include "commandsetimage.h"

#include <stdint.h>
#include <string>

#include "src/domain/animation/animationimpl.h"
#include "src/domain/animation/workspacefile.h"
#include "src/domain/undo/command.h"

class ErrorHandler;

CommandSetImage::CommandSetImage(AnimationImpl& model, int32_t scene,
		int32_t frame, WorkspaceFile& w)
		: sv(model), sc(scene), fr(frame), image(w) {
}

CommandSetImage::~CommandSetImage() {
}

Command* CommandSetImage::execute() {
	sv.replaceImage(sc, fr, image);
	return this;
}

CommandSetImageFactory::CommandSetImageFactory(AnimationImpl& model) : sv(model) {
}

CommandSetImageFactory::~CommandSetImageFactory() {
}

Command* CommandSetImageFactory::create(Parameters& ps, ErrorHandler&) {
	int sceneCount = sv.sceneCount();
	if (sceneCount == 0)
		return 0;
	int32_t sc = ps.getInteger(0, sceneCount - 1);
	int frameCount = sv.frameCount(sc);
	if (frameCount == 0)
		return 0;
	int32_t fr = ps.getInteger(0, frameCount - 1);
	std::string path;
	ps.getString(path, "?*.jpg");
	WorkspaceFile twf(path.c_str());
	return new CommandSetImage(sv, sc, fr, twf);
}
