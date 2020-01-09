/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <agateau@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "imageview.moc"

// Qt
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QScrollBar>

// KDE
#include <kdebug.h>

// Local
#include "abstractimageviewtool.h"
#include "imagescaler.h"


namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif

// Defines how the createBuffer() method should behave
enum PreviousBufferContentPolicy {
	ClearBufferIfSizeIsDifferent,
	KeepPreviousBufferContent
};

struct ImageViewPrivate {
	ImageView* mView;
	QPixmap mBackgroundTexture;
	QWidget* mViewport;
	ImageView::AlphaBackgroundMode mAlphaBackgroundMode;
	QColor mAlphaBackgroundColor;
	bool mEnlargeSmallerImages;
	Document::Ptr mDocument;
	qreal mZoom;
	bool mZoomToFit;
	QPixmap mCurrentBuffer;
	QPixmap mAlternateBuffer;
	ImageScaler* mScaler;
	QPointer<AbstractImageViewTool> mTool;
	QPointer<AbstractImageViewTool> mDefaultTool;
	bool mInsideSetZoom;


	void createBackgroundTexture() {
		mBackgroundTexture = QPixmap(32, 32);
		QPainter painter(&mBackgroundTexture);
		painter.fillRect(mBackgroundTexture.rect(), QColor(128, 128, 128));
		QColor light = QColor(192, 192, 192);
		painter.fillRect(0, 0, 16, 16, light);
		painter.fillRect(16, 16, 16, 16, light);
	}


	QSize requiredBufferSize() const {
		if (!mDocument) {
			return QSize();
		}
		QSize size;
		qreal zoom;
		if (mZoomToFit) {
			zoom = mView->computeZoomToFit();
		} else {
			zoom = mZoom;
		}

		size = mDocument->size() * zoom;
		size = size.boundedTo(mViewport->size());

		return size;
	}


	void drawAlphaBackground(QPainter* painter, const QRect& viewportRect, const QPoint& zoomedImageTopLeft) {
		if (mAlphaBackgroundMode == ImageView::AlphaBackgroundCheckBoard) {
			QPoint textureOffset(
				zoomedImageTopLeft.x() % mBackgroundTexture.width(),
				zoomedImageTopLeft.y() % mBackgroundTexture.height()
				);
			painter->drawTiledPixmap(
				viewportRect,
				mBackgroundTexture,
				textureOffset);
		} else {
			painter->fillRect(viewportRect, mAlphaBackgroundColor);
		}
	}

	void createBuffer(PreviousBufferContentPolicy policy) {
		QSize size = requiredBufferSize();
		if (size == mCurrentBuffer.size()) {
			return;
		}
		mAlternateBuffer = QPixmap();
		if (!size.isValid()) {
			mCurrentBuffer = QPixmap();
			return;
		}
		QPixmap oldBuffer = mCurrentBuffer;
		mCurrentBuffer = QPixmap(size);

		QPainter painter(&mCurrentBuffer);

		if (policy == ClearBufferIfSizeIsDifferent) {
			painter.fillRect(mCurrentBuffer.rect(), Qt::black);
			return;
		}
		painter.drawPixmap(0, 0, oldBuffer);

		const QRegion emptyRegion = QRegion(QRect(QPoint(0, 0), size)) - QRegion(oldBuffer.rect());
		if (!emptyRegion.isEmpty()) {
			if (mDocument->hasAlphaChannel()) {
				const QPoint offset = QPoint(hScroll(), vScroll());
				Q_FOREACH(const QRect& rect, emptyRegion.rects()) {
					drawAlphaBackground(&painter, rect, offset);
				}
			} else {
				Q_FOREACH(const QRect& rect, emptyRegion.rects()) {
					painter.fillRect(rect, Qt::black);
				}
			}
		}
	}


	int hScroll() const {
		if (mZoomToFit) {
			return 0;
		} else {
			return mView->horizontalScrollBar()->value();
		}
	}

	int vScroll() const {
		if (mZoomToFit) {
			return 0;
		} else {
			return mView->verticalScrollBar()->value();
		}
	}

	QRect mapViewportToZoomedImage(const QRect& viewportRect) {
		QPoint offset = mView->imageOffset();
		QRect rect = QRect(
			viewportRect.x() + hScroll() - offset.x(),
			viewportRect.y() + vScroll() - offset.y(),
			viewportRect.width(),
			viewportRect.height()
		);

		return rect;
	}


