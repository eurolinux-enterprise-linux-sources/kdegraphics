/***************************************************************************
 *   Copyright (C) 2002 by Wilco Greven <greven@kde.org>                   *
 *   Copyright (C) 2002 by Chris Cheney <ccheney@cheney.cx>                *
 *   Copyright (C) 2002 by Malcolm Hunter <malcolm.hunter@gmx.co.uk>       *
 *   Copyright (C) 2003-2004 by Christophe Devriese                        *
 *                         <Christophe.Devriese@student.kuleuven.ac.be>    *
 *   Copyright (C) 2003 by Daniel Molkentin <molkentin@kde.org>            *
 *   Copyright (C) 2003 by Andy Goossens <andygoossens@telenet.be>         *
 *   Copyright (C) 2003 by Dirk Mueller <mueller@kde.org>                  *
 *   Copyright (C) 2003 by Laurent Montel <montel@kde.org>                 *
 *   Copyright (C) 2004 by Dominique Devriese <devriese@kde.org>           *
 *   Copyright (C) 2004 by Christoph Cullmann <crossfire@babylon2k.de>     *
 *   Copyright (C) 2004 by Henrique Pinto <stampede@coltec.ufmg.br>        *
 *   Copyright (C) 2004 by Waldo Bastian <bastian@kde.org>                 *
 *   Copyright (C) 2004-2008 by Albert Astals Cid <aacid@kde.org>          *
 *   Copyright (C) 2004 by Antti Markus <antti.markus@starman.ee>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "part.h"

// qt/kde includes
#include <qapplication.h>
#include <qfile.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qtimer.h>
#include <QtGui/QPrinter>
#include <QtGui/QPrintDialog>

#include <kvbox.h>
#include <kaboutapplicationdialog.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kdirwatch.h>
#include <kstandardaction.h>
#include <kparts/genericfactory.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <knuminput.h>
#include <kio/netaccess.h>
#include <kmenu.h>
#include <kxmlguiclient.h>
#include <kxmlguifactory.h>
#include <kservicetypetrader.h>
#include <kstandarddirs.h>
#include <kstandardshortcut.h>
#include <ktemporaryfile.h>
#include <ktoggleaction.h>
#include <ktogglefullscreenaction.h>
#include <kio/job.h>
#include <kicon.h>
#include <kfilterdev.h>
#include <knewstuff2/engine.h>
#include <kdeprintdialog.h>
#include <kprintpreview.h>

// local includes
#include "aboutdata.h"
#include "extensions.h"
#include "ui/pageview.h"
#include "ui/toc.h"
#include "ui/searchwidget.h"
#include "ui/thumbnaillist.h"
#include "ui/side_reviews.h"
#include "ui/minibar.h"
#include "ui/embeddedfilesdialog.h"
#include "ui/propertiesdialog.h"
#include "ui/presentationwidget.h"
#include "ui/pagesizelabel.h"
#include "ui/bookmarklist.h"
#include "ui/findbar.h"
#include "ui/sidebar.h"
#include "ui/fileprinterpreview.h"
#include "ui/guiutils.h"
#include "conf/preferencesdialog.h"
#include "settings.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"
#include "core/generator.h"
#include "core/page.h"
#include "core/fileprinter.h"

#include <cstdio>

class FileKeeper
{
    public:
        FileKeeper()
            : m_handle( NULL )
        {
        }

        ~FileKeeper()
        {
        }

        void open( const QString & path )
        {
            if ( !m_handle )
                m_handle = std::fopen( QFile::encodeName( path ), "r" );
        }

        void close()
        {
            if ( m_handle )
            {
                int ret = std::fclose( m_handle );
                Q_UNUSED( ret )
                m_handle = NULL;
            }
        }

        KTemporaryFile* copyToTemporary() const
        {
            if ( !m_handle )
                return 0;

            KTemporaryFile * retFile = new KTemporaryFile;
            retFile->open();

            std::rewind( m_handle );
            int c = -1;
            do
            {
                c = std::fgetc( m_handle );
                if ( c == EOF )
                    break;
                if ( !retFile->putChar( (char)c ) )
                    break;
            } while ( !feof( m_handle ) );

            retFile->flush();

            return retFile;
        }

    private:
        std::FILE * m_handle;
};

K_PLUGIN_FACTORY( okularPartFactory, registerPlugin< Okular::Part >(); )
K_EXPORT_PLUGIN( okularPartFactory( okularAboutData( "okular", I18N_NOOP( "Okular" ) ) ) )

static QAction* actionForExportFormat( const Okular::ExportFormat& format, QObject *parent = 0 )
{
    QAction *act = new QAction( format.description(), parent );
    if ( !format.icon().isNull() )
    {
        act->setIcon( format.icon() );
    }
    return act;
}

static QString compressedMimeFor( const QString& mime_to_check )
{
    static QHash< QString, QString > compressedMimeMap;
    if ( compressedMimeMap.isEmpty() )
    {
        compressedMimeMap[ QString::fromLatin1( "application/x-gzip" ) ] =
            QString::fromLatin1( "application/x-gzip" );
        compressedMimeMap[ QString::fromLatin1( "application/x-bzip" ) ] =
            QString::fromLatin1( "application/x-bzip" );
        compressedMimeMap[ QString::fromLatin1( "application/x-bzpdf" ) ] =
            QString::fromLatin1( "application/x-bzip" );
        compressedMimeMap[ QString::fromLatin1( "application/x-bzpostscript" ) ] =
            QString::fromLatin1( "application/x-bzip" );
        compressedMimeMap[ QString::fromLatin1( "application/x-bzdvi" ) ] =
            QString::fromLatin1( "application/x-bzip" );
        compressedMimeMap[ QString::fromLatin1( "image/x-gzeps" ) ] =
            QString::fromLatin1( "application/x-gzip" );
        compressedMimeMap[ QString::fromLatin1( "image/x-bzeps" ) ] =
            QString::fromLatin1( "application/x-bzip" );
    }
    QHash< QString, QString >::const_iterator it = compressedMimeMap.constFind( mime_to_check );
    if ( it != compressedMimeMap.constEnd() )
        return it.value();

    return QString();
}

static Okular::Part::EmbedMode detectEmbedMode( QWidget *parentWidget, QObject *parent, const QVariantList &args )
{
    Q_UNUSED( parentWidget );

    if ( parent
         && ( parent->objectName() == QLatin1String( "okular::Shell" )
              || parent->objectName() == QLatin1String( "okular/okular__Shell" ) ) )
        return Okular::Part::NativeShellMode;

    if ( parent
         && ( QByteArray( "KHTMLPart" ) == parent->metaObject()->className() ) )
        return Okular::Part::KHTMLPartMode;

    Q_FOREACH ( const QVariant &arg, args )
    {
        if ( arg.type() == QVariant::String )
        {
            if ( arg.toString() == QLatin1String( "Print/Preview" ) )
                return Okular::Part::PrintPreviewMode;
        }
    }

    return Okular::Part::UnknownEmbedMode;
}

#undef OKULAR_KEEP_FILE_OPEN

#ifdef OKULAR_KEEP_FILE_OPEN
static bool keepFileOpen()
{
    static bool keep_file_open = !qgetenv("OKULAR_NO_KEEP_FILE_OPEN").toInt();
    return keep_file_open;
}
#endif

namespace Okular
{

Part::Part(QWidget *parentWidget,
QObject *parent,
const QVariantList &args )
: KParts::ReadOnlyPart(parent),
m_tempfile( 0 ), m_fileWasRemoved( false ), m_showMenuBarAction( 0 ), m_showFullScreenAction( 0 ), m_actionsSearched( false ),
m_cliPresentation(false), m_embedMode(detectEmbedMode(parentWidget, parent, args)), m_generatorGuiClient(0), m_keeper( 0 )
{
    // first necessary step: copy the configuration from kpdf, if available
    QString newokularconffile = KStandardDirs::locateLocal( "config", "okularpartrc" );
    if ( !QFile::exists( newokularconffile ) )
    {
        QString oldkpdfconffile = KStandardDirs::locateLocal( "config", "kpdfpartrc" );
        if ( QFile::exists( oldkpdfconffile ) )
            QFile::copy( oldkpdfconffile, newokularconffile );
    }

    QDBusConnection::sessionBus().registerObject("/okular", this, QDBusConnection::ExportScriptableSlots);

    // connect the started signal to tell the job the mimetypes we like
    connect(this, SIGNAL(started(KIO::Job *)), this, SLOT(setMimeTypes(KIO::Job *)));

    // connect the completed signal so we can put the window caption when loading remote files
    connect(this, SIGNAL(completed()), this, SLOT(setWindowTitleFromDocument()));
    connect(this, SIGNAL(canceled(const QString &)), this, SLOT(loadCancelled(const QString &)));

    // create browser extension (for printing when embedded into browser)
    m_bExtension = new BrowserExtension(this);
    // create live connect extension (for integrating with browser scripting)
    new OkularLiveConnectExtension( this );

    // we need an instance
    setComponentData(okularPartFactory::componentData());

    GuiUtils::setIconLoader( iconLoader() );

    m_sidebar = new Sidebar( parentWidget );
    setWidget( m_sidebar );

    // build the document
    m_document = new Okular::Document(widget());
    connect( m_document, SIGNAL( linkFind() ), this, SLOT( slotFind() ) );
    connect( m_document, SIGNAL( linkGoToPage() ), this, SLOT( slotGoToPage() ) );
    connect( m_document, SIGNAL( linkPresentation() ), this, SLOT( slotShowPresentation() ) );
    connect( m_document, SIGNAL( linkEndPresentation() ), this, SLOT( slotHidePresentation() ) );
    connect( m_document, SIGNAL( openUrl(const KUrl &) ), this, SLOT( openUrlFromDocument(const KUrl &) ) );
    connect( m_document->bookmarkManager(), SIGNAL( openUrl(const KUrl &) ), this, SLOT( openUrlFromBookmarks(const KUrl &) ) );
    connect( m_document, SIGNAL( close() ), this, SLOT( close() ) );

    if ( parent && parent->metaObject()->indexOfSlot( QMetaObject::normalizedSignature( "slotQuit()" ) ) != -1 )
        connect( m_document, SIGNAL( quit() ), parent, SLOT( slotQuit() ) );
    else
        connect( m_document, SIGNAL( quit() ), this, SLOT( cannotQuit() ) );
    // widgets: ^searchbar (toolbar containing label and SearchWidget)
    //      m_searchToolBar = new KToolBar( parentWidget, "searchBar" );
    //      m_searchToolBar->boxLayout()->setSpacing( KDialog::spacingHint() );
    //      QLabel * sLabel = new QLabel( i18n( "&Search:" ), m_searchToolBar, "kde toolbar widget" );
    //      m_searchWidget = new SearchWidget( m_searchToolBar, m_document );
    //      sLabel->setBuddy( m_searchWidget );
    //      m_searchToolBar->setStretchableWidget( m_searchWidget );

    int tbIndex;
    // [left toolbox: Table of Contents] | []
    m_toc = new TOC( 0, m_document );
    connect( m_toc, SIGNAL( hasTOC( bool ) ), this, SLOT( enableTOC( bool ) ) );
    tbIndex = m_sidebar->addItem( m_toc, KIcon(QApplication::isLeftToRight() ? "format-justify-left" : "format-justify-right"), i18n("Contents") );
    enableTOC( false );

    // [left toolbox: Thumbnails and Bookmarks] | []
    KVBox * thumbsBox = new ThumbnailsBox( 0 );
    thumbsBox->setSpacing( 6 );
    m_searchWidget = new SearchWidget( thumbsBox, m_document );
    m_thumbnailList = new ThumbnailList( thumbsBox, m_document );
    //	ThumbnailController * m_tc = new ThumbnailController( thumbsBox, m_thumbnailList );
    connect( m_thumbnailList, SIGNAL( urlDropped( const KUrl& ) ), SLOT( openUrlFromDocument( const KUrl & )) );
    connect( m_thumbnailList, SIGNAL( rightClick(const Okular::Page *, const QPoint &) ), this, SLOT( slotShowMenu(const Okular::Page *, const QPoint &) ) );
    tbIndex = m_sidebar->addItem( thumbsBox, KIcon( "view-preview" ), i18n("Thumbnails") );
    m_sidebar->setCurrentIndex( tbIndex );

    // [left toolbox: Reviews] | []
    m_reviewsWidget = new Reviews( 0, m_document );
    m_sidebar->addItem( m_reviewsWidget, KIcon("draw-freehand"), i18n("Reviews") );
    m_sidebar->setItemEnabled( 2, false );

    // [left toolbox: Bookmarks] | []
    m_bookmarkList = new BookmarkList( m_document, 0 );
    m_sidebar->addItem( m_bookmarkList, KIcon("bookmarks"), i18n("Bookmarks") );
    m_sidebar->setItemEnabled( 3, false );

    // widgets: [../miniBarContainer] | []
#ifdef OKULAR_ENABLE_MINIBAR
    QWidget * miniBarContainer = new QWidget( 0 );
    m_sidebar->setBottomWidget( miniBarContainer );
    QVBoxLayout * miniBarLayout = new QVBoxLayout( miniBarContainer );
    miniBarLayout->setMargin( 0 );
    // widgets: [../[spacer/..]] | []
    miniBarLayout->addItem( new QSpacerItem( 6, 6, QSizePolicy::Fixed, QSizePolicy::Fixed ) );
    // widgets: [../[../MiniBar]] | []
    QFrame * bevelContainer = new QFrame( miniBarContainer );
    bevelContainer->setFrameStyle( QFrame::StyledPanel | QFrame::Sunken );
    QVBoxLayout * bevelContainerLayout = new QVBoxLayout( bevelContainer );
    bevelContainerLayout->setMargin( 4 );
    m_progressWidget = new ProgressWidget( bevelContainer, m_document );
    bevelContainerLayout->addWidget( m_progressWidget );
    miniBarLayout->addWidget( bevelContainer );
    miniBarLayout->addItem( new QSpacerItem( 6, 6, QSizePolicy::Fixed, QSizePolicy::Fixed ) );
#endif

    // widgets: [] | [right 'pageView']
    QWidget * rightContainer = new QWidget( 0 );
    m_sidebar->setMainWidget( rightContainer );
    QVBoxLayout * rightLayout = new QVBoxLayout( rightContainer );
    rightLayout->setMargin( 0 );
    rightLayout->setSpacing( 0 );
    //	KToolBar * rtb = new KToolBar( rightContainer, "mainToolBarSS" );
    //	rightLayout->addWidget( rtb );
    m_topMessage = new PageViewTopMessage( rightContainer );
    m_topMessage->setup( i18n( "This document has embedded files. <a href=\"okular:/embeddedfiles\">Click here to see them</a> or go to File -> Embedded Files." ), KIcon( "mail-attachment" ) );
    connect( m_topMessage, SIGNAL( action() ), this, SLOT( slotShowEmbeddedFiles() ) );
    rightLayout->addWidget( m_topMessage );
    m_formsMessage = new PageViewTopMessage( rightContainer );
    m_formsMessage->setup( i18n( "This document has forms. Click on the button to interact with them, or use View -> Show Forms." ) );
    rightLayout->addWidget( m_formsMessage );
    m_pageView = new PageView( rightContainer, m_document );
    m_pageView->setFocus();      //usability setting
//    m_splitter->setFocusProxy(m_pageView);
    connect( m_pageView, SIGNAL( urlDropped( const KUrl& ) ), SLOT( openUrlFromDocument( const KUrl & )));
    connect( m_pageView, SIGNAL( rightClick(const Okular::Page *, const QPoint &) ), this, SLOT( slotShowMenu(const Okular::Page *, const QPoint &) ) );
    connect( m_document, SIGNAL( error( const QString&, int ) ), m_pageView, SLOT( errorMessage( const QString&, int ) ) );
    connect( m_document, SIGNAL( warning( const QString&, int ) ), m_pageView, SLOT( warningMessage( const QString&, int ) ) );
    connect( m_document, SIGNAL( notice( const QString&, int ) ), m_pageView, SLOT( noticeMessage( const QString&, int ) ) );
    rightLayout->addWidget( m_pageView );
    m_findBar = new FindBar( m_document, rightContainer );
    rightLayout->addWidget( m_findBar );
    QWidget * bottomBar = new QWidget( rightContainer );
    QHBoxLayout * bottomBarLayout = new QHBoxLayout( bottomBar );
    m_pageSizeLabel = new PageSizeLabel( bottomBar, m_document );
    bottomBarLayout->setMargin( 0 );
    bottomBarLayout->setSpacing( 0 );
    bottomBarLayout->addWidget( m_pageSizeLabel->antiWidget() );
    bottomBarLayout->addItem( new QSpacerItem( 5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum ) );
    m_miniBar = new MiniBar( bottomBar, m_document );
    bottomBarLayout->addWidget( m_miniBar );
    bottomBarLayout->addItem( new QSpacerItem( 5, 5, QSizePolicy::Expanding, QSizePolicy::Minimum ) );
    bottomBarLayout->addWidget( m_pageSizeLabel );
    rightLayout->addWidget( bottomBar );

    connect( m_reviewsWidget, SIGNAL( setAnnotationWindow( Okular::Annotation* ) ),
        m_pageView, SLOT( setAnnotationWindow( Okular::Annotation* ) ) );
    connect( m_reviewsWidget, SIGNAL( removeAnnotationWindow( Okular::Annotation* ) ),
        m_pageView, SLOT( removeAnnotationWindow( Okular::Annotation* ) ) );

    // add document observers
    m_document->addObserver( this );
    m_document->addObserver( m_thumbnailList );
    m_document->addObserver( m_pageView );
    m_document->registerView( m_pageView );
    m_document->addObserver( m_toc );
    m_document->addObserver( m_miniBar );
#ifdef OKULAR_ENABLE_MINIBAR
    m_document->addObserver( m_progressWidget );
#endif
    m_document->addObserver( m_reviewsWidget );
    m_document->addObserver( m_pageSizeLabel );
    m_document->addObserver( m_bookmarkList );

    connect( m_document->bookmarkManager(), SIGNAL( saved() ),
        this, SLOT( slotRebuildBookmarkMenu() ) );

    // ACTIONS
    KActionCollection * ac = actionCollection();

    // Page Traversal actions
    m_gotoPage = KStandardAction::gotoPage( this, SLOT( slotGoToPage() ), ac );
    m_gotoPage->setShortcut( QKeySequence(Qt::CTRL + Qt::Key_G) );
    // dirty way to activate gotopage when pressing miniBar's button
    connect( m_miniBar, SIGNAL( gotoPage() ), m_gotoPage, SLOT( trigger() ) );

    m_prevPage = KStandardAction::prior(this, SLOT(slotPreviousPage()), ac);
    m_prevPage->setIconText( i18nc( "Previous page", "Previous" ) );
    m_prevPage->setToolTip( i18n( "Go back to the Previous Page" ) );
    m_prevPage->setWhatsThis( i18n( "Moves to the previous page of the document" ) );
    m_prevPage->setShortcut( 0 );
    // dirty way to activate prev page when pressing miniBar's button
    connect( m_miniBar, SIGNAL( prevPage() ), m_prevPage, SLOT( trigger() ) );
#ifdef OKULAR_ENABLE_MINIBAR
    connect( m_progressWidget, SIGNAL( prevPage() ), m_prevPage, SLOT( trigger() ) );
#endif

    m_nextPage = KStandardAction::next(this, SLOT(slotNextPage()), ac );
    m_nextPage->setIconText( i18nc( "Next page", "Next" ) );
    m_nextPage->setToolTip( i18n( "Advance to the Next Page" ) );
    m_nextPage->setWhatsThis( i18n( "Moves to the next page of the document" ) );
    m_nextPage->setShortcut( 0 );
    // dirty way to activate next page when pressing miniBar's button
    connect( m_miniBar, SIGNAL( nextPage() ), m_nextPage, SLOT( trigger() ) );
#ifdef OKULAR_ENABLE_MINIBAR
    connect( m_progressWidget, SIGNAL( nextPage() ), m_nextPage, SLOT( trigger() ) );
#endif

    m_firstPage = KStandardAction::firstPage( this, SLOT( slotGotoFirst() ), ac );
    ac->addAction("first_page", m_firstPage);
    m_firstPage->setWhatsThis( i18n( "Moves to the first page of the document" ) );

    m_lastPage = KStandardAction::lastPage( this, SLOT( slotGotoLast() ), ac );
    ac->addAction("last_page",m_lastPage);
    m_lastPage->setWhatsThis( i18n( "Moves to the last page of the document" ) );

    // we do not want back and next in history in the dummy mode
    m_historyBack = 0;
    m_historyNext = 0;

    m_addBookmark = KStandardAction::addBookmark( this, SLOT( slotAddBookmark() ), ac );
    m_addBookmarkText = m_addBookmark->text();
    m_addBookmarkIcon = m_addBookmark->icon();

    m_prevBookmark = ac->addAction("previous_bookmark");
    m_prevBookmark->setText(i18n( "Previous Bookmark" ));
    m_prevBookmark->setIcon(KIcon( "go-up-search" ));
    m_prevBookmark->setWhatsThis( i18n( "Go to the previous bookmarked page" ) );
    connect( m_prevBookmark, SIGNAL( triggered() ), this, SLOT( slotPreviousBookmark() ) );

    m_nextBookmark = ac->addAction("next_bookmark");
    m_nextBookmark->setText(i18n( "Next Bookmark" ));
    m_nextBookmark->setIcon(KIcon( "go-down-search" ));
    m_nextBookmark->setWhatsThis( i18n( "Go to the next bookmarked page" ) );
    connect( m_nextBookmark, SIGNAL( triggered() ), this, SLOT( slotNextBookmark() ) );

    m_copy = KStandardAction::create( KStandardAction::Copy, m_pageView, SLOT( copyTextSelection() ), ac );

    m_selectAll = KStandardAction::selectAll( m_pageView, SLOT( selectAll() ), ac );

    // Find and other actions
    m_find = KStandardAction::find( this, SLOT( slotShowFindBar() ), ac );
    QList<QKeySequence> s = m_find->shortcuts();
    s.append( QKeySequence( Qt::Key_Slash ) );
    m_find->setShortcuts( s );
    m_find->setEnabled( false );

    m_findNext = KStandardAction::findNext( this, SLOT( slotFindNext() ), ac);
    m_findNext->setEnabled( false );

    m_findPrev = KStandardAction::findPrev( this, SLOT( slotFindPrev() ), ac );
    m_findPrev->setEnabled( false );

    m_saveCopyAs = KStandardAction::saveAs( this, SLOT( slotSaveCopyAs() ), ac );
    m_saveCopyAs->setText( i18n( "Save &Copy As..." ) );
    ac->addAction( "file_save_copy", m_saveCopyAs );
    m_saveCopyAs->setEnabled( false );

    m_saveAs = KStandardAction::saveAs( this, SLOT( slotSaveFileAs() ), ac );
    m_saveAs->setEnabled( false );

    QAction * prefs = KStandardAction::preferences( this, SLOT( slotPreferences() ), ac);
    if ( m_embedMode == NativeShellMode )
    {
        prefs->setText( i18n( "Configure Okular..." ) );
    }
    else
    {
        // TODO: improve this message
        prefs->setText( i18n( "Configure Viewer..." ) );
    }

    KAction * genPrefs = new KAction( ac );
    ac->addAction("options_configure_generators", genPrefs);
    genPrefs->setText( i18n( "Configure Backends..." ) );
    genPrefs->setIcon( KIcon( "configure" ) );
    genPrefs->setEnabled( m_document->configurableGenerators() > 0 );
    connect( genPrefs, SIGNAL( triggered( bool ) ), this, SLOT( slotGeneratorPreferences() ) );

    m_printPreview = KStandardAction::printPreview( this, SLOT( slotPrintPreview() ), ac );
    m_printPreview->setEnabled( false );

    m_showLeftPanel = ac->add<KToggleAction>("show_leftpanel");
    m_showLeftPanel->setText(i18n( "Show &Navigation Panel"));
    m_showLeftPanel->setIcon(KIcon( "view-sidetree" ));
    connect( m_showLeftPanel, SIGNAL( toggled( bool ) ), this, SLOT( slotShowLeftPanel() ) );
    m_showLeftPanel->setShortcut( Qt::Key_F7 );
    m_showLeftPanel->setChecked( Okular::Settings::showLeftPanel() );
    slotShowLeftPanel();

    QAction * importPS = ac->addAction("import_ps");
    importPS->setText(i18n("&Import PostScript as PDF..."));
    importPS->setIcon(KIcon("document-import"));
    connect(importPS, SIGNAL(triggered()), this, SLOT(slotImportPSFile()));
#if 0
    QAction * ghns = ac->addAction("get_new_stuff");
    ghns->setText(i18n("&Get Books From Internet..."));
    ghns->setIcon(KIcon("get-hot-new-stuff"));
    connect(ghns, SIGNAL(triggered()), this, SLOT(slotGetNewStuff()));
    // TEMP, REMOVE ME!
    ghns->setShortcut( Qt::Key_G );
#endif

    m_showProperties = ac->addAction("properties");
    m_showProperties->setText(i18n("&Properties"));
    m_showProperties->setIcon(KIcon("document-properties"));
    connect(m_showProperties, SIGNAL(triggered()), this, SLOT(slotShowProperties()));
    m_showProperties->setEnabled( false );

    m_showEmbeddedFiles = ac->addAction("embedded_files");
    m_showEmbeddedFiles->setText(i18n("&Embedded Files"));
    m_showEmbeddedFiles->setIcon( KIcon( "mail-attachment" ) );
    connect(m_showEmbeddedFiles, SIGNAL(triggered()), this, SLOT(slotShowEmbeddedFiles()));
    m_showEmbeddedFiles->setEnabled( false );

    m_showPresentation = ac->addAction("presentation");
    m_showPresentation->setText(i18n("P&resentation"));
    m_showPresentation->setIcon( KIcon( "view-presentation" ) );
    connect(m_showPresentation, SIGNAL(triggered()), this, SLOT(slotShowPresentation()));
    m_showPresentation->setShortcut( QKeySequence( Qt::CTRL + Qt::SHIFT + Qt::Key_P ) );
    m_showPresentation->setEnabled( false );

    m_exportAs = ac->addAction("file_export_as");
    m_exportAs->setText(i18n("E&xport As"));
    m_exportAs->setIcon( KIcon( "document-export" ) );
    m_exportAsMenu = new QMenu();
    connect(m_exportAsMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotExportAs(QAction *)));
    m_exportAs->setMenu( m_exportAsMenu );
    m_exportAsText = actionForExportFormat( Okular::ExportFormat::standardFormat( Okular::ExportFormat::PlainText ), m_exportAsMenu );
    m_exportAsMenu->addAction( m_exportAsText );
    m_exportAs->setEnabled( false );
    m_exportAsText->setEnabled( false );
    m_exportAsDocArchive = actionForExportFormat( Okular::ExportFormat(
            i18nc( "A document format, Okular-specific", "Document Archive" ),
            KMimeType::mimeType( "application/vnd.kde.okular-archive" ) ), m_exportAsMenu );
    m_exportAsMenu->addAction( m_exportAsDocArchive );
    m_exportAsDocArchive->setEnabled( false );

    m_aboutBackend = ac->addAction("help_about_backend");
    m_aboutBackend->setText(i18n("About Backend"));
    m_aboutBackend->setEnabled( false );
    connect(m_aboutBackend, SIGNAL(triggered()), this, SLOT(slotAboutBackend()));

    KAction *reload = ac->add<KAction>( "file_reload" );
    reload->setText( i18n( "Reloa&d" ) );
    reload->setIcon( KIcon( "view-refresh" ) );
    reload->setWhatsThis( i18n( "Reload the current document from disk." ) );
    connect( reload, SIGNAL(triggered()), this, SLOT(slotReload()) );
    reload->setShortcut( KStandardShortcut::reload() );
    m_reload = reload;

    KAction *closeFindBar = new KAction( i18n( "Close &Find Bar" ), ac );
    ac->addAction("close_find_bar", closeFindBar);
    connect(closeFindBar, SIGNAL(triggered()), this, SLOT(slotHideFindBar()));
    closeFindBar->setShortcut( QKeySequence( Qt::Key_Escape ) );
    widget()->addAction(closeFindBar);

    KToggleAction *blackscreenAction = new KToggleAction( i18n( "Switch Blackscreen Mode" ), ac );
    ac->addAction( "switch_blackscreen_mode", blackscreenAction );
    blackscreenAction->setShortcut( QKeySequence( Qt::Key_B ) );
    blackscreenAction->setIcon( KIcon( "view-presentation" ) );

    KToggleAction *drawingAction = new KToggleAction( i18n( "Toggle Drawing Mode" ), ac );
    ac->addAction( "presentation_drawing_mode", drawingAction );
    drawingAction->setIcon( KIcon( "draw-freehand" ) );

    KAction *eraseDrawingAction = new KAction( i18n( "Erase Drawings" ), ac );
    ac->addAction( "presentation_erase_drawings", eraseDrawingAction );
    eraseDrawingAction->setIcon( KIcon( "draw-eraser" ) );

    // document watcher and reloader
    m_watcher = new KDirWatch( this );
    connect( m_watcher, SIGNAL( dirty( const QString& ) ), this, SLOT( slotFileDirty( const QString& ) ) );
    m_dirtyHandler = new QTimer( this );
    m_dirtyHandler->setSingleShot( true );
    connect( m_dirtyHandler, SIGNAL( timeout() ),this, SLOT( slotDoFileDirty() ) );

    slotNewConfig();

    // [SPEECH] check for KTTSD presence and usability
    KService::List offers = KServiceTypeTrader::self()->query("DBUS/Text-to-Speech", "Name == 'KTTSD'");
    Okular::Settings::setUseKTTSD( !offers.isEmpty() );
    Okular::Settings::self()->writeConfig();

    rebuildBookmarkMenu( false );

    // set our XML-UI resource file
    setXMLFile("part.rc");

    m_pageView->setupBaseActions( actionCollection() );

    // ensure history actions are in the correct state
    updateViewActions();

    m_sidebar->setSidebarVisibility( false );
    if ( m_embedMode != PrintPreviewMode )
        unsetDummyMode();

#ifdef OKULAR_KEEP_FILE_OPEN
    m_keeper = new FileKeeper();
#endif
}


Part::~Part()
{
    m_document->removeObserver( this );

    if ( m_document->isOpened() )
        Part::closeUrl();

    delete m_toc;
    delete m_pageView;
    delete m_thumbnailList;
    delete m_miniBar;
#ifdef OKULAR_ENABLE_MINIBAR
    delete m_progressWidget;
#endif
    delete m_pageSizeLabel;
    delete m_reviewsWidget;
    delete m_bookmarkList;

    delete m_document;

    delete m_tempfile;

    qDeleteAll( m_bookmarkActions );

    delete m_exportAsMenu;

#ifdef OKULAR_KEEP_FILE_OPEN
    delete m_keeper;
#endif
}


bool Part::openDocument(const KUrl& url, uint page)
{
    Okular::DocumentViewport vp( page - 1 );
    vp.rePos.enabled = true;
    vp.rePos.normalizedX = 0;
    vp.rePos.normalizedY = 0;
    vp.rePos.pos = Okular::DocumentViewport::TopLeft;
    if ( vp.isValid() )
        m_document->setNextDocumentViewport( vp );
    return openUrl( url );
}


void Part::startPresentation()
{
    m_cliPresentation = true;
}


QStringList Part::supportedMimeTypes() const
{
    return m_document->supportedMimeTypes();
}


KUrl Part::realUrl() const
{
    if ( !m_realUrl.isEmpty() )
        return m_realUrl;

    return url();
}


void Part::openUrlFromDocument(const KUrl &url)
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    m_bExtension->openUrlNotify();
    m_bExtension->setLocationBarUrl(url.prettyUrl());
    openUrl(url);
}

void Part::openUrlFromBookmarks(const KUrl &_url)
{
    KUrl url = _url;
    Okular::DocumentViewport vp( _url.htmlRef() );
    if ( vp.isValid() )
        m_document->setNextDocumentViewport( vp );
    url.setHTMLRef( QString() );
    if ( m_document->currentDocument() == url )
    {
        if ( vp.isValid() )
            m_document->setViewport( vp );
    }
    else
        openUrl( url );
}

void Part::setMimeTypes(KIO::Job *job)
{
    if (job)
    {
        QStringList supportedMimeTypes = m_document->supportedMimeTypes();
        job->addMetaData("accept", supportedMimeTypes.join(", ") + ", */*;q=0.5");
    }
}

