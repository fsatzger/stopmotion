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

#ifndef COMMANDSETIMAGE_H_
#define COMMANDSETIMAGE_H_

#include "command.h"
#include "src/domain/animation/workspacefile.h"

class AnimationImpl;
class ErrorHandler;

class CommandSetImage : public Command {
	friend class CommandDuplicateImage;
	AnimationImpl& sv;
	int32_t sc;
	int32_t fr;
	WorkspaceFile image;
public:
	CommandSetImage(AnimationImpl& model, int32_t scene, int32_t frame,
			WorkspaceFile& w);
	~CommandSetImage();
	Command* execute();
};

class CommandSetImageFactory : public CommandFactory {
	AnimationImpl& sv;
public:
	CommandSetImageFactory(AnimationImpl& model);
	~CommandSetImageFactory();
	Command* create(Parameters& ps, ErrorHandler& e);
};

#endif
