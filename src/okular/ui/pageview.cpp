/***************************************************************************
 *   Copyright (C) 2004-2005 by Enrico Ros <eros.kde@email.it>             *
 *   Copyright (C) 2004-2006 by Albert Astals Cid <tsdgeos@terra.es>       *
 *                                                                         *
 *   With portions of code from kpdf/kpdf_pagewidget.cc by:                *
 *     Copyright (C) 2002 by Wilco Greven <greven@kde.org>                 *
 *     Copyright (C) 2003 by Christophe Devriese                           *
 *                           <Christophe.Devriese@student.kuleuven.ac.be>  *
 *     Copyright (C) 2003 by Laurent Montel <montel@kde.org>               *
 *     Copyright (C) 2003 by Dirk Mueller <mueller@kde.org>                *
 *     Copyright (C) 2004 by James Ots <kde@jamesots.com>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "pageview.h"

// qt/kde includes
#include <qcursor.h>
#include <qevent.h>
#include <qimage.h>
#include <qpainter.h>
#include <qtimer.h>
#include <qset.h>
#include <qscrollbar.h>
#include <qtooltip.h>
#include <qapplication.h>
#include <qclipboard.h>

#include <kaction.h>
#include <kactionmenu.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <kmenu.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kselectaction.h>
#include <ktoggleaction.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kicon.h>

// system includes
#include <math.h>
#include <stdlib.h>

// local includes
#include "formwidgets.h"
#include "pageviewutils.h"
#include "pagepainter.h"
#include "core/annotations.h"
#include "annotwindow.h"
#include "guiutils.h"
#include "annotationpopup.h"
#include "pageviewannotator.h"
#include "toolaction.h"
#include "tts.h"
#include "videowidget.h"
#include "core/action.h"
#include "core/document.h"
#include "core/form.h"
#include "core/page.h"
#include "core/misc.h"
#include "core/generator.h"
#include "core/movie.h"
#include "settings.h"

static int pageflags = PagePainter::Accessibility | PagePainter::EnhanceLinks |
                       PagePainter::EnhanceImages | PagePainter::Highlights |
                       PagePainter::TextSelection | PagePainter::Annotations;

static inline double normClamp( double value, double def )
{
    return ( value < 0.0 || value > 1.0 ) ? def : value;
}

// structure used internally by PageView for data storage
class PageViewPrivate
{
public:
    PageViewPrivate( PageView *qq );

    FormWidgetsController* formWidgetsController();
    OkularTTS* tts();
    QString selectedText() const;

    // the document, pageviewItems and the 'visible cache'
    PageView *q;
    Okular::Document * document;
    QVector< PageViewItem * > items;
    QLinkedList< PageViewItem * > visibleItems;

    // view layout (columns and continuous in Settings), zoom and mouse
    PageView::ZoomMode zoomMode;
    float zoomFactor;
    PageView::MouseMode mouseMode;
    QPoint mouseGrabPos;
    QPoint mousePressPos;
    QPoint mouseSelectPos;
    bool mouseMidZooming;
    int mouseMidLastY;
    bool mouseSelecting;
    QRect mouseSelectionRect;
    QColor mouseSelectionColor;
    bool mouseTextSelecting;
    QSet< int > pagesWithTextSelection;
    bool mouseOnRect;
    Okular::Annotation * mouseAnn;
    QPoint mouseAnnPos;

    // viewport move
    bool viewportMoveActive;
    QTime viewportMoveTime;
    QPoint viewportMoveDest;
    QTimer * viewportMoveTimer;
    // auto scroll
    int scrollIncrement;
    QTimer * autoScrollTimer;
    // annotations
    PageViewAnnotator * annotator;
    //text annotation dialogs list
    QHash< Okular::Annotation *, AnnotWindow * > m_annowindows;
    // other stuff
    QTimer * delayResizeTimer;
    bool dirtyLayout;
    bool blockViewport;                 // prevents changes to viewport
    bool blockPixmapsRequest;           // prevent pixmap requests
    PageViewMessage * messageWindow;    // in pageviewutils.h
    bool m_formsVisible;
    FormWidgetsController *formsWidgetController;
    OkularTTS * m_tts;
    QTimer * refreshTimer;
    int refreshPage;

    // infinite resizing loop prevention
    bool bothScrollbarsVisible;

    // drag scroll
    QPoint dragScrollVector;
    QTimer dragScrollTimer;

    // actions
    KAction * aRotateClockwise;
    KAction * aRotateCounterClockwise;
    KAction * aRotateOriginal;
    KSelectAction * aPageSizes;
    KToggleAction * aTrimMargins;
    KAction * aMouseNormal;
    KAction * aMouseSelect;
    KAction * aMouseTextSelect;
    KToggleAction * aToggleAnnotator;
    KSelectAction * aZoom;
    KAction * aZoomIn;
    KAction * aZoomOut;
    KToggleAction * aZoomFitWidth;
    KToggleAction * aZoomFitPage;
    KToggleAction * aZoomFitText;
    KActionMenu * aViewMode;
    KToggleAction * aViewContinuous;
    QAction * aPrevAction;
    KAction * aToggleForms;
    KAction * aSpeakDoc;
    KAction * aSpeakPage;
    KAction * aSpeakStop;
    KActionCollection * actionCollection;

    int setting_viewMode;
    int setting_viewCols;
    bool setting_centerFirst;
};

PageViewPrivate::PageViewPrivate( PageView *qq )
    : q( qq )
{
}

FormWidgetsController* PageViewPrivate::formWidgetsController()
{
    if ( !formsWidgetController )
    {
        formsWidgetController = new FormWidgetsController();
        QObject::connect( formsWidgetController, SIGNAL( changed( FormWidgetIface* ) ),
                          q, SLOT( slotFormWidgetChanged( FormWidgetIface * ) ) );
        QObject::connect( formsWidgetController, SIGNAL( action( Okular::Action* ) ),
                          q, SLOT( slotAction( Okular::Action* ) ) );
    }

    return formsWidgetController;
}

OkularTTS* PageViewPrivate::tts()
{
    if ( !m_tts )
    {
        m_tts = new OkularTTS( q );
        if ( aSpeakStop )
        {
            QObject::connect( m_tts, SIGNAL( hasSpeechs( bool ) ),
                              aSpeakStop, SLOT( setEnabled( bool ) ) );
            QObject::connect( m_tts, SIGNAL( errorMessage( const QString & ) ),
                              q, SLOT( errorMessage( const QString & ) ) );
        }
    }

    return m_tts;
}


class PageViewWidget : public QWidget
{
public:
    PageViewWidget(PageView *pv) : QWidget(pv), m_pageView(pv) {}

protected:
    bool event( QEvent *e )
    {
        if ( e->type() == QEvent::ToolTip && m_pageView->d->mouseMode == PageView::MouseNormal )
        {
            QHelpEvent * he = (QHelpEvent*)e;
            PageViewItem * pageItem = m_pageView->pickItemOnPoint( he->x(), he->y() );
            const Okular::ObjectRect * rect = 0;
            const Okular::Action * link = 0;
            const Okular::Annotation * ann = 0;
            if ( pageItem )
            {
                double nX = pageItem->absToPageX( he->x() );
                double nY = pageItem->absToPageY( he->y() );
                rect = pageItem->page()->objectRect( Okular::ObjectRect::OAnnotation, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                if ( rect )
                    ann = static_cast< const Okular::AnnotationObjectRect * >( rect )->annotation();
                else
                {
                    rect = pageItem->page()->objectRect( Okular::ObjectRect::Action, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                    if ( rect )
                        link = static_cast< const Okular::Action * >( rect->object() );
                }
            }

            if ( ann )
            {
                QRect r = rect->boundingRect( pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                r.translate( pageItem->uncroppedGeometry().topLeft() );
                QString tip = GuiUtils::prettyToolTip( ann );
                QToolTip::showText( he->globalPos(), tip, this, r );
            }
            else if ( link )
            {
                QRect r = rect->boundingRect( pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                r.translate( pageItem->uncroppedGeometry().topLeft() );
                QString tip = link->actionTip();
                if ( !tip.isEmpty() )
                    QToolTip::showText( he->globalPos(), tip, this, r );
            }
            e->accept();
            return true;
        }
        else
            // do not stop the event
            return QWidget::event( e );
    }

    // viewport events
    void paintEvent( QPaintEvent *e )
    {
        m_pageView->contentsPaintEvent(e);
    }

    void mouseMoveEvent( QMouseEvent *e )
    {
        m_pageView->contentsMouseMoveEvent(e);
    }

    void mousePressEvent( QMouseEvent *e )
    {
        m_pageView->contentsMousePressEvent(e);
    }

    void mouseReleaseEvent( QMouseEvent *e )
    {
        m_pageView->contentsMouseReleaseEvent(e);
    }


private:
   PageView *m_pageView;
};

/* PageView. What's in this file? -> quick overview.
 * Code weight (in rows) and meaning:
 *  160 - constructor and creating actions plus their connected slots (empty stuff)
 *  70  - DocumentObserver inherited methodes (important)
 *  550 - events: mouse, keyboard, drag/drop
 *  170 - slotRelayoutPages: set contents of the scrollview on continuous/single modes
 *  100 - zoom: zooming pages in different ways, keeping update the toolbar actions, etc..
 *  other misc functions: only slotRequestVisiblePixmaps and pickItemOnPoint noticeable,
 * and many insignificant stuff like this comment :-)
 */
PageView::PageView( QWidget *parent, Okular::Document *document )
    : QScrollArea( parent )
    , Okular::View( QString::fromLatin1( "PageView" ) )
{
    // create and initialize private storage structure
    d = new PageViewPrivate( this );
    d->document = document;
    d->aRotateClockwise = 0;
    d->aRotateCounterClockwise = 0;
    d->aRotateOriginal = 0;
    d->aViewMode = 0;
    d->zoomMode = PageView::ZoomFitWidth;
    d->zoomFactor = 1.0;
    d->mouseMode = MouseNormal;
    d->mouseMidZooming = false;
    d->mouseSelecting = false;
    d->mouseTextSelecting = false;
    d->mouseOnRect = false;
    d->mouseAnn = 0;
    d->viewportMoveActive = false;
    d->viewportMoveTimer = 0;
    d->scrollIncrement = 0;
    d->autoScrollTimer = 0;
    d->annotator = 0;
    d->delayResizeTimer = 0;
    d->dirtyLayout = false;
    d->blockViewport = false;
    d->blockPixmapsRequest = false;
    d->messageWindow = new PageViewMessage(this);
    d->m_formsVisible = false;
    d->formsWidgetController = 0;
    d->m_tts = 0;
    d->refreshTimer = 0;
    d->refreshPage = -1;
    d->aRotateClockwise = 0;
    d->aRotateCounterClockwise = 0;
    d->aRotateOriginal = 0;
    d->aPageSizes = 0;
    d->aTrimMargins = 0;
    d->aMouseNormal = 0;
    d->aMouseSelect = 0;
    d->aMouseTextSelect = 0;
    d->aToggleAnnotator = 0;
    d->aZoomFitWidth = 0;
    d->aZoomFitPage = 0;
    d->aZoomFitText = 0;
    d->aViewMode = 0;
    d->aViewContinuous = 0;
    d->aPrevAction = 0;
    d->aToggleForms = 0;
    d->aSpeakDoc = 0;
    d->aSpeakPage = 0;
    d->aSpeakStop = 0;
    d->actionCollection = 0;
    d->aPageSizes=0;
    d->setting_viewMode = Okular::Settings::viewMode();
    d->setting_viewCols = Okular::Settings::viewColumns();
    d->setting_centerFirst = Okular::Settings::centerFirstPageInRow();

    setFrameStyle(QFrame::NoFrame);

    setAttribute( Qt::WA_StaticContents );

    setObjectName( QLatin1String( "okular::pageView" ) );

    // widget setup: setup focus, accept drops and track mouse
    setWidget(new PageViewWidget(this));
    viewport()->setFocusProxy( this );
    viewport()->setFocusPolicy( Qt::StrongFocus );
    widget()->setAttribute( Qt::WA_OpaquePaintEvent );
    widget()->setAttribute( Qt::WA_NoSystemBackground );
    setAcceptDrops( true );
    widget()->setMouseTracking( true );
    setWidgetResizable(true);

    // conntect the padding of the viewport to pixmaps requests
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRequestVisiblePixmaps(int)));
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRequestVisiblePixmaps(int)));
    connect( &d->dragScrollTimer, SIGNAL(timeout()), this, SLOT(slotDragScroll()) );

    // set a corner button to resize the view to the page size
//    QPushButton * resizeButton = new QPushButton( viewport() );
//    resizeButton->setPixmap( SmallIcon("crop") );
//    setCornerWidget( resizeButton );
//    resizeButton->setEnabled( false );
    // connect(...);
    setAttribute( Qt::WA_InputMethodEnabled, true );

    // schedule the welcome message
    QMetaObject::invokeMethod(this, "slotShowWelcome", Qt::QueuedConnection);
}

PageView::~PageView()
{
    if ( d->m_tts )
        d->m_tts->stopAllSpeechs();

    // delete the local storage structure
    qDeleteAll(d->m_annowindows);
    // delete all widgets
    QVector< PageViewItem * >::const_iterator dIt = d->items.constBegin(), dEnd = d->items.constEnd();
    for ( ; dIt != dEnd; ++dIt )
        delete *dIt;
    delete d->formsWidgetController;
    d->document->removeObserver( this );
    delete d;
}

void PageView::setupBaseActions( KActionCollection * ac )
{
    d->actionCollection = ac;

    // Zoom actions ( higher scales takes lots of memory! )
    d->aZoom  = new KSelectAction(KIcon( "page-zoom" ), i18n("Zoom"), this);
    ac->addAction("zoom_to", d->aZoom );
    d->aZoom->setEditable( true );
    d->aZoom->setMaxComboViewCount( 13 );
    connect( d->aZoom, SIGNAL( triggered(QAction *) ), this, SLOT( slotZoom() ) );
    updateZoomText();

    d->aZoomIn = KStandardAction::zoomIn( this, SLOT( slotZoomIn() ), ac );

    d->aZoomOut = KStandardAction::zoomOut( this, SLOT( slotZoomOut() ), ac );
}