void Part::loadCancelled(const QString &reason)
{
    emit setWindowCaption( QString() );

    // when m_viewportDirty.pageNumber != -1 we come from slotDoFileDirty
    // so we don't want to show an ugly messagebox just because the document is
    // taking more than usual to be recreated
    if (m_viewportDirty.pageNumber == -1)
    {
        if (!reason.isEmpty())
        {
            KMessageBox::error( widget(), i18n("Could not open %1. Reason: %2", url().prettyUrl(), reason ) );
        }
        else
        {
            KMessageBox::error( widget(), i18n("Could not open %1", url().prettyUrl() ) );
        }
    }
}

void Part::setWindowTitleFromDocument()
{
    // If 'DocumentTitle' should be used, check if the document has one. If
    // either case is false, use the file name.
    QString title = realUrl().fileName();

    if ( Okular::Settings::displayDocumentTitle() )
    {
        const QString docTitle = m_document->metaData( "DocumentTitle" ).toString();
        if ( !docTitle.isEmpty() && !docTitle.trimmed().isEmpty() )
        {
             title = docTitle;
        }
    }

    emit setWindowCaption( title );
}

void Part::slotGeneratorPreferences( )
{
    // an instance the dialog could be already created and could be cached,
    // in which case you want to display the cached dialog
    if ( KConfigDialog::showDialog( "generator_prefs" ) )
        return;

    // we didn't find an instance of this dialog, so lets create it
    KConfigDialog * dialog = new KConfigDialog( m_pageView, "generator_prefs", Okular::Settings::self() );
    dialog->setCaption( i18n( "Configure Backends" ) );

    m_document->fillConfigDialog( dialog );

    // keep us informed when the user changes settings
    connect( dialog, SIGNAL( settingsChanged( const QString& ) ), this, SLOT( slotNewGeneratorConfig() ) );
    dialog->show();
}


