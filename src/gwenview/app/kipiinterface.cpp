// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2000-2008 Aurélien Gâteau <agateau@kde.org>
Copyright 2008      Angelo Naselli  <anaselli@linux.it>

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
#include "kipiinterface.moc"

// Qt
#include <QList>
#include <QMenu>
#include <QRegExp>

// KDE
#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <kurl.h>
#include <kxmlguifactory.h>
#include <kdirlister.h>

// KIPI
#include <libkipi/imagecollectionshared.h>
#include <libkipi/imageinfo.h>
#include <libkipi/imageinfoshared.h>
#include <libkipi/plugin.h>
#include <libkipi/pluginloader.h>

// local
#include "mainwindow.h"
#include "contextmanager.h"
#include "kipiimagecollectionselector.h"
#include "kipiuploadwidget.h"
#include <lib/jpegcontent.h>
#include <lib/mimetypeutils.h>
#include <lib/semanticinfo/sorteddirmodel.h>

namespace Gwenview {
#undef ENABLE_LOG
#undef LOG

//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif


class KIPIImageInfo : public KIPI::ImageInfoShared {
	static const QRegExp sExtensionRE;
public:
	KIPIImageInfo(KIPI::Interface* interface, const KUrl& url) : KIPI::ImageInfoShared(interface, url) {}

	QString title() {
		QString txt=_url.fileName();
		txt.replace('_', ' ');
		txt.remove(sExtensionRE);
		return txt;
	}

	QString description() {
		if (!_url.isLocalFile()) return QString();

		JpegContent content;
		bool ok=content.load(_url.toLocalFile());
		if (!ok) return QString();

		return content.comment();
	}

	void setDescription(const QString&) {}

	int angle() {
		loadMetaInfo();

		if (!mMetaInfo.isValid()) {
			return 0;
		}

		const KFileMetaInfoItem& mii = mMetaInfo.item("http://freedesktop.org/standards/xesam/1.0/core#orientation");
		bool ok = false;
		const Orientation orientation = (Orientation)mii.value().toInt(&ok);
		if (!ok) {
			return 0;
		}

		switch(orientation) {
		case NOT_AVAILABLE:
		case NORMAL:
			return 0;

		case ROT_90:
			return 90;

		case ROT_180:
			return 180;

		case ROT_270:
			return 270;

		case HFLIP:
		case VFLIP:
		case TRANSPOSE:
		case TRANSVERSE:
			kWarning() << "Can't represent an orientation value of" << orientation << "as an angle (" << _url << ')';
			return 0;
		}

		kWarning() << "Don't know how to handle an orientation value of" << orientation << '(' << _url << ')';
		return 0;
	}

	QMap<QString,QVariant> attributes() {
		return QMap<QString,QVariant>();
	}

	void delAttributes( const QStringList& ) {}

	void clearAttributes() {}

	void addAttributes(const QMap<QString, QVariant>&) {}

private:
	KFileMetaInfo mMetaInfo;

	void loadMetaInfo() {
		if (!mMetaInfo.isValid()) {
			mMetaInfo = KFileMetaInfo(_url);
		}
	}
};

const QRegExp KIPIImageInfo::sExtensionRE("\\.[a-z0-9]+$", Qt::CaseInsensitive );


struct KIPIInterfacePrivate {
	KIPIInterface* that;
	MainWindow* mMainWindow;
	KIPI::PluginLoader* mPluginLoader;

