/***************************************************************************
 *   Copyright (C) 2005 by Bjoern Erik Nilsen & Fredrik Berg Kjoelstad     *
 *   bjoern.nilsen@bjoernen.com     & fredrikbk@hotmail.com                *
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

// This widget is created from a widget in an example program for Qt. Available at
// http://www.libsdl.org/cvs/qtSDL.tar.gz

#include "frameview.h"
#include "src/technical/grabber/commandlinegrabber.h"
#include "src/foundation/preferencestool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qsize.h>
#if defined(Q_WS_X11)
#include <X11/Xlib.h>
#endif
#include <qmessagebox.h>


FrameView::FrameView(QWidget *parent, const char *name, int playbackSpeed)
 : VideoView(parent, name, WNoAutoErase)
{
	Logger::get().logDebug("FrameView is attatched to the model"
		" and the model to FrameView");
	
	screen       = NULL;
	videoSurface = NULL;
	widthConst  = 4;
	heightConst = 3;
	lastMixCount = 2;
	lastViewMode = 0;
	mode = 0;
	activeFrame = 0;
	this->playbackSpeed = PreferencesTool::get()->getPreference("fps", playbackSpeed);
	facade = DomainFacade::getFacade();
	
	char tmp[256];
	snprintf(tmp, 256, "%s/.stopmotion/capturedfile.jpg", getenv("HOME"));
	capturedImg = new char[strlen(tmp) + 1];
	strcpy(capturedImg, tmp);
	
	isPlayingVideo = false;
	grabber = NULL;
	grabThread = NULL;
	
	connect( &grabTimer, SIGNAL(timeout()), 
			this, SLOT(redraw()) );
	
	connect( &playbackTimer, SIGNAL(timeout()),
			this, SLOT(nextPlayBack()) );
	
	atexit(SDL_Quit);
	setNormalRatio();
}


FrameView::~FrameView()
{
	//off();
	SDL_FreeSurface(videoSurface);
	videoSurface = NULL;
	SDL_FreeSurface(screen);
	screen = NULL;

	for(unsigned int i=0; i<imageBuffer.size(); i++) {
		SDL_FreeSurface(imageBuffer[i]);
	}
	imageBuffer.clear();
	
	delete grabber;
	grabber = NULL;
	
	delete [] capturedImg;
	capturedImg = NULL;
}


void FrameView::setWidescreenRatio()
{
	widthConst  = 16;
	heightConst = 9;
	float minWidth = 300 * (16 / 9);
	this->setMinimumSize( (int)minWidth, 300);
	this->update();
}


void FrameView::setNormalRatio()
{
	widthConst  = 4;
	heightConst = 3;
	this->setMinimumSize(400, 300);
	this->update();
}


// abstract functions that must be implemented, but who aren't needed in
//this particular widget.
void FrameView::updateAdd(const vector<char*>&, unsigned int, Frontend*) {}
void FrameView::updateRemove(unsigned int, unsigned int) {}
void FrameView::updateMove(unsigned int, unsigned int, unsigned int) {}


void FrameView::updateNewActiveFrame(int frameNumber)
{
	if (frameNumber > -1) {
		setActiveFrame(frameNumber);
	}
	else {
		SDL_FreeSurface(videoSurface);
		videoSurface = NULL;
		this->repaint();
	}
}


void FrameView::updatePlayFrame(int frameNumber)
{
	if (frameNumber > -1) {
		setActiveFrame(frameNumber);
	}
	else {
		SDL_FreeSurface(videoSurface);
		videoSurface = NULL;
		this->repaint();
	}
}


void FrameView::resizeEvent(QResizeEvent*)
{	
	char variable[64];
	// We could get a resize event at any time, so clean previous mode
	screen = NULL;
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	// Set the new video mode with the new window size
	sprintf(variable, "SDL_WINDOWID=0x%lx", winId());
	putenv(variable);
	if ( SDL_InitSubSystem(SDL_INIT_VIDEO) < 0 ) {
		char msg[80] = "Unable to init SDL: ";
		strcat( msg, SDL_GetError() );
		Logger::get().logFatal(msg);
    }
	
	screen = SDL_SetVideoMode(width(), height(), 0, 0);
	if (!screen) {
		char msg[80] = "Unable to set vide mode: ";
		strcat( msg, SDL_GetError() );
		Logger::get().logFatal(msg);
	}
}

//#define diff_rollback
void FrameView::paintEvent(QPaintEvent *)
{
	//Are static for efficiency.
	#ifdef diff_rollback
 	static Frame *f = NULL;
	#endif
	static SDL_Surface *frameSurface = NULL;
	int activeFrame = -1;
	
#if defined(Q_WS_X11)
	// Make sure we're not conflicting with drawing from the Qt library
	XSync(QPaintDevice::x11Display(), FALSE);
#endif
	if (screen) {
		SDL_FreeSurface(screen);
		SDL_FillRect(screen, NULL, 0);
		
		if ( videoSurface ) {
			SDL_Rect dst;
			dst.x = (screen->w - videoSurface->w) / 2;
			dst.y = (screen->h - videoSurface->h) / 2;
			dst.w = videoSurface->w;
			dst.h = videoSurface->h;
			SDL_BlitSurface(videoSurface, NULL, screen, &dst);
			if(isPlayingVideo) {
				//SDL_Surface *frameSurface;
				activeFrame = facade->getActiveFrameNumber();
				switch (mode)
				{
					//Image mixing
					case 0:
					{
						//Define for rolling back to some old code for mixing. 
						//Provided in a transition phase
						#ifdef mix_rollback
						for(int i = 1; (i<=mixCount) && (activeFrame-i >= -1); i++) {
						
							//This test is needed for when the user closes scenes
							//when the camera is on.
							f = facade->getFrame(activeFrame+1-i);
							if(f == NULL) {
								break;
							}
							frameSurface = IMG_Load( f->getImagePath() );
							
							if(frameSurface != NULL) {
								SDL_Rect dst2;
								dst2.x = (screen->w - frameSurface->w) / 2;
								dst2.y = (screen->h - frameSurface->h) / 2;
								dst2.w = frameSurface->w;
								dst2.h = frameSurface->h;
								SDL_SetAlpha(frameSurface, SDL_SRCALPHA, 80/i);
								SDL_BlitSurface(frameSurface, NULL , screen, &dst2);
								SDL_FreeSurface(frameSurface);
							}
						}
						#endif
						
						#ifndef mix_rollback
						//Buffering here i the paint function is kind of ugly, but there
						//is good reasons for it being like this. However, if someone can
						//find a prettier way the code will be gratefull.
						if(activeFrame != this->activeFrame || lastMixCount != mixCount
								|| lastViewMode != mode) {
							for(unsigned int i=0; i<imageBuffer.size(); i++) {
								SDL_FreeSurface(imageBuffer[i]);
							}
							imageBuffer.clear();
							
							for(int i = activeFrame, j=0; i>=0 && j < mixCount; i--, j++) {
								imageBuffer.insert(imageBuffer.begin()+j, 
										IMG_Load(facade->getFrame(i)->getImagePath()));
							}
							this->activeFrame = activeFrame;
							lastMixCount = mixCount;
							lastViewMode = mode;
						}
						
						
						for(unsigned int i=0; i<imageBuffer.size(); ++i) {
							frameSurface = imageBuffer[i];
							
							if(frameSurface != NULL) {
								SDL_Rect dst2;
								dst2.x = (screen->w - frameSurface->w) / 2;
								dst2.y = (screen->h - frameSurface->h) / 2;
								dst2.w = frameSurface->w;
								dst2.h = frameSurface->h;
								SDL_SetAlpha(frameSurface, SDL_SRCALPHA, 80/(i+1));
								SDL_BlitSurface(frameSurface, NULL , screen, &dst2);
							}
						}
						#endif
						
						break;
					}
					//Image differentiating
					case 1: 
					{
						//Define for rolling back to some old code for diffing. 
						//Provided in a transition phase
						#ifdef diff_rollback
						f = facade->getFrame(activeFrame);
						if(f != NULL) {
							frameSurface = IMG_Load(f->getImagePath());
							
							if(frameSurface != NULL) {
								SDL_Surface *tmp = differentiateSurfaces(videoSurface, 
										frameSurface);
								SDL_Rect dst;
								dst.x = (screen->w - tmp->w) / 2;
								dst.y = (screen->h - tmp->h) / 2;
								dst.w = tmp->w;
								dst.h = tmp->h;
								SDL_BlitSurface(tmp, NULL, screen, &dst);
								SDL_FreeSurface(tmp);
								SDL_FreeSurface(frameSurface);
							}
						}
						#endif
						
						#ifndef diff_rollback
						//Buffering here i the paint function is kind of ugly, but there
						//is good reasons for it being like this. However, if someone can
						//find a prettier way the code will be gratefull.
						if(activeFrame != this->activeFrame || lastViewMode != mode) {
							for(unsigned int i=0; i<imageBuffer.size(); i++) {
								SDL_FreeSurface(imageBuffer[i]);
							}
							imageBuffer.clear();
							
							if(activeFrame >= 0) {
								imageBuffer.insert(imageBuffer.begin(), 
										IMG_Load(facade->getFrame(activeFrame)->
										getImagePath()));
							}
							
							this->activeFrame = activeFrame;
							lastViewMode = mode;	
						}
							
						if(activeFrame >= 0) {
							SDL_Surface *tmp = differentiateSurfaces(videoSurface, 
									imageBuffer[0]);
							SDL_Rect dst;
							dst.x = (screen->w - tmp->w) / 2;
							dst.y = (screen->h - tmp->h) / 2;
							dst.w = tmp->w;
							dst.h = tmp->h;
							SDL_BlitSurface(tmp, NULL, screen, &dst);
							SDL_FreeSurface(tmp);
						}
						#endif
						
						break;
					}
					//The playback is handled another way
				}
			}
		}
		SDL_Flip(screen);
	}
}


void FrameView::setActiveFrame(int frameNumber)
{
	Logger::get().logDebug("Setting new active frame in FrameView");
 	const char *fileName = DomainFacade::getFacade()->
			getFrame(frameNumber)->getImagePath();
	
	if (videoSurface) {
		SDL_FreeSurface(videoSurface);
	}
	Logger::get().logDebug("Loading image");
	videoSurface = IMG_Load(fileName);
	Logger::get().logDebug("Loading image finished");
	this->update();
}


void FrameView::updateClear()
{
	SDL_FreeSurface(videoSurface);
	videoSurface = NULL;
	this->update();
}


void FrameView::updateNewScene(int)
{

}


void FrameView::updateRemoveScene(int)
{
}


void FrameView::updateNewActiveScene(int, vector<char*>, Frontend*)
{
}

///@todo Fix the way it checs for change. Only applies to gimp.
void FrameView::updateAnimationChanged(int frameNumber)
{
	setActiveFrame(frameNumber);
}


void FrameView::updateMoveScene( int, int )
{

}


bool FrameView::on()
{
	this->isPlayingVideo = true;
	
	PreferencesTool *prefs = PreferencesTool::get();
	int activeCmd = prefs->getPreference("activedevice", 0);
	const char* prepoll = prefs->getPreference(
			QString("importprepoll%1").arg(activeCmd).ascii(), "");
	const char* startDeamon = prefs->getPreference(
			QString("importstartdeamon%1").arg(activeCmd).ascii(), "");
	const char* stopDeamon = prefs->getPreference(
			QString("importstopdeamon%1").arg(activeCmd).ascii(), "");
	
	bool isProcess = (strcmp(startDeamon, "") == 0) ? false : true;
	
	this->grabber = new CommandLineGrabber(
			capturedImg, prepoll, startDeamon, stopDeamon, isProcess);
	this->initCompleted();
	
	if ( prefs->getPreference("numberofimports", 1) > 0 ) {
		//If the grabber is running in it's own process we use a timer.
		if(grabber->isGrabberProcess() == true) {
			if(grabber->init()) {
				grabTimer.start(200);
			}
			else {
				QMessageBox::warning(this, tr("Warning"), tr(
					"Grabbing failed. This may happen if you try\n"
					"to grab from an invalid device. Please check\n"
					"your grabber settings in the preferences menu."),
					QMessageBox::Ok,
					QMessageBox::NoButton, 
					QMessageBox::NoButton);
				return false;
			}
		}
		//Otherwise a thread is needed
		else {
			grabThread = new ImageGrabThread(this, grabber);
			grabThread->start();
			grabThread->wait(500);
			
			if (grabThread->wasGrabbingSuccess() == false) {
				QMessageBox::warning(this, tr("Warning"), tr(
					"Grabbing failed. This may happen if you try\n"
					"to grab from an invalid device. Please check\n"
					"your grabber settings in the preferences menu."),
					QMessageBox::Ok,
					QMessageBox::NoButton, 
					QMessageBox::NoButton);
				return false;
			}
		}
	}
	else {
		QMessageBox::warning(this, tr("Warning"), tr(
				"You have to define an image grabber to use.\n"
				"This can be set in the preferences menu."),
				QMessageBox::Ok,
				QMessageBox::NoButton, 
				QMessageBox::NoButton);
		return false;
	}
	
	//To get the draw function to buffer for onionskinning
	lastMixCount = -1;
	
// 	setViewMode(0);
	
	return true;
}


void FrameView::off()
{
	if(grabber->isGrabberProcess() == true) {
		grabber->tearDown();
		grabTimer.stop();
		playbackTimer.stop();
	}
	else {
		grabThread->terminate();
		grabThread->wait();
		delete grabThread;
		grabThread = NULL;
	}
	
	for(unsigned int i=0; i<imageBuffer.size(); i++) {
		SDL_FreeSurface(imageBuffer[i]);
	}
	imageBuffer.clear();
	
	delete grabber;
	grabber = NULL;
		
	this->isPlayingVideo = false;
	
	SDL_FreeSurface(videoSurface);
	videoSurface = NULL;
	this->update();
}


void FrameView::redraw()
{
	if(videoSurface) {
		SDL_FreeSurface(videoSurface);
		videoSurface = NULL;
	}
	videoSurface = IMG_Load(capturedImg);
	this->update();
}


void FrameView::nextPlayBack()
{
	//Don't you just love static function variables ^^
	static unsigned int i = 0;
	
	//Need to check the that there is an active scene before checking
	//what its size is. Therefore I cant use &&.

	if( facade->getActiveSceneNumber() >= 0 ) {
		if( (int)i < (mixCount) && i < (unsigned int)facade->getActiveFrameNumber()+1 ) {
			if(videoSurface) {
				SDL_FreeSurface(videoSurface);
				videoSurface = NULL;
			}
			
			if(facade->getActiveFrameNumber() <= (mixCount)) {
				videoSurface = IMG_Load(DomainFacade::getFacade()->getFrame(i++)->getImagePath());
			}
			else {
				videoSurface = IMG_Load(facade->getFrame(
						facade->getActiveFrameNumber()-(mixCount)+i++)->getImagePath());
			}
				
			this->update();
			//Exit from function/skip redraw(). This is better than having a bool which is
			//set because this is a play function run "often".
			return;
		}
	}
	
	//This code is run if one of the two above tests fail. Can't be an else because
	//then I would have to have two such elses, and I think the return is better.
	i=0;
	redraw();
}


bool FrameView::setViewMode(int mode)
{
	//Going into playback mode.
	if(mode == 2 && this->mode != 2) {
		if(grabber->isGrabberProcess()) {
			grabTimer.stop();
			playbackTimer.start(1000/playbackSpeed);
		}
		else {
			return false;
		}
	}
	//Going out of playback mode.
	else if(mode != 2 && this->mode == 2) {
		if(grabber->isGrabberProcess()) {
			playbackTimer.stop();
			grabTimer.start(200);
		}
	}
	
	this->mode = mode;
	return true;
}


int FrameView::getViewMode()
{
	return mode;
}


void FrameView::setPlaybackSpeed(int playbackSpeed)
{
	this->playbackSpeed = playbackSpeed;
	if(playbackTimer.isActive()) {
		playbackTimer.changeInterval(1000/playbackSpeed);
	}
}


//The only thing left which is a little expensive is the MapRGB function.
SDL_Surface* FrameView::differentiateSurfaces(SDL_Surface *s1, SDL_Surface *s2)
{
	int width = s2->w;
	int height = s2->h;
	
	SDL_Surface *diffSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32,
								0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
	
	//Lock the surfaces before working with the pixels
	SDL_LockSurface( s1 );
	SDL_LockSurface( s2 );
	SDL_LockSurface( diffSurface );
	
	//Pointers to the first byte of the first pixel on the input surfaces.
	Uint8 *p1 = (Uint8 *)s1->pixels;
	Uint8 *p2 = (Uint8 *)s2->pixels;
	
	//Pointers to the first pixel on the resulting surface
	Uint32 *pDiff =  (Uint32 *)diffSurface->pixels;
	
	SDL_PixelFormat fDiff = *diffSurface->format;
	Uint32  differencePixel;
	Uint8 dr, dg, db;
	int offset = 0, pixelOffset = 0;
	int i, j;
	
	//Goes through the surfaces as one-dimensional arrays.
	for(i=0; i<height; ++i) {
		for(j=1; j<width; ++j) {
			//px[offset] is the red value of surface x, px[offset+1] the green, etc.
			dr = abs(p1[offset] - p2[offset]);
			dg = abs(p1[offset+1] - p2[offset+1]);
			db = abs(p1[offset+2] - p2[offset+2]);
			
			differencePixel = SDL_MapRGB(&fDiff, dr, dg, db);
			
			pDiff[pixelOffset++] = differencePixel;
			offset += 3;
		}
		++pixelOffset;
		offset += 3;
	}
	
	//Unlock the surfaces for displaying them.
	SDL_UnlockSurface( s1 );
	SDL_UnlockSurface( s2 );
	SDL_UnlockSurface( diffSurface );
	
	return diffSurface;
}


void FrameView::capture()
{
	#ifndef rollback
	/*
	imageBuffer.insert(imageBuffer.begin() + 0, 
			IMG_Load(facade->getFrame(facade->getActiveFrameNumber())->getImagePath()));
	
	if(facade->getActiveFrameNumber() > 0) {
		SDL_FreeSurface(imageBuffer.back());
		//delete imageBuffer.back();
		imageBuffer.pop_back();
	}*/
	
	#endif
}