void Part::notifySetup( const QVector< Okular::Page * > & /*pages*/, int setupFlags )
{
    if ( !( setupFlags & Okular::DocumentObserver::DocumentChanged ) )
        return;

    rebuildBookmarkMenu();
    updateAboutBackendAction();
    m_searchWidget->setEnabled( m_document->supportsSearching() );
}

void Part::notifyViewportChanged( bool /*smoothMove*/ )
{
    // update actions if the page is changed
    static int lastPage = -1;
    int viewportPage = m_document->viewport().pageNumber;
    if ( viewportPage != lastPage )
    {
        updateViewActions();
        lastPage = viewportPage;
    }
}

void Part::notifyPageChanged( int page, int flags )
{
    if ( !(flags & Okular::DocumentObserver::Bookmark ) )
        return;

    rebuildBookmarkMenu();
    if ( page == m_document->viewport().pageNumber )
        updateBookmarksActions();
}


void Part::goToPage(uint i)
{
    if ( i <= m_document->pages() )
        m_document->setViewportPage( i - 1 );
}


void Part::openDocument( const QString &doc )
{
    openUrl( KUrl( doc ) );
}


uint Part::pages()
{
    return m_document->pages();
}


uint Part::currentPage()
{
    return m_document->pages() ? m_document->currentPage() + 1 : 0;
}