	void setScalerRegionToVisibleRect() {
		QRect rect = mapViewportToZoomedImage(mViewport->rect());
		mScaler->setDestinationRegion(QRegion(rect));
	}


	void forceBufferRecreation() {
		mCurrentBuffer = QPixmap();
		createBuffer(ClearBufferIfSizeIsDifferent);
		setScalerRegionToVisibleRect();
	}


	void startAnimationIfNecessary() {
		if (mDocument && mView->isVisible()) {
			mDocument->startAnimation();
		}
	}


	// At least gcc 3.4.6 on FreeBSD requires a default constructor.
	ImageViewPrivate() { }
};


ImageView::ImageView(QWidget* parent)
: QAbstractScrollArea(parent)
, d(new ImageViewPrivate)
{
	d->mAlphaBackgroundMode = AlphaBackgroundCheckBoard;
	d->mAlphaBackgroundColor = Qt::black;

	d->mView = this;
	d->mZoom = 1.;
	d->mZoomToFit = true;
	d->createBackgroundTexture();
	setFrameShape(QFrame::NoFrame);
	setBackgroundRole(QPalette::Base);
	d->mViewport = new QWidget();
	setViewport(d->mViewport);
	d->mViewport->setMouseTracking(true);
	d->mViewport->setAttribute(Qt::WA_OpaquePaintEvent, true);
	horizontalScrollBar()->setSingleStep(16);
	verticalScrollBar()->setSingleStep(16);
	d->mScaler = new ImageScaler(this);
	d->mInsideSetZoom = false;
	connect(d->mScaler, SIGNAL(scaledRect(int, int, const QImage&)), 
		SLOT(updateFromScaler(int, int, const QImage&)) );
}

ImageView::~ImageView() {
	delete d;
}


void ImageView::setAlphaBackgroundMode(AlphaBackgroundMode mode) {
	d->mAlphaBackgroundMode = mode;
	if (d->mDocument && d->mDocument->hasAlphaChannel()) {
		d->forceBufferRecreation();
	}
}


void ImageView::setAlphaBackgroundColor(const QColor& color) {
	d->mAlphaBackgroundColor = color;
	if (d->mDocument && d->mDocument->hasAlphaChannel()) {
		d->forceBufferRecreation();
	}
}


void ImageView::setEnlargeSmallerImages(bool value) {
	d->mEnlargeSmallerImages = value;
	if (d->mZoomToFit) {
		setZoom(computeZoomToFit());
	}
}


void ImageView::setDocument(Document::Ptr document) {
	if (d->mDocument) {
		d->mDocument->stopAnimation();
		disconnect(d->mDocument.data(), 0, this, 0);
	}
	d->mDocument = document;
	if (!document) {
		d->mViewport->update();
		return;
	}

	connect(d->mDocument.data(), SIGNAL(metaInfoLoaded(const KUrl&)),
		SLOT(slotDocumentMetaInfoLoaded()) );
	connect(d->mDocument.data(), SIGNAL(isAnimatedUpdated()),
		SLOT(slotDocumentIsAnimatedUpdated()) );

	const Document::LoadingState state = d->mDocument->loadingState();
	if (state == Document::MetaInfoLoaded || state == Document::Loaded) {
		slotDocumentMetaInfoLoaded();
	}
}


void ImageView::slotDocumentMetaInfoLoaded() {
	if (d->mDocument->size().isValid()) {
		finishSetDocument();
	} else {
		// Could not retrieve image size from meta info, we need to load the
		// full image now.
		connect(d->mDocument.data(), SIGNAL(loaded(const KUrl&)),
			SLOT(finishSetDocument()) );
		d->mDocument->loadFullImage();
	}
}


void ImageView::finishSetDocument() {
	if (!d->mDocument->size().isValid()) {
		kError() << "No valid image size available, this should not happen!";
		return;
	}

	d->createBuffer(ClearBufferIfSizeIsDifferent);
	d->mScaler->setDocument(d->mDocument);

	connect(d->mDocument.data(), SIGNAL(imageRectUpdated(const QRect&)),
		SLOT(updateImageRect(const QRect&)) );

	if (d->mZoomToFit) {
		// Set the zoom to an invalid value to make sure setZoom() does not
		// return early because the new zoom is the same as the old zoom.
		d->mZoom = -1;
		setZoom(computeZoomToFit());
	} else {
		QRect rect(QPoint(0, 0), d->mDocument->size());
		updateImageRect(rect);
		updateScrollBars();
	}

	d->startAnimationIfNecessary();
	d->mViewport->update();
}


