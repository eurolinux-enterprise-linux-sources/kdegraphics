// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*	Gwenview - A simple image viewer for KDE
	Copyright 2000-2007 Aurélien Gâteau <agateau@kde.org>
	This class is based on the ImagePreviewJob class from Konqueror.
*/
/*	This file is part of the KDE project
	Copyright (C) 2000 David Faure <faure@kde.org>
				  2000 Carsten Pfeiffer <pfeiffer@kde.org>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#include "thumbnailloadjob.moc"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

// Qt
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QMatrix>
#include <QPixmap>
#include <QTimer>

// KDE
#include <kapplication.h>
#include <kcodecs.h>
#include <kdebug.h>
#include <kde_file.h>
#include <kfileitem.h>
#include <kio/jobuidelegate.h>
#include <kio/previewjob.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>

// Local
#include "mimetypeutils.h"
#include "jpegcontent.h"
#include "imageutils.h"
#include "urlutils.h"

namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x << endl
#else
#define LOG(x) ;
#endif


static QString generateOriginalUri(const KUrl& url_) {
	KUrl url = url_;
	// Don't include the password if any
	url.setPass(QString::null);	//krazy:exclude=nullstrassign for old broken gcc
	return url.url();
}


static QString generateThumbnailPath(const QString& uri, ThumbnailGroup::Enum group) {
	KMD5 md5( QFile::encodeName(uri) );
	QString baseDir=ThumbnailLoadJob::thumbnailBaseDir(group);
	return baseDir + QString(QFile::encodeName( md5.hexDigest())) + ".png";
}

//------------------------------------------------------------------------
//
// ThumbnailThread
//
//------------------------------------------------------------------------
ThumbnailThread::ThumbnailThread()
: mCancel(false) {}

void ThumbnailThread::load(
	const QString& originalUri, time_t originalTime, int originalSize, const QString& originalMimeType,
	const QString& pixPath,
	const QString& thumbnailPath,
	ThumbnailGroup::Enum group)
{
	QMutexLocker lock( &mMutex );
	assert( mPixPath.isNull());

	mOriginalUri = originalUri;
	mOriginalTime = originalTime;
	mOriginalSize = originalSize;
	mOriginalMimeType = originalMimeType;
	mPixPath = pixPath;
	mThumbnailPath = thumbnailPath;
	mThumbnailGroup = group;
	if(!isRunning()) start();
	mCond.wakeOne();
}

bool ThumbnailThread::testCancel() {
	QMutexLocker lock( &mMutex );
	return mCancel;
}

void ThumbnailThread::cancel() {
	QMutexLocker lock( &mMutex );
	mCancel = true;
	mCond.wakeOne();
}

void ThumbnailThread::run() {
	LOG("");
	while( !testCancel()) {
		{
			QMutexLocker lock(&mMutex);
			// empty mPixPath means nothing to do
			LOG("Waiting for mPixPath");
			if (mPixPath.isNull()) {
				LOG("mPixPath.isNull");
				mCond.wait(&mMutex);
			}
		}
		if(testCancel()) {
			return;
		}
		{
			QMutexLocker lock(&mMutex);
			Q_ASSERT(!mPixPath.isNull());
			LOG("Loading" << mPixPath);
			loadThumbnail();
			mPixPath.clear(); // done, ready for next
		}
		if(testCancel()) {
			return;
		}
		{
			LOG("emitting done signal");
			QSize size(mOriginalWidth, mOriginalHeight);
			QMutexLocker lock(&mMutex);
			done(mImage, size);
			LOG("Done");
		}
	}
	LOG("Ending thread");
}

void ThumbnailThread::loadThumbnail() {
	mImage = QImage();
	bool needCaching=true;
	int pixelSize = ThumbnailGroup::pixelSize(mThumbnailGroup);
	Orientation orientation = NORMAL;

	QImageReader reader(mPixPath);
	// If it's a Jpeg, try to load an embedded thumbnail, if available
	if (reader.format() == "jpeg") {
		JpegContent content;
		content.load(mPixPath);
		mOriginalWidth = content.size().width();
		mOriginalHeight = content.size().height();
		QImage thumbnail = content.thumbnail();
		orientation = content.orientation();

		if (qMax(thumbnail.width(), thumbnail.height()) >= pixelSize) {
			mImage = thumbnail;
			needCaching = false;
		}
	}

	// Generate thumbnail from full image
	if (mImage.isNull()) {
		const QSize originalSize = reader.size();
		if (originalSize.isValid() && reader.supportsOption(QImageIOHandler::ScaledSize)) {
			int scale;
			const int maxSize = qMax(originalSize.width(), originalSize.height());
			for (scale=1; pixelSize*scale*2 <= maxSize && scale <= 8; scale *= 2) {}
			const QSize scaledSize = originalSize / scale;
			if (!scaledSize.isEmpty()) {
				reader.setScaledSize(scaledSize);
			}
		}

		QImage originalImage;
		if (reader.read(&originalImage)) {
			mOriginalWidth = originalSize.width();
			mOriginalHeight = originalSize.height();

			if (qMax(mOriginalWidth, mOriginalHeight)<=pixelSize ) {
				mImage=originalImage;
				needCaching = false;
			} else {
				mImage = originalImage.scaled(pixelSize, pixelSize, Qt::KeepAspectRatio);
			}
		}
	}

	if (mImage.isNull()) {
		kWarning() << "Could not generate thumbnail for file" << mOriginalUri;
		return;
	}

	// Rotate if necessary
	if (orientation != NORMAL && orientation != NOT_AVAILABLE) {
		QMatrix matrix = ImageUtils::transformMatrix(orientation);
		mImage = mImage.transformed(matrix);
	}

	if (needCaching) {
		storeThumbnailInCache();
	}
}


void ThumbnailThread::storeThumbnailInCache() {
	mImage.setText("Thumb::Uri"          , 0, mOriginalUri);
	mImage.setText("Thumb::MTime"        , 0, QString::number(mOriginalTime));
	mImage.setText("Thumb::Size"         , 0, QString::number(mOriginalSize));
	mImage.setText("Thumb::Mimetype"     , 0, mOriginalMimeType);
	mImage.setText("Thumb::Image::Width" , 0, QString::number(mOriginalWidth));
	mImage.setText("Thumb::Image::Height", 0, QString::number(mOriginalHeight));
	mImage.setText("Software"            , 0, "Gwenview");

	QString thumbnailDir = ThumbnailLoadJob::thumbnailBaseDir(mThumbnailGroup);
	KStandardDirs::makeDir(thumbnailDir, 0700);

	KTemporaryFile tmp;
	tmp.setPrefix(thumbnailDir + "/gwenview");
	tmp.setSuffix(".png");
	if (!tmp.open()) {
		kWarning() << "Could not create a temporary file.";
		return;
	}

	if (!mImage.save(tmp.fileName(), "png")) {
		kWarning() << "Could not save thumbnail for file" << mOriginalUri;
		return;
	}

	KDE_rename(QFile::encodeName(tmp.fileName()), QFile::encodeName(mThumbnailPath));
}


//------------------------------------------------------------------------
//
// ThumbnailLoadJob static methods
//
//------------------------------------------------------------------------
static QString sThumbnailBaseDir;
QString ThumbnailLoadJob::thumbnailBaseDir() {
	if (sThumbnailBaseDir.isEmpty()) {
		sThumbnailBaseDir = QDir::homePath() + "/.thumbnails/";
	}
	return sThumbnailBaseDir;
}


void ThumbnailLoadJob::setThumbnailBaseDir(const QString& dir) {
	sThumbnailBaseDir = dir;
}


QString ThumbnailLoadJob::thumbnailBaseDir(ThumbnailGroup::Enum group) {
	QString dir = thumbnailBaseDir();
	switch (group) {
	case ThumbnailGroup::Normal:
		dir += "normal/";
		break;
	case ThumbnailGroup::Large:
		dir += "large/";
		break;
	}
	return dir;
}


void ThumbnailLoadJob::deleteImageThumbnail(const KUrl& url) {
	QString uri=generateOriginalUri(url);
	QFile::remove(generateThumbnailPath(uri, ThumbnailGroup::Normal));
	QFile::remove(generateThumbnailPath(uri, ThumbnailGroup::Large));
}


//------------------------------------------------------------------------
//
// ThumbnailLoadJob implementation
//
//------------------------------------------------------------------------
ThumbnailLoadJob::ThumbnailLoadJob(const KFileItemList& items, ThumbnailGroup::Enum group)
: KIO::Job()
, mState( STATE_NEXTTHUMB )
, mThumbnailGroup(group)
{
	LOG((int)this);

	// Look for images and store the items in our todo list
	Q_ASSERT(!items.empty());
	mItems = items;
	mCurrentItem = KFileItem();

	connect(&mThumbnailThread, SIGNAL(done(const QImage&, const QSize&)),
		SLOT(thumbnailReady(const QImage&, const QSize&)),
		Qt::QueuedConnection);
}


ThumbnailLoadJob::~ThumbnailLoadJob() {
	LOG((int)this);
	if (hasSubjobs()) {
		LOG("Killing subjob");
		KJob* job = subjobs().first();
		job->kill();
		removeSubjob(job);
	}
	mThumbnailThread.cancel();
	mThumbnailThread.wait();
}


void ThumbnailLoadJob::start() {
	if (mItems.isEmpty()) {
		LOG("Nothing to do");
		emitResult();
		delete this;
		return;
	}

	determineNextIcon();
}


const KFileItemList& ThumbnailLoadJob::pendingItems() const {
	return mItems;
}


void ThumbnailLoadJob::setThumbnailGroup(ThumbnailGroup::Enum group) {
	mThumbnailGroup = group;
}


//-Internal--------------------------------------------------------------
void ThumbnailLoadJob::appendItem(const KFileItem& item) {
	if (!mItems.contains(item)) {
		mItems.append(item);
	}
}


void ThumbnailLoadJob::removeItems(const KFileItemList& itemList) {
	Q_FOREACH(const KFileItem& item, itemList) {
		// If we are removing the next item, update to be the item after or the
		// first if we removed the last item
		mItems.removeAll( item );

		if (item == mCurrentItem) {
			// Abort current item
			mCurrentItem = KFileItem();
			if (hasSubjobs()) {
				KJob* job = subjobs().first();
				job->kill();
				removeSubjob(job);
			}
		}
	}

	// No more current item, carry on to the next remaining item
	if (mCurrentItem.isNull()) {
		determineNextIcon();
	}
}


void ThumbnailLoadJob::determineNextIcon() {
	LOG((int)this);
	mState = STATE_NEXTTHUMB;

	// No more items ?
	if (mItems.isEmpty()) {
		// Done
		LOG("emitting result");
		emitResult();
		delete this;
		return;
	}

	mCurrentItem = mItems.first();
	mItems.pop_front();
	LOG("mCurrentItem.url=" << mCurrentItem.url());

	// First, stat the orig file
	mState = STATE_STATORIG;
	mCurrentUrl = mCurrentItem.url();
	mCurrentUrl.cleanPath();

	// Do direct stat instead of using KIO if the file is local (faster)
	bool directStatOk = false;
	if (UrlUtils::urlIsFastLocalFile(mCurrentUrl)) {
		KDE_struct_stat buff;
		if ( KDE_stat( QFile::encodeName(mCurrentUrl.path()), &buff ) == 0 )  {
			directStatOk = true;
			mOriginalTime = buff.st_mtime;
			QTimer::singleShot( 0, this, SLOT( checkThumbnail()));
		}
	}
	if (!directStatOk) {
		KIO::Job* job = KIO::stat(mCurrentUrl, KIO::HideProgressInfo);
		job->ui()->setWindow(KApplication::kApplication()->activeWindow());
		LOG( "KIO::stat orig" << mCurrentUrl.url() );
		addSubjob(job);
	}
	LOG("/determineNextIcon" << (int)this);
}


void ThumbnailLoadJob::slotResult(KJob * job) {
	LOG(mState);
	removeSubjob(job);
	Q_ASSERT(subjobs().isEmpty()); // We should have only one job at a time

	switch (mState) {
	case STATE_NEXTTHUMB:
		Q_ASSERT(false);
		determineNextIcon();
		return;

	case STATE_STATORIG: {
		// Could not stat original, drop this one and move on to the next one
		if (job->error()) {
			emitThumbnailLoadingFailed();
			determineNextIcon();
			return;
		}

		// Get modification time of the original file
		KIO::UDSEntry entry = static_cast<KIO::StatJob*>(job)->statResult();
		mOriginalTime = entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, -1);
		checkThumbnail();
		return;
	}

	case STATE_DOWNLOADORIG:
		if (job->error()) {
			emitThumbnailLoadingFailed();
			LOG("Delete temp file" << mTempPath);
			QFile::remove(mTempPath);
			mTempPath.clear();
			determineNextIcon();
		} else {
			startCreatingThumbnail(mTempPath);
		}
		return;

	case STATE_PREVIEWJOB:
		determineNextIcon();
		return;
	}
}


void ThumbnailLoadJob::thumbnailReady( const QImage& _img, const QSize& _size) {
	QImage img = _img;
	QSize size = _size;
	if ( !img.isNull()) {
		emitThumbnailLoaded(img, size);
	} else {
		emitThumbnailLoadingFailed();
	}
	if( !mTempPath.isEmpty()) {
		LOG("Delete temp file" << mTempPath);
		QFile::remove(mTempPath);
		mTempPath.clear();
	}
	determineNextIcon();
}

void ThumbnailLoadJob::checkThumbnail() {
	// If we are in the thumbnail dir, just load the file
	if (mCurrentUrl.isLocalFile()
		&& mCurrentUrl.directory().startsWith(thumbnailBaseDir()) )
	{
		QImage image(mCurrentUrl.toLocalFile());
		emitThumbnailLoaded(image, image.size());
		determineNextIcon();
		return;
	}
	QSize imagesize;

	mOriginalUri=generateOriginalUri(mCurrentUrl);
	mThumbnailPath=generateThumbnailPath(mOriginalUri, mThumbnailGroup);

	LOG("Stat thumb" << mThumbnailPath);

	QImage thumb;
	if ( thumb.load(mThumbnailPath) ) {
		if (thumb.text("Thumb::Uri", 0) == mOriginalUri &&
			thumb.text("Thumb::MTime", 0).toInt() == mOriginalTime )
		{
			int width=0, height=0;
			QSize size;
			bool ok;

			width=thumb.text("Thumb::Image::Width", 0).toInt(&ok);
			if (ok) height=thumb.text("Thumb::Image::Height", 0).toInt(&ok);
			if (ok) {
				size=QSize(width, height);
			} else {
				LOG("Thumbnail for" << mOriginalUri << "does not contain correct image size information");
				KFileMetaInfo fmi(mCurrentUrl);
				if (fmi.isValid()) {
					KFileMetaInfoItem item=fmi.item("Dimensions");
					if (item.isValid()) {
						size=item.value().toSize();
					} else {
						LOG("KFileMetaInfoItem for" << mOriginalUri << "did not get image size information");
					}
				} else {
					LOG("Could not get a valid KFileMetaInfo instance for" << mOriginalUri);
				}
			}
			emitThumbnailLoaded(thumb, size);
			determineNextIcon();
			return;
		}
	}

	// Thumbnail not found or not valid
	if (MimeTypeUtils::fileItemKind(mCurrentItem) == MimeTypeUtils::KIND_RASTER_IMAGE) {
		if (mCurrentUrl.isLocalFile()) {
			// Original is a local file, create the thumbnail
			startCreatingThumbnail(mCurrentUrl.toLocalFile());
		} else {
			// Original is remote, download it
			mState=STATE_DOWNLOADORIG;
			
			KTemporaryFile tempFile;
			tempFile.setAutoRemove(false);
			if (!tempFile.open()) {
				kWarning() << "Couldn't create temp file to download " << mCurrentUrl.prettyUrl();
				emitThumbnailLoadingFailed();
				determineNextIcon();
				return;
			}
			mTempPath = tempFile.fileName();

			KUrl url;
			url.setPath(mTempPath);
			KIO::Job* job=KIO::file_copy(mCurrentUrl, url,-1, KIO::Overwrite | KIO::HideProgressInfo);
			job->ui()->setWindow(KApplication::kApplication()->activeWindow());
			LOG("Download remote file" << mCurrentUrl.prettyUrl() << "to" << url.pathOrUrl());
			addSubjob(job);
		}
	} else {
		// Not a raster image, use a KPreviewJob
		LOG("Starting a KPreviewJob for" << mCurrentItem.url());
		mState=STATE_PREVIEWJOB;
		KFileItemList list;
		list.append(mCurrentItem);
		KIO::Job* job=KIO::filePreview(list, ThumbnailGroup::pixelSize(mThumbnailGroup));
		//job->ui()->setWindow(KApplication::kApplication()->activeWindow());
		connect(job, SIGNAL(gotPreview(const KFileItem&, const QPixmap&)),
			this, SLOT(slotGotPreview(const KFileItem&, const QPixmap&)) );
		connect(job, SIGNAL(failed(const KFileItem&)),
			this, SLOT(emitThumbnailLoadingFailed()) );
		addSubjob(job);
	}
}

void ThumbnailLoadJob::startCreatingThumbnail(const QString& pixPath) {
	LOG("Creating thumbnail from" << pixPath);
	mThumbnailThread.load( mOriginalUri, mOriginalTime, mCurrentItem.size(),
		mCurrentItem.mimetype(), pixPath, mThumbnailPath, mThumbnailGroup);
}


void ThumbnailLoadJob::slotGotPreview(const KFileItem& item, const QPixmap& pixmap) {
	LOG(mCurrentItem.url());
	QSize size;
	emit thumbnailLoaded(item, pixmap, size);
}


void ThumbnailLoadJob::emitThumbnailLoaded(const QImage& img, const QSize& size) {
	LOG(mCurrentItem.url());
	QPixmap thumb = QPixmap::fromImage(img);
	emit thumbnailLoaded(mCurrentItem, thumb, size);
}


void ThumbnailLoadJob::emitThumbnailLoadingFailed() {
	LOG(mCurrentItem.url());
	emit thumbnailLoadingFailed(mCurrentItem);
}


} // namespace