QString Part::currentDocument()
{
    return m_document->currentDocument().pathOrUrl();
}


QString Part::documentMetaData( const QString &metaData ) const
{
    const Okular::DocumentInfo * info = m_document->documentInfo();
    if ( info )
    {
        QDomElement docElement = info->documentElement();
        for ( QDomNode node = docElement.firstChild(); !node.isNull(); node = node.nextSibling() )
        {
            const QDomElement element = node.toElement();
            if ( metaData.compare( element.tagName(), Qt::CaseInsensitive ) == 0 )
                return element.attribute( "value" );
        }
    }

    return QString();
}


bool Part::slotImportPSFile()
{
    QString app = KStandardDirs::findExe( "ps2pdf" );
    if ( app.isEmpty() )
    {
        // TODO point the user to their distro packages?
        KMessageBox::error( widget(), i18n( "The program \"ps2pdf\" was not found, so Okular can not import PS files using it." ), i18n("ps2pdf not found") );
        return false;
    }

    KUrl url = KFileDialog::getOpenUrl( KUrl(), "application/postscript", this->widget() );
    if ( url.isLocalFile() )
    {
        KTemporaryFile tf;
        tf.setSuffix( ".pdf" );
        tf.setAutoRemove( false );
        if ( !tf.open() )
            return false;
        m_temporaryLocalFile = tf.fileName();
        tf.close();

        setLocalFilePath( url.path() );
        QStringList args;
        QProcess *p = new QProcess();
        args << url.toLocalFile() << m_temporaryLocalFile;
        m_pageView->displayMessage(i18n("Importing PS file as PDF (this may take a while)..."));
        connect(p, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(psTransformEnded(int, QProcess::ExitStatus)));
        p->start(app, args);
        return true;
    }

    m_temporaryLocalFile.clear();
    return false;
}