Document::Ptr ImageView::document() const {
	return d->mDocument;
}


void ImageView::slotDocumentIsAnimatedUpdated() {
	d->startAnimationIfNecessary();
}


void ImageView::updateImageRect(const QRect& imageRect) {
	LOG("imageRect" << imageRect);
	QRect viewportRect = mapToViewport(imageRect);
	viewportRect = viewportRect.intersected(d->mViewport->rect());
	if (viewportRect.isEmpty()) {
		return;
	}

	QSize bufferSize = d->requiredBufferSize();
	if (bufferSize != d->mCurrentBuffer.size()) {
		LOG("Need a new buffer");
		// Since the required buffer size is not the same as our current buffer
		// size, the image must have been resized. Call setDocument(), it will
		// take care of resizing the buffer and repainting the whole image.
		setDocument(d->mDocument);
		return;
	}

	d->setScalerRegionToVisibleRect();
	d->mViewport->update();
}


void ImageView::paintEvent(QPaintEvent* event) {
	QPainter painter(d->mViewport);
	QColor bgColor = palette().color(backgroundRole());

	if (!d->mDocument) {
		painter.fillRect(rect(), bgColor);
		return;
	}

	painter.setClipRect(event->rect());
	QPoint offset = imageOffset();

	// Erase pixels around the image
	QRect imageRect(offset, d->mCurrentBuffer.size());
	QRegion emptyRegion = QRegion(event->rect()) - QRegion(imageRect);
	Q_FOREACH(const QRect& rect, emptyRegion.rects()) {
		painter.fillRect(rect, bgColor);
	}

	painter.drawPixmap(offset, d->mCurrentBuffer);

	if (d->mTool) {
		d->mTool->paint(&painter);
	}
}

void ImageView::resizeEvent(QResizeEvent*) {
	if (d->mZoomToFit) {
		setZoom(computeZoomToFit());
		// Make sure one can't use mousewheel in zoom-to-fit mode
		horizontalScrollBar()->setRange(0, 0);
		verticalScrollBar()->setRange(0, 0);
	} else {
		d->createBuffer(KeepPreviousBufferContent);
		updateScrollBars();
		d->setScalerRegionToVisibleRect();
	}
}

QPoint ImageView::imageOffset() const {
	int left = qMax( (d->mViewport->width() - d->mCurrentBuffer.width()) / 2, 0);
	int top = qMax( (d->mViewport->height() - d->mCurrentBuffer.height()) / 2, 0);

	return QPoint(left, top);
}


void ImageView::setZoom(qreal zoom, const QPoint& _center) {
	if (!d->mDocument) {
		return;
	}

	qreal oldZoom = d->mZoom;
	if (qAbs(zoom - oldZoom) < 0.001) {
		return;
	}
	d->mZoom = zoom;

	QPoint center;
	if (_center == QPoint(-1, -1)) {
		center = QPoint(d->mViewport->width() / 2, d->mViewport->height() / 2);
	} else {
		center = _center;
	}

	// If we zoom more than twice, then assume the user wants to see the real
	// pixels, for example to fine tune a crop operation
	if (d->mZoom < 2.) {
		d->mScaler->setTransformationMode(Qt::SmoothTransformation);
	} else {
		d->mScaler->setTransformationMode(Qt::FastTransformation);
	}

	// Get offset *before* resizing the buffer, otherwise we get the new offset
	QPoint oldOffset = imageOffset();
	d->createBuffer(ClearBufferIfSizeIsDifferent);
	if (d->mZoom < oldZoom && (d->mCurrentBuffer.width() < d->mViewport->width() || d->mCurrentBuffer.height() < d->mViewport->height())) {
		// Trigger an update to erase borders
		d->mViewport->update();
	}

	d->mInsideSetZoom = true;

	/*
	We want to keep the point at viewport coordinates "center" at the same
	position after zooming. The coordinates of this point in image coordinates
	can be expressed like this:

	                      oldScroll + center
	imagePointAtOldZoom = ------------------
	                           oldZoom

	                   scroll + center
	imagePointAtZoom = ---------------
	                        zoom

	So we want:

	    imagePointAtOldZoom = imagePointAtZoom

	    oldScroll + center   scroll + center
	<=> ------------------ = ---------------
	          oldZoom             zoom

	              zoom
	<=> scroll = ------- (oldScroll + center) - center
	             oldZoom
	*/

	/*
	Compute oldScroll
	It's useless to take the new offset in consideration because if a direction
	of the new offset is not 0, we won't be able to center on a specific point
	in that direction.
	*/
	QPointF oldScroll = QPointF(d->hScroll(), d->vScroll()) - oldOffset;

	QPointF scroll = (zoom / oldZoom) * (oldScroll + center) - center;

	updateScrollBars();
	horizontalScrollBar()->setValue(int(scroll.x()));
	verticalScrollBar()->setValue(int(scroll.y()));
	d->mInsideSetZoom = false;

	d->mScaler->setZoom(d->mZoom);
	d->setScalerRegionToVisibleRect();
	emit zoomChanged(d->mZoom);
}

