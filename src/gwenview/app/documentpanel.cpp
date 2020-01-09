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
#include "documentpanel.moc"

// Qt
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

// KDE
#include <kactioncollection.h>
#include <kactioncategory.h>
#include <kdebug.h>
#include <klocale.h>
#include <kmenu.h>
#include <kmessagebox.h>
#include <ktoggleaction.h>

// Local
#include "splitter.h"
#include <lib/documentview/abstractdocumentviewadapter.h>
#include <lib/documentview/documentview.h>
#include <lib/paintutils.h>
#include <lib/gwenviewconfig.h>
#include <lib/statusbartoolbutton.h>
#include <lib/thumbnailview/thumbnailbarview.h>
#include <lib/zoomwidget.h>


namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif


static QString rgba(const QColor &color) {
	return QString::fromAscii("rgba(%1, %2, %3, %4)")
		.arg(color.red())
		.arg(color.green())
		.arg(color.blue())
		.arg(color.alpha());
}


static QString gradient(Qt::Orientation orientation, const QColor &color, int value) {
	int x2, y2;
	if (orientation == Qt::Horizontal) {
		x2 = 0;
		y2 = 1;
	} else {
		x2 = 1;
		y2 = 0;
	}
	QString grad =
		"qlineargradient(x1:0, y1:0, x2:%1, y2:%2,"
		"stop:0 %3, stop: 1 %4)";
	return grad
		.arg(x2)
		.arg(y2)
		.arg(rgba(PaintUtils::adjustedHsv(color, 0, 0, qMin(255 - color.value(), value/2))))
		.arg(rgba(PaintUtils::adjustedHsv(color, 0, 0, -qMin(color.value(), value/2))))
		;
}


/*
 * Layout of mThumbnailSplitter is:
 *
 * +-mThumbnailSplitter--------------------------------+
 * |+-mAdapterContainer-------------------------------+|
 * ||+-mDocumentView---------------------------------+||
 * |||                                               |||
 * |||                                               |||
 * |||                                               |||
 * |||                                               |||
 * |||                                               |||
 * ||+-----------------------------------------------+||
 * ||+-mStatusBarContainer---------------------------+||
 * |||[mToggleThumbnailBarButton]       [mZoomWidget]|||
 * ||+-----------------------------------------------+||
 * |+-------------------------------------------------+|
 * |===================================================|
 * |+-mThumbnailBar-----------------------------------+|
 * ||                                                 ||
 * ||                                                 ||
 * |+-------------------------------------------------+|
 * +---------------------------------------------------+
 */
struct DocumentPanelPrivate {
	DocumentPanel* that;
	KActionCollection* mActionCollection;
	QSplitter *mThumbnailSplitter;
	QWidget* mAdapterContainer;
	DocumentView* mDocumentView;
	QToolButton* mToggleThumbnailBarButton;
	QWidget* mStatusBarContainer;
	ThumbnailBarView* mThumbnailBar;
	KToggleAction* mToggleThumbnailBarAction;

	bool mFullScreenMode;
	QPalette mNormalPalette;
	QPalette mFullScreenPalette;
	bool mThumbnailBarVisibleBeforeFullScreen;

	void setupThumbnailBar() {
		mThumbnailBar = new ThumbnailBarView;
		ThumbnailBarItemDelegate* delegate = new ThumbnailBarItemDelegate(mThumbnailBar);
		mThumbnailBar->setItemDelegate(delegate);
		mThumbnailBar->setVisible(GwenviewConfig::thumbnailBarIsVisible());
	}

	void setupThumbnailBarStyleSheet() {
		Qt::Orientation orientation = mThumbnailBar->orientation();
		QColor bgColor = mNormalPalette.color(QPalette::Normal, QPalette::Window);
		QColor bgSelColor = mNormalPalette.color(QPalette::Normal, QPalette::Highlight);

		// Avoid dark and bright colors
		bgColor.setHsv(bgColor.hue(), bgColor.saturation(), (127 + 3 * bgColor.value()) / 4);

		QColor leftBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, qMin(20, 255 - bgColor.value()));
		QColor rightBorderColor = PaintUtils::adjustedHsv(bgColor, 0, 0, -qMin(40, bgColor.value()));
		QColor borderSelColor = PaintUtils::adjustedHsv(bgSelColor, 0, 0, -qMin(60, bgSelColor.value()));