	void setupPluginsMenu() {
		QMenu* menu = static_cast<QMenu*>(
			mMainWindow->factory()->container("plugins", mMainWindow));
		QObject::connect(menu, SIGNAL(aboutToShow()),
			that, SLOT(loadPlugins()) );
	}
};

KIPIInterface::KIPIInterface(MainWindow* mainWindow)
:KIPI::Interface(mainWindow)
, d(new KIPIInterfacePrivate) {
	d->that = this;
	d->mMainWindow = mainWindow;
	d->mPluginLoader = 0;

	d->setupPluginsMenu();
	QObject::connect(d->mMainWindow->contextManager(), SIGNAL(selectionChanged()),
		this, SLOT(slotSelectionChanged()) );
	QObject::connect(d->mMainWindow->contextManager(), SIGNAL(currentDirUrlChanged()),
		this, SLOT(slotDirectoryChanged()) );
#if 0
//TODO instead of delaying can we load them all at start-up to use actions somewhere else?
// delay a bit, so that it's called after loadPlugins()
	QTimer::singleShot( 0, this, SLOT( init()));
#endif
}


KIPIInterface::~KIPIInterface() {
	delete d;
}


void KIPIInterface::loadPlugins() {
	// Already done
	if (d->mPluginLoader) {
		return;
	}

	d->mPluginLoader = new KIPI::PluginLoader(QStringList(), this);
	connect(d->mPluginLoader, SIGNAL(replug()), SLOT( slotReplug()) );
	d->mPluginLoader->loadPlugins();
}


// Helper class for slotReplug(), gcc does not want to instantiate templates
// with local classes, so this is declared outside of slotReplug()
struct MenuInfo {
	QString mName;
	QList<QAction*> mActions;
	MenuInfo() {}
	MenuInfo(const QString& name) : mName(name) {}
};

void KIPIInterface::slotReplug() {
	typedef QMap<KIPI::Category, MenuInfo> CategoryMap;
	CategoryMap categoryMap;
	categoryMap[KIPI::ImagesPlugin]=MenuInfo("image_actions");
	categoryMap[KIPI::EffectsPlugin]=MenuInfo("effect_actions");
	categoryMap[KIPI::ToolsPlugin]=MenuInfo("tool_actions");
	categoryMap[KIPI::ImportPlugin]=MenuInfo("import_actions");
	categoryMap[KIPI::ExportPlugin]=MenuInfo("export_actions");
	categoryMap[KIPI::BatchPlugin]=MenuInfo("batch_actions");
	categoryMap[KIPI::CollectionsPlugin]=MenuInfo("collection_actions");

	// Fill the mActions
	KIPI::PluginLoader::PluginList pluginList = d->mPluginLoader->pluginList();
	Q_FOREACH(KIPI::PluginLoader::Info* pluginInfo, pluginList) {
		if (!pluginInfo->shouldLoad()) {
			continue;
		}
		KIPI::Plugin* plugin = pluginInfo->plugin();
		if (!plugin) {
			kWarning() << "Plugin from library" << pluginInfo->library() << "failed to load";
			continue;
		}

		plugin->setup(d->mMainWindow);
		QList<KAction*> actions = plugin->actions();
		Q_FOREACH(KAction* action, actions) {
			KIPI::Category category = plugin->category(action);

			if (!categoryMap.contains(category)) {
				kWarning() << "Unknown category '" << category;
				continue;
			}

			categoryMap[category].mActions << action;
		}
		// FIXME: Port
		//plugin->actionCollection()->readShortcutSettings();
	}

	// Create a dummy "no plugin" action list
	KAction* noPluginAction = d->mMainWindow->actionCollection()->add<KAction>("no_plugin");
	noPluginAction->setText(i18n("No Plugin"));
	noPluginAction->setShortcutConfigurable(false);
	noPluginAction->setEnabled(false);
	QList<QAction*> noPluginList;
	noPluginList << noPluginAction;

	// Fill the menu
	Q_FOREACH(const MenuInfo& info, categoryMap) {
		d->mMainWindow->unplugActionList(info.mName);
		if (info.mActions.count()>0) {
			d->mMainWindow->plugActionList(info.mName, info.mActions);
		} else {
			d->mMainWindow->plugActionList(info.mName, noPluginList);
		}
	}
}


void KIPIInterface::init() {
	slotDirectoryChanged();
	slotSelectionChanged();
}

KIPI::ImageCollection KIPIInterface::currentAlbum() {
	LOG("");
	const ContextManager* contextManager = d->mMainWindow->contextManager();
	const KUrl url = contextManager->currentDirUrl();
	const SortedDirModel* model = contextManager->dirModel();

	KUrl::List list;
	const int count = model->rowCount();
	for (int row = 0; row < count; ++row) {
		const QModelIndex& index = model->index(row, 0);
		const KFileItem item = model->itemForIndex(index);
		if (MimeTypeUtils::fileItemKind(item) == MimeTypeUtils::KIND_RASTER_IMAGE) {
			list << item.targetUrl();
		}
	}

	return KIPI::ImageCollection(new ImageCollection(url, url.fileName(), list));
}


KIPI::ImageCollection KIPIInterface::currentSelection() {
	LOG("");

	KFileItemList fileList = d->mMainWindow->contextManager()->selection();
	KUrl::List list = fileList.urlList();
	KUrl url = d->mMainWindow->contextManager()->currentUrl();
	
	return KIPI::ImageCollection(new ImageCollection(url, url.fileName(), list));
}


QList<KIPI::ImageCollection> KIPIInterface::allAlbums() {
	LOG("");
	QList<KIPI::ImageCollection> list;
	list << currentAlbum() << currentSelection();
	return list;
}


KIPI::ImageInfo KIPIInterface::info(const KUrl& url) {
	LOG("");
	return KIPI::ImageInfo( new KIPIImageInfo(this, url) );
}

int KIPIInterface::features() const {
	return KIPI::HostAcceptNewImages;
}

/**
 * KDirLister will pick up the image if necessary, so no updating is needed
 * here, it is however necessary to discard caches if the plugin preserves timestamp
 */
bool KIPIInterface::addImage(const KUrl&, QString&) {
//TODO	setContext(const KUrl& currentUrl, const KFileItemList& selection)?
	//Cache::instance()->invalidate( url );
	return true;
}

void KIPIInterface::delImage(const KUrl&) {
//TODO
}

void KIPIInterface::refreshImages( const KUrl::List&) {
// TODO
}

KIPI::ImageCollectionSelector* KIPIInterface::imageCollectionSelector(QWidget *parent) {
	return new KIPIImageCollectionSelector(this, parent);
}

KIPI::UploadWidget* KIPIInterface::uploadWidget(QWidget *parent) {
	return (new KIPIUploadWidget(this, parent));
}

void KIPIInterface::slotSelectionChanged() {
	emit selectionChanged(d->mMainWindow->contextManager()->selection().count() >0);
}


void KIPIInterface::slotDirectoryChanged() {
	emit currentAlbumChanged(true);
}

} //namespace
