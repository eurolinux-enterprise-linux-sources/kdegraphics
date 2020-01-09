/***************************************************************************
 *   Copyright (C) 2006 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "bookmarklist.h"

// qt/kde includes
#include <qaction.h>
#include <qcursor.h>
#include <qheaderview.h>
#include <qlayout.h>
#include <qtoolbar.h>
#include <qtreewidget.h>

#include <kdebug.h>
#include <kicon.h>
#include <klocale.h>
#include <kmenu.h>
#include <ktreewidgetsearchline.h>

#include "pageitemdelegate.h"
#include "core/action.h"
#include "core/bookmarkmanager.h"
#include "core/document.h"

static const int BookmarkItemType = QTreeWidgetItem::UserType + 1;
static const int FileItemType = QTreeWidgetItem::UserType + 2;
static const int UrlRole = Qt::UserRole + 1;

class BookmarkItem : public QTreeWidgetItem
{
    public:
        BookmarkItem( const KBookmark& bm )
            : QTreeWidgetItem( BookmarkItemType ), m_bookmark( bm )
        {
            setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable );
            m_url = m_bookmark.url();
            m_viewport = Okular::DocumentViewport( m_url.htmlRef() );
            m_url.setHTMLRef( QString() );
            setText( 0, m_bookmark.fullText() );
            if ( m_viewport.isValid() )
                setData( 0, PageItemDelegate::PageRole, QString::number( m_viewport.pageNumber + 1 ) );
        }

        virtual QVariant data( int column, int role ) const
        {
            switch ( role )
            {
                case Qt::ToolTipRole:
                    return m_bookmark.fullText();
            }
            return QTreeWidgetItem::data( column, role );
        }

        virtual bool operator<( const QTreeWidgetItem& other ) const
        {
            if ( other.type() == BookmarkItemType )
            {
                const BookmarkItem *cmp = static_cast< const BookmarkItem* >( &other );
                const int v = m_viewport.pageNumber - cmp->m_viewport.pageNumber;
                if ( v != 0 )
                    return v < 0;
            }
            return QTreeWidgetItem::operator<( other );
        }

        KBookmark& bookmark()
        {
            return m_bookmark;
        }

        const Okular::DocumentViewport& viewport() const
        {
            return m_viewport;
        }

        KUrl url() const
        {
            return m_url;
        }

    private:
        KBookmark m_bookmark;
        KUrl m_url;
        Okular::DocumentViewport m_viewport;
};


BookmarkList::BookmarkList( Okular::Document *document, QWidget *parent )
    : QWidget( parent ), m_document( document ), m_currentDocumentItem( 0 )
{
    QVBoxLayout *mainlay = new QVBoxLayout( this );
    mainlay->setMargin( 0 );
    mainlay->setSpacing( 6 );

    m_searchLine = new KTreeWidgetSearchLine( this );
    mainlay->addWidget( m_searchLine );

    m_tree = new QTreeWidget( this );
    mainlay->addWidget( m_tree );
    QStringList cols;
    cols.append( "Bookmarks" );
    m_tree->setContextMenuPolicy(  Qt::CustomContextMenu );
    m_tree->setHeaderLabels( cols );
    m_tree->setSortingEnabled( false );
    m_tree->setRootIsDecorated( true );
    m_tree->setAlternatingRowColors( true );
    m_tree->setItemDelegate( new PageItemDelegate( m_tree ) );
    m_tree->header()->hide();
    m_tree->setSelectionBehavior( QAbstractItemView::SelectRows );
    m_tree->setEditTriggers( QAbstractItemView::EditKeyPressed );
    connect( m_tree, SIGNAL( itemActivated( QTreeWidgetItem *, int ) ), this, SLOT( slotExecuted( QTreeWidgetItem * ) ) );
    connect( m_tree, SIGNAL( customContextMenuRequested( const QPoint& ) ), this, SLOT( slotContextMenu( const QPoint& ) ) );
    m_searchLine->addTreeWidget( m_tree );

    QToolBar * bookmarkController = new QToolBar( this );
    mainlay->addWidget( bookmarkController );
    bookmarkController->setObjectName( "BookmarkControlBar" );
    // change toolbar appearance
    bookmarkController->setIconSize( QSize( 16, 16 ) );
    bookmarkController->setMovable( false );
    QSizePolicy sp = bookmarkController->sizePolicy();
    sp.setVerticalPolicy( QSizePolicy::Minimum );
    bookmarkController->setSizePolicy( sp );
    // insert a togglebutton [show only bookmarks in the current document]
    m_showBoomarkOnlyAction = bookmarkController->addAction( KIcon( "bookmarks" ), i18n( "Current document only" ) );
    m_showBoomarkOnlyAction->setCheckable( true );
    connect( m_showBoomarkOnlyAction, SIGNAL( toggled( bool ) ), this, SLOT( slotFilterBookmarks( bool ) ) );

    connect( m_document->bookmarkManager(), SIGNAL( bookmarksChanged( const KUrl& ) ), this, SLOT( slotBookmarksChanged( const KUrl& ) ) );

    rebuildTree( m_showBoomarkOnlyAction->isChecked() );
}

BookmarkList::~BookmarkList()
{
    m_document->removeObserver( this );
}

uint BookmarkList::observerId() const
{
    return BOOKMARKLIST_ID;
}

void BookmarkList::notifySetup( const QVector< Okular::Page * > & pages, int setupFlags )
{
    if ( !( setupFlags & Okular::DocumentObserver::DocumentChanged ) )
        return;

    // clear contents
    m_searchLine->clear();

    if ( m_showBoomarkOnlyAction->isChecked() )
    {
        rebuildTree( m_showBoomarkOnlyAction->isChecked() );
    }
    else
    {
        disconnect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );
        if ( m_currentDocumentItem && m_currentDocumentItem != m_tree->invisibleRootItem()  )
        {
            m_currentDocumentItem->setIcon( 0, QIcon() );
        }
        m_currentDocumentItem = itemForUrl( m_document->currentDocument() );
        if ( m_currentDocumentItem && m_currentDocumentItem != m_tree->invisibleRootItem()  )
        {
            m_currentDocumentItem->setIcon( 0, KIcon( "bookmarks" ) );
            m_currentDocumentItem->setExpanded( true );
        }
        connect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );
    }
}

void BookmarkList::slotFilterBookmarks( bool on )
{
    rebuildTree( on );
}

void BookmarkList::slotExecuted( QTreeWidgetItem * item )
{
    BookmarkItem* bmItem = dynamic_cast<BookmarkItem*>( item );
    if ( !bmItem || !bmItem->viewport().isValid() )
        return;

    goTo( bmItem );
}

void BookmarkList::slotChanged( QTreeWidgetItem * item )
{
    BookmarkItem* bmItem = dynamic_cast<BookmarkItem*>( item );
    if ( !bmItem || !bmItem->viewport().isValid() )
        return;

    bmItem->bookmark().setFullText( bmItem->text( 0 ) );
    m_document->bookmarkManager()->save();
}

void BookmarkList::slotContextMenu( const QPoint& p )
{
    QTreeWidgetItem * item = m_tree->itemAt( p );
    BookmarkItem* bmItem = item ? dynamic_cast<BookmarkItem*>( item ) : 0;
    if ( !bmItem || !bmItem->viewport().isValid() )
        return;

    KMenu menu( this );
    QAction * gotobm = menu.addAction( i18n( "Go to This Bookmark" ) );
    QAction * editbm = menu.addAction( KIcon( "edit-rename" ), i18n( "Rename Bookmark" ) );
    QAction * removebm = menu.addAction( KIcon( "list-remove" ), i18n( "Remove Bookmark" ) );
    QAction * res = menu.exec( QCursor::pos() );
    if ( !res )
        return;

    if ( res == gotobm )
        goTo( bmItem );
    else if ( res == editbm )
        m_tree->editItem( item, 0 );
    else if ( res == removebm )
        m_document->bookmarkManager()->removeBookmark( bmItem->url(), bmItem->bookmark() );
}

void BookmarkList::slotBookmarksChanged( const KUrl& url )
{
    // special case here, as m_currentDocumentItem could represent
    // the invisible root item
    if ( url == m_document->currentDocument() )
    {
        selectiveUrlUpdate( m_document->currentDocument(), m_currentDocumentItem );
        return;
    }

    // we are showing the bookmarks for the current document only
    if ( m_showBoomarkOnlyAction->isChecked() )
        return;

    QTreeWidgetItem *item = itemForUrl( url );
    if ( item )
    {
        selectiveUrlUpdate( url, item );
    }
}

QList<QTreeWidgetItem*> createItems( const KUrl& baseurl, const KBookmark::List& bmlist )
{
    (void)baseurl;
    QList<QTreeWidgetItem*> ret;
    foreach ( const KBookmark& bm, bmlist )
    {
//        kDebug().nospace() << "checking '" << tmp << "'";
//        kDebug().nospace() << "      vs '" << baseurl << "'";
        // TODO check that bm and baseurl are the same (#ref excluded)
        QTreeWidgetItem * item = new BookmarkItem( bm );
        ret.append( item );
    }
    return ret;
}

void BookmarkList::rebuildTree( bool filter )
{
    // disconnect and reconnect later, otherwise we'll get many itemChanged()
    // signals for all the current items
    disconnect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );

    m_currentDocumentItem = 0;
    m_tree->clear();

    KUrl::List urls = m_document->bookmarkManager()->files();
    if ( filter )
    {
        if ( m_document->isOpened() )
        {
        foreach ( const KUrl& url, urls )
        {
            if ( url == m_document->currentDocument() )
            {
                m_tree->addTopLevelItems( createItems( url, m_document->bookmarkManager()->bookmarks( url ) ) );
                m_currentDocumentItem = m_tree->invisibleRootItem();
                break;
            }
        }
        }
    }
    else
    {
        QTreeWidgetItem * currenturlitem = 0;
        foreach ( const KUrl& url, urls )
        {
            QList<QTreeWidgetItem*> subitems = createItems( url, m_document->bookmarkManager()->bookmarks( url ) );
            if ( !subitems.isEmpty() )
            {
                QTreeWidgetItem * item = new QTreeWidgetItem( m_tree, FileItemType );
                QString fileString = url.isLocalFile() ? url.path() : url.prettyUrl();
                item->setText( 0, fileString );
                item->setToolTip( 0, i18ncp( "%1 is the file name", "%1\n\nOne bookmark", "%1\n\n%2 bookmarks", fileString, subitems.count() ) );
                item->setData( 0, UrlRole, qVariantFromValue( url ) );
                item->addChildren( subitems );
                if ( !currenturlitem && url == m_document->currentDocument() )
                {
                    currenturlitem = item;
                }
            }
        }
        if ( currenturlitem )
        {
            currenturlitem->setExpanded( true );
            currenturlitem->setIcon( 0, KIcon( "bookmarks" ) );
            m_tree->scrollToItem( currenturlitem, QAbstractItemView::PositionAtTop );
            m_currentDocumentItem = currenturlitem;
        }
    }

    m_tree->sortItems( 0, Qt::AscendingOrder );

    connect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );
}

void BookmarkList::goTo( BookmarkItem * item )
{
    if ( item->url() == m_document->currentDocument() )
    {
        m_document->setViewport( item->viewport() );
    }
    else
    {
        Okular::GotoAction action( item->url().pathOrUrl(), item->viewport() );
        m_document->processAction( &action );
    }
}

void BookmarkList::selectiveUrlUpdate( const KUrl& url, QTreeWidgetItem*& item )
{
    disconnect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );

    const KBookmark::List urlbookmarks = m_document->bookmarkManager()->bookmarks( url );
    if ( urlbookmarks.isEmpty() )
    {
        if ( item != m_tree->invisibleRootItem() )
        {
            m_tree->invisibleRootItem()->removeChild( item );
            item = 0;
        }
        else if ( item )
        {
            for ( int i = item->childCount(); i >= 0; --i )
            {
                item->removeChild( item->child( i ) );
            }
        }
    }
    else
    {
        const QString fileString = url.isLocalFile() ? url.path() : url.prettyUrl();
        if ( item )
        {
            for ( int i = item->childCount(); i >= 0; --i )
            {
                item->removeChild( item->child( i ) );
            }
        }
        else
        {
            item = new QTreeWidgetItem( m_tree, FileItemType );
            item->setIcon( 0, KIcon( "bookmarks" ) );
            item->setExpanded( true );
            item->setText( 0, fileString );
        }
        item->addChildren( createItems( url, urlbookmarks ) );
        if ( item != m_tree->invisibleRootItem() )
        {
            item->setToolTip( 0, i18ncp( "%1 is the file name", "%1\n\nOne bookmark", "%1\n\n%2 bookmarks", fileString, item->childCount() ) );
        }
    }

    connect( m_tree, SIGNAL( itemChanged( QTreeWidgetItem *, int ) ), this, SLOT( slotChanged( QTreeWidgetItem * ) ) );
}

QTreeWidgetItem* BookmarkList::itemForUrl( const KUrl& url ) const
{
    const int count = m_tree->topLevelItemCount();
    for ( int i = 0; i < count; ++i )
    {
        QTreeWidgetItem *item = m_tree->topLevelItem( i );
        const KUrl itemurl = item->data( 0, UrlRole ).value< KUrl >();
        if ( itemurl.isValid() && itemurl == url )
        {
            return item;
        }
    }
    return 0;
}

#include "bookmarklist.moc"