		QString viewCss =
			"#thumbnailBarView {"
			"	background-color: rgba(0, 0, 0, 10%);"
			"}";

		QString itemCss =
			"QListView::item {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %3;"
			"}";
		itemCss = itemCss.arg(
			gradient(orientation, bgColor, 46),
			gradient(orientation, leftBorderColor, 36),
			gradient(orientation, rightBorderColor, 26));

		QString itemSelCss =
			"QListView::item:selected {"
			"	background-color: %1;"
			"	border-left: 1px solid %2;"
			"	border-right: 1px solid %2;"
			"}";
		itemSelCss = itemSelCss.arg(
			gradient(orientation, bgSelColor, 56),
			rgba(borderSelColor));

		QString css = viewCss + itemCss + itemSelCss;
		if (orientation == Qt::Vertical) {
			css.replace("left", "top").replace("right", "bottom");
		}

		mThumbnailBar->setStyleSheet(css);
	}

	void setupAdapterContainer() {
		mAdapterContainer = new QWidget;

		QVBoxLayout* layout = new QVBoxLayout(mAdapterContainer);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(mDocumentView);
		layout->addWidget(mStatusBarContainer);
	}

	void setupDocumentView(SlideShow* slideShow) {
		mDocumentView = new DocumentView(0, slideShow, mActionCollection);

		// Connect context menu
		mDocumentView->setContextMenuPolicy(Qt::CustomContextMenu);
		QObject::connect(mDocumentView, SIGNAL(customContextMenuRequested(const QPoint&)),
			that, SLOT(showContextMenu()) );

		QObject::connect(mDocumentView, SIGNAL(completed()),
			that, SIGNAL(completed()) );
		QObject::connect(mDocumentView, SIGNAL(previousImageRequested()),
			that, SIGNAL(previousImageRequested()) );
		QObject::connect(mDocumentView, SIGNAL(nextImageRequested()),
			that, SIGNAL(nextImageRequested()) );
		QObject::connect(mDocumentView, SIGNAL(captionUpdateRequested(const QString&)),
			that, SIGNAL(captionUpdateRequested(const QString&)) );
		QObject::connect(mDocumentView, SIGNAL(toggleFullScreenRequested()),
			that, SIGNAL(toggleFullScreenRequested()) );
	}

	void setupStatusBar() {
		mStatusBarContainer = new QWidget;
		mToggleThumbnailBarButton = new StatusBarToolButton;

		QHBoxLayout* layout = new QHBoxLayout(mStatusBarContainer);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(mToggleThumbnailBarButton);
		layout->addStretch();
		layout->addWidget(mDocumentView->zoomWidget());
		mDocumentView->zoomWidget()->hide();
	}

	void setupSplitter() {
		Qt::Orientation orientation = GwenviewConfig::thumbnailBarOrientation();
		mThumbnailSplitter = new Splitter(orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal, that);
		mThumbnailBar->setOrientation(orientation);
		mThumbnailBar->setRowCount(GwenviewConfig::thumbnailBarRowCount());
		mThumbnailSplitter->addWidget(mAdapterContainer);
		mThumbnailSplitter->addWidget(mThumbnailBar);
		mThumbnailSplitter->setSizes(GwenviewConfig::thumbnailSplitterSizes());

		QVBoxLayout* layout = new QVBoxLayout(that);
		layout->setMargin(0);
		layout->addWidget(mThumbnailSplitter);
	}

	void applyPalette() {
		QPalette palette = mFullScreenMode ? mFullScreenPalette : mNormalPalette;
		that->setPalette(palette);

		if (!mDocumentView->adapter()) {
			return;
		}
		QWidget* widget = mDocumentView->adapter()->widget();
		if (!widget) {
			return;
		}

		QPalette partPalette = widget->palette();
		partPalette.setBrush(widget->backgroundRole(), palette.base());
		partPalette.setBrush(widget->foregroundRole(), palette.text());
		widget->setPalette(partPalette);
	}

	void saveSplitterConfig() {
		if (mThumbnailBar->isVisible()) {
			GwenviewConfig::setThumbnailSplitterSizes(mThumbnailSplitter->sizes());
		}
	}

};