bool Part::openFile()
{
    KMimeType::Ptr mime;
    if ( !arguments().mimeType().isEmpty() )
    {
        mime = KMimeType::mimeType( arguments().mimeType() );
    }
    if ( !mime )
    {
        mime = KMimeType::findByPath( localFilePath() );
    }
    bool isCompressedFile = false;
    bool uncompressOk = true;
    QString fileNameToOpen = localFilePath();
    QString compressedMime = compressedMimeFor( mime->name() );
    if ( compressedMime.isEmpty() )
        compressedMime = compressedMimeFor( mime->parentMimeType() );
    if ( !compressedMime.isEmpty() )
    {
        isCompressedFile = true;
        uncompressOk = handleCompressed( fileNameToOpen, localFilePath(), compressedMime );
        mime = KMimeType::findByPath( fileNameToOpen );
    }
    bool ok = false;
    if ( uncompressOk )
    {
        if ( mime->is( "application/vnd.kde.okular-archive" ) )
            ok = m_document->openDocumentArchive( fileNameToOpen, url() );
        else
            ok = m_document->openDocument( fileNameToOpen, url(), mime );
    }
    bool canSearch = m_document->supportsSearching();

    // update one-time actions
    m_find->setEnabled( ok && canSearch );
    m_findNext->setEnabled( ok && canSearch );
    m_findPrev->setEnabled( ok && canSearch );
    m_saveAs->setEnabled( ok && m_document->canSaveChanges() );
    m_saveCopyAs->setEnabled( ok );
    emit enablePrintAction( ok && m_document->printingSupport() != Okular::Document::NoPrinting );
    m_printPreview->setEnabled( ok && m_document->printingSupport() != Okular::Document::NoPrinting );
    m_showProperties->setEnabled( ok );
    bool hasEmbeddedFiles = ok && m_document->embeddedFiles() && m_document->embeddedFiles()->count() > 0;
    m_showEmbeddedFiles->setEnabled( hasEmbeddedFiles );
    m_topMessage->setVisible( hasEmbeddedFiles );
    // m_pageView->toggleFormsAction() may be null on dummy mode
    m_formsMessage->setVisible( ok && m_pageView->toggleFormsAction() && m_pageView->toggleFormsAction()->isEnabled() );
    m_showPresentation->setEnabled( ok );
    if ( ok )
    {
        m_exportFormats = m_document->exportFormats();
        QList<Okular::ExportFormat>::ConstIterator it = m_exportFormats.constBegin();
        QList<Okular::ExportFormat>::ConstIterator itEnd = m_exportFormats.constEnd();
        QMenu *menu = m_exportAs->menu();
        for ( ; it != itEnd; ++it )
        {
            menu->addAction( actionForExportFormat( *it ) );
        }
        if ( isCompressedFile )
        {
            m_realUrl = url();
        }
#ifdef OKULAR_KEEP_FILE_OPEN
        if ( keepFileOpen() )
            m_keeper->open( fileNameToOpen );
#endif
    }
    m_exportAsText->setEnabled( ok && m_document->canExportToText() );
    m_exportAsDocArchive->setEnabled( ok );
    m_exportAs->setEnabled( ok );

    // update viewing actions
    updateViewActions();

    m_fileWasRemoved = false;

    if ( !ok )
    {
        // if can't open document, update windows so they display blank contents
        m_pageView->widget()->update();
        m_thumbnailList->update();
        return false;
    }

    // set the file to the fileWatcher
    if ( url().isLocalFile() )
    {
        if ( !m_watcher->contains( localFilePath() ) ) m_watcher->addFile(localFilePath());
        QFileInfo fi(localFilePath());
        if ( !m_watcher->contains( fi.absolutePath() ) ) m_watcher->addDir(fi.absolutePath());
    }

    // if the 'OpenTOC' flag is set, open the TOC
    if ( m_document->metaData( "OpenTOC" ).toBool() && m_sidebar->isItemEnabled( 0 ) )
    {
        m_sidebar->setCurrentIndex( 0 );
    }
    // if the 'StartFullScreen' flag is set, or the command line flag was
    // specified, start presentation
    if ( m_document->metaData( "StartFullScreen" ).toBool() || m_cliPresentation )
    {
        bool goAheadWithPresentationMode = true;
        if ( !m_cliPresentation )
        {
            const QString text = i18n( "The document requested to be launched in presentation mode.\n"
                                       "Do you want to allow it?" );
            const QString caption = i18n( "Presentation Mode" );
            const KGuiItem yesItem = KGuiItem( i18n( "Allow" ), "dialog-ok", i18n( "Allow the presentation mode" ) );
            const KGuiItem noItem = KGuiItem( i18n( "Do Not Allow" ), "process-stop", i18n( "Do not allow the presentation mode" ) );
            const int result = KMessageBox::questionYesNo( widget(), text, caption, yesItem, noItem );
            if ( result == KMessageBox::No )
                goAheadWithPresentationMode = false;
        }
        m_cliPresentation = false;
        if ( goAheadWithPresentationMode )
            QMetaObject::invokeMethod( this, "slotShowPresentation", Qt::QueuedConnection );
    }
    m_generatorGuiClient = factory() ? m_document->guiClient() : 0;
    if ( m_generatorGuiClient )
        factory()->addClient( m_generatorGuiClient );
    return true;
}

bool Part::openUrl(const KUrl &_url)
{
    KUrl url( _url );
    if ( url.hasHTMLRef() )
    {
        const QString dest = url.htmlRef();
        bool ok = true;
        const int page = dest.toInt( &ok );
        if ( ok )
        {
            Okular::DocumentViewport vp( page - 1 );
            vp.rePos.enabled = true;
            vp.rePos.normalizedX = 0;
            vp.rePos.normalizedY = 0;
            vp.rePos.pos = Okular::DocumentViewport::TopLeft;
            m_document->setNextDocumentViewport( vp );
        }
        else
        {
            m_document->setNextDocumentDestination( dest );
        }
        url.setHTMLRef( QString() );
    }

    // this calls in sequence the 'closeUrl' and 'openFile' methods
    bool openOk = KParts::ReadOnlyPart::openUrl( url );

    if ( openOk )
    {
        m_viewportDirty.pageNumber = -1;

        setWindowTitleFromDocument();
    }

    return openOk;
}


bool Part::closeUrl()
{
    if (!m_temporaryLocalFile.isNull() && m_temporaryLocalFile != localFilePath())
    {
        QFile::remove( m_temporaryLocalFile );
        m_temporaryLocalFile.clear();
    }

    slotHidePresentation();
    m_find->setEnabled( false );
    m_findNext->setEnabled( false );
    m_findPrev->setEnabled( false );
    m_saveAs->setEnabled( false );
    m_saveCopyAs->setEnabled( false );
    m_printPreview->setEnabled( false );
    m_showProperties->setEnabled( false );
    m_showEmbeddedFiles->setEnabled( false );
    m_exportAs->setEnabled( false );
    m_exportAsText->setEnabled( false );
    m_exportAsDocArchive->setEnabled( false );
    m_exportFormats.clear();
    QMenu *menu = m_exportAs->menu();
    QList<QAction*> acts = menu->actions();
    int num = acts.count();
    for ( int i = 2; i < num; ++i )
    {
        menu->removeAction( acts.at(i) );
        delete acts.at(i);
    }
    m_showPresentation->setEnabled( false );
    emit setWindowCaption("");
    emit enablePrintAction(false);
    m_realUrl = KUrl();
    if ( url().isLocalFile() )
    {
        m_watcher->removeFile( localFilePath() );
        QFileInfo fi(localFilePath());
        m_watcher->removeDir( fi.absolutePath() );
    }
    m_fileWasRemoved = false;
    if ( m_generatorGuiClient )
        factory()->removeClient( m_generatorGuiClient );
    m_generatorGuiClient = 0;
    m_document->closeDocument();
    updateViewActions();
    delete m_tempfile;
    m_tempfile = 0;
    if ( widget() )
    {
        m_searchWidget->clearText();
        m_topMessage->setVisible( false );
        m_formsMessage->setVisible( false );
    }
#ifdef OKULAR_KEEP_FILE_OPEN
    m_keeper->close();
#endif
    return KParts::ReadOnlyPart::closeUrl();
}


void Part::close()
{
    if ( m_embedMode == NativeShellMode )
    {
        closeUrl();
    }
    else KMessageBox::information( widget(), i18n( "This link points to a close document action that does not work when using the embedded viewer." ), QString(), "warnNoCloseIfNotInOkular" );
}


void Part::cannotQuit()
{
    KMessageBox::information( widget(), i18n( "This link points to a quit application action that does not work when using the embedded viewer." ), QString(), "warnNoQuitIfNotInOkular" );
}


void Part::slotShowLeftPanel()
{
    bool showLeft = m_showLeftPanel->isChecked();
    Okular::Settings::setShowLeftPanel( showLeft );
    Okular::Settings::self()->writeConfig();
    // show/hide left panel
    m_sidebar->setSidebarVisibility( showLeft );
}


void Part::slotFileDirty( const QString& path )
{
    // The beauty of this is that each start cancels the previous one.
    // This means that timeout() is only fired when there have
    // no changes to the file for the last 750 milisecs.
    // This ensures that we don't update on every other byte that gets
    // written to the file.
    if ( path == localFilePath() )
    {
        m_dirtyHandler->start( 750 );
    }
    else
    {
        QFileInfo fi(localFilePath());
        if ( fi.absolutePath() == path )
        {
            // Our parent has been dirtified
            if (!QFile::exists(localFilePath()))
            {
                m_fileWasRemoved = true;
            }
            else if (m_fileWasRemoved && QFile::exists(localFilePath()))
            {
                // we need to watch the new file
                m_watcher->removeFile(localFilePath());
                m_watcher->addFile(localFilePath());
                m_dirtyHandler->start( 750 );
            }
        }
    }
}


