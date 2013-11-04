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
#include "src/application/modelhandler.h"

#include "src/presentation/frontends/qtfrontend/mainwindowgui.h"
#include "src/domain/domainfacade.h"

#include <QChar>
#include <QProcess>


ModelHandler::ModelHandler ( QObject *parent, QStatusBar *sb, FrameBar *frameBar,
		ExternalChangeMonitor *changeMonitor, char *lastVisitedDir, const char *name )
		: QObject(parent), frameBar(frameBar), statusBar(sb),
		lastVisitedDir(lastVisitedDir), changeMonitor(changeMonitor) {
	fileDialog = NULL;
	removeFramesButton = NULL;
	setObjectName(name);
}


ModelHandler::~ModelHandler() {
}


void ModelHandler::setRemoveFramesButton(QPushButton* removeFramesButton) {
	this->removeFramesButton = removeFramesButton;
}


void ModelHandler::chooseFrame() {
	fileDialog = new QFileDialog((MainWindowGUI*)parent(), tr("Choose frames to add"),
			QString::fromLocal8Bit(lastVisitedDir));
	QStringList filters;
	filters << "Images (*.png *.jpg *.jpeg  *.gif *.PNG *.JPG *.JPEG *.GIF)"
			<< "Joint Photographic Ex. Gr. (*.jpg *.jpeg *.JPG *.JPEG)"
			<< "Portable Network Graphics (*.png *.PNG)"
			<< "GIMP native (*.xcf *.XCF)"
			<< "Tagged Image File Format (*.tif *.TIF)"
			<< "Windows Bitmap (*.bmp *.BMP)"
			<< "TrueVision Targa (*.tga *.TGA)"
			<< "Portable Anymap (*.pnm *.PNM)"
			<< "X11 Pixmap (*.xpm *.XPM)"
			<< "ZSoft IBM PC Paintbrush (*.pcx *.PCX)"
			<< "CompuServe Graph. Interch. Format (*.gif *.GIF)"
			<< "Interleaved Bitmap (*.lbm *.iff *.LBM *.IFF)"
			<< "All files (*)";
	fileDialog->setFilters(filters);
	fileDialog->setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog->setFileMode(QFileDialog::ExistingFiles);

	//PicturePreview* p = new PicturePreview(fileDialog);

	QObject::connect( fileDialog, SIGNAL(filesSelected (const QStringList &)),
			this, SLOT(addFrames(const QStringList &)) );

	fileDialog->show();
}


// TODO: implement with std strings
void ModelHandler::addFrames(const QStringList & fileNames) {
	Logger::get().logDebug("addFrames in modelhandler");
	QStringList names(fileNames);

	changeMonitor->suspendMonitor();
	// the fileDialog pointer is NULL when adding of frames is
	// done by drag 'n drop
	if ( fileDialog != NULL ) {
		fileDialog->hide();
		strcpy( lastVisitedDir, fileDialog->directory().path().toLocal8Bit().constData() );
	}

	if ( !names.isEmpty() ) {
		vector<const char*> fNames;
		QStringList::Iterator it = names.begin();
		while (it != names.end() ) {
			QByteArray fileName = it->toLocal8Bit();
			char *f = new char[fileName.length() + 1];
			strcpy(f, fileName.constData());
			fNames.push_back(f);
			++it;
		}
		// trim to size :)
		vector<const char*>(fNames).swap(fNames);
		int scene = frameBar->getActiveScene();
		int frame = frameBar->getActiveFrame() + 1;
		if (scene < 0) {
			scene = 0;
		}
		DomainFacade* facade = DomainFacade::getFacade();
		int frameCount = facade->getSceneSize(scene);
		if (frameCount < frame)
			frame = frameCount;
		if (frame < 0)
			frame = 0;
		facade->addFrames(scene, frame, fNames);

		// Cleanup
		for (unsigned i = 0; i < fNames.size(); ++i) {
			delete [] fNames[i];
			fNames[i] = NULL;
		}

		emit modelChanged();
	}
	changeMonitor->resumeMonitor();
}


void ModelHandler::addFrame( const QString &fileName ) {
	if (fileDialog != NULL) {
		fileDialog->hide();
		strcpy( lastVisitedDir, fileDialog->directory().path().toLocal8Bit().constData() );
	}

	QStringList fileNames;
	fileNames.push_back(fileName);
	this->addFrames(fileNames);
}


void ModelHandler::removeFrames() {
	if (removeFramesButton->isEnabled()) {
		int selectionFrame = frameBar->getSelectionAnchor();
		int activeScene = frameBar->getActiveScene();
		int activeFrame = frameBar->getActiveFrame();
		int lowend = (selectionFrame < activeFrame ) ? selectionFrame : activeFrame;
		int highend = (selectionFrame > activeFrame ) ? selectionFrame : activeFrame;

		DomainFacade::getFacade()->removeFrames(activeScene, lowend,
				highend - lowend + 1);
		statusBar->showMessage( tr("Removed the selected frame"), 2000 );
	}
}


void ModelHandler::newScene() {
	int activeScene = frameBar->getActiveScene();

	if (activeScene >= 0) {
		DomainFacade::getFacade()->newScene(activeScene+1);
	}
	else {
		int numScenes = DomainFacade::getFacade()->getNumberOfScenes();
		if(numScenes > 0) {
			DomainFacade::getFacade()->newScene(numScenes);
		}
		else {
			DomainFacade::getFacade()->newScene(activeScene+1);
		}
	}
	emit modelChanged();
}


void ModelHandler::removeScene() {
	DomainFacade::getFacade()->removeScene(frameBar->getActiveScene());
}


/*!
    \fn ModelHandler::editCurrentFrame()
 */
int ModelHandler::editCurrentFrame() {
	const char *gimpCommand = Util::checkCommand("gimp");
	if (!gimpCommand) {
		QMessageBox::warning(static_cast<MainWindowGUI *>(parent()), tr("Warning"),
			tr("You do not have Gimp installed on your system"),
			QMessageBox::Ok, Qt::NoButton, Qt::NoButton);
		return 1;
	}

	// Determine the active scene and active frame.
	int activeScene = frameBar->getActiveScene();
	int activeFrame = frameBar->getActiveFrame();

	if (activeScene < 0 || activeFrame < 0) {
		QMessageBox::warning(static_cast<MainWindowGUI *>(parent()), tr("Warning"),
			tr("There is no active frame to open"),
			QMessageBox::Ok, Qt::NoButton, Qt::NoButton);
		return 1;
	}

	const char *path = DomainFacade::getFacade()
			->getImagePath(activeScene, activeFrame);

	QStringList argList;
	// arg0 are the options, and arg1 is the path of the frame.
	// Start Gimp without splash screen.
	argList.append(QLatin1String("--no-splash"));
	argList.append(QString::fromLocal8Bit(path));

	QProcess process;
	if (!process.startDetached(QLatin1String(gimpCommand), argList)) {
		QMessageBox::warning(static_cast<MainWindowGUI *>(parent()), tr("Warning"),
			tr("Failed to start Gimp!"),
			QMessageBox::Ok, Qt::NoButton, Qt::NoButton);
		return 1;
	}

	return 0;
}