DocumentPanel::DocumentPanel(QWidget* parent, SlideShow* slideShow, KActionCollection* actionCollection)
: QWidget(parent)
, d(new DocumentPanelPrivate)
{
	d->that = this;
	d->mActionCollection = actionCollection;
	d->mFullScreenMode = false;
	d->mThumbnailBarVisibleBeforeFullScreen = false;
	d->mFullScreenPalette = QPalette(palette());
	d->mFullScreenPalette.setColor(QPalette::Base, Qt::black);
	d->mFullScreenPalette.setColor(QPalette::Text, Qt::white);

	QShortcut* toggleFullScreenShortcut = new QShortcut(this);
	toggleFullScreenShortcut->setKey(Qt::Key_Return);
	connect(toggleFullScreenShortcut, SIGNAL(activated()), SIGNAL(toggleFullScreenRequested()) );

	d->setupDocumentView(slideShow);

	d->setupStatusBar();

	d->setupAdapterContainer();

	d->setupThumbnailBar();

	d->setupSplitter();

	KActionCategory* view = new KActionCategory(i18nc("@title actions category - means actions changing smth in interface","View"), actionCollection);

	d->mToggleThumbnailBarAction = view->add<KToggleAction>(QString("toggle_thumbnailbar"));
	d->mToggleThumbnailBarAction->setText(i18n("Thumbnail Bar"));
	d->mToggleThumbnailBarAction->setIcon(KIcon("folder-image"));
	d->mToggleThumbnailBarAction->setShortcut(Qt::CTRL | Qt::Key_B);
	d->mToggleThumbnailBarAction->setChecked(GwenviewConfig::thumbnailBarIsVisible());
	connect(d->mToggleThumbnailBarAction, SIGNAL(triggered(bool)),
		this, SLOT(setThumbnailBarVisibility(bool)) );
	d->mToggleThumbnailBarButton->setDefaultAction(d->mToggleThumbnailBarAction);
}


DocumentPanel::~DocumentPanel() {
	delete d;
}