void Part::slotDoFileDirty()
{
    // do the following the first time the file is reloaded
    if ( m_viewportDirty.pageNumber == -1 )
    {
        // store the current viewport
        m_viewportDirty = m_document->viewport();

        // store the current toolbox pane
        m_dirtyToolboxIndex = m_sidebar->currentIndex();
        m_wasSidebarVisible = m_sidebar->isSidebarVisible();

        // store if presentation view was open
        m_wasPresentationOpen = ((PresentationWidget*)m_presentationWidget != 0);

        // inform the user about the operation in progress
        m_pageView->displayMessage( i18n("Reloading the document...") );
    }

    // close and (try to) reopen the document
    if ( KParts::ReadOnlyPart::openUrl( url() ) )
    {
        // on successful opening, restore the previous viewport
        if ( m_viewportDirty.pageNumber >= (int) m_document->pages() )
            m_viewportDirty.pageNumber = (int) m_document->pages() - 1;
        m_document->setViewport( m_viewportDirty );
        m_viewportDirty.pageNumber = -1;
        if ( m_sidebar->currentIndex() != m_dirtyToolboxIndex && m_sidebar->isItemEnabled( m_dirtyToolboxIndex ) )
        {
            m_sidebar->setCurrentIndex( m_dirtyToolboxIndex );
        }
        if ( m_sidebar->isSidebarVisible() != m_wasSidebarVisible )
        {
            m_sidebar->setCurrentIndex( m_sidebar->currentIndex() );
        }
        if (m_wasPresentationOpen) slotShowPresentation();
        emit enablePrintAction(true && m_document->printingSupport() != Okular::Document::NoPrinting);
    }
    else
    {
        // start watching the file again (since we dropped it on close)
        m_watcher->addFile(localFilePath());
        m_dirtyHandler->start( 750 );
    }
}


void Part::updateViewActions()
{
    bool opened = m_document->pages() > 0;
    if ( opened )
    {
        bool atBegin = m_document->currentPage() < 1;
        bool atEnd = m_document->currentPage() >= (m_document->pages() - 1);
        m_gotoPage->setEnabled( m_document->pages() > 1 );
        m_firstPage->setEnabled( !atBegin );
        m_prevPage->setEnabled( !atBegin );
        m_lastPage->setEnabled( !atEnd );
        m_nextPage->setEnabled( !atEnd );
        if (m_historyBack) m_historyBack->setEnabled( !m_document->historyAtBegin() );
        if (m_historyNext) m_historyNext->setEnabled( !m_document->historyAtEnd() );
        m_reload->setEnabled( true );
        m_copy->setEnabled( true );
        m_selectAll->setEnabled( true );
    }
    else
    {
        m_gotoPage->setEnabled( false );
        m_firstPage->setEnabled( false );
        m_lastPage->setEnabled( false );
        m_prevPage->setEnabled( false );
        m_nextPage->setEnabled( false );
        if (m_historyBack) m_historyBack->setEnabled( false );
        if (m_historyNext) m_historyNext->setEnabled( false );
        m_reload->setEnabled( false );
        m_copy->setEnabled( false );
        m_selectAll->setEnabled( false );
    }
    updateBookmarksActions();
}


void Part::updateBookmarksActions()
{
    bool opened = m_document->pages() > 0;
    if ( opened )
    {
        m_addBookmark->setEnabled( true );
        if ( m_document->bookmarkManager()->isBookmarked( m_document->currentPage() ) )
        {
            m_addBookmark->setText( i18n( "Remove Bookmark" ) );
            m_addBookmark->setIcon( KIcon( "edit-delete-bookmark" ) );
        }
        else
        {
            m_addBookmark->setText( m_addBookmarkText );
            m_addBookmark->setIcon( m_addBookmarkIcon );
        }
    }
    else
    {
        m_addBookmark->setEnabled( false );
        m_addBookmark->setText( m_addBookmarkText );
        m_addBookmark->setIcon( m_addBookmarkIcon );
    }
}


void Part::enableTOC(bool enable)
{
    m_sidebar->setItemEnabled(0, enable);
}

void Part::slotRebuildBookmarkMenu()
{
    rebuildBookmarkMenu();
}

void Part::slotShowFindBar()
{
    m_findBar->show();
    m_findBar->focusAndSetCursor();
}

void Part::slotHideFindBar()
{
    if ( m_findBar->maybeHide() )
    {
        m_pageView->setFocus();
    }
}

//BEGIN go to page dialog
class GotoPageDialog : public KDialog
{
    public:
        GotoPageDialog(QWidget *p, int current, int max) : KDialog(p)
        {
            setCaption(i18n("Go to Page"));
            setButtons(Ok | Cancel);
            setDefaultButton(Ok);

            QWidget *w = new QWidget(this);
            setMainWidget(w);

            QVBoxLayout *topLayout = new QVBoxLayout(w);
            topLayout->setMargin(0);
            topLayout->setSpacing(spacingHint());
            e1 = new KIntNumInput(current, w);
            e1->setRange(1, max);
            e1->setEditFocus(true);
            e1->setSliderEnabled(true);

            QLabel *label = new QLabel(i18n("&Page:"), w);
            label->setBuddy(e1);
            topLayout->addWidget(label);
            topLayout->addWidget(e1);
                                 // A little bit extra space
            topLayout->addSpacing(spacingHint());
            topLayout->addStretch(10);
            e1->setFocus();
        }

        int getPage() const
        {
            return e1->value();
        }

    protected:
        KIntNumInput *e1;
};
//END go to page dialog

void Part::slotGoToPage()
{
    GotoPageDialog pageDialog( m_pageView, m_document->currentPage() + 1, m_document->pages() );
    if ( pageDialog.exec() == QDialog::Accepted )
        m_document->setViewportPage( pageDialog.getPage() - 1 );
}


void Part::slotPreviousPage()
{
    if ( m_document->isOpened() && !(m_document->currentPage() < 1) )
        m_document->setViewportPage( m_document->currentPage() - 1 );
}


void Part::slotNextPage()
{
    if ( m_document->isOpened() && m_document->currentPage() < (m_document->pages() - 1) )
        m_document->setViewportPage( m_document->currentPage() + 1 );
}


void Part::slotGotoFirst()
{
    if ( m_document->isOpened() )
        m_document->setViewportPage( 0 );
}


void Part::slotGotoLast()
{
    if ( m_document->isOpened() )
        m_document->setViewportPage( m_document->pages() - 1 );
}


void Part::slotHistoryBack()
{
    m_document->setPrevViewport();
}


void Part::slotHistoryNext()
{
    m_document->setNextViewport();
}


void Part::slotAddBookmark()
{
    uint current = m_document->currentPage();
    if ( m_document->bookmarkManager()->isBookmarked( current ) )
    {
        m_document->bookmarkManager()->removeBookmark( current );
    }
    else
    {
        m_document->bookmarkManager()->addBookmark( current );
    }
}


void Part::slotPreviousBookmark()
{
    uint current = m_document->currentPage();
    // we are at the first page
    if ( current == 0 )
        return;

    for ( int i = current - 1; i >= 0; --i )
    {
        if ( m_document->bookmarkManager()->isBookmarked( i ) )
        {
            m_document->setViewportPage( i );
            break;
        }
    }
}


void Part::slotNextBookmark()
{
    uint current = m_document->currentPage();
    uint pages = m_document->pages();
    // we are at the last page
    if ( current == pages )
        return;

    for ( uint i = current + 1; i < pages; ++i )
    {
        if ( m_document->bookmarkManager()->isBookmarked( i ) )
        {
            m_document->setViewportPage( i );
            break;
        }
    }
}


void Part::slotFind()
{
    // when in presentation mode, there's already a search bar, taking care of
    // the 'find' requests
    if ( (PresentationWidget*)m_presentationWidget != 0 )
    {
        m_presentationWidget->slotFind();
    }
    else
    {
        slotShowFindBar();
    }
}


void Part::slotFindNext()
{
    if (m_findBar->isHidden())
        slotShowFindBar();
    else
        m_findBar->findNext();
}


void Part::slotFindPrev()
{
    if (m_findBar->isHidden())
        slotShowFindBar();
    else
        m_findBar->findPrev();
}


void Part::slotSaveFileAs()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    KUrl saveUrl = KFileDialog::getSaveUrl( KUrl("kfiledialog:///okular/" + url().fileName()), QString(), widget() );
    if ( !saveUrl.isValid() || saveUrl.isEmpty() )
        return;

    if ( KIO::NetAccess::exists( saveUrl, KIO::NetAccess::DestinationSide, widget() ) )
    {
        if (KMessageBox::warningContinueCancel( widget(), i18n("A file named \"%1\" already exists. Are you sure you want to overwrite it?", saveUrl.fileName()), QString(), KGuiItem(i18n("Overwrite"))) != KMessageBox::Continue)
            return;
    }

    KTemporaryFile tf;
    QString fileName;
    if ( !tf.open() )
    {
        KMessageBox::information( widget(), i18n("Could not open the temporary file for saving." ) );
            return;
    }
    fileName = tf.fileName();
    tf.close();

    if ( !m_document->saveChanges( fileName ) )
    {
        KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", fileName ) );
        return;
    }

    KIO::Job *copyJob = KIO::file_copy( fileName, saveUrl, -1, KIO::Overwrite );
    if ( !KIO::NetAccess::synchronousRun( copyJob, widget() ) )
        KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", saveUrl.prettyUrl() ) );
}


