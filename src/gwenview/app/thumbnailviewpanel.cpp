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
#include "thumbnailviewpanel.moc"

// Qt
#include <QMenu>
#include <QSlider>
#include <QToolTip>
#include <QVBoxLayout>

// KDE
#include <kactioncollection.h>
#include <kactioncategory.h>
#include <kactionmenu.h>
#include <kdebug.h>
#include <kfileitem.h>
#include <kfileplacesmodel.h>
#include <klineedit.h>
#include <klocale.h>
#include <kselectaction.h>
#include <kstatusbar.h>
#include <ktoggleaction.h>
#include <kurlnavigator.h>

// Local
#include <filtercontroller.h>
#include <lib/gwenviewconfig.h>
#include <lib/semanticinfo/abstractsemanticinfobackend.h>
#include <lib/semanticinfo/sorteddirmodel.h>
#include <lib/semanticinfo/tagmodel.h>
#include <lib/sorting.h>
#include <lib/archiveutils.h>
#include <lib/thumbnailview/previewitemdelegate.h>
#include <lib/thumbnailview/thumbnailview.h>
#include <ui_thumbnailviewpanel.h>


namespace Gwenview {

inline Sorting::Enum sortingFromSortAction(const QAction* action) {
	Q_ASSERT(action);
	return Sorting::Enum(action->data().toInt());
}

struct ThumbnailViewPanelPrivate : public Ui_ThumbnailViewPanel {
	ThumbnailViewPanel* that;
	KFilePlacesModel* mFilePlacesModel;
	KUrlNavigator* mUrlNavigator;
	SortedDirModel* mDirModel;
	int mDocumentCount;
	KActionCollection* mActionCollection;
	FilterController* mFilterController;
	KSelectAction* mSortAction;
	QActionGroup* mThumbnailDetailsActionGroup;
	PreviewItemDelegate* mDelegate;

	void setupWidgets() {
		setupUi(that);
		that->layout()->setMargin(0);

		// mThumbnailView
		mThumbnailView->setModel(mDirModel);

		mDelegate = new PreviewItemDelegate(mThumbnailView);
		mThumbnailView->setItemDelegate(mDelegate);
		mThumbnailView->setSelectionMode(QAbstractItemView::ExtendedSelection);

		// mUrlNavigator (use stupid layouting code because KUrlNavigator ctor
		// can't be used directly from Designer)
		mFilePlacesModel = new KFilePlacesModel(that);
		mUrlNavigator = new KUrlNavigator(mFilePlacesModel, KUrl(), mUrlNavigatorContainer);
		QVBoxLayout* layout = new QVBoxLayout(mUrlNavigatorContainer);
		layout->setMargin(0);
		layout->addWidget(mUrlNavigator);

		// Thumbnail slider
		QObject::connect(mThumbnailSlider, SIGNAL(valueChanged(int)),
			mThumbnailView, SLOT(setThumbnailSize(int)) );
		QObject::connect(mThumbnailSlider, SIGNAL(actionTriggered(int)),
			that, SLOT(updateSliderToolTip(int)) );
	}