void DocumentPanel::loadConfig() {
	// FIXME: Not symetric with saveConfig(). Check if it matters.
	if (d->mDocumentView->adapter()) {
		d->mDocumentView->adapter()->loadConfig();
	}

	Qt::Orientation orientation = GwenviewConfig::thumbnailBarOrientation();
	d->mThumbnailSplitter->setOrientation(orientation == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
	d->mThumbnailBar->setOrientation(orientation);
	d->setupThumbnailBarStyleSheet();

	int oldRowCount = d->mThumbnailBar->rowCount();
	int newRowCount = GwenviewConfig::thumbnailBarRowCount();
	if (oldRowCount != newRowCount) {
		d->mThumbnailBar->setUpdatesEnabled(false);
		int gridSize = d->mThumbnailBar->gridSize().width();

		d->mThumbnailBar->setRowCount(newRowCount);

		// Adjust splitter to ensure thumbnail size remains the same
		int delta = (newRowCount - oldRowCount) * gridSize;
		QList<int> sizes = d->mThumbnailSplitter->sizes();
		Q_ASSERT(sizes.count() == 2);
		sizes[0] -= delta;
		sizes[1] += delta;
		d->mThumbnailSplitter->setSizes(sizes);

		d->mThumbnailBar->setUpdatesEnabled(true);
	}
}


void DocumentPanel::saveConfig() {
	d->saveSplitterConfig();
	GwenviewConfig::setThumbnailBarIsVisible(d->mToggleThumbnailBarAction->isChecked());
}


void DocumentPanel::setThumbnailBarVisibility(bool visible) {
	d->saveSplitterConfig();
	d->mThumbnailBar->setVisible(visible);
}


int DocumentPanel::statusBarHeight() const {
	return d->mStatusBarContainer->height();
}


void DocumentPanel::setFullScreenMode(bool fullScreenMode) {
	d->mFullScreenMode = fullScreenMode;
	d->mStatusBarContainer->setVisible(!fullScreenMode);
	d->applyPalette();
	if (fullScreenMode) {
		d->mThumbnailBarVisibleBeforeFullScreen = d->mToggleThumbnailBarAction->isChecked();
		if (d->mThumbnailBarVisibleBeforeFullScreen) {
			d->mToggleThumbnailBarAction->trigger();
		}
	} else if (d->mThumbnailBarVisibleBeforeFullScreen) {
		d->mToggleThumbnailBarAction->trigger();
	}
	d->mToggleThumbnailBarAction->setEnabled(!fullScreenMode);
}


bool DocumentPanel::isFullScreenMode() const {
	return d->mFullScreenMode;
}


ThumbnailBarView* DocumentPanel::thumbnailBar() const {
	return d->mThumbnailBar;
}


inline void addActionToMenu(KMenu* menu, KActionCollection* actionCollection, const char* name) {
	QAction* action = actionCollection->action(name);
	if (action) {
		menu->addAction(action);
	}
}


void DocumentPanel::showContextMenu() {
	KMenu menu(this);
	addActionToMenu(&menu, d->mActionCollection, "fullscreen");
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "go_previous");
	addActionToMenu(&menu, d->mActionCollection, "go_next");
	if (d->mDocumentView->adapter()->canZoom()) {
		menu.addSeparator();
		addActionToMenu(&menu, d->mActionCollection, "view_actual_size");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_to_fit");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_in");
		addActionToMenu(&menu, d->mActionCollection, "view_zoom_out");
	}
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "file_copy_to");
	addActionToMenu(&menu, d->mActionCollection, "file_move_to");
	addActionToMenu(&menu, d->mActionCollection, "file_link_to");
	menu.addSeparator();
	addActionToMenu(&menu, d->mActionCollection, "file_open_with");
	menu.exec(QCursor::pos());
}


QSize DocumentPanel::sizeHint() const {
	return QSize(400, 300);
}


KUrl DocumentPanel::url() const {
	if (!d->mDocumentView->adapter()) {
		LOG("!d->mDocumentView->adapter()");
		return KUrl();
	}

	if (!d->mDocumentView->adapter()->document()) {
		LOG("!d->mDocumentView->adapter()->document()");
		return KUrl();
	}

	return d->mDocumentView->adapter()->document()->url();
}


Document::Ptr DocumentPanel::currentDocument() const {
	if (!d->mDocumentView->adapter()) {
		return Document::Ptr();
	}

	return d->mDocumentView->adapter()->document();
}


bool DocumentPanel::isEmpty() const {
	return d->mDocumentView->isEmpty();
}


ImageView* DocumentPanel::imageView() const {
	return d->mDocumentView->adapter()->imageView();
}


DocumentView* DocumentPanel::documentView() const {
	return d->mDocumentView;
}


void DocumentPanel::setNormalPalette(const QPalette& palette) {
	d->mNormalPalette = palette;
	d->applyPalette();
	d->setupThumbnailBarStyleSheet();
}


void DocumentPanel::openUrl(const KUrl& url) {
	d->mDocumentView->openUrl(url);
}


void DocumentPanel::reload() {
	if (!d->mDocumentView->adapter()) {
		return;
	}
	Document::Ptr doc = d->mDocumentView->adapter()->document();
	if (!doc) {
		kWarning() << "!doc";
		return;
	}
	if (doc->isModified()) {
		KGuiItem cont = KStandardGuiItem::cont();
		cont.setText(i18nc("@action:button", "Discard Changes and Reload"));
		int answer = KMessageBox::warningContinueCancel(this,
			i18nc("@info", "This image has been modified. Reloading it will discard all your changes."),
			QString() /* caption */,
			cont);
		if (answer != KMessageBox::Continue) {
			return;
		}
	}
	doc->reload();
}


void DocumentPanel::reset() {
	d->mDocumentView->reset();
}


} // namespace