qreal ImageView::zoom() const {
	return d->mZoom;
}

bool ImageView::zoomToFit() const {
	return d->mZoomToFit;
}

void ImageView::setZoomToFit(bool on) {
	d->mZoomToFit = on;
	if (d->mZoomToFit) {
		setZoom(computeZoomToFit());
	}
}

void ImageView::updateScrollBars() {
	if (!d->mDocument || d->mZoomToFit) {
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		return;
	}
	setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

	int max;
	int width = d->mViewport->width();
	int height = d->mViewport->height();

	max = qMax(0, int(d->mDocument->width() * d->mZoom) - width);
	horizontalScrollBar()->setRange(0, max);
	horizontalScrollBar()->setPageStep(width);

	max = qMax(0, int(d->mDocument->height() * d->mZoom) - height);
	verticalScrollBar()->setRange(0, max);
	verticalScrollBar()->setPageStep(height);
}


void ImageView::scrollContentsBy(int dx, int dy) {
	if (d->mInsideSetZoom) {
		// Do not scroll anything: since we are zooming the whole viewport will
		// eventually be repainted
		return;
	}
	// Scroll existing
	{
		if (d->mAlternateBuffer.isNull()) {
			d->mAlternateBuffer = QPixmap(d->mCurrentBuffer.size());
		}
		QPainter painter(&d->mAlternateBuffer);
		painter.drawPixmap(dx, dy, d->mCurrentBuffer);
	}
	qSwap(d->mCurrentBuffer, d->mAlternateBuffer);

	// Scale missing parts
	QRegion region;
	int posX = d->hScroll();
	int posY = d->vScroll();
	int width = d->mViewport->width();
	int height = d->mViewport->height();

	QRect rect;
	if (dx > 0) {
		rect = QRect(posX, posY, dx, height);
	} else {
		rect = QRect(posX + width + dx, posY, -dx, height);
	}
	region |= rect;

	if (dy > 0) {
		rect = QRect(posX, posY, width, dy);
	} else {
		rect = QRect(posX, posY + height + dy, width, -dy);
	}
	region |= rect;

	d->mScaler->setDestinationRegion(region);
	d->mViewport->update();
}


void ImageView::updateFromScaler(int zoomedImageLeft, int zoomedImageTop, const QImage& image) {
	LOG("");
	int viewportLeft = zoomedImageLeft - d->hScroll();
	int viewportTop = zoomedImageTop - d->vScroll();

	{
		QPainter painter(&d->mCurrentBuffer);
		if (d->mDocument->hasAlphaChannel()) {
			d->drawAlphaBackground(
				&painter, QRect(viewportLeft, viewportTop, image.width(), image.height()),
				QPoint(zoomedImageLeft, zoomedImageTop)
				);
		} else {
			painter.setCompositionMode(QPainter::CompositionMode_Source);
		}
		painter.drawImage(viewportLeft, viewportTop, image);
		// Debug rects
		/*
		QPen pen(Qt::red);
		pen.setStyle(Qt::DotLine);
		painter.setPen(pen);
		painter.drawRect(viewportLeft, viewportTop, image.width() - 1, image.height() - 1);
		*/
	}
	QPoint offset = imageOffset();
	d->mViewport->update(
		offset.x() + viewportLeft,
		offset.y() + viewportTop,
		image.width(),
		image.height());
}


void ImageView::setCurrentTool(AbstractImageViewTool* tool) {
	if (d->mTool) {
		d->mTool->toolDeactivated();
	}
	d->mTool = tool ? QPointer<AbstractImageViewTool>(tool) : d->mDefaultTool;
	if (d->mTool) {
		d->mTool->toolActivated();
	}
	d->mViewport->update();
}