	void setupActions(KActionCollection* actionCollection) {
		KActionCategory* view=new KActionCategory(i18nc("@title actions category - means actions changing smth in interface","View"), actionCollection);
		KAction* action = view->addAction("edit_location",that, SLOT(editLocation()));
		action->setText(i18nc("@action:inmenu Navigation Bar", "Edit Location"));
		action->setShortcut(Qt::Key_F6);

		mSortAction = view->add<KSelectAction>("sort_order");
		mSortAction->setText(i18nc("@action:inmenu", "Sort By"));
		action = mSortAction->addAction(i18nc("@addAction:inmenu", "Name"));
		action->setData(QVariant(Sorting::Name));
		action = mSortAction->addAction(i18nc("@addAction:inmenu", "Date"));
		action->setData(QVariant(Sorting::Date));
		action = mSortAction->addAction(i18nc("@addAction:inmenu", "Size"));
		action->setData(QVariant(Sorting::Size));
		QObject::connect(mSortAction, SIGNAL(triggered(QAction*)),
			that, SLOT(updateSortOrder()));

		mThumbnailDetailsActionGroup = new QActionGroup(that);
		mThumbnailDetailsActionGroup->setExclusive(false);
		KActionMenu* thumbnailDetailsAction = view->add<KActionMenu>("thumbnail_details");
		thumbnailDetailsAction->setText(i18nc("@action:inmenu", "Thumbnail Details"));
		#define addAction(text, detail) \
			action = new KAction(that); \
			thumbnailDetailsAction->addAction(action); \
			action->setText(text); \
			action->setCheckable(true); \
			action->setChecked(GwenviewConfig::thumbnailDetails() & detail); \
			action->setData(QVariant(detail)); \
			mThumbnailDetailsActionGroup->addAction(action); \
			QObject::connect(action, SIGNAL(triggered(bool)), \
				that, SLOT(updateThumbnailDetails()));
		addAction(i18nc("@action:inmenu", "Filename"), PreviewItemDelegate::FileNameDetail);
		addAction(i18nc("@action:inmenu", "Date"), PreviewItemDelegate::DateDetail);
		#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		addAction(i18nc("@action:inmenu", "Rating"), PreviewItemDelegate::RatingDetail);
		#endif
		#undef addAction

		KActionCategory* file=new KActionCategory(i18nc("@title actions category","File"), actionCollection);
		action = file->addAction("add_folder_to_places",that, SLOT(addFolderToPlaces()));
		action->setText(i18nc("@action:inmenu", "Add Folder to Places"));
	}

	void setupFilterController() {
		QMenu* menu = new QMenu(mAddFilterButton);
		mFilterController = new FilterController(mFilterFrame, mDirModel);
		Q_FOREACH(QAction* action, mFilterController->actionList()) {
			menu->addAction(action);
		}
		mAddFilterButton->setMenu(menu);
	}

	void updateDocumentCountLabel() {
		QString text = i18ncp("@label", "%1 document", "%1 documents", mDocumentCount);
		mDocumentCountLabel->setText(text);
	}

	void setupDocumentCountConnections() {
		QObject::connect(mDirModel, SIGNAL(rowsInserted(const QModelIndex&, int, int)),
			that, SLOT(slotDirModelRowsInserted(const QModelIndex&, int, int)) );

		QObject::connect(mDirModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
			that, SLOT(slotDirModelRowsAboutToBeRemoved(const QModelIndex&, int, int)) );

		QObject::connect(mDirModel, SIGNAL(modelReset()),
			that, SLOT(slotDirModelReset()) );
	}


