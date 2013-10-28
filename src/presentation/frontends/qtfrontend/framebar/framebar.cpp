/***************************************************************************
 *   Copyright (C) 2005-2013 by Linuxstopmotion contributors;              *
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
#include "src/presentation/frontends/qtfrontend/framebar/framebar.h"

#include "src/domain/domainfacade.h"
#include "src/presentation/frontends/qtfrontend/framebar/framethumbview.h"
#include "src/presentation/frontends/qtfrontend/framebar/scenethumbview.h"
#include "src/presentation/frontends/qtfrontend/framebar/scenearrowbutton.h"
#include "framepreferencesmenu.h"

#include <QImage>
#include <QFrame>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QResizeEvent>
#include <QDropEvent>
#include <QVBoxLayout>
#include <QPixmap>
#include <QScrollBar>
#include <QImageReader>
#include <QDebug>

static QImage tryReadImage(const char *filename) {
    if (!filename) {
        qWarning() << "Couldn't read image: Invalid file name";
        return QImage();
    }

    QImageReader imageReader(QString::fromLocal8Bit(filename));
    const QImage image = imageReader.read();
    if (!image.isNull())
        return image;

    if (imageReader.error() != QImageReader::UnsupportedFormatError
        && imageReader.error() != QImageReader::InvalidDataError) {
        qWarning() << "Couldn't read image:" << imageReader.errorString();
        return image;
    }

    // At this point we most likely failed to read the image because the suffix
    // of the file was different from the actual image format.
    // Loop through all the supported formats and see if we can find a match.
    const QList<QByteArray> supportedImageFormats = QImageReader::supportedImageFormats();
    for (int i = 0; i < supportedImageFormats.size(); ++i) {
        QImageReader anotherImageReader(filename);
        anotherImageReader.setFormat(supportedImageFormats.at(i));
        const QImage anotherImage = anotherImageReader.read();
        if (!anotherImage.isNull())
            return anotherImage;
    }

    qWarning() << "Couldn't read image:" << imageReader.errorString();
    return image;
}

FrameBar::FrameBar(QWidget *parent)
	: QScrollArea(parent), activeFrameObserver(0) {
	preferencesMenu = 0;
	activeFrame = -1;
	activeScene = -1;
	movingScene = 0;
	openingScene = false;
	selecting = false;
	selectionFrame = -1;
	scrollDirection = 0;

	lowerScrollAreaX = this->x() + FRAME_WIDTH;
	upperScrollAreaX = this->width() - FRAME_WIDTH;

	lowerAccelScrollAreaX = lowerScrollAreaX - (FRAME_WIDTH >> 1);
	upperAccelScrollAreaX = upperScrollAreaX + (FRAME_WIDTH >> 1);

	minScrollAreaX = lowerScrollAreaX - FRAME_WIDTH + 20;
	maxScrollAreaX = upperScrollAreaX + FRAME_WIDTH - 20;

	minScrollAreaY = this->y() + 20;
	maxScrollAreaY = this->y() + FRAME_HEIGHT - 20;

	scrollTimer = new QTimer(this);
	connect(scrollTimer, SIGNAL(timeout()), this, SLOT(scroll()));
	scrollBar = horizontalScrollBar();

	mainWidget = new QWidget;
	mainWidget->setMinimumHeight(FRAME_HEIGHT);
	mainWidget->setMaximumHeight(FRAME_HEIGHT);
	mainWidget->setMinimumWidth(FRAME_WIDTH);
	mainWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

	setWidget(mainWidget);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	setMaximumHeight(FRAME_HEIGHT + horizontalScrollBar()->height() - 5);
	setMinimumHeight(FRAME_HEIGHT + horizontalScrollBar()->height() - 5);
	setBackgroundRole(QPalette::Dark);
	setAcceptDrops(true);

	Logger::get().logDebug("FrameBar is attatched to the model");
	DomainFacade::getFacade()->attach(this);
}


FrameBar::~FrameBar() {
}


void FrameBar::updateAdd(int scene, int index, int numFrames) {
	if (scene != activeScene) {
		Logger::get().logDebug("Adding in framebar");
		addFrames(index, numFrames);
		emit modelSizeChanged(DomainFacade::getFacade()->getModelSize());
	}
	updateNewActiveFrame(scene, index + numFrames - 1);
}


void FrameBar::updateRemove(int scene, int fromFrame, int toFrame) {
	if (scene == activeScene) {
		Logger::get().logDebug("Receiving notification about the removal of a frame in the model");
		removeFrames(fromFrame, toFrame);
		emit modelSizeChanged(DomainFacade::getFacade()->getModelSize());
	}
	updateNewActiveFrame(scene, fromFrame);
}


void FrameBar::updateMove(int fromScene, int fromFrame, int count,
		int toScene, int toFrame) {
	if (fromScene == activeScene) {
		if (toScene == activeScene) {
			Logger::get().logDebug("Moving in framebar");
			moveFrames(fromFrame, fromFrame + count - 1, toFrame);
		} else {
			Logger::get().logDebug("Moving from framebar");
			removeFrames(fromFrame, fromFrame + count - 1);
			emit modelSizeChanged(DomainFacade::getFacade()->getModelSize());
		}
	} else if (toScene == activeScene) {
		Logger::get().logDebug("Moving to framebar");
		addFrames(toFrame, count);
		emit modelSizeChanged(DomainFacade::getFacade()->getModelSize());
	}
}


void FrameBar::updateNewActiveFrame(int sceneNumber, int frameNumber) {
	setActiveScene(sceneNumber);
	setActiveFrame(frameNumber);

	if ( preferencesMenu->isVisible() ) {
		if (frameNumber >= 0) {
			showPreferencesMenu();
		}
		else {
			preferencesMenu->close();
		}
	}

	// For writing the frame number in the frame number display
	emit newActiveFrame( QString(tr("Frame number: ")) + QString("%1").arg(frameNumber + 1) );

	// For setting the value in the spinbox in the gotomenu
	emit newActiveFrame(frameNumber + 1);
}


void FrameBar::updateClear() {
	unsigned int size = thumbViews.size();
	for (unsigned int i = 0; i < size; ++i) {
		delete thumbViews[i];
	}
	thumbViews.clear();
	activeFrame = -1;
	activeScene = -1;
}


void FrameBar::updatePlayFrame(int, int) {}


void FrameBar::updateAnimationChanged(int sceneNumber, int frameNumber) {
	if (sceneNumber != activeScene)
		return;
	const char *path = DomainFacade::getFacade()->getImagePath(sceneNumber,
				frameNumber);
	if (!path)
		return;
	QImage scaled = tryReadImage(path).scaled(FRAME_WIDTH, FRAME_HEIGHT);
	int thumbNumber = frameNumber + activeScene + 1;
	thumbViews[thumbNumber]->setPixmap(QPixmap::fromImage(scaled));
	thumbViews[thumbNumber]->update();
}


void FrameBar::addFrames(int index, int numFrames) {
	//TODO as we're no longer allowing cancelling during this operation, we
	// should implement caching and lazy loading so that this isn't a problem.
	Logger::get().logDebug("Adding frames in framebar");

	DomainFacade* anim = DomainFacade::getFacade();
	int size = thumbViews.size();
	int from = index + activeScene + 1;
	int to = size - DomainFacade::getFacade()->getNumberOfScenes() + activeScene + 1;
	int moveDistance = numFrames - (activeScene + 1);

	// Move the frames behind the place we are inserting the new ones.
	for (int i = from; i < size; ++i) {
		thumbViews[i]->move(thumbViews[i]->x() + (FRAME_WIDTH + SPACE) * numFrames, 0 );
		if (i < to) {
			thumbViews[i]->setNumber(i + moveDistance);
		}
	}

	ThumbView *thumb = 0;

	// Adds the new frames to the framebar
	for (int i = 0; i != numFrames; ++i) {
		thumb = new FrameThumbView(this, 0, index + i);
		thumb->setMinimumSize(FRAME_WIDTH, FRAME_HEIGHT);
		thumb->setMaximumSize(FRAME_WIDTH, FRAME_HEIGHT);
		thumb->setScaledContents(true);
		const char* imagePath = anim->getImagePath(activeScene, index + i);
		if (imagePath) {
			thumb->setPixmap(QPixmap::fromImage(tryReadImage(imagePath)
					.scaled(FRAME_WIDTH, FRAME_HEIGHT)));
		}
		thumb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		thumb->setParent(mainWidget);
		thumb->move((FRAME_WIDTH + SPACE) * (index + activeScene + 1 + i), 0);
		thumb->show();

		//Sets the note icon on the respective frames.
		if (anim->getNumberOfSounds(activeScene, i) != 0) {
			thumb->setHasSounds(true);
		}

		thumbViews.insert(thumbViews.begin() + index + activeScene + 1 + i, thumb);
	}
	mainWidget->resize((FRAME_WIDTH + SPACE) * thumbViews.size() - SPACE, FRAME_HEIGHT);
}


void FrameBar::removeFrames(unsigned int fromFrame, unsigned int toFrame) {
	fromFrame += activeScene + 1;
	toFrame += activeScene + 1;

	unsigned int numFrames = thumbViews.size();

	// The frames to be deleted are between other frames
	if ( toFrame < numFrames - 1) {

		// Move all frames behind the deleted frames forward.
		unsigned stop = numFrames - DomainFacade::getFacade()->getNumberOfScenes() + activeScene + 1;
		for (unsigned k = toFrame + 1; k < numFrames; ++k) {
			thumbViews[k]->move(thumbViews[k]->x() - (toFrame - fromFrame + 1) * (FRAME_WIDTH + SPACE), 0);
			if (k < stop) {
				thumbViews[k]->setNumber(k - (toFrame - fromFrame + 2) - activeScene);
			}
		}
	}

	for (unsigned int i = fromFrame; i <= toFrame; ++i) {
		delete thumbViews[fromFrame];
		thumbViews.erase( thumbViews.begin() + fromFrame );
	}

	mainWidget->resize((FRAME_WIDTH + SPACE) * thumbViews.size(), FRAME_HEIGHT);
}


void FrameBar::moveFrames(int fromFrame, int toFrame, int movePosition) {
	fromFrame += activeScene + 1;
	toFrame += activeScene + 1;
	movePosition += activeScene + 1;

	if (movePosition < fromFrame) {
		for (int i = movePosition; i < fromFrame; ++i) {
			thumbViews[i]->move(thumbViews[i]->x() + (FRAME_WIDTH + SPACE) * (toFrame - fromFrame + 1), 0 );
			thumbViews[i]->setNumber(i + (toFrame - fromFrame) - activeScene);
		}

		for (int j = fromFrame; j <= toFrame; ++j) {
			thumbViews[j]->move( thumbViews[j]->x() - (FRAME_WIDTH + SPACE) * (fromFrame-movePosition), 0 );
			moveThumbView(j, j - (fromFrame - movePosition));
		}
	}
	else if (movePosition > fromFrame) {
		for (int i = toFrame + 1; i <= movePosition; ++i) {
			thumbViews[i]->move(thumbViews[i]->x() - (FRAME_WIDTH + SPACE) * (toFrame - fromFrame + 1), 0 );
			thumbViews[i]->setNumber(i - (toFrame - fromFrame + 2) - activeScene);
		}

		for (int j = fromFrame, k = toFrame; j <= toFrame; ++j, --k) {
			thumbViews[k]->move(thumbViews[k]->x() + (FRAME_WIDTH + SPACE) * (movePosition - toFrame), 0 );
			moveThumbView(k, k + (movePosition-toFrame));
		}
	}
}


void FrameBar::moveThumbView(int fromPosition, int toPosition) {
	ThumbView *f = thumbViews[fromPosition];
	f->setNumber(toPosition - (activeScene + 1));
	f->setSelected(false);
	thumbViews.erase(thumbViews.begin() + fromPosition);
	thumbViews.insert(thumbViews.begin() + toPosition, f);
}


void FrameBar::setActiveFrame(int frameNumber) {
	// If there is a frame to set as active
	if (frameNumber >= 0) {
		Logger::get().logDebug("Setting new active frame in FrameBar");

		int max = thumbViews.size() - 1;
		int thumbNumber = frameNumber + activeScene + 1;
		int from = activeFrame + activeScene + 1;
		int to = selectionFrame + activeScene + 1;
		int highend = (from < to) ? to : from;
		int lowend = (from > to) ? to : from;

		if (max < highend)
			highend = max;
		if (lowend < 0)
			lowend = 0;

		for (int i = lowend; i <= highend; ++i) {
			thumbViews[i]->setSelected(false);
		}

		if (0 <= thumbNumber && thumbNumber <= max)
			thumbViews[thumbNumber]->setSelected(true);
		ensureVisible(thumbNumber * (FRAME_WIDTH + SPACE) + FRAME_WIDTH/2,
				FRAME_HEIGHT/2, FRAME_WIDTH/2, FRAME_HEIGHT/2);
	}

	activeFrame = frameNumber;
	selectionFrame = frameNumber;
	selecting = false;
}


void FrameBar::setSelecting(bool selecting) {
	this->selecting = selecting;
}


bool FrameBar::isSelecting() const {
	return selecting;
}


void FrameBar::setSelection(int selectionFrame)
{
	this->selectionFrame = selectionFrame;
	selectionFrame += activeScene + 1;

	int activeFrame = this->activeFrame + activeScene + 1;
	if (selectionFrame >= activeFrame) {
		for (int i = activeFrame; i <= selectionFrame; ++i) {
			thumbViews[i]->setSelected(true);
		}
	}
	else if (this->selectionFrame < activeFrame) {
		for (int i = selectionFrame; i <= activeFrame; ++i) {
			thumbViews[i]->setSelected(true);
		}
	}
}


int FrameBar::getSelectionFrame() const
{
	return selectionFrame;
}


void FrameBar::setPreferencesMenu( FramePreferencesMenu * preferencesMenu )
{
	this->preferencesMenu = preferencesMenu;
}


void FrameBar::showPreferencesMenu()
{
	preferencesMenu->open();
}


void FrameBar::frameSoundsChanged()
{
	int activeFrame = getActiveFrame();
	int activeThumb = activeFrame + getActiveScene() + 1;

	int soundCount = DomainFacade::getFacade()->soundCount(
			getActiveScene(), activeFrame);
	if (0 < soundCount) {
		thumbViews[activeThumb]->setHasSounds(true);
	}
	else {
		thumbViews[activeThumb]->setHasSounds(false);
	}
}


void FrameBar::updateNewScene( int index )
{
	this->newScene(index);
}


void FrameBar::newScene(int index)
{
	Logger::get().logDebug("Adding new scene thumb to framebar");

	if (index > 0)  {
		unsigned from = index + (DomainFacade::getFacade()->getSceneSize(index - 1));
		unsigned numThumbs = thumbViews.size();
		for (unsigned i = from; i < numThumbs; i++) {
			thumbViews[i]->move(thumbViews[i]->x() + (FRAME_WIDTH + SPACE), 0 );
			thumbViews[i]->setNumber(thumbViews[i]->getNumber() + 1);
		}
	}

	ThumbView *thumb = new SceneThumbView(this, 0, index, "scene");
	setOpeningScene(false);
	thumb->setMinimumSize(FRAME_WIDTH, FRAME_HEIGHT);
	thumb->setMaximumSize(FRAME_WIDTH, FRAME_HEIGHT);
	thumb->setScaledContents(true);
	thumb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	thumb->setParent(mainWidget);

	if (getActiveScene() >= 0) {
		index = (index > 0) ? index + DomainFacade::getFacade()->getSceneSize(index - 1) : index;
	}

	thumb->move((FRAME_WIDTH + SPACE) * index, 0);
	thumb->show();
	thumbViews.insert(thumbViews.begin() + index, thumb);

	emit modelSizeChanged(DomainFacade::getFacade()->getModelSize());
}


void FrameBar::updateRemoveScene( int sceneNumber )
{
	this->removeScene(sceneNumber);
}


void FrameBar::removeScene(int sceneNumber)
{
	int delThumb = sceneNumber;

	if (sceneNumber > activeScene && sceneNumber > 0) {
		delThumb += DomainFacade::getFacade()->getSceneSize(activeScene);
	}

	delete thumbViews[delThumb];
	thumbViews.erase(thumbViews.begin() + delThumb);

	unsigned int numFrames = thumbViews.size();
	for (unsigned int i = delThumb; i < numFrames; ++i) {
		thumbViews[i]->move(thumbViews[i]->x() - (FRAME_WIDTH + SPACE), 0 );
		if (strcmp(thumbViews[i]->objectName().toLatin1().constData(), "scene") == 0) {
			thumbViews[i]->setNumber(thumbViews[i]->getNumber() - 1);
		}
	}

	if (activeScene > sceneNumber) {
		activeScene -= 1;
	}
}


void FrameBar::updateMoveScene( int sceneNumber, int movePosition )
{
	this->moveScene(sceneNumber, movePosition);
}


void FrameBar::moveScene(int sceneNumber, int movePosition)
{
	if (thumbViews.size() <= 0)
		return;
	if (movePosition < sceneNumber) {
		for (int i = movePosition; i < sceneNumber; ++i) {
			thumbViews[i]->move(thumbViews[i]->x() + (FRAME_WIDTH + SPACE), 0 );
			thumbViews[i]->setNumber(thumbViews[i]->getNumber() + 1);
		}
	}
	else {
		for (int i = sceneNumber + 1; i <= movePosition; ++i) {
			thumbViews[i]->move(thumbViews[i]->x() - (FRAME_WIDTH + SPACE), 0 );
			thumbViews[i]->setNumber(thumbViews[i]->getNumber()-1);
		}
	}

	ThumbView *const tv = thumbViews[sceneNumber];
	tv->move(tv->x() - (FRAME_WIDTH + SPACE) * (sceneNumber - movePosition), 0 );
	tv->setNumber(movePosition);

	thumbViews.erase(thumbViews.begin() + sceneNumber);
	thumbViews.insert(thumbViews.begin() + movePosition, tv);
}


void FrameBar::setActiveScene(int sceneNumber) {
	DomainFacade* anim = DomainFacade::getFacade();
	int maxScene = anim->getNumberOfScenes() - 1;
	if (sceneNumber < maxScene) {
		sceneNumber = maxScene;
	}
	if (sceneNumber == activeScene)
		return;
	if (activeScene >= 0) {
		this->removeFrames(0, anim->getSceneSize(activeScene) - 1);
		if (activeScene < thumbViews.size())
			thumbViews[activeScene]->setOpened(false);
	}

	this->activeScene = sceneNumber;

	if (sceneNumber >= 0) {
		thumbViews[activeScene]->setOpened(true);
		int count = anim->getSceneSize(activeScene);
		if (count > 0) {
			this->addFrames(0, count);
			setActiveFrame(0);
		}
		else {
			setActiveFrame(-1);
		}
	}

	updateObserver();

	ensureVisible((FRAME_WIDTH + SPACE) * thumbViews.size() + FRAME_WIDTH, FRAME_HEIGHT);
	emit newMaximumValue(DomainFacade::getFacade()->getSceneSize(activeScene));
}


int FrameBar::getMovingScene() const
{
	return movingScene;
}


void FrameBar::setMovingScene(int movingScene)
{
	this->movingScene = movingScene;
}


// TODO: Check for other mime types as well
void FrameBar::dragEnterEvent(QDragEnterEvent *event)
{
	if ( event->mimeData()->hasUrls() ) {
		event->accept();
	}
	else {
		event->ignore();
	}
}


void FrameBar::dropEvent(QDropEvent *event)
{
	scrollTimer->stop();
	scrollDirection = 0;

	int index = (event->pos().x() + -mainWidget->pos().x()) / (FRAME_WIDTH + SPACE);
	if ( index < static_cast<int>(thumbViews.size() ) ) {
		thumbViews[index]->contentsDropped(event);
	}
}


void FrameBar::dragMoveEvent(QDragMoveEvent *event)
{
	int dragPosX = event->pos().x();
	int dragPosY = event->pos().y();

	if (dragPosX < lowerScrollAreaX || dragPosX > upperScrollAreaX) {
		if ( !scrollTimer->isActive() ) {
			scrollDirection = dragPosX < lowerScrollAreaX ? -1 : 1;
			scrollTimer->start(50);
		}
		// Increase speed
		else if (dragPosX < lowerAccelScrollAreaX || dragPosX > upperAccelScrollAreaX ) {
			scrollTimer->setInterval(25);
		}
		// Decrease speed
		else {
			scrollTimer->setInterval(50);
		}
	}
	else {
		scrollTimer->stop();
		scrollDirection = 0;
	}

	// Stop scrolling if we're dragging outside framebar
	bool outsideHorizontal = dragPosX < minScrollAreaX || dragPosX > maxScrollAreaX;
	bool outsideVertical = dragPosY < minScrollAreaY || dragPosY > maxScrollAreaY;
	if (outsideHorizontal || outsideVertical) {
		scrollTimer->stop();
		scrollDirection = 0;
	}
}


void FrameBar::resizeEvent(QResizeEvent *event)
{
	lowerScrollAreaX = this->x() + FRAME_WIDTH;
	upperScrollAreaX = this->width() - FRAME_WIDTH;

	lowerAccelScrollAreaX = lowerScrollAreaX - (FRAME_WIDTH >> 1);
	upperAccelScrollAreaX = upperScrollAreaX + (FRAME_WIDTH >> 1);

	minScrollAreaX = lowerScrollAreaX - FRAME_WIDTH + 20;
	maxScrollAreaX = upperScrollAreaX + FRAME_WIDTH - 20;

	minScrollAreaY = this->y() + 20;
	maxScrollAreaY = this->y() + FRAME_HEIGHT - 20;

	QScrollArea::resizeEvent(event);
}


void FrameBar::scroll()
{
	if (scrollDirection == -1) {
		scrollBar->setSliderPosition(scrollBar->sliderPosition() - 15);
	}
	else if (scrollDirection == 1) {
		scrollBar->setSliderPosition(scrollBar->sliderPosition() + 15);
	}
}


void FrameBar::setOpeningScene(bool openingScene)
{
	this->openingScene = openingScene;
}


bool FrameBar::isOpeningScene() const
{
	return openingScene;
}


int FrameBar::getFrameWidth() const
{
	return FRAME_WIDTH;
}


int FrameBar::getFrameHeight() const
{
	return FRAME_HEIGHT;
}


int FrameBar::getSpace() const
{
	return SPACE;
}

int FrameBar::getActiveFrame() const {
	return activeFrame;
}

int FrameBar::getActiveScene() const {
	return activeScene;
}

void FrameBar::setObserver(ActiveFrameObserver* observer) {
	activeFrameObserver = observer;
}

void FrameBar::updateObserver() {
	if (activeFrameObserver)
		activeFrameObserver->updateNewActiveFrame(
				activeScene, activeFrame);
}