void Part::slotSaveCopyAs()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    KUrl saveUrl = KFileDialog::getSaveUrl( KUrl("kfiledialog:///okular/" + url().fileName()), QString(), widget() );
    if ( saveUrl.isValid() && !saveUrl.isEmpty() )
    {
        if ( KIO::NetAccess::exists( saveUrl, KIO::NetAccess::DestinationSide, widget() ) )
        {
            if (KMessageBox::warningContinueCancel( widget(), i18n("A file named \"%1\" already exists. Are you sure you want to overwrite it?", saveUrl.fileName()), QString(), KGuiItem(i18n("Overwrite"))) != KMessageBox::Continue)
                return;
        }

        // make use of the already downloaded (in case of remote URLs) file,
        // no point in downloading that again
        KUrl srcUrl = KUrl::fromPath( localFilePath() );
        KTemporaryFile * tempFile = 0;
        // duh, our local file disappeared...
        if ( !QFile::exists( localFilePath() ) )
        {
            if ( url().isLocalFile() )
            {
#ifdef OKULAR_KEEP_FILE_OPEN
                // local file: try to get it back from the open handle on it
                if ( ( tempFile = m_keeper->copyToTemporary() ) )
                    srcUrl = KUrl::fromPath( tempFile->fileName() );
#else
                const QString msg = i18n( "Okular cannot copy %1 to the specified location.\n\nThe document does not exist anymore.", localFilePath() );
                KMessageBox::sorry( widget(), msg );
                return;
#endif
            }
            else
            {
                // we still have the original remote URL of the document,
                // so copy the document from there
                srcUrl = url();
            }
        }

        KIO::Job *copyJob = KIO::file_copy( srcUrl, saveUrl, -1, KIO::Overwrite );
        if ( !KIO::NetAccess::synchronousRun( copyJob, widget() ) )
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", saveUrl.prettyUrl() ) );

        delete tempFile;
    }
}


void Part::slotGetNewStuff()
{
    KNS::Engine engine(widget());
    engine.init( "okular.knsrc" );
    // show the modal dialog over pageview and execute it
    KNS::Entry::List entries = engine.downloadDialogModal( m_pageView );
    Q_UNUSED( entries )
}


void Part::slotPreferences()
{
    // an instance the dialog could be already created and could be cached,
    // in which case you want to display the cached dialog
    if ( PreferencesDialog::showDialog( "preferences" ) )
        return;

    // we didn't find an instance of this dialog, so lets create it
    PreferencesDialog * dialog = new PreferencesDialog( m_pageView, Okular::Settings::self() );
    // keep us informed when the user changes settings
    connect( dialog, SIGNAL( settingsChanged( const QString & ) ), this, SLOT( slotNewConfig() ) );

    dialog->show();
}


void Part::slotNewConfig()
{
    // Apply settings here. A good policy is to check whether the setting has
    // changed before applying changes.

    // Watch File
    bool watchFile = Okular::Settings::watchFile();
    if ( watchFile && m_watcher->isStopped() )
        m_watcher->startScan();
    if ( !watchFile && !m_watcher->isStopped() )
    {
        m_dirtyHandler->stop();
        m_watcher->stopScan();
    }

    // Main View (pageView)
    m_pageView->reparseConfig();

    // update document settings
    m_document->reparseConfig();

    // update TOC settings
    if ( m_sidebar->isItemEnabled(0) )
        m_toc->reparseConfig();

    // update ThumbnailList contents
    if ( Okular::Settings::showLeftPanel() && !m_thumbnailList->isHidden() )
        m_thumbnailList->updateWidgets();
}


void Part::slotNewGeneratorConfig()
{
    // Apply settings here. A good policy is to check whether the setting has
    // changed before applying changes.

    // NOTE: it's not needed to reload the configuration of the Document,
    // the Document itself will take care of that

    // Main View (pageView)
    m_pageView->reparseConfig();

    // update TOC settings
    if ( m_sidebar->isItemEnabled(0) )
        m_toc->reparseConfig();

    // update ThumbnailList contents
    if ( Okular::Settings::showLeftPanel() && !m_thumbnailList->isHidden() )
        m_thumbnailList->updateWidgets();
}


void Part::slotPrintPreview()
{
    if (m_document->pages() == 0) return;

    QPrinter printer;

    // Native printing supports KPrintPreview, Postscript needs to use FilePrinterPreview
    if ( m_document->printingSupport() == Okular::Document::NativePrinting )
    {
        KPrintPreview previewdlg( &printer, widget() );
        setupPrint( printer );
        doPrint( printer );
        previewdlg.exec();
    }
    else
    {
        // Generate a temp filename for Print to File, then release the file so generator can write to it
        KTemporaryFile tf;
        tf.setAutoRemove( true );
        tf.setSuffix( ".ps" );
        tf.open();
        printer.setOutputFileName( tf.fileName() );
        tf.close();
        setupPrint( printer );
        doPrint( printer );
        if ( QFile::exists( printer.outputFileName() ) )
        {
            Okular::FilePrinterPreview previewdlg( printer.outputFileName(), widget() );
            previewdlg.exec();
        }
    }
}


void Part::slotShowMenu(const Okular::Page *page, const QPoint &point)
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    bool reallyShow = false;
    if (!m_actionsSearched)
    {
        // the quest for options_show_menubar
        KActionCollection *ac;
        QAction *act;

        if (factory())
        {
            QList<KXMLGUIClient*> clients(factory()->clients());
            for(int i = 0 ; (!m_showMenuBarAction || !m_showFullScreenAction) && i < clients.size(); ++i)
            {
                ac = clients.at(i)->actionCollection();
                // show_menubar
                act = ac->action("options_show_menubar");
                if (act && qobject_cast<KToggleAction*>(act))
                    m_showMenuBarAction = qobject_cast<KToggleAction*>(act);
                // fullscreen
                act = ac->action("fullscreen");
                if (act && qobject_cast<KToggleFullScreenAction*>(act))
                    m_showFullScreenAction = qobject_cast<KToggleFullScreenAction*>(act);
            }
        }
        m_actionsSearched = true;
    }

    KMenu *popup = new KMenu( widget() );
    QAction *addBookmark = 0;
    QAction *removeBookmark = 0;
    QAction *fitPageWidth = 0;
    if (page)
    {
        popup->addTitle( i18n( "Page %1", page->number() + 1 ) );
        if ( m_document->bookmarkManager()->isBookmarked( page->number() ) )
            removeBookmark = popup->addAction( KIcon("edit-delete-bookmark"), i18n("Remove Bookmark") );
        else
            addBookmark = popup->addAction( KIcon("bookmark-new"), i18n("Add Bookmark") );
        if ( m_pageView->canFitPageWidth() )
            fitPageWidth = popup->addAction( KIcon("zoom-fit-best"), i18n("Fit Width") );
        popup->addAction( m_prevBookmark );
        popup->addAction( m_nextBookmark );
        reallyShow = true;
    }
    /*
        //Albert says: I have not ported this as i don't see it does anything
        if ( d->mouseOnRect ) // and rect->objectType() == ObjectRect::Image ...
        {
            m_popup->insertItem( SmallIcon("document-save"), i18n("Save Image..."), 4 );
            m_popup->setItemEnabled( 4, false );
    }*/

    if ((m_showMenuBarAction && !m_showMenuBarAction->isChecked()) || (m_showFullScreenAction && m_showFullScreenAction->isChecked()))
    {
        popup->addTitle( i18n( "Tools" ) );
        if (m_showMenuBarAction && !m_showMenuBarAction->isChecked()) popup->addAction(m_showMenuBarAction);
        if (m_showFullScreenAction && m_showFullScreenAction->isChecked()) popup->addAction(m_showFullScreenAction);
        reallyShow = true;

    }

    if (reallyShow)
    {
        QAction *res = popup->exec(point);
        if (res)
        {
            if (res == addBookmark) m_document->bookmarkManager()->addBookmark( page->number() );
            else if (res == removeBookmark) m_document->bookmarkManager()->removeBookmark( page->number() );
            else if (res == fitPageWidth) m_pageView->fitPageWidth( page->number() );
        }
    }
    delete popup;
}


void Part::slotShowProperties()
{
    PropertiesDialog *d = new PropertiesDialog(widget(), m_document);
    d->exec();
    delete d;
}


void Part::slotShowEmbeddedFiles()
{
    EmbeddedFilesDialog *d = new EmbeddedFilesDialog(widget(), m_document);
    d->exec();
    delete d;
}


void Part::slotShowPresentation()
{
    if ( !m_presentationWidget )
    {
        m_presentationWidget = new PresentationWidget( widget(), m_document, actionCollection() );
    }
}


void Part::slotHidePresentation()
{
    if ( m_presentationWidget )
        delete (PresentationWidget*) m_presentationWidget;
}


void Part::slotTogglePresentation()
{
    if ( m_document->isOpened() )
    {
        if ( !m_presentationWidget )
            m_presentationWidget = new PresentationWidget( widget(), m_document, actionCollection() );
        else delete (PresentationWidget*) m_presentationWidget;
    }
}


void Part::reload()
{
    if ( m_document->isOpened() )
    {
        slotReload();
    }
}


void Part::slotAboutBackend()
{
    const KComponentData *data = m_document->componentData();
    if ( !data )
        return;

    KAboutData aboutData( *data->aboutData() );

    if ( aboutData.programIconName().isEmpty() || aboutData.programIconName() == aboutData.appName() )
    {
        if ( const Okular::DocumentInfo *documentInfo = m_document->documentInfo() )
        {
            const QString mimeTypeName = documentInfo->get("mimeType");
            if ( !mimeTypeName.isEmpty() )
            {
                if ( KMimeType::Ptr type = KMimeType::mimeType( mimeTypeName ) )
                    aboutData.setProgramIconName( type->iconName() );
            }
        }
    }

    KAboutApplicationDialog dlg( &aboutData, widget() );
    dlg.exec();
}