void PageView::setupActions( KActionCollection * ac )
{
    d->actionCollection = ac;

    // orientation menu actions
    d->aRotateClockwise = new KAction( KIcon( "object-rotate-right" ), i18n( "Rotate Right" ), this );
    d->aRotateClockwise->setIconText( i18nc( "Rotate right", "Right" ) );
    ac->addAction( "view_orientation_rotate_cw", d->aRotateClockwise );
    d->aRotateClockwise->setEnabled( false );
    connect( d->aRotateClockwise, SIGNAL( triggered() ), this, SLOT( slotRotateClockwise() ) );
    d->aRotateCounterClockwise = new KAction( KIcon( "object-rotate-left" ), i18n( "Rotate Left" ), this );
    d->aRotateCounterClockwise->setIconText( i18nc( "Rotate left", "Left" ) );
    ac->addAction( "view_orientation_rotate_ccw", d->aRotateCounterClockwise );
    d->aRotateCounterClockwise->setEnabled( false );
    connect( d->aRotateCounterClockwise, SIGNAL( triggered() ), this, SLOT( slotRotateCounterClockwise() ) );
    d->aRotateOriginal = new KAction( i18n( "Original Orientation" ), this );
    ac->addAction( "view_orientation_original", d->aRotateOriginal );
    d->aRotateOriginal->setEnabled( false );
    connect( d->aRotateOriginal, SIGNAL( triggered() ), this, SLOT( slotRotateOriginal() ) );

    d->aPageSizes = new KSelectAction(i18n("&Page Size"), this);
    ac->addAction("view_pagesizes", d->aPageSizes);
    d->aPageSizes->setEnabled( false );

    connect( d->aPageSizes , SIGNAL( triggered( int ) ),
         this, SLOT( slotPageSizes( int ) ) );

    d->aTrimMargins  = new KToggleAction( i18n( "&Trim Margins" ), this );
    ac->addAction("view_trim_margins", d->aTrimMargins );
    connect( d->aTrimMargins, SIGNAL( toggled( bool ) ), SLOT( slotTrimMarginsToggled( bool ) ) );
    d->aTrimMargins->setChecked( Okular::Settings::trimMargins() );

    d->aZoomFitWidth  = new KToggleAction(KIcon( "zoom-fit-width" ), i18n("Fit &Width"), this);
    ac->addAction("view_fit_to_width", d->aZoomFitWidth );
    connect( d->aZoomFitWidth, SIGNAL( toggled( bool ) ), SLOT( slotFitToWidthToggled( bool ) ) );

    d->aZoomFitPage  = new KToggleAction(KIcon( "zoom-fit-best" ), i18n("Fit &Page"), this);
    ac->addAction("view_fit_to_page", d->aZoomFitPage );
    connect( d->aZoomFitPage, SIGNAL( toggled( bool ) ), SLOT( slotFitToPageToggled( bool ) ) );

/*
    d->aZoomFitText  = new KToggleAction(KIcon( "zoom-fit-best" ), i18n("Fit &Text"), this);
    ac->addAction("zoom_fit_text", d->aZoomFitText );
    connect( d->aZoomFitText, SIGNAL( toggled( bool ) ), SLOT( slotFitToTextToggled( bool ) ) );
*/

    // View-Layout actions
    d->aViewMode = new KActionMenu( KIcon( "view-split-left-right" ), i18n( "&View Mode" ), this );
    d->aViewMode->setDelayed( false );
#define ADD_VIEWMODE_ACTION( text, name, id ) \
do { \
    KAction *vm = new KAction( text, d->aViewMode->menu() ); \
    vm->setCheckable( true ); \
    vm->setData( qVariantFromValue( id ) ); \
    d->aViewMode->addAction( vm ); \
    ac->addAction( name, vm ); \
    vmGroup->addAction( vm ); \
} while( 0 )
    ac->addAction("view_render_mode", d->aViewMode );
    QActionGroup *vmGroup = new QActionGroup( d->aViewMode->menu() );
    ADD_VIEWMODE_ACTION( i18n( "Single Page" ), "view_render_mode_single", 0 );
    ADD_VIEWMODE_ACTION( i18n( "Facing Pages" ), "view_render_mode_facing", 1 );
    ADD_VIEWMODE_ACTION( i18n( "Overview" ), "view_render_mode_overview", 2 );
    d->aViewMode->menu()->actions().at( Okular::Settings::viewMode() )->setChecked( true );
    connect( vmGroup, SIGNAL( triggered( QAction* ) ), this, SLOT( slotViewMode( QAction* ) ) );
#undef ADD_VIEWMODE_ACTION

    d->aViewContinuous  = new KToggleAction(KIcon( "view-list-text" ), i18n("&Continuous"), this);
    ac->addAction("view_continuous", d->aViewContinuous );
    connect( d->aViewContinuous, SIGNAL( toggled( bool ) ), SLOT( slotContinuousToggled( bool ) ) );
    d->aViewContinuous->setChecked( Okular::Settings::viewContinuous() );

    // Mouse-Mode actions
    QActionGroup * actGroup = new QActionGroup( this );
    actGroup->setExclusive( true );
    d->aMouseNormal  = new KAction( KIcon( "input-mouse" ), i18n( "&Browse Tool" ), this );
    ac->addAction("mouse_drag", d->aMouseNormal );
    connect( d->aMouseNormal, SIGNAL( triggered() ), this, SLOT( slotSetMouseNormal() ) );
    d->aMouseNormal->setIconText( i18nc( "Browse Tool", "Browse" ) );
    d->aMouseNormal->setCheckable( true );
    d->aMouseNormal->setShortcut( Qt::CTRL + Qt::Key_1 );
    d->aMouseNormal->setActionGroup( actGroup );
    d->aMouseNormal->setChecked( true );

    KAction * mz  = new KAction(KIcon( "page-zoom" ), i18n("&Zoom Tool"), this);
    ac->addAction("mouse_zoom", mz );
    connect( mz, SIGNAL( triggered() ), this, SLOT( slotSetMouseZoom() ) );
    mz->setIconText( i18nc( "Zoom Tool", "Zoom" ) );
    mz->setCheckable( true );
    mz->setShortcut( Qt::CTRL + Qt::Key_2 );
    mz->setActionGroup( actGroup );

    d->aMouseSelect  = new KAction(KIcon( "select-rectangular" ), i18n("&Selection Tool"), this);
    ac->addAction("mouse_select", d->aMouseSelect );
    connect( d->aMouseSelect, SIGNAL( triggered() ), this, SLOT( slotSetMouseSelect() ) );
    d->aMouseSelect->setIconText( i18nc( "Select Tool", "Selection" ) );
    d->aMouseSelect->setCheckable( true );
    d->aMouseSelect->setShortcut( Qt::CTRL + Qt::Key_3 );
    d->aMouseSelect->setActionGroup( actGroup );

    d->aMouseTextSelect  = new KAction(KIcon( "draw-text" ), i18n("&Text Selection Tool"), this);
    ac->addAction("mouse_textselect", d->aMouseTextSelect );
    connect( d->aMouseTextSelect, SIGNAL( triggered() ), this, SLOT( slotSetMouseTextSelect() ) );
    d->aMouseTextSelect->setIconText( i18nc( "Text Selection Tool", "Text Selection" ) );
    d->aMouseTextSelect->setCheckable( true );
    d->aMouseTextSelect->setShortcut( Qt::CTRL + Qt::Key_4 );
    d->aMouseTextSelect->setActionGroup( actGroup );

    d->aToggleAnnotator  = new KToggleAction(KIcon( "draw-freehand" ), i18n("&Review"), this);
    ac->addAction("mouse_toggle_annotate", d->aToggleAnnotator );
    d->aToggleAnnotator->setCheckable( true );
    connect( d->aToggleAnnotator, SIGNAL( toggled( bool ) ), SLOT( slotToggleAnnotator( bool ) ) );
    d->aToggleAnnotator->setShortcut( Qt::Key_F6 );

    ToolAction *ta = new ToolAction( this );
    ac->addAction( "mouse_selecttools", ta );
    ta->addAction( d->aMouseSelect );
    ta->addAction( d->aMouseTextSelect );

    // speak actions
    d->aSpeakDoc = new KAction( KIcon( "text-speak" ), i18n( "Speak Whole Document" ), this );
    ac->addAction( "speak_document", d->aSpeakDoc );
    d->aSpeakDoc->setEnabled( false );
    connect( d->aSpeakDoc, SIGNAL( triggered() ), SLOT( slotSpeakDocument() ) );

    d->aSpeakPage = new KAction( KIcon( "text-speak" ), i18n( "Speak Current Page" ), this );
    ac->addAction( "speak_current_page", d->aSpeakPage );
    d->aSpeakPage->setEnabled( false );
    connect( d->aSpeakPage, SIGNAL( triggered() ), SLOT( slotSpeakCurrentPage() ) );

    d->aSpeakStop = new KAction( KIcon( "media-playback-stop" ), i18n( "Stop Speaking" ), this );
    ac->addAction( "speak_stop_all", d->aSpeakStop );
    d->aSpeakStop->setEnabled( false );
    connect( d->aSpeakStop, SIGNAL( triggered() ), SLOT( slotStopSpeaks() ) );

    // Other actions
    KAction * su  = new KAction(i18n("Scroll Up"), this);
    ac->addAction("view_scroll_up", su );
    connect( su, SIGNAL( triggered() ), this, SLOT( slotScrollUp() ) );
    su->setShortcut( QKeySequence(Qt::SHIFT + Qt::Key_Up) );
    addAction(su);

    KAction * sd  = new KAction(i18n("Scroll Down"), this);
    ac->addAction("view_scroll_down", sd );
    connect( sd, SIGNAL( triggered() ), this, SLOT( slotScrollDown() ) );
    sd->setShortcut( QKeySequence(Qt::SHIFT + Qt::Key_Down) );
    addAction(sd);

    d->aToggleForms = new KAction( this );
    ac->addAction( "view_toggle_forms", d->aToggleForms );
    connect( d->aToggleForms, SIGNAL( triggered() ), this, SLOT( slotToggleForms() ) );
    d->aToggleForms->setEnabled( false );
    toggleFormWidgets( false );
}

bool PageView::canFitPageWidth() const
{
    return Okular::Settings::viewMode() != 0 || d->zoomMode != ZoomFitWidth;
}

void PageView::fitPageWidth( int page )
{
    // zoom: Fit Width, columns: 1. setActions + relayout + setPage + update
    d->zoomMode = ZoomFitWidth;
    Okular::Settings::setViewMode( 0 );
    d->aZoomFitWidth->setChecked( true );
    d->aZoomFitPage->setChecked( false );
//    d->aZoomFitText->setChecked( false );
    d->aViewMode->menu()->actions().at( 0 )->setChecked( true );
    viewport()->setUpdatesEnabled( false );
    slotRelayoutPages();
    viewport()->setUpdatesEnabled( true );
    d->document->setViewportPage( page );
    updateZoomText();
    setFocus();
}

void PageView::setAnnotationWindow( Okular::Annotation * annotation )
{
    if ( !annotation )
        return;

    // find the annot window
    AnnotWindow* existWindow = 0;
    QHash< Okular::Annotation *, AnnotWindow * >::ConstIterator it = d->m_annowindows.constFind( annotation );
    if ( it != d->m_annowindows.constEnd() )
    {
        existWindow = *it;
    }

    if ( existWindow == 0 )
    {
        existWindow = new AnnotWindow( this, annotation );

        d->m_annowindows.insert( annotation, existWindow );
    }

    existWindow->show();
}

void PageView::removeAnnotationWindow( Okular::Annotation *annotation )
{
    QHash< Okular::Annotation *, AnnotWindow * >::Iterator it = d->m_annowindows.find( annotation );
    if ( it != d->m_annowindows.end() )
    {
        delete *it;
        d->m_annowindows.erase( it );
    }
}

void PageView::displayMessage( const QString & message,PageViewMessage::Icon icon,int duration )
{
    if ( !Okular::Settings::showOSD() )
    {
        if (icon == PageViewMessage::Error)
            KMessageBox::error( this, message );
        else
            return;
    }

    // hide messageWindow if string is empty
    if ( message.isEmpty() )
        return d->messageWindow->hide();

    // display message (duration is length dependant)
    if (duration==-1)
        duration = 500 + 100 * message.length();
    d->messageWindow->display( message, icon, duration );
}

void PageView::reparseConfig()
{
    // set the scroll bars policies
    Qt::ScrollBarPolicy scrollBarMode = Okular::Settings::showScrollBars() ?
        Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
    if ( horizontalScrollBarPolicy() != scrollBarMode )
    {
        setHorizontalScrollBarPolicy( scrollBarMode );
        setVerticalScrollBarPolicy( scrollBarMode );
    }

    const int viewMode = Okular::Settings::viewMode();
    if ( ( viewMode == 2 && ( (int)Okular::Settings::viewColumns() != d->setting_viewCols ) )
         || ( viewMode > 0 && ( Okular::Settings::centerFirstPageInRow() != d->setting_centerFirst ) )
       )
    {
        d->setting_viewMode = Okular::Settings::viewMode();
        d->setting_viewCols = Okular::Settings::viewColumns();
        d->setting_centerFirst = Okular::Settings::centerFirstPageInRow();

        slotRelayoutPages();
    }
}

KAction *PageView::toggleFormsAction() const
{
    return d->aToggleForms;
}

QString PageViewPrivate::selectedText() const
{
    if ( pagesWithTextSelection.isEmpty() )
        return QString();

    QString text;
    QList< int > selpages = pagesWithTextSelection.toList();
    qSort( selpages );
    const Okular::Page * pg = 0;
    if ( selpages.count() == 1 )
    {
        pg = document->page( selpages.first() );
        text.append( pg->text( pg->textSelection() ) );
    }
    else
    {
        pg = document->page( selpages.first() );
        text.append( pg->text( pg->textSelection() ) );
        int end = selpages.count() - 1;
        for( int i = 1; i < end; ++i )
        {
            pg = document->page( selpages.at( i ) );
            text.append( pg->text() );
        }
        pg = document->page( selpages.last() );
        text.append( pg->text( pg->textSelection() ) );
    }
    return text;
}

void PageView::copyTextSelection() const
{
    const QString text = d->selectedText();
    if ( !text.isEmpty() )
    {
        QClipboard *cb = QApplication::clipboard();
        cb->setText( text, QClipboard::Clipboard );
    }
}

void PageView::selectAll()
{
    if ( d->mouseMode != MouseTextSelect )
        return;

    QVector< PageViewItem * >::const_iterator it = d->items.constBegin(), itEnd = d->items.constEnd();
    for ( ; it < itEnd; ++it )
    {
        Okular::RegularAreaRect * area = textSelectionForItem( *it );
        d->pagesWithTextSelection.insert( (*it)->pageNumber() );
        d->document->setPageTextSelection( (*it)->pageNumber(), area, palette().color( QPalette::Active, QPalette::Highlight ) );
    }
}

//BEGIN DocumentObserver inherited methods
void PageView::notifySetup( const QVector< Okular::Page * > & pageSet, int setupFlags )
{
    bool documentChanged = setupFlags & Okular::DocumentObserver::DocumentChanged;
    // reuse current pages if nothing new
    if ( ( pageSet.count() == d->items.count() ) && !documentChanged && !( setupFlags & Okular::DocumentObserver::NewLayoutForPages ) )
    {
        int count = pageSet.count();
        for ( int i = 0; (i < count) && !documentChanged; i++ )
            if ( (int)pageSet[i]->number() != d->items[i]->pageNumber() )
                documentChanged = true;
        if ( !documentChanged )
            return;
    }

    // delete all widgets (one for each page in pageSet)
    QVector< PageViewItem * >::const_iterator dIt = d->items.constBegin(), dEnd = d->items.constEnd();
    for ( ; dIt != dEnd; ++dIt )
        delete *dIt;
    d->items.clear();
    d->visibleItems.clear();
    d->pagesWithTextSelection.clear();
    toggleFormWidgets( false );
    if ( d->formsWidgetController )
        d->formsWidgetController->dropRadioButtons();

    bool haspages = !pageSet.isEmpty();
    bool hasformwidgets = false;
    // create children widgets
    QVector< Okular::Page * >::const_iterator setIt = pageSet.constBegin(), setEnd = pageSet.constEnd();
    for ( ; setIt != setEnd; ++setIt )
    {
        PageViewItem * item = new PageViewItem( *setIt );
        d->items.push_back( item );
#ifdef PAGEVIEW_DEBUG
        kDebug().nospace() << "cropped geom for " << d->items.last()->pageNumber() << " is " << d->items.last()->croppedGeometry();
#endif
        const QLinkedList< Okular::FormField * > pageFields = (*setIt)->formFields();
        QLinkedList< Okular::FormField * >::const_iterator ffIt = pageFields.constBegin(), ffEnd = pageFields.constEnd();
        for ( ; ffIt != ffEnd; ++ffIt )
        {
            Okular::FormField * ff = *ffIt;
            FormWidgetIface * w = FormWidgetFactory::createWidget( ff, widget() );
            if ( w )
            {
                w->setPageItem( item );
                w->setFormWidgetsController( d->formWidgetsController() );
                w->setVisibility( false );
                w->setCanBeFilled( d->document->isAllowed( Okular::AllowFillForms ) );
                item->formWidgets().insert( ff->id(), w );
                hasformwidgets = true;
            }
        }
        const QLinkedList< Okular::Annotation * > annotations = (*setIt)->annotations();
        QLinkedList< Okular::Annotation * >::const_iterator aIt = annotations.constBegin(), aEnd = annotations.constEnd();
        for ( ; aIt != aEnd; ++aIt )
        {
            Okular::Annotation * a = *aIt;
            if ( a->subType() == Okular::Annotation::AMovie )
            {
                Okular::MovieAnnotation * movieAnn = static_cast< Okular::MovieAnnotation * >( a );
                VideoWidget * vw = new VideoWidget( movieAnn, d->document, widget() );
                item->videoWidgets().insert( movieAnn->movie(), vw );
                vw->show();
            }
        }
    }

    // invalidate layout so relayout/repaint will happen on next viewport change
    if ( haspages )
        // TODO for Enrico: Check if doing always the slotRelayoutPages() is not
        // suboptimal in some cases, i'd say it is not but a recheck will not hurt
        // Need slotRelayoutPages() here instead of d->dirtyLayout = true
        // because opening a document from another document will not trigger a viewportchange
        // so pages are never relayouted
        QMetaObject::invokeMethod(this, "slotRelayoutPages", Qt::QueuedConnection);
    else
    {
        // update the mouse cursor when closing because we may have close through a link and
        // want the cursor to come back to the normal cursor
        updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
        setWidgetResizable(true);
    }

    // OSD to display pages
    if ( documentChanged && pageSet.count() > 0 && Okular::Settings::showOSD() )
        d->messageWindow->display(
            i18np(" Loaded a one-page document.",
                 " Loaded a %1-page document.",
                 pageSet.count() ),
            PageViewMessage::Info, 4000 );

    if ( d->aPageSizes )
    { // may be null if dummy mode is on
        bool pageSizes = d->document->supportsPageSizes();
        d->aPageSizes->setEnabled( pageSizes );
        // set the new page sizes:
        // - if the generator supports them
        // - if the document changed
        if ( pageSizes && documentChanged )
        {
            QStringList items;
            foreach ( const Okular::PageSize &p, d->document->pageSizes() )
                items.append( p.name() );
            d->aPageSizes->setItems( items );
        }
    }
    if ( d->aRotateClockwise )
        d->aRotateClockwise->setEnabled( haspages );
    if ( d->aRotateCounterClockwise )
        d->aRotateCounterClockwise->setEnabled( haspages );
    if ( d->aRotateOriginal )
        d->aRotateOriginal->setEnabled( haspages );
    if ( d->aToggleForms )
    { // may be null if dummy mode is on
        d->aToggleForms->setEnabled( haspages && hasformwidgets );
    }
    bool allowAnnotations = d->document->isAllowed( Okular::AllowNotes );
    if ( d->annotator )
    {
        bool allowTools = haspages && allowAnnotations;
        d->annotator->setToolsEnabled( allowTools );
        d->annotator->setTextToolsEnabled( allowTools && d->document->supportsSearching() );
    }
    if ( d->aToggleAnnotator )
    {
        if ( !allowAnnotations && d->aToggleAnnotator->isChecked() )
        {
            d->aToggleAnnotator->trigger();
        }
        d->aToggleAnnotator->setEnabled( allowAnnotations );
    }
    if ( d->aSpeakDoc )
    {
        const bool enablettsactions = haspages ? Okular::Settings::useKTTSD() : false;
        d->aSpeakDoc->setEnabled( enablettsactions );
        d->aSpeakPage->setEnabled( enablettsactions );
    }
}

