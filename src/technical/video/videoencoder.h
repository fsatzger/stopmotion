/***************************************************************************
 *   Copyright (C) 2005-2008 by Bjoern Erik Nilsen & Fredrik Berg Kjoelstad*
 *   bjoern.nilsen@bjoernen.com & fredrikbk@hotmail.com                    *
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
#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

/**
 * Class for gathering information needed to export a stopmotion project to
 * a video file.
 * @author Bjoern Erik Nilsen & Fredrik Berg Kjoelstad
 */
class VideoEncoder
{
public:
	VideoEncoder();
	~VideoEncoder();
	
	/**
	 * Gets the command line registered by the user in the preferences menu.
	 * This is the command which should be used to start the encoder.
	 * @return the start command
	 */
	const char* getStartCommand();
	
	/**
	 * Gets the command line registered by the user in the preferences menu.
	 * This is the command which should be used to stop the encoder.
	 * @return the stop command
	 */
	const char* getStopCommand();
	
	/**
	 * Gets the output file generated by the encoder.
	 * @return the output file
	 */
	const char* getOutputFile();
	
	/**
	 * Sets the start command to be used for starting the encoder and generate
	 * a video file.
	 * @param command the start command to be used for generating the video file
	 */
	void setStartCommand(const char* command);
	
	/**
	 * Sets the stop command to be used for stopping the encoder.
	 * @param command the stop command to be used in case the user want to 
	 * cancel the export
	 */
	void setStopCommand(const char* command);
	
	/**
	 * Sets the file which should be generated by the encoder.
	 */
	void setOutputFile(const char* file);
	
	/**
	 * Checks if the registered encoder is a valid one. Checks if the encoder can
	 * be executed, the output file is a valid file etc.
	 * @return true if valid, false otherwise
	 */
	bool isValid();
	
private:
	char *startCommand;
	char *stopCommand;
	char *outputFile;
};

#endif
