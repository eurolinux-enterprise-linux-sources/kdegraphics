/***************************************************************************
 *   Copyright (C) 2004 by Enrico Ros <eros.kde@email.it>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _OKULAR_PRESENTATIONWIDGET_H_
#define _OKULAR_PRESENTATIONWIDGET_H_

#include <qlist.h>
#include <qpixmap.h>
#include <qstringlist.h>
#include <qwidget.h>
#include "core/observer.h"
#include "core/pagetransition.h"

class QLineEdit;
class QToolBar;
class QTimer;
class KActionCollection;
class KSelectAction;
class AnnotatorEngine;
struct PresentationFrame;
class PresentationSearchBar;

namespace Okular {
class Action;
class Annotation;
class Document;
class Page;
}

/**
 * @short A widget that shows pages as fullscreen slides (with transitions fx).
 *
 * This is a fullscreen widget that displays 
 */
class PresentationWidget : public QWidget, public Okular::DocumentObserver
{
    Q_OBJECT
    public:
        PresentationWidget( QWidget * parent, Okular::Document * doc, KActionCollection * collection );
        ~PresentationWidget();

        // inherited from DocumentObserver
        uint observerId() const { return PRESENTATION_ID; }
        void notifySetup( const QVector< Okular::Page * > & pages, int setupFlags );
        void notifyViewportChanged( bool smoothMove );
        void notifyPageChanged( int pageNumber, int changedFlags );
        bool canUnloadPixmap( int pageNumber ) const;

    public slots:
        void slotFind();

    protected:
        // widget events
        bool event( QEvent * e );
        void keyPressEvent( QKeyEvent * e );
        void wheelEvent( QWheelEvent * e );
        void mousePressEvent( QMouseEvent * e );
        void mouseReleaseEvent( QMouseEvent * e );
        void mouseMoveEvent( QMouseEvent * e );
        void paintEvent( QPaintEvent * e );
        void resizeEvent( QResizeEvent * e );
        void leaveEvent( QEvent * e );

    private:
        const Okular::Action * getLink( int x, int y, QRect * geometry = 0 ) const;
        void testCursorOnLink( int x, int y );
        void overlayClick( const QPoint & position );
        void changePage( int newPage );
        void generatePage( bool disableTransition = false );
        void generateIntroPage( QPainter & p );
        void generateContentsPage( int page, QPainter & p );
        void generateOverlay();
        void initTransition( const Okular::PageTransition *transition );
        const Okular::PageTransition defaultTransition() const;
        const Okular::PageTransition defaultTransition( int ) const;
        QRect routeMouseDrawingEvent( QMouseEvent * );
        void startAutoChangeTimer();
        void recalcGeometry();
        void repositionContent();
        void requestPixmaps();
        void setScreen( int );
        void applyNewScreenSize( const QSize & oldSize );
        void inhibitScreenSaver();
        void allowScreenSaver();
        // create actions that interact with this widget
        void setupActions( KActionCollection * collection );

        // cache stuff
        int m_width;
        int m_height;
        QPixmap m_lastRenderedPixmap;
        QPixmap m_lastRenderedOverlay;
        QRect m_overlayGeometry;
        const Okular::Action * m_pressedLink;
        bool m_handCursor;
        QList< Okular::Annotation * > m_currentPageDrawings;
        AnnotatorEngine * m_drawingEngine;
        QRect m_drawingRect;
        int m_screen;
        int m_screenSaverCookie;

        // transition related
        QTimer * m_transitionTimer;
        QTimer * m_overlayHideTimer;
        QTimer * m_nextPageTimer;
        int m_transitionDelay;
        int m_transitionMul;
        QList< QRect > m_transitionRects;

        // misc stuff
        QWidget * m_parentWidget;
        Okular::Document * m_document;
        QVector< PresentationFrame * > m_frames;
        int m_frameIndex;
        QStringList m_metaStrings;
        QToolBar * m_topBar;
        QLineEdit *m_pagesEdit;
        PresentationSearchBar *m_searchBar;
        KActionCollection * m_ac;
        KSelectAction * m_screenSelect;
        bool m_isSetup;
        bool m_blockNotifications;
        bool m_inBlackScreenMode;

    private slots:
        void slotNextPage();
        void slotPrevPage();
        void slotFirstPage();
        void slotLastPage();
        void slotHideOverlay();
        void slotTransitionStep();
        void slotDelayedEvents();
        void slotPageChanged();
        void togglePencilMode( bool );
        void clearDrawings();
        void screenResized( int );
        void chooseScreen( QAction * );
        void toggleBlackScreenMode( bool );
};

#endif