void PageView::notifyViewportChanged( bool smoothMove )
{
    // if we are the one changing viewport, skip this nofity
    if ( d->blockViewport )
        return;

    // block setViewport outgoing calls
    d->blockViewport = true;

    // find PageViewItem matching the viewport description
    const Okular::DocumentViewport & vp = d->document->viewport();
    PageViewItem * item = 0;
    QVector< PageViewItem * >::const_iterator iIt = d->items.constBegin(), iEnd = d->items.constEnd();
    for ( ; iIt != iEnd; ++iIt )
        if ( (*iIt)->pageNumber() == vp.pageNumber )
        {
            item = *iIt;
            break;
        }
    if ( !item )
    {
        kWarning() << "viewport for page" << vp.pageNumber << "has no matching item!";
        d->blockViewport = false;
        return;
    }
#ifdef PAGEVIEW_DEBUG
    kDebug() << "document viewport changed";
#endif
    // relayout in "Single Pages" mode or if a relayout is pending
    d->blockPixmapsRequest = true;
    if ( !Okular::Settings::viewContinuous() || d->dirtyLayout )
        slotRelayoutPages();

    // restore viewport center or use default {x-center,v-top} alignment
    const QRect & r = item->croppedGeometry();
    int newCenterX = r.left(),
        newCenterY = r.top();
    if ( vp.rePos.enabled )
    {
        if ( vp.rePos.pos == Okular::DocumentViewport::Center )
        {
            newCenterX += (int)( normClamp( vp.rePos.normalizedX, 0.5 ) * (double)r.width() );
            newCenterY += (int)( normClamp( vp.rePos.normalizedY, 0.0 ) * (double)r.height() );
        }
        else
        {
            // TopLeft
            newCenterX += (int)( normClamp( vp.rePos.normalizedX, 0.0 ) * (double)r.width() + viewport()->width() / 2 );
            newCenterY += (int)( normClamp( vp.rePos.normalizedY, 0.0 ) * (double)r.height() + viewport()->height() / 2 );
        }
    }
    else
    {
        newCenterX += r.width() / 2;
        newCenterY += viewport()->height() / 2 - 10;
    }

    // if smooth movement requested, setup parameters and start it
    if ( smoothMove )
    {
        d->viewportMoveActive = true;
        d->viewportMoveTime.start();
        d->viewportMoveDest.setX( newCenterX );
        d->viewportMoveDest.setY( newCenterY );
        if ( !d->viewportMoveTimer )
        {
            d->viewportMoveTimer = new QTimer( this );
            connect( d->viewportMoveTimer, SIGNAL( timeout() ),
                     this, SLOT( slotMoveViewport() ) );
        }
        d->viewportMoveTimer->start( 25 );
        verticalScrollBar()->setEnabled( false );
        horizontalScrollBar()->setEnabled( false );
    }
    else
        center( newCenterX, newCenterY );
    d->blockPixmapsRequest = false;

    // request visible pixmaps in the current viewport and recompute it
    slotRequestVisiblePixmaps();

    // enable setViewport calls
    d->blockViewport = false;

    // update zoom text if in a ZoomFit/* zoom mode
    if ( d->zoomMode != ZoomFixed )
        updateZoomText();

    // since the page has moved below cursor, update it
    updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
}

void PageView::notifyPageChanged( int pageNumber, int changedFlags )
{
    // only handle pixmap / highlight changes notifies
    if ( changedFlags & DocumentObserver::Bookmark )
        return;

    if ( changedFlags & DocumentObserver::Annotations )
    {
        const QLinkedList< Okular::Annotation * > annots = d->document->page( pageNumber )->annotations();
        const QLinkedList< Okular::Annotation * >::ConstIterator annItEnd = annots.end();
        QHash< Okular::Annotation*, AnnotWindow * >::Iterator it = d->m_annowindows.begin();
        for ( ; it != d->m_annowindows.end(); )
        {
            QLinkedList< Okular::Annotation * >::ConstIterator annIt = qFind( annots, it.key() );
            if ( annIt != annItEnd )
            {
                (*it)->reloadInfo();
                ++it;
            }
            else
            {
                delete *it;
                it = d->m_annowindows.erase( it );
            }
        }
    }

    if ( changedFlags & DocumentObserver::BoundingBox )
    {
#ifdef PAGEVIEW_DEBUG
        kDebug() << "BoundingBox change on page" << pageNumber;
#endif
        slotRelayoutPages();
        slotRequestVisiblePixmaps(); // TODO: slotRelayoutPages() may have done this already!
        // Repaint the whole widget since layout may have changed
        widget()->update();
        return;
    }

    // iterate over visible items: if page(pageNumber) is one of them, repaint it
    QLinkedList< PageViewItem * >::const_iterator iIt = d->visibleItems.constBegin(), iEnd = d->visibleItems.constEnd();
    for ( ; iIt != iEnd; ++iIt )
        if ( (*iIt)->pageNumber() == pageNumber && (*iIt)->isVisible() )
        {
            // update item's rectangle plus the little outline
            QRect expandedRect = (*iIt)->croppedGeometry();
            expandedRect.adjust( -1, -1, 3, 3 );
            widget()->update( expandedRect );

            // if we were "zoom-dragging" do not overwrite the "zoom-drag" cursor
            if ( cursor().shape() != Qt::SizeVerCursor )
            {
                // since the page has been regenerated below cursor, update it
                updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
            }
            break;
        }
}

void PageView::notifyContentsCleared( int changedFlags )
{
    // if pixmaps were cleared, re-ask them
    if ( changedFlags & DocumentObserver::Pixmap )
        QMetaObject::invokeMethod(this, "slotRequestVisiblePixmaps", Qt::QueuedConnection);
}

void PageView::notifyZoom( int factor )
{
    if ( factor > 0 )
        updateZoom( ZoomIn );
    else
        updateZoom( ZoomOut );
}

bool PageView::canUnloadPixmap( int pageNumber ) const
{
    if ( Okular::Settings::memoryLevel() != Okular::Settings::EnumMemoryLevel::Aggressive )
    {
        // if the item is visible, forbid unloading
        QLinkedList< PageViewItem * >::const_iterator vIt = d->visibleItems.constBegin(), vEnd = d->visibleItems.constEnd();
        for ( ; vIt != vEnd; ++vIt )
            if ( (*vIt)->pageNumber() == pageNumber )
                return false;
    }
    else
    {
        // forbid unloading of the visible items, and of the previous and next
        QLinkedList< PageViewItem * >::const_iterator vIt = d->visibleItems.constBegin(), vEnd = d->visibleItems.constEnd();
        for ( ; vIt != vEnd; ++vIt )
            if ( abs( (*vIt)->pageNumber() - pageNumber ) <= 1 )
                return false;
    }
    // if hidden premit unloading
    return true;
}
//END DocumentObserver inherited methods

//BEGIN View inherited methods
bool PageView::supportsCapability( ViewCapability capability ) const
{
    switch ( capability )
    {
        case Zoom:
        case ZoomModality:
            return true;
    }
    return false;
}

Okular::View::CapabilityFlags PageView::capabilityFlags( ViewCapability capability ) const
{
    switch ( capability )
    {
        case Zoom:
        case ZoomModality:
            return CapabilityRead | CapabilityWrite | CapabilitySerializable;
    }
    return 0;
}

QVariant PageView::capability( ViewCapability capability ) const
{
    switch ( capability )
    {
        case Zoom:
            return d->zoomFactor;
        case ZoomModality:
            return d->zoomMode;
    }
    return QVariant();
}

void PageView::setCapability( ViewCapability capability, const QVariant &option )
{
    switch ( capability )
    {
        case Zoom:
        {
            bool ok = true;
            double factor = option.toDouble( &ok );
            if ( ok && factor > 0.0 )
            {
                d->zoomFactor = static_cast< float >( factor );
                updateZoom( ZoomRefreshCurrent );
            }
            break;
        }
        case ZoomModality:
        {
            bool ok = true;
            int mode = option.toInt( &ok );
            if ( ok )
            {
                if ( mode >= 0 && mode < 3 )
                    updateZoom( (ZoomMode)mode );
            }
            break;
        }
    }
}

//END View inherited methods

//BEGIN widget events
void PageView::contentsPaintEvent(QPaintEvent *pe)
{
        // create the rect into contents from the clipped screen rect
        QRect viewportRect = viewport()->rect();
        viewportRect.translate( horizontalScrollBar()->value(), verticalScrollBar()->value() );
        QRect contentsRect = pe->rect().intersect( viewportRect );
        if ( !contentsRect.isValid() )
            return;

#ifdef PAGEVIEW_DEBUG
        kDebug() << "paintevent" << contentsRect;
#endif

        // create the screen painter. a pixel painted at contentsX,contentsY
        // appears to the top-left corner of the scrollview.
        QPainter screenPainter( widget() );

        // selectionRect is the normalized mouse selection rect
        QRect selectionRect = d->mouseSelectionRect;
        if ( !selectionRect.isNull() )
            selectionRect = selectionRect.normalized();
        // selectionRectInternal without the border
        QRect selectionRectInternal = selectionRect;
        selectionRectInternal.adjust( 1, 1, -1, -1 );
        // color for blending
        QColor selBlendColor = (selectionRect.width() > 8 || selectionRect.height() > 8) ?
                            d->mouseSelectionColor : Qt::red;

        // subdivide region into rects
        const QVector<QRect> &allRects = pe->region().rects();
        uint numRects = allRects.count();

        // preprocess rects area to see if it worths or not using subdivision
        uint summedArea = 0;
        for ( uint i = 0; i < numRects; i++ )
        {
            const QRect & r = allRects[i];
            summedArea += r.width() * r.height();
        }
        // very elementary check: SUMj(Region[j].area) is less than boundingRect.area
        bool useSubdivision = summedArea < (0.6 * contentsRect.width() * contentsRect.height());
        if ( !useSubdivision )
            numRects = 1;

        // iterate over the rects (only one loop if not using subdivision)
        for ( uint i = 0; i < numRects; i++ )
        {
            if ( useSubdivision )
            {
                // set 'contentsRect' to a part of the sub-divided region
                contentsRect = allRects[i].normalized().intersect( viewportRect );
                if ( !contentsRect.isValid() )
                    continue;
            }
#ifdef PAGEVIEW_DEBUG
            kDebug() << contentsRect;
#endif

            // note: this check will take care of all things requiring alpha blending (not only selection)
            bool wantCompositing = !selectionRect.isNull() && contentsRect.intersects( selectionRect );

            if ( wantCompositing && Okular::Settings::enableCompositing() )
            {
                // create pixmap and open a painter over it (contents{left,top} becomes pixmap {0,0})
                QPixmap doubleBuffer( contentsRect.size() );
                QPainter pixmapPainter( &doubleBuffer );
                pixmapPainter.translate( -contentsRect.left(), -contentsRect.top() );

                // 1) Layer 0: paint items and clear bg on unpainted rects
                drawDocumentOnPainter( contentsRect, &pixmapPainter );
                // 2) Layer 1a: paint (blend) transparent selection
                if ( !selectionRect.isNull() && selectionRect.intersects( contentsRect ) &&
                    !selectionRectInternal.contains( contentsRect ) )
                {
                    QRect blendRect = selectionRectInternal.intersect( contentsRect );
                    // skip rectangles covered by the selection's border
                    if ( blendRect.isValid() )
                    {
                        // grab current pixmap into a new one to colorize contents
                        QPixmap blendedPixmap( blendRect.width(), blendRect.height() );
                        QPainter p( &blendedPixmap );
                        p.drawPixmap( 0, 0, doubleBuffer,
                                    blendRect.left() - contentsRect.left(), blendRect.top() - contentsRect.top(),
                                    blendRect.width(), blendRect.height() );

                        QColor blCol = selBlendColor.dark( 140 );
                        blCol.setAlphaF( 0.2 );
                        p.fillRect( blendedPixmap.rect(), blCol );
                        p.end();
                        // copy the blended pixmap back to its place
                        pixmapPainter.drawPixmap( blendRect.left(), blendRect.top(), blendedPixmap );
                    }
                    // draw border (red if the selection is too small)
                    pixmapPainter.setPen( selBlendColor );
                    pixmapPainter.drawRect( selectionRect.adjusted( 0, 0, -1, -1 ) );
                }
                // 3) Layer 1: give annotator painting control
                if ( d->annotator && d->annotator->routePaints( contentsRect ) )
                    d->annotator->routePaint( &pixmapPainter, contentsRect );
                // 4) Layer 2: overlays
                if ( Okular::Settings::debugDrawBoundaries() )
                {
                    pixmapPainter.setPen( Qt::blue );
                    pixmapPainter.drawRect( contentsRect );
                }

                // finish painting and draw contents
                pixmapPainter.end();
                screenPainter.drawPixmap( contentsRect.left(), contentsRect.top(), doubleBuffer );
            }
            else
            {
                // 1) Layer 0: paint items and clear bg on unpainted rects
                drawDocumentOnPainter( contentsRect, &screenPainter );
                // 2) Layer 1: paint opaque selection
                if ( !selectionRect.isNull() && selectionRect.intersects( contentsRect ) &&
                    !selectionRectInternal.contains( contentsRect ) )
                {
                    screenPainter.setPen( palette().color( QPalette::Active, QPalette::Highlight ).dark(110) );
                    screenPainter.drawRect( selectionRect );
                }
                // 3) Layer 1: give annotator painting control
                if ( d->annotator && d->annotator->routePaints( contentsRect ) )
                    d->annotator->routePaint( &screenPainter, contentsRect );
                // 4) Layer 2: overlays
                if ( Okular::Settings::debugDrawBoundaries() )
                {
                    screenPainter.setPen( Qt::red );
                    screenPainter.drawRect( contentsRect );
                }
            }
        }
}