	int documentCountInIndexRange(const QModelIndex& parent, int start, int end) {
		int count = 0;
		for (int row=start; row<=end; ++row) {
			QModelIndex index = mDirModel->index(row, 0, parent);
			KFileItem item = mDirModel->itemForIndex(index);
			if (!ArchiveUtils::fileItemIsDirOrArchive(item)) {
				++count;
			}
		}
		return count;
	}
};


ThumbnailViewPanel::ThumbnailViewPanel(QWidget* parent, SortedDirModel* dirModel, KActionCollection* actionCollection)
: QWidget(parent)
, d(new ThumbnailViewPanelPrivate) {
	d->that = this;
	d->mDirModel = dirModel;
	d->mDocumentCount = 0;
	d->mActionCollection = actionCollection;
	d->setupWidgets();
	d->setupActions(actionCollection);
	d->setupFilterController();
	d->setupDocumentCountConnections();
	loadConfig();
	updateSortOrder();
	updateThumbnailDetails();
}


ThumbnailViewPanel::~ThumbnailViewPanel() {
	delete d;
}


void ThumbnailViewPanel::loadConfig() {
	d->mUrlNavigator->setUrlEditable(GwenviewConfig::urlNavigatorIsEditable());
	d->mUrlNavigator->setShowFullPath(GwenviewConfig::urlNavigatorShowFullPath());

	d->mThumbnailSlider->setValue(GwenviewConfig::thumbnailSize());
	updateSliderToolTip(QAbstractSlider::SliderNoAction);
	// If GwenviewConfig::thumbnailSize() returns the current value of
	// mThumbnailSlider, it won't emit valueChanged() and the thumbnail view
	// won't be updated. That's why we do it ourself.
	d->mThumbnailView->setThumbnailSize(GwenviewConfig::thumbnailSize());

	Q_FOREACH(QAction* action, d->mSortAction->actions()) {
		if (sortingFromSortAction(action) == GwenviewConfig::sorting()) {
			d->mSortAction->setCurrentAction(action);
			break;
		}
	}
}


void ThumbnailViewPanel::saveConfig() const {
	GwenviewConfig::setUrlNavigatorIsEditable(d->mUrlNavigator->isUrlEditable());
	GwenviewConfig::setUrlNavigatorShowFullPath(d->mUrlNavigator->showFullPath());
	GwenviewConfig::setThumbnailSize(d->mThumbnailSlider->value());
	GwenviewConfig::setSorting(sortingFromSortAction(d->mSortAction->currentAction()));
	GwenviewConfig::setThumbnailDetails(d->mDelegate->thumbnailDetails());
}


ThumbnailView* ThumbnailViewPanel::thumbnailView() const {
	return d->mThumbnailView;
}


KUrlNavigator* ThumbnailViewPanel::urlNavigator() const {
	return d->mUrlNavigator;
}


void ThumbnailViewPanel::editLocation() {
	d->mUrlNavigator->setUrlEditable(true);
	d->mUrlNavigator->setFocus();
}


void ThumbnailViewPanel::addFolderToPlaces() {
	KUrl url = d->mUrlNavigator->url();
	QString text = url.fileName();
	if (text.isEmpty()) {
		text = url.pathOrUrl();
	}
	d->mFilePlacesModel->addPlace(text, url);
}


void ThumbnailViewPanel::slotDirModelRowsInserted(const QModelIndex& parent, int start, int end) {
	int count = d->documentCountInIndexRange(parent, start, end);
	if (count > 0) {
		d->mDocumentCount += count;
		d->updateDocumentCountLabel();
	}
}


void ThumbnailViewPanel::slotDirModelRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) {
	int count = d->documentCountInIndexRange(parent, start, end);
	if (count > 0) {
		d->mDocumentCount -= count;
		d->updateDocumentCountLabel();
	}
}


void ThumbnailViewPanel::slotDirModelReset() {
	d->mDocumentCount = 0;
	d->updateDocumentCountLabel();
}


void ThumbnailViewPanel::updateSortOrder() {
	const QAction* action = d->mSortAction->currentAction();
	if (!action) {
		kWarning() << "!action, this should not happen!";
		return;
	}

	// This works because for now Sorting::Enum maps to KDirModel::ModelColumns
	d->mDirModel->setSortRole(sortingFromSortAction(action));
}


void ThumbnailViewPanel::updateThumbnailDetails() {
	PreviewItemDelegate::ThumbnailDetails details = 0;
	Q_FOREACH(const QAction* action, d->mThumbnailDetailsActionGroup->actions()) {
		if (action->isChecked()) {
			details |= PreviewItemDelegate::ThumbnailDetail(action->data().toInt());
		}
	}
	d->mDelegate->setThumbnailDetails(details);
}


void ThumbnailViewPanel::applyPalette(const QPalette& palette) {
	d->mThumbnailView->setPalette(palette);
}


void ThumbnailViewPanel::updateSliderToolTip(int actionTriggered) {
	// FIXME: i18n?
	const int size = d->mThumbnailSlider->sliderPosition();
	const QString text = QString("%1 x %2").arg(size).arg(size);
	d->mThumbnailSlider->setToolTip(text);

	if (actionTriggered != QAbstractSlider::SliderNoAction) {
		// If we are updating because of a direct action on the slider, show
		// the tooltip immediatly.
		const QPoint pos = d->mThumbnailSlider->mapToGlobal(QPoint(0, d->mThumbnailSlider->height() / 2));
		QToolTip::showText(pos, text, d->mThumbnailSlider);
	}
}


} // namespace