void Part::slotExportAs(QAction * act)
{
    QList<QAction*> acts = m_exportAs->menu() ? m_exportAs->menu()->actions() : QList<QAction*>();
    int id = acts.indexOf( act );
    if ( ( id < 0 ) || ( id >= acts.count() ) )
        return;

    QString filter;
    switch ( id )
    {
        case 0:
            filter = "text/plain";
            break;
        case 1:
            filter = "application/vnd.kde.okular-archive";
            break;
        default:
            filter = m_exportFormats.at( id - 2 ).mimeType()->name();
            break;
    }
    QString fileName = KFileDialog::getSaveFileName( url().isLocalFile() ? url().directory() : QString(), filter, widget() );
    if ( !fileName.isEmpty() )
    {
        bool saved = false;
        switch ( id )
        {
            case 0:
                saved = m_document->exportToText( fileName );
                break;
            case 1:
                saved = m_document->saveDocumentArchive( fileName );
                break;
            default:
                saved = m_document->exportTo( fileName, m_exportFormats.at( id - 2 ) );
                break;
        }
        if ( !saved )
            KMessageBox::information( widget(), i18n("File could not be saved in '%1'. Try to save it to another location.", fileName ) );
    }
}


void Part::slotReload()
{
    // stop the dirty handler timer, otherwise we may conflict with the
    // auto-refresh system
    m_dirtyHandler->stop();

    slotDoFileDirty();
}


void Part::slotPrint()
{
    if (m_document->pages() == 0) return;

    QPrinter printer;
    QPrintDialog *printDialog = 0;
    QWidget *printConfigWidget = 0;

    // Must do certain QPrinter setup before creating QPrintDialog
    setupPrint( printer );

    // Create the Print Dialog with extra config widgets if required
    if ( m_document->canConfigurePrinter() )
    {
        printConfigWidget = m_document->printConfigurationWidget();
    }
    if ( printConfigWidget )
    {
        printDialog = KdePrint::createPrintDialog( &printer, QList<QWidget*>() << printConfigWidget, widget() );
    }
    else
    {
        printDialog = KdePrint::createPrintDialog( &printer, widget() );
    }

    if ( printDialog )
    {

        // Set the available Print Range
        printDialog->setMinMax( 1, m_document->pages() );
        printDialog->setFromTo( 1, m_document->pages() );

        // If the user has bookmarked pages for printing, then enable Selection
        if ( !m_document->bookmarkedPageRange().isEmpty() )
        {
            printDialog->addEnabledOption( QAbstractPrintDialog::PrintSelection );
        }

        // If the Document type doesn't support print to both PS & PDF then disable the Print Dialog option
        if ( printDialog->isOptionEnabled( QAbstractPrintDialog::PrintToFile ) &&
             !m_document->supportsPrintToFile() )
        {
            printDialog->setEnabledOptions( printDialog->enabledOptions() ^ QAbstractPrintDialog::PrintToFile );
        }

        if ( printDialog->exec() )
            doPrint( printer );
        delete printDialog;
    }
}


void Part::setupPrint( QPrinter &printer )
{
    double width, height;
    int landscape, portrait;
    const Okular::Page *page;

    // if some pages are landscape and others are not the most common win as QPrinter does
    // not accept a per page setting
    landscape = 0;
    portrait = 0;
    for (uint i = 0; i < m_document->pages(); i++)
    {
        page = m_document->page(i);
        width = page->width();
        height = page->height();
        if (page->orientation() == Okular::Rotation90 || page->orientation() == Okular::Rotation270) qSwap(width, height);
        if (width > height) landscape++;
        else portrait++;
    }
    if (landscape > portrait) printer.setOrientation(QPrinter::Landscape);

    // title
    QString title = m_document->metaData( "DocumentTitle" ).toString();
    if ( title.isEmpty() )
    {
        title = m_document->currentDocument().fileName();
    }
    if ( !title.isEmpty() )
    {
        printer.setDocName( title );
    }
}


void Part::doPrint(QPrinter &printer)
{
    if (!m_document->isAllowed(Okular::AllowPrint))
    {
        KMessageBox::error(widget(), i18n("Printing this document is not allowed."));
        return;
    }

    if (!m_document->print(printer))
    {
        KMessageBox::error(widget(), i18n("Could not print the document. Please report to bugs.kde.org"));
    }
}


void Part::restoreDocument(const KConfigGroup &group)
{
    KUrl url ( group.readPathEntry( "URL", QString() ) );
    if ( url.isValid() )
    {
        QString viewport = group.readEntry( "Viewport" );
        if (!viewport.isEmpty()) m_document->setNextDocumentViewport( Okular::DocumentViewport( viewport ) );
        openUrl( url );
    }
}


void Part::saveDocumentRestoreInfo(KConfigGroup &group)
{
    group.writePathEntry( "URL", url().url() );
    group.writeEntry( "Viewport", m_document->viewport().toString() );
}


void Part::psTransformEnded(int exit, QProcess::ExitStatus status)
{
    Q_UNUSED( exit )
    if ( status != QProcess::NormalExit )
        return;

    QProcess *senderobj = sender() ? qobject_cast< QProcess * >( sender() ) : 0;
    if ( senderobj )
    {
        senderobj->close();
        senderobj->deleteLater();
    }

    setLocalFilePath( m_temporaryLocalFile );
    openUrl( m_temporaryLocalFile );
    m_temporaryLocalFile.clear();
}


void Part::unsetDummyMode()
{
    if ( m_embedMode == PrintPreviewMode )
       return;

    m_sidebar->setItemEnabled( 2, true );
    m_sidebar->setItemEnabled( 3, true );
    m_sidebar->setSidebarVisibility( Okular::Settings::showLeftPanel() );

    // add back and next in history
    m_historyBack = KStandardAction::documentBack( this, SLOT( slotHistoryBack() ), actionCollection() );
    m_historyBack->setWhatsThis( i18n( "Go to the place you were before" ) );

    m_historyNext = KStandardAction::documentForward( this, SLOT( slotHistoryNext() ), actionCollection());
    m_historyNext->setWhatsThis( i18n( "Go to the place you were after" ) );

    m_pageView->setupActions( actionCollection() );

    // attach the actions of the children widgets too
    m_formsMessage->setActionButton( m_pageView->toggleFormsAction() );

    // ensure history actions are in the correct state
    updateViewActions();
}


bool Part::handleCompressed( QString &destpath, const QString &path, const QString &compressedMimetype )
{
    m_tempfile = 0;

    // we are working with a compressed file, decompressing
    // temporary file for decompressing
    KTemporaryFile *newtempfile = new KTemporaryFile();
    newtempfile->setAutoRemove(true);

    if ( !newtempfile->open() )
    {
        KMessageBox::error( widget(),
            i18n("<qt><strong>File Error!</strong> Could not create temporary file "
            "<nobr><strong>%1</strong></nobr>.</qt>",
            strerror(newtempfile->error())));
        delete newtempfile;
        return false;
    }

    // decompression filer
    QIODevice* filterDev = KFilterDev::deviceForFile( path, compressedMimetype );
    if (!filterDev)
    {
        delete newtempfile;
        return false;
    }

    if ( !filterDev->open(QIODevice::ReadOnly) )
    {
        KMessageBox::detailedError( widget(),
            i18n("<qt><strong>File Error!</strong> Could not open the file "
            "<nobr><strong>%1</strong></nobr> for uncompression. "
            "The file will not be loaded.</qt>", path),
            i18n("<qt>This error typically occurs if you do "
            "not have enough permissions to read the file. "
            "You can check ownership and permissions if you "
            "right-click on the file in the Dolphin "
            "file manager and then choose the 'Properties' tab.</qt>"));

        delete filterDev;
        delete newtempfile;
        return false;
    }

    QByteArray buf(1024, '\0');
    int read = 0, wrtn = 0;

    while ((read = filterDev->read(buf.data(), buf.size())) > 0)
    {
        wrtn = newtempfile->write(buf.data(), read);
        if ( read != wrtn )
            break;
    }
    delete filterDev;
    if ((read != 0) || (newtempfile->size() == 0))
    {
        KMessageBox::detailedError(widget(),
            i18n("<qt><strong>File Error!</strong> Could not uncompress "
            "the file <nobr><strong>%1</strong></nobr>. "
            "The file will not be loaded.</qt>", path ),
            i18n("<qt>This error typically occurs if the file is corrupt. "
            "If you want to be sure, try to decompress the file manually "
            "using command-line tools.</qt>"));
        delete newtempfile;
        return false;
    }
    m_tempfile = newtempfile;
    destpath = m_tempfile->fileName();
    return true;
}

void Part::rebuildBookmarkMenu( bool unplugActions )
{
    if ( unplugActions )
    {
        unplugActionList( "bookmarks_currentdocument" );
        qDeleteAll( m_bookmarkActions );
        m_bookmarkActions.clear();
    }
    KUrl u = m_document->currentDocument();
    if ( u.isValid() )
    {
        m_bookmarkActions = m_document->bookmarkManager()->actionsForUrl( u );
    }
    bool havebookmarks = true;
    if ( m_bookmarkActions.isEmpty() )
    {
        havebookmarks = false;
        QAction * a = new QAction( 0 );
        a->setText( i18n( "No Bookmarks" ) );
        a->setEnabled( false );
        m_bookmarkActions.append( a );
    }
    plugActionList( "bookmarks_currentdocument", m_bookmarkActions );

    m_prevBookmark->setEnabled( havebookmarks );
    m_nextBookmark->setEnabled( havebookmarks );
}

void Part::updateAboutBackendAction()
{
    const KComponentData *data = m_document->componentData();
    if ( data )
    {
        m_aboutBackend->setEnabled( true );
    }
    else
    {
        m_aboutBackend->setEnabled( false );
    }
}

} // namespace Okular

#include "part.moc"

/* kate: replace-tabs on; indent-width 4; */