void PageView::resizeEvent( QResizeEvent *e )
{
    if ( d->items.isEmpty() )
    {
        widget()->resize(e->size());
        return;
    }

    if ( d->zoomMode == ZoomFitWidth && d->bothScrollbarsVisible && !horizontalScrollBar()->isVisible() && !verticalScrollBar()->isVisible() && qAbs(e->oldSize().height() - e->size().height()) < horizontalScrollBar()->height() * 1.25 )
    {
        // this saves us from infinite resizing loop because of scrollbars appearing and disappearing
        // see bug 160628 for more info
        // TODO looks are still a bit ugly because things are left uncentered 
        // but better a bit ugly than unusable
        d->bothScrollbarsVisible = false;
        widget()->resize( e->size() );
        return;
    }

    // start a timer that will refresh the pixmap after 0.2s
    if ( !d->delayResizeTimer )
    {
        d->delayResizeTimer = new QTimer( this );
        d->delayResizeTimer->setSingleShot( true );
        connect( d->delayResizeTimer, SIGNAL( timeout() ), this, SLOT( slotRelayoutPages() ) );
    }
    d->delayResizeTimer->start( 200 );

    d->bothScrollbarsVisible = horizontalScrollBar()->isVisible() && verticalScrollBar()->isVisible();
}

void PageView::keyPressEvent( QKeyEvent * e )
{
    e->accept();

    // if performing a selection or dyn zooming, disable keys handling
    if ( ( d->mouseSelecting && e->key() != Qt::Key_Escape ) || d->mouseMidZooming )
        return;

    // if viewport is moving, disable keys handling
    if ( d->viewportMoveActive )
        return;

    // move/scroll page by using keys
    switch ( e->key() )
    {
        case Qt::Key_J:
        case Qt::Key_K:
        case Qt::Key_Down:
        case Qt::Key_PageDown:
        case Qt::Key_Space:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
        case Qt::Key_Backspace:
            if ( e->key() == Qt::Key_Down
                 || e->key() == Qt::Key_PageDown
                 || e->key() == Qt::Key_J
                 || ( e->key() == Qt::Key_Space && ( e->modifiers() & Qt::ShiftModifier ) != Qt::ShiftModifier ) )
            {
                // if in single page mode and at the bottom of the screen, go to next page
                if ( Okular::Settings::viewContinuous() || verticalScrollBar()->value() < verticalScrollBar()->maximum() )
                {
                    if ( e->key() == Qt::Key_Down || e->key() == Qt::Key_J )
                        verticalScrollBar()->triggerAction( QScrollBar::SliderSingleStepAdd );
                    else
                        verticalScrollBar()->triggerAction( QScrollBar::SliderPageStepAdd );
                }
                else if ( (int)d->document->currentPage() < d->items.count() - 1 )
                {
                   // more optimized than document->setNextPage and then move view to top
                    Okular::DocumentViewport newViewport = d->document->viewport();
                    newViewport.pageNumber += d->document->currentPage() ? viewColumns() : 1;
                    if ( newViewport.pageNumber >= (int)d->items.count() )
                        newViewport.pageNumber = d->items.count() - 1;
                    newViewport.rePos.enabled = true;
                    newViewport.rePos.normalizedY = 0.0;
                    d->document->setViewport( newViewport );
                }
            }
            else
            {
                // if in single page mode and at the top of the screen, go to \ page
                if ( Okular::Settings::viewContinuous() || verticalScrollBar()->value() > verticalScrollBar()->minimum() )
                {
                   if ( e->key() == Qt::Key_Up || e->key() == Qt::Key_K )
                        verticalScrollBar()->triggerAction( QScrollBar::SliderSingleStepSub );
                   else
                       verticalScrollBar()->triggerAction( QScrollBar::SliderPageStepSub );
                }
                else if ( d->document->currentPage() > 0 )
                {
                    // more optimized than document->setPrevPage and then move view to bottom
                    Okular::DocumentViewport newViewport = d->document->viewport();
                    newViewport.pageNumber -= viewColumns();
                    if ( newViewport.pageNumber < 0 )
                        newViewport.pageNumber = 0;
                    newViewport.rePos.enabled = true;
                    newViewport.rePos.normalizedY = 1.0;
                    d->document->setViewport( newViewport );
                }
            }
            break;
        case Qt::Key_Left:
        case Qt::Key_H:
            horizontalScrollBar()->triggerAction( QScrollBar::SliderSingleStepSub );
            break;
        case Qt::Key_Right:
        case Qt::Key_L:
            horizontalScrollBar()->triggerAction( QScrollBar::SliderSingleStepAdd );
            break;
        case Qt::Key_Escape:
            selectionClear();
            d->mousePressPos = QPoint();
            if ( d->aPrevAction )
            {
                d->aPrevAction->trigger();
                d->aPrevAction = 0;
            }
            break;
        case Qt::Key_Shift:
        case Qt::Key_Control:
            if ( d->autoScrollTimer )
            {
                if ( d->autoScrollTimer->isActive() )
                    d->autoScrollTimer->stop();
                else
                    slotAutoScoll();
                return;
            }
            // else fall trhough
        default:
            e->ignore();
            return;
    }
    // if a known key has been pressed, stop scrolling the page
    if ( d->autoScrollTimer )
    {
        d->scrollIncrement = 0;
        d->autoScrollTimer->stop();
    }
}

void PageView::keyReleaseEvent( QKeyEvent * e )
{
    e->accept();

    if ( d->annotator && d->annotator->routeEvents() )
    {
        if ( d->annotator->routeKeyEvent( e ) )
            return;
    }

    if ( e->key() == Qt::Key_Escape && d->autoScrollTimer )
    {
        d->scrollIncrement = 0;
        d->autoScrollTimer->stop();
    }
}

void PageView::inputMethodEvent( QInputMethodEvent * e )
{
    Q_UNUSED(e)
}

static QPoint rotateInRect( const QPoint &rotated, Okular::Rotation rotation )
{
    QPoint ret;

    switch ( rotation )
    {
        case Okular::Rotation90:
            ret = QPoint( rotated.y(), -rotated.x() );
            break;
        case Okular::Rotation180:
            ret = QPoint( -rotated.x(), -rotated.y() );
            break;
        case Okular::Rotation270:
            ret = QPoint( -rotated.y(), rotated.x() );
            break;
        case Okular::Rotation0:  // no modifications
        default: // other cases
            ret = rotated;
    }

    return ret;
}

void PageView::contentsMouseMoveEvent( QMouseEvent * e )
{
    // don't perform any mouse action when no document is shown
    if ( d->items.isEmpty() )
        return;

    // don't perform any mouse action when viewport is autoscrolling
    if ( d->viewportMoveActive )
        return;

    // if holding mouse mid button, perform zoom
    if ( d->mouseMidZooming && (e->buttons() & Qt::MidButton) )
    {
        int mouseY = e->globalPos().y();
        int deltaY = d->mouseMidLastY - mouseY;

        // wrap mouse from top to bottom
        QRect mouseContainer = KGlobalSettings::desktopGeometry( this );
        if ( mouseY <= mouseContainer.top() + 4 &&
             d->zoomFactor < 3.99 )
        {
            mouseY = mouseContainer.bottom() - 5;
            QCursor::setPos( e->globalPos().x(), mouseY );
        }
        // wrap mouse from bottom to top
        else if ( mouseY >= mouseContainer.bottom() - 4 &&
                  d->zoomFactor > 0.11 )
        {
            mouseY = mouseContainer.top() + 5;
            QCursor::setPos( e->globalPos().x(), mouseY );
        }
        // remember last position
        d->mouseMidLastY = mouseY;

        // update zoom level, perform zoom and redraw
        if ( deltaY )
        {
            d->zoomFactor *= ( 1.0 + ( (double)deltaY / 500.0 ) );
            updateZoom( ZoomRefreshCurrent );
            viewport()->repaint();
        }
        return;
    }

    // if we're editing an annotation, dispatch event to it
    if ( d->annotator && d->annotator->routeEvents() )
    {
        PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
        d->annotator->routeEvent( e, pageItem );
        return;
    }

    bool leftButton = (e->buttons() == Qt::LeftButton);
    bool rightButton = (e->buttons() == Qt::RightButton);
    switch ( d->mouseMode )
    {
        case MouseNormal:
            if ( leftButton )
            {
                if ( d->mouseAnn )
                {
                    PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
                    if ( pageItem )
                    {
                        const QRect & itemRect = pageItem->uncroppedGeometry();
                        QPoint newpos = QPoint( e->x(), e->y() ) - itemRect.topLeft();
                        Okular::NormalizedRect r = d->mouseAnn->boundingRectangle();
                        QPoint p( newpos - d->mouseAnnPos );
                        QPointF pf( rotateInRect( p, pageItem->page()->rotation() ) );
                        if ( pageItem->page()->rotation() % 2 == 0 )
                        {
                            pf.rx() /= pageItem->uncroppedWidth();
                            pf.ry() /= pageItem->uncroppedHeight();
                        }
                        else
                        {
                            pf.rx() /= pageItem->uncroppedHeight();
                            pf.ry() /= pageItem->uncroppedWidth();
                        }
                        d->mouseAnn->translate( Okular::NormalizedPoint( pf.x(), pf.y() ) );
                        d->mouseAnnPos = newpos;
                        d->document->modifyPageAnnotation( pageItem->pageNumber(), d->mouseAnn );
                    }
                }
                // drag page
                else if ( !d->mouseGrabPos.isNull() )
                {
                    QPoint mousePos = e->globalPos();
                    QPoint delta = d->mouseGrabPos - mousePos;

                    // wrap mouse from top to bottom
                    QRect mouseContainer = KGlobalSettings::desktopGeometry( this );
                    if ( mousePos.y() <= mouseContainer.top() + 4 &&
                         verticalScrollBar()->value() < verticalScrollBar()->maximum() - 10 )
                    {
                        mousePos.setY( mouseContainer.bottom() - 5 );
                        QCursor::setPos( mousePos );
                    }
                    // wrap mouse from bottom to top
                    else if ( mousePos.y() >= mouseContainer.bottom() - 4 &&
                              verticalScrollBar()->value() > 10 )
                    {
                        mousePos.setY( mouseContainer.top() + 5 );
                        QCursor::setPos( mousePos );
                    }
                    // remember last position
                    d->mouseGrabPos = mousePos;

                    // scroll page by position increment
                    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + delta.x());
                    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta.y());
                }
            }
            else if ( rightButton && !d->mousePressPos.isNull() )
            {
                // if mouse moves 5 px away from the press point, switch to 'selection'
                int deltaX = d->mousePressPos.x() - e->globalPos().x(),
                    deltaY = d->mousePressPos.y() - e->globalPos().y();
                if ( deltaX > 5 || deltaX < -5 || deltaY > 5 || deltaY < -5 )
                {
                    d->aPrevAction = d->aMouseNormal;
                    d->aMouseSelect->trigger();
                    QPoint newPos(e->x() + deltaX, e->y() + deltaY);
                    selectionStart( newPos, palette().color( QPalette::Active, QPalette::Highlight ).light( 120 ), false );
                    selectionEndPoint( e->pos() );
                    break;
                }
            }
            else
            {
                // only hovering the page, so update the cursor
                updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
            }
            break;

        case MouseZoom:
        case MouseSelect:
        case MouseImageSelect:
            // set second corner of selection
            if ( d->mouseSelecting )
                selectionEndPoint( e->pos() );
            break;
        case MouseTextSelect:
            // if mouse moves 5 px away from the press point and the document soupports text extraction, do 'textselection'
            if ( !d->mouseTextSelecting && !d->mousePressPos.isNull() && d->document->supportsSearching() && ( ( e->pos() - d->mouseSelectPos ).manhattanLength() > 5 ) )
            {
                d->mouseTextSelecting = true;
            }
            if ( d->mouseTextSelecting )
            {
                int first = -1;
                QList< Okular::RegularAreaRect * > selections = textSelections( e->pos(), d->mouseSelectPos, first );
                QSet< int > pagesWithSelectionSet;
                for ( int i = 0; i < selections.count(); ++i )
                    pagesWithSelectionSet.insert( i + first );

                QSet< int > noMoreSelectedPages = d->pagesWithTextSelection - pagesWithSelectionSet;
                // clear the selection from pages not selected anymore
                foreach( int p, noMoreSelectedPages )
                {
                    d->document->setPageTextSelection( p, 0, QColor() );
                }
                // set the new selection for the selected pages
                foreach( int p, pagesWithSelectionSet )
                {
                    d->document->setPageTextSelection( p, selections[ p - first ], palette().color( QPalette::Active, QPalette::Highlight ) );
                }
                d->pagesWithTextSelection = pagesWithSelectionSet;
            }
            updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
            break;
    }
}

void PageView::contentsMousePressEvent( QMouseEvent * e )
{
    // don't perform any mouse action when no document is shown
    if ( d->items.isEmpty() )
        return;

    // if performing a selection or dyn zooming, disable mouse press
    if ( d->mouseSelecting || d->mouseMidZooming || d->viewportMoveActive )
        return;

    // if the page is scrolling, stop it
    if ( d->autoScrollTimer )
    {
        d->scrollIncrement = 0;
        d->autoScrollTimer->stop();
    }

    // if pressing mid mouse button while not doing other things, begin 'continuous zoom' mode
    if ( e->button() == Qt::MidButton )
    {
        d->mouseMidZooming = true;
        d->mouseMidLastY = e->globalPos().y();
        setCursor( Qt::SizeVerCursor );
        return;
    }

    // if we're editing an annotation, dispatch event to it
    if ( d->annotator && d->annotator->routeEvents() )
    {
        PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
        d->annotator->routeEvent( e, pageItem );
        return;
    }

    // update press / 'start drag' mouse position
    d->mousePressPos = e->globalPos();

    // handle mode dependant mouse press actions
    bool leftButton = e->button() == Qt::LeftButton,
         rightButton = e->button() == Qt::RightButton;

//   Not sure we should erase the selection when clicking with left.
     if ( d->mouseMode != MouseTextSelect )
       textSelectionClear();

    switch ( d->mouseMode )
    {
        case MouseNormal:   // drag start / click / link following
            if ( leftButton )
            {
                PageViewItem * pageItem = 0;
                if ( ( e->modifiers() & Qt::ControlModifier ) && ( pageItem = pickItemOnPoint( e->x(), e->y() ) ) )
                {
                    // find out normalized mouse coords inside current item
                    const QRect & itemRect = pageItem->uncroppedGeometry();
                    double nX = pageItem->absToPageX(e->x());
                    double nY = pageItem->absToPageY(e->y());
                    const Okular::ObjectRect * orect = pageItem->page()->objectRect( Okular::ObjectRect::OAnnotation, nX, nY, itemRect.width(), itemRect.height() );
                    d->mouseAnnPos = QPoint( e->x(), e->y() ) - itemRect.topLeft();
                    if ( orect )
                        d->mouseAnn = ( (Okular::AnnotationObjectRect *)orect )->annotation();
                    // consider no annotation caught if its type is not movable
                    if ( d->mouseAnn && !d->mouseAnn->canBeMoved() )
                        d->mouseAnn = 0;
                }
                if ( !d->mouseAnn )
                {
                d->mouseGrabPos = d->mouseOnRect ? QPoint() : d->mousePressPos;
                if ( !d->mouseOnRect )
                    setCursor( Qt::SizeAllCursor );
                }
            }
            else if ( rightButton )
            {
                PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
                if ( pageItem )
                {
                    // find out normalized mouse coords inside current item
                    const QRect & itemRect = pageItem->uncroppedGeometry();
                    double nX = pageItem->absToPageX(e->x());
                    double nY = pageItem->absToPageY(e->y());
                    Okular::Annotation * ann = 0;
                    const Okular::ObjectRect * orect = pageItem->page()->objectRect( Okular::ObjectRect::OAnnotation, nX, nY, itemRect.width(), itemRect.height() );
                    if ( orect )
                        ann = ( (Okular::AnnotationObjectRect *)orect )->annotation();
                    if ( ann )
                    {
                        AnnotationPopup popup( d->document, this );
                        popup.addAnnotation( ann, pageItem->pageNumber() );

                        connect( &popup, SIGNAL( setAnnotationWindow( Okular::Annotation* ) ),
                                 this, SLOT( setAnnotationWindow( Okular::Annotation* ) ) );
                        connect( &popup, SIGNAL( removeAnnotationWindow( Okular::Annotation* ) ),
                                 this, SLOT( removeAnnotationWindow( Okular::Annotation* ) ) );

                        popup.exec( e->globalPos() );
                    }
                }
            }
            break;

        case MouseZoom:     // set first corner of the zoom rect
            if ( leftButton )
                selectionStart( e->pos(), palette().color( QPalette::Active, QPalette::Highlight ), false );
            else if ( rightButton )
                updateZoom( ZoomOut );
            break;

        case MouseSelect:   // set first corner of the selection rect
        case MouseImageSelect:
             if ( leftButton )
             {
                selectionStart( e->pos(), palette().color( QPalette::Active, QPalette::Highlight ).light( 120 ), false );
             }
            break;
        case MouseTextSelect:
            d->mouseSelectPos = e->pos();
            if ( !rightButton )
            {
                textSelectionClear();
            }
            break;
    }
}

