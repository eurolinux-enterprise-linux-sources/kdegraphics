// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <agateau@kde.org>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "gvcore.h"

// Qt
#include <QApplication>
#include <QFuture>
#include <QFutureWatcher>
#include <QList>
#include <QProgressDialog>
#include <QStandardItemModel>
#include <QtConcurrentMap>
#include <QWidget>

// KDE
#include <kfiledialog.h>
#include <kimageio.h>
#include <kio/netaccess.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kurl.h>

// Local
#include <lib/document/documentfactory.h>
#include <lib/gwenviewconfig.h>
#include <lib/historymodel.h>
#include <lib/mimetypeutils.h>
#include <lib/semanticinfo/semanticinfodirmodel.h>
#include <lib/semanticinfo/sorteddirmodel.h>
#include <lib/transformimageoperation.h>

namespace Gwenview {


struct GvCorePrivate {
	GvCore* q;
	QWidget* mParent;
	SortedDirModel* mDirModel;
	HistoryModel* mRecentFoldersModel;
	HistoryModel* mRecentUrlsModel;

	bool showSaveAsDialog(const KUrl& url, KUrl* outUrl, QByteArray* format) {
		KFileDialog dialog(url, QString(), mParent);
		dialog.setOperationMode(KFileDialog::Saving);
		dialog.setSelection(url.fileName());
		dialog.setMimeFilter(
			KImageIO::mimeTypes(KImageIO::Writing), // List
			MimeTypeUtils::urlMimeType(url)         // Default
			);

		// Show dialog
		do {
			if (!dialog.exec()) {
				return false;
			}

			const QString mimeType = dialog.currentMimeFilter();
			if (mimeType.isEmpty()) {
				KMessageBox::sorry(
					mParent,
					i18nc("@info",
						"No image format selected.")
					);
				continue;
			}

			const QStringList typeList = KImageIO::typeForMime(mimeType);
			if (typeList.count() > 0) {
				*format = typeList[0].toAscii();
				break;
			}
			KMessageBox::sorry(
				mParent,
				i18nc("@info",
					"Gwenview cannot save images as %1.", mimeType)
				);
		} while (true);

		*outUrl = dialog.selectedUrl();
		return true;
	}
};


GvCore::GvCore(QWidget* parent, SortedDirModel* dirModel)
: QObject(parent)
, d(new GvCorePrivate) {
	d->q = this;
	d->mParent = parent;
	d->mDirModel = dirModel;
	d->mRecentFoldersModel = 0;
	d->mRecentUrlsModel = 0;

	connect(GwenviewConfig::self(), SIGNAL(configChanged()),
		SLOT(slotConfigChanged()));
}


GvCore::~GvCore() {
	delete d;
}


QAbstractItemModel* GvCore::recentFoldersModel() const {
	if (!d->mRecentFoldersModel) {
		d->mRecentFoldersModel = new HistoryModel(const_cast<GvCore*>(this), KStandardDirs::locateLocal("appdata", "recentfolders/"));
	}
	return d->mRecentFoldersModel;
}


QAbstractItemModel* GvCore::recentUrlsModel() const {
	if (!d->mRecentUrlsModel) {
		d->mRecentUrlsModel = new HistoryModel(const_cast<GvCore*>(this), KStandardDirs::locateLocal("appdata", "recenturls/"));
	}
	return d->mRecentUrlsModel;
}


AbstractSemanticInfoBackEnd* GvCore::semanticInfoBackEnd() const {
	return d->mDirModel->semanticInfoBackEnd();
}


void GvCore::addUrlToRecentFolders(const KUrl& url) {
	if (!GwenviewConfig::historyEnabled()) {
		return;
	}
	recentFoldersModel();
	d->mRecentFoldersModel->addUrl(url);
}


void GvCore::addUrlToRecentUrls(const KUrl& url) {
	if (!GwenviewConfig::historyEnabled()) {
		return;
	}
	recentUrlsModel();
	d->mRecentUrlsModel->addUrl(url);
}


struct SaveAllHelper {
	typedef QPair<KUrl, QString> ErrorListItem;
	typedef QList<ErrorListItem> ErrorList;

	SaveAllHelper(ErrorList* errorList)
	: mErrorList(errorList) {}

	void operator()(Document::Ptr doc) {
		KUrl url = doc->url();
		if (!doc->save(url, doc->format())) {
			*mErrorList << qMakePair(url, doc->errorString());
		}
	}

