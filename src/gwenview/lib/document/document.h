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
#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <lib/gwenviewlib_export.h>

#include <string.h>
#include <exiv2/image.hpp>

// Qt
#include <QObject>
#include <QSharedData>
#include <QSize>

// KDE
#include <ksharedptr.h>

// Local
#include <lib/mimetypeutils.h>

class QImage;
class QRect;
class QSize;
class QUndoStack;

class KUrl;

namespace Gwenview {

class AbstractDocumentEditor;
class AbstractDocumentImpl;
class DocumentFactory;
class DocumentPrivate;
class ImageMetaInfoModel;


/**
 * This class represents an image.
 *
 * It handles loading and saving the image, applying operations and maintaining
 * the document undo stack.
 *
 * It is capable of loading down sampled versions of an image using
 * prepareDownSampledImageForZoom() and downSampledImageForZoom(). Down sampled
 * images load much faster than the full image but you need to load the full
 * image to manipulate it( use loadFullImage() to do so).
 *
 * To get a Document instance for url, ask for one with
 * DocumentFactory::instance()->load(url);
 */
class GWENVIEWLIB_EXPORT Document : public QObject, public QSharedData {
	Q_OBJECT
public:
	/**
	 * Document won't produce down sampled images for any zoom value higher than maxDownSampledZoom().
	 *
	 * Note: We can't use the enum {} trick to declare this constant, that's
	 * why it's defined as a static method
	 */
	static qreal maxDownSampledZoom();

	enum LoadingState {
		Loading,        ///< Image is loading
		KindDetermined, ///< Image is still loading, but kind has been determined
		MetaInfoLoaded, ///< Image is still loading, but meta info has been loaded
		Loaded,         ///< Full image has been loaded
		LoadingFailed   ///< Image loading has failed
	};

	typedef KSharedPtr<Document> Ptr;
	~Document();

	/**
	 * Returns a message for the last error which happened
	 */
	QString errorString() const;

	void reload();

	void loadFullImage();

	/**
	 * Prepare a version of the image down sampled to be a bit bigger than
	 * size() * @a zoom.
	 * Do not ask for a down sampled image for @a zoom >= to MaxDownSampledZoom.
	 *
	 * @return true if the image is ready, false if not. In this case the
	 * downSampledImageReady() signal will be emitted.
	 */
	bool prepareDownSampledImageForZoom(qreal zoom);

	LoadingState loadingState() const;

	MimeTypeUtils::Kind kind() const;

	bool isModified() const;

	const QImage& image() const;

	const QImage& downSampledImageForZoom(qreal zoom) const;

	/**
	 * Returns an implementation of AbstractDocumentEditor if this document can
	 * be edited.
	 */
	AbstractDocumentEditor* editor();

	KUrl url() const;

	bool save(const KUrl& url, const QByteArray& format);

	QByteArray format() const;

	void waitUntilLoaded();

	QSize size() const;

	int width() const { return size().width(); }

	int height() const { return size().height(); }

	bool hasAlphaChannel() const;

	ImageMetaInfoModel* metaInfo() const;

	QUndoStack* undoStack() const;

	void setKeepRawData(bool);

	bool keepRawData() const;

	/**
	 * Returns how much bytes the document is using
	 */
	int memoryUsage() const;

	/**
	 * Returns the compressed version of the document, if it is still
	 * available.
	 */
	QByteArray rawData() const;

	/**
	 * Returns true if the image can be edited.
	 * You must ensure it has been fully loaded with loadFullImage() first.
	 */
	bool isEditable() const;

	/**
	 * Returns true if the image is animated (eg: gif or mng format)
	 */
	bool isAnimated() const;

	/**
	 * Starts animation. Only sensible if isAnimated() returns true.
	 */
	void startAnimation();

	/**
	 * Stops animation. Only sensible if isAnimated() returns true.
	 */
	void stopAnimation();

Q_SIGNALS:
	void downSampledImageReady();
	void imageRectUpdated(const QRect&);
	void kindDetermined(const KUrl&);
	void metaInfoLoaded(const KUrl&);
	void loaded(const KUrl&);
	void loadingFailed(const KUrl&);
	void saved(const KUrl&);
	void modified(const KUrl&);
	void metaInfoUpdated();
	void isAnimatedUpdated();

private Q_SLOTS:
	void emitMetaInfoLoaded();
	void emitLoaded();
	void emitLoadingFailed();
	void slotUndoIndexChanged();

private:
	friend class DocumentFactory;
	friend class AbstractDocumentImpl;

	void setImageInternal(const QImage&);
	void setKind(MimeTypeUtils::Kind);
	void setFormat(const QByteArray&);
	void setSize(const QSize&);
	void setExiv2Image(Exiv2::Image::AutoPtr);
	void setDownSampledImage(const QImage&, int invertedZoom);
	void switchToImpl(AbstractDocumentImpl* impl);
	void setErrorString(const QString&);

	Document(const KUrl&);
	DocumentPrivate * const d;
};

} // namespace

#endif /* DOCUMENT_H */