void PageView::contentsMouseReleaseEvent( QMouseEvent * e )
{
    // stop the drag scrolling
    d->dragScrollTimer.stop();

    // don't perform any mouse action when no document is shown..
    if ( d->items.isEmpty() )
    {
        // ..except for right Clicks (emitted even it viewport is empty)
        if ( e->button() == Qt::RightButton )
            emit rightClick( 0, e->globalPos() );
        return;
    }

    // don't perform any mouse action when viewport is autoscrolling
    if ( d->viewportMoveActive )
        return;

    // handle mode indepent mid buttom zoom
    if ( d->mouseMidZooming && (e->button() == Qt::MidButton) )
    {
        d->mouseMidZooming = false;
        // request pixmaps since it was disabled during drag
        slotRequestVisiblePixmaps();
        // the cursor may now be over a link.. update it
        updateCursor( e->pos() );
        return;
    }

    // if we're editing an annotation, dispatch event to it
    if ( d->annotator && d->annotator->routeEvents() )
    {
        PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
        d->annotator->routeEvent( e, pageItem );
        return;
    }

    if ( d->mouseAnn )
    {
        setCursor( Qt::ArrowCursor );
        d->mouseAnn = 0;
    }

    bool leftButton = e->button() == Qt::LeftButton;
    bool rightButton = e->button() == Qt::RightButton;
    switch ( d->mouseMode )
    {
        case MouseNormal:{
            // return the cursor to its normal state after dragging
            if ( cursor().shape() == Qt::SizeAllCursor )
                updateCursor( e->pos() );

            PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );

            // if the mouse has not moved since the press, that's a -click-
            if ( leftButton && pageItem && d->mousePressPos == e->globalPos())
            {
                double nX = pageItem->absToPageX(e->x());
                double nY = pageItem->absToPageY(e->y());
                const Okular::ObjectRect * rect;
                rect = pageItem->page()->objectRect( Okular::ObjectRect::Action, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                if ( rect )
                {
                    // handle click over a link
                    const Okular::Action * action = static_cast< const Okular::Action * >( rect->object() );
                    d->document->processAction( action );
                }
                else if ( e->modifiers() == Qt::ShiftModifier )
                {
                    // TODO: find a better way to activate the source reference "links"
                    // for the moment they are activated with Shift + left click
                    // Search the nearest source reference.
                    rect = pageItem->page()->objectRect( Okular::ObjectRect::SourceRef, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                    if ( !rect )
                    {
                        static const double s_minDistance = 0.025; // FIXME?: empirical value?
                        double distance = 0.0;
                        rect = pageItem->page()->nearestObjectRect( Okular::ObjectRect::SourceRef, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight(), &distance );
                        if ( rect && ( distance > s_minDistance ) )
                            rect = 0;
                    }
                    if ( rect )
                    {
                        const Okular::SourceReference * ref = static_cast< const Okular::SourceReference * >( rect->object() );
                        d->document->processSourceReference( ref );
                    }
                }
                else
                {
#if 0
                    // a link can move us to another page or even to another document, there's no point in trying to
                    //  process the click on the image once we have processes the click on the link
                    rect = pageItem->page()->objectRect( Okular::ObjectRect::Image, nX, nY, pageItem->width(), pageItem->height() );
                    if ( rect )
                    {
                        // handle click over a image
                    }
/*		Enrico and me have decided this is not worth the trouble it generates
                    else
                    {
                        // if not on a rect, the click selects the page
                        // if ( pageItem->pageNumber() != (int)d->document->currentPage() )
                        d->document->setViewportPage( pageItem->pageNumber(), PAGEVIEW_ID );
                    }*/
#endif
                }
            }
            else if ( rightButton )
            {
                if ( pageItem && d->mousePressPos == e->globalPos() )
                {
                    double nX = pageItem->absToPageX(e->x());
                    double nY = pageItem->absToPageY(e->y());
                    const Okular::ObjectRect * rect;
                    rect = pageItem->page()->objectRect( Okular::ObjectRect::Action, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                    if ( rect )
                    {
                        // handle right click over a link
                        const Okular::Action * link = static_cast< const Okular::Action * >( rect->object() );
                        // creating the menu and its actions
                        KMenu menu( this );
                        QAction * actProcessLink = menu.addAction( i18n( "Follow This Link" ) );
                        QAction * actCopyLinkLocation = 0;
                        if ( dynamic_cast< const Okular::BrowseAction * >( link ) )
                            actCopyLinkLocation = menu.addAction( KIcon( "edit-copy" ), i18n( "Copy Link Address" ) );
                        QAction * res = menu.exec( e->globalPos() );
                        if ( res )
                        {
                            if ( res == actProcessLink )
                            {
                                d->document->processAction( link );
                            }
                            else if ( res == actCopyLinkLocation )
                            {
                                const Okular::BrowseAction * browseLink = static_cast< const Okular::BrowseAction * >( link );
                                QClipboard *cb = QApplication::clipboard();
                                cb->setText( browseLink->url(), QClipboard::Clipboard );
                                if ( cb->supportsSelection() )
                                    cb->setText( browseLink->url(), QClipboard::Selection );
                            }
                        }
                    }
                    else
                    {
                        // a link can move us to another page or even to another document, there's no point in trying to
                        //  process the click on the image once we have processes the click on the link
                        rect = pageItem->page()->objectRect( Okular::ObjectRect::Image, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
                        if ( rect )
                        {
                            // handle right click over a image
                        }
                        else
                        {
                            // right click (if not within 5 px of the press point, the mode
                            // had been already changed to 'Selection' instead of 'Normal')
                            emit rightClick( pageItem->page(), e->globalPos() );
                        }
                    }
                }
                else
                {
                    // right click (if not within 5 px of the press point, the mode
                    // had been already changed to 'Selection' instead of 'Normal')
                    emit rightClick( pageItem ? pageItem->page() : 0, e->globalPos() );
                }
            }
            }break;

        case MouseZoom:
            // if a selection rect has been defined, zoom into it
            if ( leftButton && d->mouseSelecting )
            {
                QRect selRect = d->mouseSelectionRect.normalized();
                if ( selRect.width() <= 8 && selRect.height() <= 8 )
                {
                    selectionClear();
                    break;
                }

                // find out new zoom ratio and normalized view center (relative to the contentsRect)
                double zoom = qMin( (double)viewport()->width() / (double)selRect.width(), (double)viewport()->height() / (double)selRect.height() );
                double nX = (double)(selRect.left() + selRect.right()) / (2.0 * (double)widget()->width());
                double nY = (double)(selRect.top() + selRect.bottom()) / (2.0 * (double)widget()->height());

                // zoom up to 400%
                if ( d->zoomFactor <= 4.0 || zoom <= 1.0 )
                {
                    d->zoomFactor *= zoom;
                    viewport()->setUpdatesEnabled( false );
                    updateZoom( ZoomRefreshCurrent );
                    viewport()->setUpdatesEnabled( true );
                }

                // recenter view and update the viewport
                center( (int)(nX * widget()->width()), (int)(nY * widget()->height()) );
                widget()->update();

                // hide message box and delete overlay window
                selectionClear();
            }
            break;

        case MouseSelect:
        case MouseImageSelect:
        {
            // if mouse is released and selection is null this is a rightClick
            if ( rightButton && !d->mouseSelecting )
            {
                PageViewItem * pageItem = pickItemOnPoint( e->x(), e->y() );
                emit rightClick( pageItem ? pageItem->page() : 0, e->globalPos() );
                break;
            }

            // if a selection is defined, display a popup
            if ( (!leftButton && !d->aPrevAction) || (!rightButton && d->aPrevAction) ||
                 !d->mouseSelecting )
                break;

            QRect selectionRect = d->mouseSelectionRect.normalized();
            if ( selectionRect.width() <= 8 && selectionRect.height() <= 8 )
            {
                selectionClear();
                if ( d->aPrevAction )
                {
                    d->aPrevAction->trigger();
                    d->aPrevAction = 0;
                }
                break;
            }

            // if we support text generation
            QString selectedText;
            if (d->document->supportsSearching())
            {
                // grab text in selection by extracting it from all intersected pages
                const Okular::Page * okularPage=0;
                QVector< PageViewItem * >::const_iterator iIt = d->items.constBegin(), iEnd = d->items.constEnd();
                for ( ; iIt != iEnd; ++iIt )
                {
                    PageViewItem * item = *iIt;
                    if ( !item->isVisible() )
                        continue;

                    const QRect & itemRect = item->croppedGeometry();
                    if ( selectionRect.intersects( itemRect ) )
                    {
                        // request the textpage if there isn't one
                        okularPage= item->page();
                        kWarning() << "checking if page" << item->pageNumber() << "has text:" << okularPage->hasTextPage();
                        if ( !okularPage->hasTextPage() )
                            d->document->requestTextPage( okularPage->number() );
                        // grab text in the rect that intersects itemRect
                        QRect relativeRect = selectionRect.intersect( itemRect );
                        relativeRect.translate( -item->uncroppedGeometry().topLeft() );
                        Okular::RegularAreaRect rects;
                        rects.append( Okular::NormalizedRect( relativeRect, item->uncroppedWidth(), item->uncroppedHeight() ) );
                        selectedText += okularPage->text( &rects );
                    }
                }
            }

            // popup that ask to copy:text and copy/save:image
            KMenu menu( this );
            QAction *textToClipboard = 0, *speakText = 0, *imageToClipboard = 0, *imageToFile = 0;
            if ( d->document->supportsSearching() && !selectedText.isEmpty() )
            {
                menu.addTitle( i18np( "Text (1 character)", "Text (%1 characters)", selectedText.length() ) );
                textToClipboard = menu.addAction( KIcon("edit-copy"), i18n( "Copy to Clipboard" ) );
                if ( !d->document->isAllowed( Okular::AllowCopy ) )
                {
                    textToClipboard->setEnabled( false );
                    textToClipboard->setText( i18n("Copy forbidden by DRM") );
                }
                if ( Okular::Settings::useKTTSD() )
                    speakText = menu.addAction( KIcon("text-speak"), i18n( "Speak Text" ) );
            }
            menu.addTitle( i18n( "Image (%1 by %2 pixels)", selectionRect.width(), selectionRect.height() ) );
            imageToClipboard = menu.addAction( KIcon("image-x-generic"), i18n( "Copy to Clipboard" ) );
            imageToFile = menu.addAction( KIcon("document-save"), i18n( "Save to File..." ) );
            QAction *choice = menu.exec( e->globalPos() );
            // check if the user really selected an action
            if ( choice )
            {
            // IMAGE operation chosen
            if ( choice == imageToClipboard || choice == imageToFile )
            {
                // renders page into a pixmap
                QPixmap copyPix( selectionRect.width(), selectionRect.height() );
                QPainter copyPainter( &copyPix );
                copyPainter.translate( -selectionRect.left(), -selectionRect.top() );
                drawDocumentOnPainter( selectionRect, &copyPainter );
                copyPainter.end();

                if ( choice == imageToClipboard )
                {
                    // [2] copy pixmap to clipboard
                    QClipboard *cb = QApplication::clipboard();
                    cb->setPixmap( copyPix, QClipboard::Clipboard );
                    if ( cb->supportsSelection() )
                        cb->setPixmap( copyPix, QClipboard::Selection );
                    d->messageWindow->display( i18n( "Image [%1x%2] copied to clipboard.", copyPix.width(), copyPix.height() ) );
                }
                else if ( choice == imageToFile )
                {
                    // [3] save pixmap to file
                    QString fileName = KFileDialog::getSaveFileName( KUrl(), "image/png image/jpeg", this );
                    if ( fileName.isEmpty() )
                        d->messageWindow->display( i18n( "File not saved." ), PageViewMessage::Warning );
                    else
                    {
                        KMimeType::Ptr mime = KMimeType::findByUrl( fileName );
                        QString type;
                        if ( !mime || mime == KMimeType::defaultMimeTypePtr() )
                            type = "PNG";
                        else
                            type = mime->name().section( '/', -1 ).toUpper();
                        copyPix.save( fileName, qPrintable( type ) );
                        d->messageWindow->display( i18n( "Image [%1x%2] saved to %3 file.", copyPix.width(), copyPix.height(), type ) );
                    }
                }
            }
            // TEXT operation chosen
            else
            {
                if ( choice == textToClipboard )
                {
                    // [1] copy text to clipboard
                    QClipboard *cb = QApplication::clipboard();
                    cb->setText( selectedText, QClipboard::Clipboard );
                    if ( cb->supportsSelection() )
                        cb->setText( selectedText, QClipboard::Selection );
                }
                else if ( choice == speakText )
                {
                    // [2] speech selection using KTTSD
                    d->tts()->say( selectedText );
                }
            }
            }
            // clear widget selection and invalidate rect
            selectionClear();

            // restore previous action if came from it using right button
            if ( d->aPrevAction )
            {
                d->aPrevAction->trigger();
                d->aPrevAction = 0;
            }
            }break;
            case MouseTextSelect:
                setCursor( Qt::ArrowCursor );
                if ( d->mouseTextSelecting )
                {
                    d->mouseTextSelecting = false;
//                    textSelectionClear();
                    if ( d->document->isAllowed( Okular::AllowCopy ) )
                    {
                        const QString text = d->selectedText();
                        if ( !text.isEmpty() )
                        {
                            QClipboard *cb = QApplication::clipboard();
                            if ( cb->supportsSelection() )
                                cb->setText( text, QClipboard::Selection );
                        }
                    }
                }
                else if ( !d->mousePressPos.isNull() && rightButton )
                {
                    KMenu menu( this );
                    QAction *textToClipboard = menu.addAction( KIcon( "edit-copy" ), i18n( "Copy Text" ) );
                    QAction *speakText = 0;
                    if ( Okular::Settings::useKTTSD() )
                        speakText = menu.addAction( KIcon( "text-speak" ), i18n( "Speak Text" ) );
                    if ( !d->document->isAllowed( Okular::AllowCopy ) )
                    {
                        textToClipboard->setEnabled( false );
                        textToClipboard->setText( i18n("Copy forbidden by DRM") );
                    }
                    QAction *choice = menu.exec( e->globalPos() );
                    // check if the user really selected an action
                    if ( choice )
                    {
                        if ( choice == textToClipboard )
                            copyTextSelection();
                        else if ( choice == speakText )
                        {
                            const QString text = d->selectedText();
                            d->tts()->say( text );
                        }
                    }
                }
            break;
    }

    // reset mouse press / 'drag start' position
    d->mousePressPos = QPoint();
}

void PageView::wheelEvent( QWheelEvent *e )
{
    // don't perform any mouse action when viewport is autoscrolling
    if ( d->viewportMoveActive )
        return;

    if ( !d->document->isOpened() )
    {
        QScrollArea::wheelEvent( e );
        return;
    }

    int delta = e->delta(),
        vScroll = verticalScrollBar()->value();
    e->accept();
    if ( (e->modifiers() & Qt::ControlModifier) == Qt::ControlModifier ) {
        if ( e->delta() < 0 )
            slotZoomOut();
        else
            slotZoomIn();
    }
    else if ( delta <= -120 && !Okular::Settings::viewContinuous() && vScroll == verticalScrollBar()->maximum() )
    {
        // go to next page
        if ( (int)d->document->currentPage() < d->items.count() - 1 )
        {
            // more optimized than document->setNextPage and then move view to top
            Okular::DocumentViewport newViewport = d->document->viewport();
            newViewport.pageNumber += d->document->currentPage() ? viewColumns() : 1;
            if ( newViewport.pageNumber >= (int)d->items.count() )
                newViewport.pageNumber = d->items.count() - 1;
            newViewport.rePos.enabled = true;
            newViewport.rePos.normalizedY = 0.0;
            d->document->setViewport( newViewport );
        }
    }
    else if ( delta >= 120 && !Okular::Settings::viewContinuous() && vScroll == verticalScrollBar()->minimum() )
    {
        // go to prev page
        if ( d->document->currentPage() > 0 )
        {
            // more optimized than document->setPrevPage and then move view to bottom
            Okular::DocumentViewport newViewport = d->document->viewport();
            newViewport.pageNumber -= viewColumns();
            if ( newViewport.pageNumber < 0 )
                newViewport.pageNumber = 0;
            newViewport.rePos.enabled = true;
            newViewport.rePos.normalizedY = 1.0;
            d->document->setViewport( newViewport );
        }
    }
    else
        QScrollArea::wheelEvent( e );

    QPoint cp = widget()->mapFromGlobal(mapToGlobal(e->pos()));
    updateCursor(cp);
}

void PageView::dragEnterEvent( QDragEnterEvent * ev )
{
    ev->accept();
}

void PageView::dragMoveEvent( QDragMoveEvent * ev )
{
    ev->accept();
}

void PageView::dropEvent( QDropEvent * ev )
{
    if (  KUrl::List::canDecode(  ev->mimeData() ) )
        emit urlDropped( KUrl::List::fromMimeData( ev->mimeData() ).first() );
}
//END widget events

QList< Okular::RegularAreaRect * > PageView::textSelections( const QPoint& start, const QPoint& end, int& firstpage )
{
    firstpage = -1;
    QList< Okular::RegularAreaRect * > ret;
    QSet< int > affectedItemsSet;
    QRect selectionRect = QRect( start, end ).normalized();
    foreach( PageViewItem * item, d->items )
    {
        if ( item->isVisible() && selectionRect.intersects( item->croppedGeometry() ) )
            affectedItemsSet.insert( item->pageNumber() );
    }
#ifdef PAGEVIEW_DEBUG
    kDebug() << ">>>> item selected by mouse:" << affectedItemsSet.count();
#endif

    if ( !affectedItemsSet.isEmpty() )
    {
        // is the mouse drag line the ne-sw diagonal of the selection rect?
        bool direction_ne_sw = start == selectionRect.topRight() || start == selectionRect.bottomLeft();

        int tmpmin = d->document->pages();
        int tmpmax = 0;
        foreach( int p, affectedItemsSet )
        {
            if ( p < tmpmin ) tmpmin = p;
            if ( p > tmpmax ) tmpmax = p;
        }

        PageViewItem * a = pickItemOnPoint( (int)( direction_ne_sw ? selectionRect.right() : selectionRect.left() ), (int)selectionRect.top() );
        int min = a && ( a->pageNumber() != tmpmax ) ? a->pageNumber() : tmpmin;
        PageViewItem * b = pickItemOnPoint( (int)( direction_ne_sw ? selectionRect.left() : selectionRect.right() ), (int)selectionRect.bottom() );
        int max = b && ( b->pageNumber() != tmpmin ) ? b->pageNumber() : tmpmax;

        QList< int > affectedItemsIds;
        for ( int i = min; i <= max; ++i )
            affectedItemsIds.append( i );
#ifdef PAGEVIEW_DEBUG
        kDebug() << ">>>> pages:" << affectedItemsIds;
#endif
        firstpage = affectedItemsIds.first();

        if ( affectedItemsIds.count() == 1 )
        {
            PageViewItem * item = d->items[ affectedItemsIds.first() ];
            selectionRect.translate( -item->uncroppedGeometry().topLeft() );
            ret.append( textSelectionForItem( item,
                direction_ne_sw ? selectionRect.topRight() : selectionRect.topLeft(),
                direction_ne_sw ? selectionRect.bottomLeft() : selectionRect.bottomRight() ) );
        }
        else if ( affectedItemsIds.count() > 1 )
        {
            // first item
            PageViewItem * first = d->items[ affectedItemsIds.first() ];
            QRect geom = first->croppedGeometry().intersect( selectionRect ).translated( -first->uncroppedGeometry().topLeft() );
            ret.append( textSelectionForItem( first,
                selectionRect.bottom() > geom.height() ? ( direction_ne_sw ? geom.topRight() : geom.topLeft() ) : ( direction_ne_sw ? geom.bottomRight() : geom.bottomLeft() ),
                QPoint() ) );
            // last item
            PageViewItem * last = d->items[ affectedItemsIds.last() ];
            geom = last->croppedGeometry().intersect( selectionRect ).translated( -last->uncroppedGeometry().topLeft() );
            // the last item needs to appended at last...
            Okular::RegularAreaRect * lastArea = textSelectionForItem( last,
                QPoint(),
                selectionRect.bottom() > geom.height() ? ( direction_ne_sw ? geom.bottomLeft() : geom.bottomRight() ) : ( direction_ne_sw ? geom.topLeft() : geom.topRight() ) );
            affectedItemsIds.removeFirst();
            affectedItemsIds.removeLast();
            // item between the two above
            foreach( int page, affectedItemsIds )
            {
                ret.append( textSelectionForItem( d->items[ page ] ) );
            }
            ret.append( lastArea );
        }
    }
    return ret;
}


void PageView::drawDocumentOnPainter( const QRect & contentsRect, QPainter * p )
{
    // when checking if an Item is contained in contentsRect, instead of
    // growing PageViewItems rects (for keeping outline into account), we
    // grow the contentsRect
    QRect checkRect = contentsRect;
    checkRect.adjust( -3, -3, 1, 1 );

    // create a region from which we'll subtract painted rects
    QRegion remainingArea( contentsRect );

    // iterate over all items painting the ones intersecting contentsRect
    QVector< PageViewItem * >::const_iterator iIt = d->items.constBegin(), iEnd = d->items.constEnd();
    for ( ; iIt != iEnd; ++iIt )
    {
        // check if a piece of the page intersects the contents rect
        if ( !(*iIt)->isVisible() || !(*iIt)->croppedGeometry().intersects( checkRect ) )
            continue;

        // get item and item's outline geometries
        PageViewItem * item = *iIt;
        QRect itemGeometry = item->croppedGeometry(),
              outlineGeometry = itemGeometry;
        outlineGeometry.adjust( -1, -1, 3, 3 );

        // move the painter to the top-left corner of the real page
        p->save();
        p->translate( itemGeometry.left(), itemGeometry.top() );

        // draw the page outline (black border and 2px bottom-right shadow)
        if ( !itemGeometry.contains( contentsRect ) )
        {
            int itemWidth = itemGeometry.width(),
                itemHeight = itemGeometry.height();
            // draw simple outline
            p->setPen( Qt::black );
            p->drawRect( -1, -1, itemWidth + 1, itemHeight + 1 );
            // draw bottom/right gradient
            static int levels = 2;
            int r = QColor(Qt::gray).red() / (levels + 2),
                g = QColor(Qt::gray).green() / (levels + 2),
                b = QColor(Qt::gray).blue() / (levels + 2);
            for ( int i = 0; i < levels; i++ )
            {
                p->setPen( QColor( r * (i+2), g * (i+2), b * (i+2) ) );
                p->drawLine( i, i + itemHeight + 1, i + itemWidth + 1, i + itemHeight + 1 );
                p->drawLine( i + itemWidth + 1, i, i + itemWidth + 1, i + itemHeight );
                p->setPen( Qt::gray );
                p->drawLine( -1, i + itemHeight + 1, i - 1, i + itemHeight + 1 );
                p->drawLine( i + itemWidth + 1, -1, i + itemWidth + 1, i - 1 );
            }
        }

        // draw the page using the PagePainter with all flags active
        if ( contentsRect.intersects( itemGeometry ) )
        {
            QRect pixmapRect = contentsRect.intersect( itemGeometry );
            pixmapRect.translate( -item->croppedGeometry().topLeft() );
            PagePainter::paintCroppedPageOnPainter( p, item->page(), PAGEVIEW_ID, pageflags,
                item->uncroppedWidth(), item->uncroppedHeight(), pixmapRect,
                item->crop() );
        }

        // remove painted area from 'remainingArea' and restore painter
        remainingArea -= outlineGeometry.intersect( contentsRect );
        p->restore();
    }

    // fill with background color the unpainted area
    const QVector<QRect> &backRects = remainingArea.rects();
    int backRectsNumber = backRects.count();
    // the previous color here was Qt::gray
    QColor backColor = widget()->palette().color( QPalette::Dark );
    for ( int jr = 0; jr < backRectsNumber; jr++ )
        p->fillRect( backRects[ jr ], backColor );
}

void PageView::updateItemSize( PageViewItem * item, int colWidth, int rowHeight )
{
    const Okular::Page * okularPage = item->page();
    double width = okularPage->width(),
           height = okularPage->height(),
           zoom = d->zoomFactor;
    Okular::NormalizedRect crop( 0., 0., 1., 1. );

    // Handle cropping
    if ( Okular::Settings::trimMargins() && okularPage->isBoundingBoxKnown()
         && !okularPage->boundingBox().isNull() )
    {
        crop = okularPage->boundingBox();

        // Rotate the bounding box
        for ( int i = okularPage->rotation(); i > 0; --i )
        {
            Okular::NormalizedRect rot = crop;
            crop.left   = 1 - rot.bottom;
            crop.top    = rot.left;
            crop.right  = 1 - rot.top;
            crop.bottom = rot.right;
        }

        // Expand the crop slightly beyond the bounding box
        static const double cropExpandRatio = 0.04;
        double cropExpand = cropExpandRatio * ( (crop.right-crop.left) + (crop.bottom-crop.top) ) / 2;
        crop = Okular::NormalizedRect(
            crop.left - cropExpand,
            crop.top - cropExpand,
            crop.right + cropExpand,
            crop.bottom + cropExpand ) & Okular::NormalizedRect( 0, 0, 1, 1 );

        // We currently generate a larger image and then crop it, so if the
        // crop rect is very small the generated image is huge. Hence, we shouldn't
        // let the crop rect become too small.
        // Make sure we crop by at most 50% in either dimension:
        static const double minCropRatio = 0.5;
        if ( ( crop.right - crop.left ) < minCropRatio )
        {
            double newLeft = ( crop.left + crop.right ) / 2 - minCropRatio/2;
            crop.left = qMax( 0.0, qMin( 1.0 - minCropRatio, newLeft ) );
            crop.right = crop.left + minCropRatio;
        }
        if ( ( crop.bottom - crop.top ) < minCropRatio )
        {
            double newTop = ( crop.top + crop.bottom ) / 2 - minCropRatio/2;
            crop.top = qMax( 0.0, qMin( 1.0 - minCropRatio, newTop ) );
            crop.bottom = crop.top + minCropRatio;
        }

        width *= ( crop.right - crop.left );
        height *= ( crop.bottom - crop.top );
#ifdef PAGEVIEW_DEBUG
        kDebug() << "Cropped page" << okularPage->number() << "to" << crop
                 << "width" << width << "height" << height << "by bbox" << okularPage->boundingBox();
#endif
    }

    if ( d->zoomMode == ZoomFixed )
    {
        width *= zoom;
        height *= zoom;
        item->setWHZC( (int)width, (int)height, d->zoomFactor, crop );
    }
    else if ( d->zoomMode == ZoomFitWidth )
    {
        height = ( height / width ) * colWidth;
        zoom = (double)colWidth / width;
        item->setWHZC( colWidth, (int)height, zoom, crop );
        d->zoomFactor = zoom;
    }
    else if ( d->zoomMode == ZoomFitPage )
    {
        double scaleW = (double)colWidth / (double)width;
        double scaleH = (double)rowHeight / (double)height;
        zoom = qMin( scaleW, scaleH );
        item->setWHZC( (int)(zoom * width), (int)(zoom * height), zoom, crop );
        d->zoomFactor = zoom;
    }
#ifndef NDEBUG
    else
        kDebug() << "calling updateItemSize with unrecognized d->zoomMode!";
#endif
}

PageViewItem * PageView::pickItemOnPoint( int x, int y )
{
    PageViewItem * item = 0;
    QLinkedList< PageViewItem * >::const_iterator iIt = d->visibleItems.constBegin(), iEnd = d->visibleItems.constEnd();
    for ( ; iIt != iEnd; ++iIt )
    {
        PageViewItem * i = *iIt;
        const QRect & r = i->croppedGeometry();
        if ( x < r.right() && x > r.left() && y < r.bottom() )
        {
            if ( y > r.top() )
                item = i;
            break;
        }
    }
    return item;
}

void PageView::textSelectionClear()
{
   // something to clear
    if ( !d->pagesWithTextSelection.isEmpty() )
    {
        QSet< int >::ConstIterator it = d->pagesWithTextSelection.constBegin(), itEnd = d->pagesWithTextSelection.constEnd();
        for ( ; it != itEnd; ++it )
            d->document->setPageTextSelection( *it, 0, QColor() );
        d->pagesWithTextSelection.clear();
    }
}

void PageView::selectionStart( const QPoint & pos, const QColor & color, bool /*aboveAll*/ )
{
    selectionClear();
    d->mouseSelecting = true;
    d->mouseSelectionRect.setRect( pos.x(), pos.y(), 1, 1 );
    d->mouseSelectionColor = color;
    // ensures page doesn't scroll
    if ( d->autoScrollTimer )
    {
        d->scrollIncrement = 0;
        d->autoScrollTimer->stop();
    }
}

void PageView::selectionEndPoint( const QPoint & pos )
{
    if ( !d->mouseSelecting )
        return;

    if (pos.x() < horizontalScrollBar()->value()) d->dragScrollVector.setX(pos.x() - horizontalScrollBar()->value());
    else if (horizontalScrollBar()->value() + viewport()->width() < pos.x()) d->dragScrollVector.setX(pos.x() - horizontalScrollBar()->value() - viewport()->width());
    else d->dragScrollVector.setX(0);

    if (pos.y() < verticalScrollBar()->value()) d->dragScrollVector.setY(pos.y() - verticalScrollBar()->value());
    else if (verticalScrollBar()->value() + viewport()->height() < pos.y()) d->dragScrollVector.setY(pos.y() - verticalScrollBar()->value() - viewport()->height());
    else d->dragScrollVector.setY(0);

    if (d->dragScrollVector != QPoint(0, 0))
    {
        if (!d->dragScrollTimer.isActive()) d->dragScrollTimer.start(100);
    }
    else d->dragScrollTimer.stop();

    // update the selection rect
    QRect updateRect = d->mouseSelectionRect;
    d->mouseSelectionRect.setBottomLeft( pos );
    updateRect |= d->mouseSelectionRect;
    widget()->update( updateRect.adjusted( -1, -1, 1, 1 ) );
}

static Okular::NormalizedPoint rotateInNormRect( const QPoint &rotated, const QRect &rect, Okular::Rotation rotation )
{
    Okular::NormalizedPoint ret;

    switch ( rotation )
    {
        case Okular::Rotation0:
            ret = Okular::NormalizedPoint( rotated.x(), rotated.y(), rect.width(), rect.height() );
            break;
        case Okular::Rotation90:
            ret = Okular::NormalizedPoint( rotated.y(), rect.width() - rotated.x(), rect.height(), rect.width() );
            break;
        case Okular::Rotation180:
            ret = Okular::NormalizedPoint( rect.width() - rotated.x(), rect.height() - rotated.y(), rect.width(), rect.height() );
            break;
        case Okular::Rotation270:
            ret = Okular::NormalizedPoint( rect.height() - rotated.y(), rotated.x(), rect.height(), rect.width() );
            break;
    }

    return ret;
}

Okular::RegularAreaRect * PageView::textSelectionForItem( PageViewItem * item, const QPoint & startPoint, const QPoint & endPoint )
{
    const QRect & geometry = item->uncroppedGeometry();
    Okular::NormalizedPoint startCursor( 0.0, 0.0 );
    if ( !startPoint.isNull() )
    {
        startCursor = rotateInNormRect( startPoint, geometry, item->page()->rotation() );
    }
    Okular::NormalizedPoint endCursor( 1.0, 1.0 );
    if ( !endPoint.isNull() )
    {
        endCursor = rotateInNormRect( endPoint, geometry, item->page()->rotation() );
    }
    Okular::TextSelection mouseTextSelectionInfo( startCursor, endCursor );

    const Okular::Page * okularPage = item->page();

    if ( !okularPage->hasTextPage() )
        d->document->requestTextPage( okularPage->number() );

    Okular::RegularAreaRect * selectionArea = okularPage->textArea( &mouseTextSelectionInfo );
#ifdef PAGEVIEW_DEBUG
    kDebug().nospace() << "text areas (" << okularPage->number() << "): " << ( selectionArea ? QString::number( selectionArea->count() ) : "(none)" );
#endif
    return selectionArea;
}

void PageView::selectionClear()
{
    QRect updatedRect = d->mouseSelectionRect.normalized().adjusted( 0, 0, 1, 1 );
    d->mouseSelecting = false;
    d->mouseSelectionRect.setCoords( 0, 0, 0, 0 );
    widget()->update( updatedRect );
}

void PageView::updateZoom( ZoomMode newZoomMode )
{
    if ( newZoomMode == ZoomFixed )
    {
        if ( d->aZoom->currentItem() == 0 )
            newZoomMode = ZoomFitWidth;
        else if ( d->aZoom->currentItem() == 1 )
            newZoomMode = ZoomFitPage;
    }

    float newFactor = d->zoomFactor;
    QAction * checkedZoomAction = 0;
    switch ( newZoomMode )
    {
        case ZoomFixed:{ //ZoomFixed case
            QString z = d->aZoom->currentText();
            // kdelibs4 sometimes adds accelerators to actions' text directly :(
            z.remove ('&');
            z.remove ('%');
            newFactor = KGlobal::locale()->readNumber( z ) / 100.0;
            }break;
        case ZoomIn:
            newFactor += (newFactor > 0.99) ? ( newFactor > 1.99 ? 0.5 : 0.2 ) : 0.1;
            newZoomMode = ZoomFixed;
            break;
        case ZoomOut:
            newFactor -= (newFactor > 1.01) ? ( newFactor > 2.01 ? 0.5 : 0.2 ) : 0.1;
            newZoomMode = ZoomFixed;
            break;
        case ZoomFitWidth:
            checkedZoomAction = d->aZoomFitWidth;
            break;
        case ZoomFitPage:
            checkedZoomAction = d->aZoomFitPage;
            break;
        case ZoomFitText:
            checkedZoomAction = d->aZoomFitText;
            break;
        case ZoomRefreshCurrent:
            newZoomMode = ZoomFixed;
            d->zoomFactor = -1;
            break;
    }
    if ( newFactor > 4.0 )
        newFactor = 4.0;
    if ( newFactor < 0.1 )
        newFactor = 0.1;

    if ( newZoomMode != d->zoomMode || (newZoomMode == ZoomFixed && newFactor != d->zoomFactor ) )
    {
        // rebuild layout and update the whole viewport
        d->zoomMode = newZoomMode;
        d->zoomFactor = newFactor;
        // be sure to block updates to document's viewport
        bool prevState = d->blockViewport;
        d->blockViewport = true;
        slotRelayoutPages();
        d->blockViewport = prevState;
        // request pixmaps
        slotRequestVisiblePixmaps();
        // update zoom text
        updateZoomText();
        // update actions checked state
        if ( d->aZoomFitWidth )
        {
        d->aZoomFitWidth->setChecked( checkedZoomAction == d->aZoomFitWidth );
        d->aZoomFitPage->setChecked( checkedZoomAction == d->aZoomFitPage );
//        d->aZoomFitText->setChecked( checkedZoomAction == d->aZoomFitText );
        }
    }

    d->aZoomIn->setEnabled( d->zoomFactor < 3.9 );
    d->aZoomOut->setEnabled( d->zoomFactor > 0.2 );
}

void PageView::updateZoomText()
{
    // use current page zoom as zoomFactor if in ZoomFit/* mode
    if ( d->zoomMode != ZoomFixed && d->items.count() > 0 )
        d->zoomFactor = d->items[ qMax( 0, (int)d->document->currentPage() ) ]->zoomFactor();
    float newFactor = d->zoomFactor;
    d->aZoom->removeAllActions();

    // add items that describe fit actions
    QStringList translated;
    translated << i18n("Fit Width") << i18n("Fit Page") /*<< i18n("Fit Text")*/;

    // add percent items
    QString double_oh( "00" );
    const float zoomValue[10] = { 0.12, 0.25, 0.33, 0.50, 0.66, 0.75, 1.00, 1.25, 1.50, 2.00 };
    int idx = 0,
        selIdx = 2; // use 3 if "fit text" present
    bool inserted = false; //use: "d->zoomMode != ZoomFixed" to hide Fit/* zoom ratio
    while ( idx < 10 || !inserted )
    {
        float value = idx < 10 ? zoomValue[ idx ] : newFactor;
        if ( !inserted && newFactor < (value - 0.0001) )
            value = newFactor;
        else
            idx ++;
        if ( value > (newFactor - 0.0001) && value < (newFactor + 0.0001) )
            inserted = true;
        if ( !inserted )
            selIdx++;
        QString localValue( KGlobal::locale()->formatNumber( value * 100.0, 2 ) );
        localValue.remove( KGlobal::locale()->decimalSymbol() + double_oh );
        // remove a trailing zero in numbers like 66.70
        if ( localValue.right( 1 ) == QLatin1String( "0" ) && localValue.indexOf( KGlobal::locale()->decimalSymbol() ) > -1 )
            localValue.chop( 1 );
        translated << QString( "%1%" ).arg( localValue );
    }
    d->aZoom->setItems( translated );

    // select current item in list
    if ( d->zoomMode == ZoomFitWidth )
        selIdx = 0;
    else if ( d->zoomMode == ZoomFitPage )
        selIdx = 1;
    else if ( d->zoomMode == ZoomFitText )
        selIdx = 2;
    d->aZoom->setCurrentItem( selIdx );
}

void PageView::updateCursor( const QPoint &p )
{
    // detect the underlaying page (if present)
    PageViewItem * pageItem = pickItemOnPoint( p.x(), p.y() );
    if ( pageItem )
    {
        double nX = pageItem->absToPageX(p.x());
        double nY = pageItem->absToPageY(p.y());

        // if over a ObjectRect (of type Link) change cursor to hand
        if ( d->mouseMode == MouseTextSelect )
            setCursor( Qt::IBeamCursor );
        else if ( d->mouseAnn )
            setCursor( Qt::ClosedHandCursor );
        else
        {
            const Okular::ObjectRect * linkobj = pageItem->page()->objectRect( Okular::ObjectRect::Action, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
            const Okular::ObjectRect * annotobj = pageItem->page()->objectRect( Okular::ObjectRect::OAnnotation, nX, nY, pageItem->uncroppedWidth(), pageItem->uncroppedHeight() );
            if ( linkobj && !annotobj )
            {
                d->mouseOnRect = true;
                setCursor( Qt::PointingHandCursor );
            }
            else
            {
                d->mouseOnRect = false;
                if ( annotobj
                     && ( QApplication::keyboardModifiers() & Qt::ControlModifier )
                     && static_cast< const Okular::AnnotationObjectRect * >( annotobj )->annotation()->canBeMoved() )
                {
                    setCursor( Qt::OpenHandCursor );
                }
                else
                {
                    setCursor( Qt::ArrowCursor );
                }
            }
        }
    }
    else
    {
        // if there's no page over the cursor and we were showing the pointingHandCursor
        // go back to the normal one
        d->mouseOnRect = false;
        setCursor( Qt::ArrowCursor );
    }
}

int PageView::viewColumns() const
{
    int nr = Okular::Settings::viewMode();
    if (nr<2)
	return nr+1;
    return Okular::Settings::viewColumns();
}

int PageView::viewRows() const
{
    if ( Okular::Settings::viewMode() < 2 )
	return 1;
    return Okular::Settings::viewRows();
}

void PageView::center(int cx, int cy)
{
    horizontalScrollBar()->setValue(cx - viewport()->width() / 2);
    verticalScrollBar()->setValue(cy - viewport()->height() / 2);
}

void PageView::toggleFormWidgets( bool on )
{
    bool somehadfocus = false;
    QVector< PageViewItem * >::const_iterator dIt = d->items.constBegin(), dEnd = d->items.constEnd();
    for ( ; dIt != dEnd; ++dIt )
    {
        bool hadfocus = (*dIt)->setFormWidgetsVisible( on );
        somehadfocus = somehadfocus || hadfocus;
    }
    if ( somehadfocus )
        setFocus();
    d->m_formsVisible = on;
    if ( d->aToggleForms ) // it may not exist if we are on dummy mode
    {
        if ( d->m_formsVisible )
        {
            d->aToggleForms->setText( i18n( "Hide Forms" ) );
        }
        else
        {
            d->aToggleForms->setText( i18n( "Show Forms" ) );
        }
    }
}

//BEGIN private SLOTS
void PageView::slotRelayoutPages()
// called by: notifySetup, viewportResizeEvent, slotViewMode, slotContinuousToggled, updateZoom
{
    // set an empty container if we have no pages
    int pageCount = d->items.count();
    if ( pageCount < 1 )
    {
        setWidgetResizable(true);
        return;
    }

    // if viewport was auto-moving, stop it
    if ( d->viewportMoveActive )
    {
        center( d->viewportMoveDest.x(), d->viewportMoveDest.y() );
        d->viewportMoveActive = false;
        d->viewportMoveTimer->stop();
        verticalScrollBar()->setEnabled( true );
        horizontalScrollBar()->setEnabled( true );
    }

    // common iterator used in this method and viewport parameters
    QVector< PageViewItem * >::const_iterator iIt, iEnd = d->items.constEnd();
    int viewportWidth = viewport()->width(),
        viewportHeight = viewport()->height(),
        fullWidth = 0,
        fullHeight = 0;
    QRect viewportRect( horizontalScrollBar()->value(), verticalScrollBar()->value(), viewportWidth, viewportHeight );

    // handle the 'center first page in row' stuff
    int nCols = viewColumns();
    bool centerFirstPage = Okular::Settings::centerFirstPageInRow() && nCols > 1;
    const bool continuousView = Okular::Settings::viewContinuous();

    // set all items geometry and resize contents. handle 'continuous' and 'single' modes separately

    PageViewItem * currentItem = d->items[ qMax( 0, (int)d->document->currentPage() ) ];

        // handle the 'centering on first row' stuff
        if ( centerFirstPage )
            pageCount += nCols - 1;
        // Here we find out column's width and row's height to compute a table
        // so we can place widgets 'centered in virtual cells'.
	int nRows;

// 	if ( Okular::Settings::viewMode() < 2 )
        	nRows = (int)ceil( (float)pageCount / (float)nCols );
// 		nRows=(int)ceil( (float)pageCount / (float) Okular::Settings::viewRows() );
// 	else
// 		nRows = Okular::Settings::viewRows();

        int * colWidth = new int[ nCols ],
            * rowHeight = new int[ nRows ],
            cIdx = 0,
            rIdx = 0;
        for ( int i = 0; i < nCols; i++ )
            colWidth[ i ] = viewportWidth / nCols;
        for ( int i = 0; i < nRows; i++ )
            rowHeight[ i ] = 0;
        // handle the 'centering on first row' stuff
        if ( centerFirstPage )
        {
            pageCount -= nCols - 1;
            cIdx += nCols - 1;
        }

        // 1) find the maximum columns width and rows height for a grid in
        // which each page must well-fit inside a cell
        for ( iIt = d->items.constBegin(); iIt != iEnd; ++iIt )
        {
            PageViewItem * item = *iIt;
            // update internal page size (leaving a little margin in case of Fit* modes)
            updateItemSize( item, colWidth[ cIdx ] - 6, viewportHeight - 12 );
            // find row's maximum height and column's max width
            if ( item->croppedWidth() + 6 > colWidth[ cIdx ] )
                colWidth[ cIdx ] = item->croppedWidth() + 6;
            if ( item->croppedHeight() + 12 > rowHeight[ rIdx ] )
                rowHeight[ rIdx ] = item->croppedHeight() + 12;
            // handle the 'centering on first row' stuff
            // update col/row indices
            if ( ++cIdx == nCols )
            {
                cIdx = 0;
                rIdx++;
            }
        }

        const int pageRowIdx = ( ( centerFirstPage ? nCols - 1 : 0 ) + currentItem->pageNumber() ) / nCols;

        // 2) compute full size
        for ( int i = 0; i < nCols; i++ )
            fullWidth += colWidth[ i ];
        if ( continuousView )
        {
            for ( int i = 0; i < nRows; i++ )
                fullHeight += rowHeight[ i ];
        }
        else
            fullHeight = rowHeight[ pageRowIdx ];

        // 3) arrange widgets inside cells (and refine fullHeight if needed)
        int insertX = 0,
            insertY = fullHeight < viewportHeight ? ( viewportHeight - fullHeight ) / 2 : 0;
        const int origInsertY = insertY;
        cIdx = 0;
        rIdx = 0;
        if ( centerFirstPage )
        {
            cIdx += nCols - 1;
            for ( int i = 0; i < cIdx; ++i )
                insertX += colWidth[ i ];
        }
        for ( iIt = d->items.constBegin(); iIt != iEnd; ++iIt )
        {
            PageViewItem * item = *iIt;
            int cWidth = colWidth[ cIdx ],
                rHeight = rowHeight[ rIdx ];
            if ( continuousView || rIdx == pageRowIdx )
            {
                const bool reallyDoCenterFirst = item->pageNumber() == 0 && centerFirstPage;
                item->moveTo( (reallyDoCenterFirst ? 0 : insertX) + ( (reallyDoCenterFirst ? fullWidth : cWidth) - item->croppedWidth()) / 2,
                              (continuousView ? insertY : origInsertY) + (rHeight - item->croppedHeight()) / 2 );
                item->setVisible( true );
            }
            else
            {
                item->moveTo( 0, 0 );
                item->setVisible( false );
            }
            item->setFormWidgetsVisible( d->m_formsVisible );
            // advance col/row index
            insertX += cWidth;
            if ( ++cIdx == nCols )
            {
                cIdx = 0;
                rIdx++;
                insertX = 0;
                insertY += rHeight;
            }
#ifdef PAGEVIEW_DEBUG
            kWarning() << "updating size for pageno" << item->pageNumber() << "cropped" << item->croppedGeometry() << "uncropped" << item->uncroppedGeometry();
#endif
        }

        delete [] colWidth;
        delete [] rowHeight;

    // 3) reset dirty state
    d->dirtyLayout = false;

    horizontalScrollBar()->setRange( 0, qMax( 0, fullWidth - viewport()->width() ) );
    verticalScrollBar()->setRange( 0, qMax( 0, fullHeight - viewport()->height() ) );

    // 4) update scrollview's contents size and recenter view
    bool wasUpdatesEnabled = viewport()->updatesEnabled();
    if ( fullWidth != widget()->width() || fullHeight != widget()->height() )
    {
        const Okular::DocumentViewport vp = d->document->viewport();
        // disable updates and resize the viewportContents
        if ( wasUpdatesEnabled )
            viewport()->setUpdatesEnabled( false );
        setWidgetResizable(false);
        fullWidth = qMax(fullWidth, viewport()->width());
        fullHeight = qMax(fullHeight, viewport()->height());
        widget()->resize( fullWidth, fullHeight );
        // restore previous viewport if defined and updates enabled
        if ( wasUpdatesEnabled )
        {
            if ( vp.pageNumber >= 0 )
            {
                int prevX = horizontalScrollBar()->value(),
                    prevY = verticalScrollBar()->value();
                const QRect & geometry = d->items[ vp.pageNumber ]->croppedGeometry();
                double nX = vp.rePos.enabled ? normClamp( vp.rePos.normalizedX, 0.5 ) : 0.5,
                       nY = vp.rePos.enabled ? normClamp( vp.rePos.normalizedY, 0.0 ) : 0.0;
                center( geometry.left() + qRound( nX * (double)geometry.width() ),
                        geometry.top() + qRound( nY * (double)geometry.height() ) );
                // center() usually moves the viewport, that requests pixmaps too.
                // if that doesn't happen we have to request them by hand
                if ( prevX == horizontalScrollBar()->value() && prevY == verticalScrollBar()->value() )
                    slotRequestVisiblePixmaps();
            }
            // or else go to center page
            else
                center( fullWidth / 2, 0 );
            viewport()->setUpdatesEnabled( true );
        }
    }

    // 5) update the whole viewport if updated enabled
    if ( wasUpdatesEnabled )
        widget()->update();
}

void PageView::slotRequestVisiblePixmaps( int newValue )
{
    // if requests are blocked (because raised by an unwanted event), exit
    if ( d->blockPixmapsRequest || d->viewportMoveActive ||
         d->mouseMidZooming )
        return;

    // precalc view limits for intersecting with page coords inside the lOOp
    bool isEvent = newValue != -1 && !d->blockViewport;
    QRect viewportRect( horizontalScrollBar()->value(),
                        verticalScrollBar()->value(),
                        viewport()->width(), viewport()->height() );

    // some variables used to determine the viewport
    int nearPageNumber = -1;
    double viewportCenterX = (viewportRect.left() + viewportRect.right()) / 2.0,
           viewportCenterY = (viewportRect.top() + viewportRect.bottom()) / 2.0,
           focusedX = 0.5,
           focusedY = 0.0,
           minDistance = -1.0;

    // iterate over all items
    d->visibleItems.clear();
    QLinkedList< Okular::PixmapRequest * > requestedPixmaps;
    QVector< Okular::VisiblePageRect * > visibleRects;
    QVector< PageViewItem * >::const_iterator iIt = d->items.constBegin(), iEnd = d->items.constEnd();
    for ( ; iIt != iEnd; ++iIt )
    {
        PageViewItem * i = *iIt;
        if ( !i->isVisible() )
            continue;
#ifdef PAGEVIEW_DEBUG
        kWarning() << "checking page" << i->pageNumber();
        kWarning().nospace() << "viewportRect is " << viewportRect << ", page item is " << i->croppedGeometry() << " intersect : " << viewportRect.intersects( i->croppedGeometry() );
#endif
        // if the item doesn't intersect the viewport, skip it
        QRect intersectionRect = viewportRect.intersect( i->croppedGeometry() );
        if ( intersectionRect.isEmpty() )
            continue;

        // add the item to the 'visible list'
        d->visibleItems.push_back( i );
        Okular::VisiblePageRect * vItem = new Okular::VisiblePageRect( i->pageNumber(), Okular::NormalizedRect( intersectionRect.translated( -i->uncroppedGeometry().topLeft() ), i->uncroppedWidth(), i->uncroppedHeight() ) );
        visibleRects.push_back( vItem );
#ifdef PAGEVIEW_DEBUG
        kWarning() << "checking for pixmap for page" << i->pageNumber() << "=" << i->page()->hasPixmap( PAGEVIEW_ID, i->uncroppedWidth(), i->uncroppedHeight() );
        kWarning() << "checking for text for page" << i->pageNumber() << "=" << i->page()->hasTextPage();
#endif
        // if the item has not the right pixmap, add a request for it
        // TODO: We presently request a pixmap for the full page, and then render just the crop part. This waste memory and cycles.
        if ( !i->page()->hasPixmap( PAGEVIEW_ID, i->uncroppedWidth(), i->uncroppedHeight() ) )
        {
#ifdef PAGEVIEW_DEBUG
            kWarning() << "rerequesting visible pixmaps for page" << i->pageNumber() << "!";
#endif
            Okular::PixmapRequest * p = new Okular::PixmapRequest(
                    PAGEVIEW_ID, i->pageNumber(), i->uncroppedWidth(), i->uncroppedHeight(), PAGEVIEW_PRIO, true );
            requestedPixmaps.push_back( p );
        }

        // look for the item closest to viewport center and the relative
        // position between the item and the viewport center
        if ( isEvent )
        {
            const QRect & geometry = i->croppedGeometry();
            // compute distance between item center and viewport center (slightly moved left)
            double distance = hypot( (geometry.left() + geometry.right()) / 2 - (viewportCenterX - 4),
                                     (geometry.top() + geometry.bottom()) / 2 - viewportCenterY );
            if ( distance >= minDistance && nearPageNumber != -1 )
                continue;
            nearPageNumber = i->pageNumber();
            minDistance = distance;
            if ( geometry.height() > 0 && geometry.width() > 0 )
            {
                focusedX = ( viewportCenterX - (double)geometry.left() ) / (double)geometry.width();
                focusedY = ( viewportCenterY - (double)geometry.top() ) / (double)geometry.height();
            }
        }
    }

    // if preloading is enabled, add the pages before and after in preloading
    if ( !d->visibleItems.isEmpty() &&
         Okular::Settings::memoryLevel() != Okular::Settings::EnumMemoryLevel::Low &&
         Okular::Settings::enableThreading() )
    {
        // as the requests are done in the order as they appear in the list,
        // request first the next page and then the previous

        // add the page after the 'visible series' in preload
        int tailRequest = d->visibleItems.last()->pageNumber() + 1;
        if ( tailRequest < (int)d->items.count() )
        {
            PageViewItem * i = d->items[ tailRequest ];
            // request the pixmap if not already present
            if ( !i->page()->hasPixmap( PAGEVIEW_ID, i->uncroppedWidth(), i->uncroppedHeight() ) && i->uncroppedWidth() > 0 )
                requestedPixmaps.push_back( new Okular::PixmapRequest(
                        PAGEVIEW_ID, i->pageNumber(), i->uncroppedWidth(), i->uncroppedHeight(), PAGEVIEW_PRELOAD_PRIO, true ) );
        }

        // add the page before the 'visible series' in preload
        int headRequest = d->visibleItems.first()->pageNumber() - 1;
        if ( headRequest >= 0 )
        {
            PageViewItem * i = d->items[ headRequest ];
            // request the pixmap if not already present
            if ( !i->page()->hasPixmap( PAGEVIEW_ID, i->uncroppedWidth(), i->uncroppedHeight() ) && i->uncroppedWidth() > 0 )
                requestedPixmaps.push_back( new Okular::PixmapRequest(
                        PAGEVIEW_ID, i->pageNumber(), i->uncroppedWidth(), i->uncroppedHeight(), PAGEVIEW_PRELOAD_PRIO, true ) );
        }
    }

    // send requests to the document
    if ( !requestedPixmaps.isEmpty() )
    {
        d->document->requestPixmaps( requestedPixmaps );
    }
    // if this functions was invoked by viewport events, send update to document
    if ( isEvent && nearPageNumber != -1 )
    {
        // determine the document viewport
        Okular::DocumentViewport newViewport( nearPageNumber );
        newViewport.rePos.enabled = true;
        newViewport.rePos.normalizedX = focusedX;
        newViewport.rePos.normalizedY = focusedY;
        // set the viewport to other observers
        d->document->setViewport( newViewport , PAGEVIEW_ID);
    }
    d->document->setVisiblePageRects( visibleRects, PAGEVIEW_ID );
}

void PageView::slotMoveViewport()
{
    // converge to viewportMoveDest in 1 second
    int diffTime = d->viewportMoveTime.elapsed();
    if ( diffTime >= 667 || !d->viewportMoveActive )
    {
        center( d->viewportMoveDest.x(), d->viewportMoveDest.y() );
        d->viewportMoveTimer->stop();
        d->viewportMoveActive = false;
        slotRequestVisiblePixmaps();
        verticalScrollBar()->setEnabled( true );
        horizontalScrollBar()->setEnabled( true );
        return;
    }

    // move the viewport smoothly (kmplot: p(x)=1+0.47*(x-1)^3-0.25*(x-1)^4)
    float convergeSpeed = (float)diffTime / 667.0,
          x = ((float)viewport()->width() / 2.0) + horizontalScrollBar()->value(),
          y = ((float)viewport()->height() / 2.0) + verticalScrollBar()->value(),
          diffX = (float)d->viewportMoveDest.x() - x,
          diffY = (float)d->viewportMoveDest.y() - y;
    convergeSpeed *= convergeSpeed * (1.4 - convergeSpeed);
    center( (int)(x + diffX * convergeSpeed),
            (int)(y + diffY * convergeSpeed ) );
}

void PageView::slotAutoScoll()
{
    // the first time create the timer
    if ( !d->autoScrollTimer )
    {
        d->autoScrollTimer = new QTimer( this );
        d->autoScrollTimer->setSingleShot( true );
        connect( d->autoScrollTimer, SIGNAL( timeout() ), this, SLOT( slotAutoScoll() ) );
    }

    // if scrollIncrement is zero, stop the timer
    if ( !d->scrollIncrement )
    {
        d->autoScrollTimer->stop();
        return;
    }

    // compute delay between timer ticks and scroll amount per tick
    int index = abs( d->scrollIncrement ) - 1;  // 0..9
    const int scrollDelay[10] =  { 200, 100, 50, 30, 20, 30, 25, 20, 30, 20 };
    const int scrollOffset[10] = {   1,   1,  1,  1,  1,  2,  2,  2,  4,  4 };
    d->autoScrollTimer->start( scrollDelay[ index ] );
    int delta = d->scrollIncrement > 0 ? scrollOffset[ index ] : -scrollOffset[ index ];
    verticalScrollBar()->setValue(verticalScrollBar()->value() + delta);
}

void PageView::slotDragScroll()
{
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + d->dragScrollVector.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() + d->dragScrollVector.y());
    QPoint p = widget()->mapFromGlobal( QCursor::pos() );
    selectionEndPoint( p );
}

void PageView::slotShowWelcome()
{
    // show initial welcome text
    d->messageWindow->display( i18n( "Welcome" ), PageViewMessage::Info, 2000 );
}

void PageView::slotZoom()
{
    setFocus();
    updateZoom( ZoomFixed );
}

void PageView::slotZoomIn()
{
    updateZoom( ZoomIn );
}

void PageView::slotZoomOut()
{
    updateZoom( ZoomOut );
}

void PageView::slotFitToWidthToggled( bool on )
{
    if ( on ) updateZoom( ZoomFitWidth );
}

void PageView::slotFitToPageToggled( bool on )
{
    if ( on ) updateZoom( ZoomFitPage );
}

void PageView::slotFitToTextToggled( bool on )
{
    if ( on ) updateZoom( ZoomFitText );
}

void PageView::slotViewMode( QAction *action )
{
    const int nr = action->data().toInt();
    if ( (int)Okular::Settings::viewMode() != nr )
    {
        Okular::Settings::setViewMode( nr );
        Okular::Settings::self()->writeConfig();
        if ( d->document->pages() > 0 )
            slotRelayoutPages();
    }
}

void PageView::slotContinuousToggled( bool on )
{
    if ( Okular::Settings::viewContinuous() != on )
    {
        Okular::Settings::setViewContinuous( on );
        Okular::Settings::self()->writeConfig();
        if ( d->document->pages() > 0 )
            slotRelayoutPages();
    }
}

void PageView::slotSetMouseNormal()
{
    d->mouseMode = MouseNormal;
    // hide the messageWindow
    d->messageWindow->hide();
    // reshow the annotator toolbar if hiding was forced
    if ( d->aToggleAnnotator->isChecked() )
        slotToggleAnnotator( true );
    // force an update of the cursor
    updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
}

void PageView::slotSetMouseZoom()
{
    d->mouseMode = MouseZoom;
    // change the text in messageWindow (and show it if hidden)
    d->messageWindow->display( i18n( "Select zooming area. Right-click to zoom out." ), PageViewMessage::Info, -1 );
    // force hiding of annotator toolbar
    if ( d->annotator )
        d->annotator->setEnabled( false );
    // force an update of the cursor
    updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
}

void PageView::slotSetMouseSelect()
{
    d->mouseMode = MouseSelect;
    // change the text in messageWindow (and show it if hidden)
    d->messageWindow->display( i18n( "Draw a rectangle around the text/graphics to copy." ), PageViewMessage::Info, -1 );
    // force hiding of annotator toolbar
    if ( d->annotator )
        d->annotator->setEnabled( false );
    // force an update of the cursor
    updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
}

void PageView::slotSetMouseTextSelect()
{
    d->mouseMode = MouseTextSelect;
    // change the text in messageWindow (and show it if hidden)
    d->messageWindow->display( i18n( "Select text" ), PageViewMessage::Info, -1 );
    // force hiding of annotator toolbar
    if ( d->annotator )
        d->annotator->setEnabled( false );
    // force an update of the cursor
    updateCursor( widget()->mapFromGlobal( QCursor::pos() ) );
}

void PageView::slotToggleAnnotator( bool on )
{
    // the 'inHere' trick is needed as the slotSetMouseZoom() calls this
    static bool inHere = false;
    if ( inHere )
        return;
    inHere = true;

    // the annotator can be used in normal mouse mode only, so if asked for it,
    // switch to normal mode
    if ( on && d->mouseMode != MouseNormal )
        d->aMouseNormal->trigger();

    // create the annotator object if not present
    if ( !d->annotator )
    {
        d->annotator = new PageViewAnnotator( this, d->document );
        bool allowTools = d->document->pages() > 0 && d->document->isAllowed( Okular::AllowNotes );
        d->annotator->setToolsEnabled( allowTools );
        d->annotator->setTextToolsEnabled( allowTools && d->document->supportsSearching() );
    }

    // initialize/reset annotator (and show/hide toolbar)
    d->annotator->setEnabled( on );

    inHere = false;
}

void PageView::slotScrollUp()
{
    if ( d->scrollIncrement < -9 )
        return;
    d->scrollIncrement--;
    slotAutoScoll();
    setFocus();
}

void PageView::slotScrollDown()
{
    if ( d->scrollIncrement > 9 )
        return;
    d->scrollIncrement++;
    slotAutoScoll();
    setFocus();
}

void PageView::slotRotateClockwise()
{
    int id = ( (int)d->document->rotation() + 1 ) % 4;
    d->document->setRotation( id );
}

void PageView::slotRotateCounterClockwise()
{
    int id = ( (int)d->document->rotation() + 3 ) % 4;
    d->document->setRotation( id );
}

void PageView::slotRotateOriginal()
{
    d->document->setRotation( 0 );
}

void PageView::slotPageSizes( int newsize )
{
    if ( newsize < 0 || newsize >= d->document->pageSizes().count() )
        return;

    d->document->setPageSize( d->document->pageSizes().at( newsize ) );
}

void PageView::slotTrimMarginsToggled( bool on )
{
    if ( Okular::Settings::trimMargins() != on )
    {
        Okular::Settings::setTrimMargins( on );
        Okular::Settings::self()->writeConfig();
        if ( d->document->pages() > 0 )
        {
            slotRelayoutPages();
            slotRequestVisiblePixmaps(); // TODO: slotRelayoutPages() may have done this already!
        }
    }
}

void PageView::slotToggleForms()
{
    toggleFormWidgets( !d->m_formsVisible );
}

void PageView::slotFormWidgetChanged( FormWidgetIface *w )
{
    if ( !d->refreshTimer )
    {
        d->refreshTimer = new QTimer( this );
        d->refreshTimer->setSingleShot( true );
        connect( d->refreshTimer, SIGNAL( timeout() ),
                 this, SLOT( slotRefreshPage() ) );
    }
    d->refreshPage = w->pageItem()->pageNumber();
    d->refreshTimer->start( 1000 );
}

void PageView::slotRefreshPage()
{
    const int req = d->refreshPage;
    if ( req < 0 )
        return;
    d->refreshPage = -1;
    QMetaObject::invokeMethod( d->document, "refreshPixmaps", Qt::QueuedConnection,
                               Q_ARG( int, req ) );
}

void PageView::slotSpeakDocument()
{
    QString text;
    QVector< PageViewItem * >::const_iterator it = d->items.constBegin(), itEnd = d->items.constEnd();
    for ( ; it < itEnd; ++it )
    {
        Okular::RegularAreaRect * area = textSelectionForItem( *it );
        text.append( (*it)->page()->text( area ) );
        text.append( '\n' );
        delete area;
    }

    d->tts()->say( text );
}

void PageView::slotSpeakCurrentPage()
{
    const int currentPage = d->document->viewport().pageNumber;

    PageViewItem *item = d->items.at( currentPage );
    Okular::RegularAreaRect * area = textSelectionForItem( item );
    const QString text = item->page()->text( area );
    delete area;

    d->tts()->say( text );
}

void PageView::slotStopSpeaks()
{
    if ( !d->m_tts )
        return;

    d->m_tts->stopAllSpeechs();
}

void PageView::slotAction( Okular::Action *action )
{
    d->document->processAction( action );
}
//END private SLOTS

#include "pageview.moc"

/* kate: replace-tabs on; indent-width 4; */