AbstractImageViewTool* ImageView::currentTool() const {
	return d->mTool;
}


void ImageView::setDefaultTool(AbstractImageViewTool* tool) {
	d->mDefaultTool = tool;
	if (!d->mTool && tool) {
		setCurrentTool(tool);
	}
}


AbstractImageViewTool* ImageView::defaultTool() const {
	return d->mDefaultTool;
}


QPoint ImageView::mapToViewport(const QPoint& src) const {
	QPoint dst(int(src.x() * d->mZoom), int(src.y() * d->mZoom));

	dst += imageOffset();

	dst.rx() -= d->hScroll();
	dst.ry() -= d->vScroll();

	return dst;
}


QPointF ImageView::mapToViewportF(const QPointF& src) const {
	QPointF dst(src.x() * d->mZoom, src.y() * d->mZoom);

	dst += imageOffset();

	dst.rx() -= d->hScroll();
	dst.ry() -= d->vScroll();

	return dst;
}


QRect ImageView::mapToViewport(const QRect& src) const {
	QRect dst(
		mapToViewport(src.topLeft()),
		mapToViewport(src.bottomRight())
	);
	return dst;
}


QRectF ImageView::mapToViewportF(const QRectF& src) const {
	QRectF dst(
		mapToViewportF(src.topLeft()),
		mapToViewportF(src.bottomRight())
	);
	return dst;
}


QPoint ImageView::mapToImage(const QPoint& src) const {
	QPoint dst = src;
	
	dst.rx() += d->hScroll();
	dst.ry() += d->vScroll();

	dst -= imageOffset();

	return QPoint(int(dst.x() / d->mZoom), int(dst.y() / d->mZoom));
}


QPointF ImageView::mapToImageF(const QPointF& src) const {
	QPointF dst = src;

	dst.rx() += d->hScroll();
	dst.ry() += d->vScroll();

	dst -= imageOffset();

	return dst / d->mZoom;
}


QRect ImageView::mapToImage(const QRect& src) const {
	QRect dst(
		mapToImage(src.topLeft()),
		mapToImage(src.bottomRight())
	);
	return dst;
}


QRectF ImageView::mapToImageF(const QRectF& src) const {
	QRectF dst(
		mapToImageF(src.topLeft()),
		mapToImageF(src.bottomRight())
	);
	return dst;
}


qreal ImageView::computeZoomToFit() const {
	qreal zoom = qMin(computeZoomToFitWidth(), computeZoomToFitHeight());

	if (!d->mEnlargeSmallerImages) {
		zoom = qMin(zoom, qreal(1.0));
	}

	return zoom;
}


qreal ImageView::computeZoomToFitWidth() const {
	if (!d->mDocument || !d->mDocument->size().isValid()) {
		return 1.;
	}
	return qreal(d->mViewport->width()) / d->mDocument->width();
}


qreal ImageView::computeZoomToFitHeight() const {
	if (!d->mDocument || !d->mDocument->size().isValid()) {
		return 1.;
	}
	return qreal(d->mViewport->height()) / d->mDocument->height();
}


void ImageView::showEvent(QShowEvent* event) {
	QAbstractScrollArea::showEvent(event);
	d->startAnimationIfNecessary();
}


void ImageView::hideEvent(QHideEvent* event) {
	QAbstractScrollArea::hideEvent(event);
	if (d->mDocument) {
		d->mDocument->stopAnimation();
	}
}


void ImageView::mousePressEvent(QMouseEvent* event) {
	if (d->mTool) {
		d->mTool->mousePressEvent(event);
	}
}


void ImageView::mouseMoveEvent(QMouseEvent* event) {
	if (d->mTool) {
		d->mTool->mouseMoveEvent(event);
	}
}


void ImageView::mouseReleaseEvent(QMouseEvent* event) {
	if (d->mTool) {
		d->mTool->mouseReleaseEvent(event);
	}
}


void ImageView::wheelEvent(QWheelEvent* event) {
	if (d->mTool) {
		d->mTool->wheelEvent(event);
	}
}


void ImageView::keyPressEvent(QKeyEvent* event) {
	if (d->mTool) {
		d->mTool->keyPressEvent(event);
	}
	QAbstractScrollArea::keyPressEvent(event);
}


void ImageView::keyReleaseEvent(QKeyEvent* event) {
	if (d->mTool) {
		d->mTool->keyReleaseEvent(event);
	}
	QAbstractScrollArea::keyReleaseEvent(event);
}


} // namespace