	ErrorList* mErrorList;
};

void GvCore::saveAll() {
	SaveAllHelper::ErrorList errorList;
	SaveAllHelper helper(&errorList);

	// Separate local and remote urls because saving to remote urls uses KIO
	// methods, which are not thread-safe.
	// We do not use a KUrl::List because DocumentFactory::load() is not
	// thread-safe.
	QList<Document::Ptr> localDocs;
	QList<Document::Ptr> remoteDocs;

	Q_FOREACH(const KUrl& url, DocumentFactory::instance()->modifiedDocumentList()) {
		Document::Ptr doc = DocumentFactory::instance()->load(url);
		if (url.isLocalFile()) {
			localDocs << doc;
		} else {
			remoteDocs << doc;
		}
	}

	QProgressDialog progress(d->mParent);
	progress.setLabelText(i18nc("@info:progress saving all image changes", "Saving..."));
	progress.setCancelButtonText(i18n("&Stop"));
	progress.setMinimum(0);
	progress.setMaximum(localDocs.size() + remoteDocs.size());
	progress.setWindowModality(Qt::WindowModal);
	progress.show();

	// Save local urls
	QFuture<void> future = QtConcurrent::map(localDocs, helper);

	QFutureWatcher<void> watcher;
	watcher.setFuture(future);
	connect(&watcher, SIGNAL(progressValueChanged(int)),
		&progress, SLOT(setValue(int)) );

	connect(&progress, SIGNAL(canceled()),
		&watcher, SLOT(cancel()) );

	watcher.waitForFinished();

	// Save remote urls
	Q_FOREACH(Document::Ptr doc, remoteDocs) {
		if (progress.wasCanceled()) {
			break;
		}
		helper(doc);
		progress.setValue(progress.value() + 1);
		qApp->processEvents();
	}

	progress.close();

	// Done, show message if necessary
	if (errorList.count() > 0) {
		QString msg = i18ncp("@info", "One document could not be saved:", "%1 documents could not be saved:", errorList.count());
		msg += "<ul>";
		Q_FOREACH(const SaveAllHelper::ErrorListItem& item, errorList) {
			const KUrl& url = item.first;
			QString name = url.fileName().isEmpty() ? url.pathOrUrl() : url.fileName();
			msg += "<li>"
				+ i18nc("@info %1 is the name of the document which failed to save, %2 is the reason for the failure",
					"<filename>%1</filename>: %2", name, item.second)
				+ "</li>";
		}
		msg += "</ul>";
		KMessageBox::sorry(d->mParent, msg);
	}
}


void GvCore::save(const KUrl& url) {
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QByteArray format = doc->format();
	const QStringList availableTypes = KImageIO::types(KImageIO::Writing);
	if (availableTypes.contains(QString(format))) {
		if (!doc->save(url, format)) {
			QString name = url.fileName().isEmpty() ? url.pathOrUrl() : url.fileName();
			QString msg = i18nc("@info", "<b>Saving <filename>%1</filename> failed:</b><br>%2",
				name, doc->errorString());
			KMessageBox::sorry(d->mParent, msg);
		}
		return;
	}

	// We don't know how to save in 'format', ask the user for a format we can
	// write to.
	KGuiItem saveUsingAnotherFormat = KStandardGuiItem::saveAs();
	saveUsingAnotherFormat.setText(i18n("Save using another format"));
	int result = KMessageBox::warningContinueCancel(
		d->mParent,
		i18n("Gwenview cannot save images in '%1' format.", QString(format)),
		QString() /* caption */,
		saveUsingAnotherFormat
		);
	if (result == KMessageBox::Continue) {
		saveAs(url);
	}
}


void GvCore::saveAs(const KUrl& url) {
	QByteArray format;
	KUrl saveAsUrl;
	if (!d->showSaveAsDialog(url, &saveAsUrl, &format)) {
		return;
	}

	// Check for overwrite
	if (KIO::NetAccess::exists(saveAsUrl, KIO::NetAccess::DestinationSide, d->mParent)) {
		int answer = KMessageBox::warningContinueCancel(
			d->mParent,
			i18nc("@info",
				"A file named <filename>%1</filename> already exists.\n"
				"Are you sure you want to overwrite it?",
				saveAsUrl.fileName()),
			QString(),
			KStandardGuiItem::overwrite());
		if (answer == KMessageBox::Cancel) {
			return;
		}
	}

	// Start save
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->save(saveAsUrl, format.data());
}


static void applyTransform(const KUrl& url, Orientation orientation) {
	if (!GvCore::ensureDocumentIsEditable(url)) {
		return;
	}
	TransformImageOperation* op = new TransformImageOperation(orientation);
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	op->setDocument(doc);
	doc->undoStack()->push(op);
}


void GvCore::rotateLeft(const KUrl& url) {
	applyTransform(url, ROT_270);
}


void GvCore::rotateRight(const KUrl& url) {
	applyTransform(url, ROT_90);
}


void GvCore::setRating(const KUrl& url, int rating) {
	QModelIndex index = d->mDirModel->indexForUrl(url);
	if (!index.isValid()) {
		kWarning() << "invalid index!";
		return;
	}
	d->mDirModel->setData(index, rating, SemanticInfoDirModel::RatingRole);
}


bool GvCore::ensureDocumentIsEditable(const KUrl& url) {
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();
	if (doc->isEditable()) {
		return true;
	}

	KMessageBox::sorry(
		QApplication::activeWindow(),
		i18nc("@info", "Gwenview cannot edit this kind of image.")
		);
	return false;
}


static void clearModel(QAbstractItemModel* model) {
	model->removeRows(0, model->rowCount());
}

void GvCore::slotConfigChanged() {
	if (!GwenviewConfig::historyEnabled()) {
		clearModel(recentFoldersModel());
		clearModel(recentUrlsModel());
	}
}


} // namespace
