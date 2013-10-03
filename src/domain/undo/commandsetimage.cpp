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

#include "commandsetimage.h"
#include "scenevector.h"
#include "workspacefile.h"
#include "src/domain/animation/scene.h"

CommandSetImage::CommandSetImage(SceneVector& model, int32_t scene,
		int32_t frame, TemporaryWorkspaceFile& w)
		: sv(model), sc(scene), fr(frame), image(w) {
}

CommandSetImage::~CommandSetImage() {
}

Command* CommandSetImage::execute() {
	sv.getScene(sc)->replaceImage(fr, image);
	return this;
}

CommandSetImageFactory::CommandSetImageFactory(SceneVector& model) : sv(model) {
}

CommandSetImageFactory::~CommandSetImageFactory() {
}

Command* CommandSetImageFactory::create(Parameters& ps) {
	int32_t sc = ps.getInteger(0, sv.sceneCount() - 1);
	int32_t fr = ps.getInteger(0, sv.frameCount(sc) - 1);
	std::string path;
	ps.getString(path);
	TemporaryWorkspaceFile twf(path.c_str());
	return new CommandSetImage(sv, sc, fr, twf);
}
