/***************************************************************************
 *   Copyright (C) 2005 by Enrico Ros <eros.kde@email.it>                  *
 *   Copyright (C) 2006 by Albert Astals Cid <aacid@kde.org>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "minibar.h"

// qt / kde includes
#include <qapplication.h>
#include <qevent.h>
#include <qframe.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlayout.h>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QtGui/QToolButton>
#include <qvalidator.h>
#include <qpainter.h>
#include <kicon.h>
#include <kicontheme.h>
#include <klineedit.h>
#include <klocale.h>
#include <kacceleratormanager.h>

// local includes
#include "core/document.h"

// [private widget] lineEdit for entering/validating page numbers
class PagesEdit : public KLineEdit
{
    public:
        PagesEdit( MiniBar * parent );
        void setPagesNumber( int pages );
        void setText( const QString & );

    protected:
        void focusInEvent( QFocusEvent * e );
        void focusOutEvent( QFocusEvent * e );
        void mousePressEvent( QMouseEvent * e );
        void wheelEvent( QWheelEvent * e );

    private:
        MiniBar * m_miniBar;
        bool m_eatClick;
        QString backString;
        QIntValidator * m_validator;
};

// [private widget] a flat qpushbutton that enlights on hover
class HoverButton : public QToolButton
{
    public:
        HoverButton( QWidget * parent );
};


/** MiniBar **/

MiniBar::MiniBar( QWidget * parent, Okular::Document * document )
    : QWidget( parent ), m_document( document ),
    m_currentPage( -1 )
{
    setObjectName( "miniBar" );

    QHBoxLayout * horLayout = new QHBoxLayout( this );

    horLayout->setMargin( 0 );
    horLayout->setSpacing( 3 );

    QSize buttonSize( KIconLoader::SizeSmallMedium, KIconLoader::SizeSmallMedium );
    // bottom: left prev_page button
    m_prevButton = new HoverButton( this );
    m_prevButton->setIcon( KIcon( layoutDirection() == Qt::RightToLeft ? "arrow-right" : "arrow-left" ) );
    m_prevButton->setIconSize( buttonSize );
    horLayout->addWidget( m_prevButton );
    // bottom: left lineEdit (current page box)
    m_pagesEdit = new PagesEdit( this );
    horLayout->addWidget( m_pagesEdit );
    // bottom: central 'of' label
    horLayout->addSpacing(5);
    horLayout->addWidget( new QLabel( i18nc( "Layouted like: '5 [pages] of 10'", "of" ), this ) );
    // bottom: right button
    m_pagesButton = new HoverButton( this );
    horLayout->addWidget( m_pagesButton );
    // bottom: right next_page button
    m_nextButton = new HoverButton( this );
    m_nextButton->setIcon( KIcon( layoutDirection() == Qt::RightToLeft ? "arrow-left" : "arrow-right" ) );
    m_nextButton->setIconSize( buttonSize );
    horLayout->addWidget( m_nextButton );

    QSizePolicy sp = sizePolicy();
    sp.setHorizontalPolicy( QSizePolicy::Fixed );
    sp.setVerticalPolicy( QSizePolicy::Fixed );
    setSizePolicy( sp );

    // resize width of widgets
    resizeForPage( 0 );

    // connect signals from child widgets to internal handlers / signals bouncers
    connect( m_pagesEdit, SIGNAL( returnPressed() ), this, SLOT( slotChangePage() ) );
    connect( m_pagesButton, SIGNAL( clicked() ), this, SIGNAL( gotoPage() ) );
    connect( m_prevButton, SIGNAL( clicked() ), this, SIGNAL( prevPage() ) );
    connect( m_nextButton, SIGNAL( clicked() ), this, SIGNAL( nextPage() ) );

    resize( minimumSizeHint() );

    // widget starts disabled (will be enabled after opening a document)
    setEnabled( false );
}

MiniBar::~MiniBar()
{
    m_document->removeObserver( this );
}

void MiniBar::notifySetup( const QVector< Okular::Page * > & pageVector, int setupFlags )
{
    // only process data when document changes
    if ( !( setupFlags & Okular::DocumentObserver::DocumentChanged ) )
        return;

    // if document is closed or has no pages, hide widget
    int pages = pageVector.count();
    if ( pages < 1 )
    {
        m_currentPage = -1;
        setEnabled( false );
        return;
    }

    // resize width of widgets
    resizeForPage( pages );

    // update child widgets
    m_pagesEdit->setPagesNumber( pages );
    m_pagesButton->setText( QString::number( pages ) );
    m_prevButton->setEnabled( false );
    m_nextButton->setEnabled( false );

    resize( minimumSizeHint() );

    setEnabled( true );
}

void MiniBar::notifyViewportChanged( bool /*smoothMove*/ )
{
    // get current page number
    int page = m_document->viewport().pageNumber;
    int pages = m_document->pages();

    // if the document is opened and page is changed
    if ( page != m_currentPage && pages > 0 )
    {
        m_currentPage = page;
        // update prev/next button state
        m_prevButton->setEnabled( page > 0 );
        m_nextButton->setEnabled( page < ( pages - 1 ) );
        // update text on widgets
        m_pagesEdit->setText( QString::number( page + 1 ) );
    }
}

void MiniBar::slotChangePage()
{
    // get text from the lineEdit
    QString pageNumber = m_pagesEdit->text();

    // convert it to page number and go to that page
    bool ok;
    int number = pageNumber.toInt( &ok ) - 1;
    if ( ok && number >= 0 && number < (int)m_document->pages() &&
         number != m_currentPage )
    {
        m_document->setViewportPage( number );
        m_pagesEdit->clearFocus();
    }
}

void MiniBar::slotEmitNextPage()
{
    // emit signal
    nextPage();
}

void MiniBar::slotEmitPrevPage()
{
    // emit signal
    prevPage();
}

void MiniBar::resizeForPage( int pages )
{
    int numberWidth = 10 + fontMetrics().width( QString::number( pages ) );
    m_pagesEdit->setMinimumWidth( numberWidth );
    m_pagesEdit->setMaximumWidth( 2 * numberWidth );
    m_pagesButton->setMinimumWidth( numberWidth );
    m_pagesButton->setMaximumWidth( 2 * numberWidth );
}



/** ProgressWidget **/

ProgressWidget::ProgressWidget( QWidget * parent, Okular::Document * document )
    : QWidget( parent ), m_document( document ),
    m_currentPage( -1 ), m_progressPercentage( -1 )
{
    setObjectName( "progress" );
    setAttribute( Qt::WA_OpaquePaintEvent, true );
    setFixedHeight( 4 );
    setMouseTracking( true );
}

ProgressWidget::~ProgressWidget()
{
    m_document->removeObserver( this );
}

void ProgressWidget::notifyViewportChanged( bool /*smoothMove*/ )
{
    // get current page number
    int page = m_document->viewport().pageNumber;
    int pages = m_document->pages();

    // if the document is opened and page is changed
    if ( page != m_currentPage && pages > 0 )
    {
        // update percentage
        m_currentPage = page;
        float percentage = pages < 2 ? 1.0 : (float)page / (float)(pages - 1);
        setProgress( percentage );
    }
}

void ProgressWidget::setProgress( float percentage )
{
    m_progressPercentage = percentage;
    update();
}

void ProgressWidget::slotGotoNormalizedPage( float index )
{
    // figure out page number and go to that page
    int number = (int)( index * (float)m_document->pages() );
    if ( number >= 0 && number < (int)m_document->pages() &&
         number != m_currentPage )
        m_document->setViewportPage( number );
}

void ProgressWidget::mouseMoveEvent( QMouseEvent * e )
{
    if ( ( QApplication::mouseButtons() & Qt::LeftButton ) && width() > 0 )
        slotGotoNormalizedPage( (float)( QApplication::isRightToLeft() ? width() - e->x() : e->x() ) / (float)width() );
}

void ProgressWidget::mousePressEvent( QMouseEvent * e )
{
    if ( e->button() == Qt::LeftButton && width() > 0 )
        slotGotoNormalizedPage( (float)( QApplication::isRightToLeft() ? width() - e->x() : e->x() ) / (float)width() );
}

void ProgressWidget::wheelEvent( QWheelEvent * e )
{
    if ( e->delta() > 0 )
        emit nextPage();
    else
        emit prevPage();
}

void ProgressWidget::paintEvent( QPaintEvent * e )
{
    QPainter p( this );

    if ( m_progressPercentage < 0.0 )
    {
        p.fillRect( rect(), palette().color( QPalette::Active, QPalette::HighlightedText ) );
        return;
    }

    // find out the 'fill' and the 'clear' rectangles
    int w = width(),
        h = height(),
        l = (int)( (float)w * m_progressPercentage );
    QRect cRect = ( QApplication::isRightToLeft() ? QRect( 0, 0, w - l, h ) : QRect( l, 0, w - l, h ) ).intersect( e->rect() );
    QRect fRect = ( QApplication::isRightToLeft() ? QRect( w - l, 0, l, h ) : QRect( 0, 0, l, h ) ).intersect( e->rect() );

    QPalette pal = palette();
    // paint clear rect
    if ( cRect.isValid() )
        p.fillRect( cRect, pal.color( QPalette::Active, QPalette::HighlightedText ) );
    // draw a frame-like outline
    //p.setPen( palette().active().mid() );
    //p.drawRect( 0,0, w, h );
    // paint fill rect
    if ( fRect.isValid() )
        p.fillRect( fRect, pal.color( QPalette::Active, QPalette::Highlight ) );
    if ( l && l != w  )
    {
        p.setPen( pal.color( QPalette::Active, QPalette::Highlight ).dark( 120 ) );
        int delta = QApplication::isRightToLeft() ? w - l : l;
        p.drawLine( delta, 0, delta, h );
    }
}


/** PagesEdit **/

PagesEdit::PagesEdit( MiniBar * parent )
    : KLineEdit( parent ), m_miniBar( parent ), m_eatClick( false )
{
    // use an integer validator
    m_validator = new QIntValidator( 1, 1, this );
    setValidator( m_validator );

    // customize text properties
    setAlignment( Qt::AlignCenter );

    // send a focus out event
    QFocusEvent fe( QEvent::FocusOut );
    QApplication::sendEvent( this, &fe );
}

void PagesEdit::setPagesNumber( int pages )
{
    m_validator->setTop( pages );
}

void PagesEdit::setText( const QString & text )
{
    // store a copy of the string
    backString = text;
    // call default handler if hasn't focus
    if ( !hasFocus() )
        KLineEdit::setText( text );
}

void PagesEdit::focusInEvent( QFocusEvent * e )
{
    // select all text
    selectAll();
    if ( e->reason() == Qt::MouseFocusReason )
        m_eatClick = true;
    // change background color to the default 'edit' color
    QPalette pal = palette();
    pal.setColor( QPalette::Active, QPalette::Base, QApplication::palette().color( QPalette::Active, QPalette::Base ) );
    setPalette( pal );
    // call default handler
    KLineEdit::focusInEvent( e );
}

void PagesEdit::focusOutEvent( QFocusEvent * e )
{
    // change background color to a dark tone
    QPalette pal = palette();
    pal.setColor( QPalette::Base, QApplication::palette().color( QPalette::Base ).dark( 102 ) );
    setPalette( pal );
    // restore text
    KLineEdit::setText( backString );
    // call default handler
    KLineEdit::focusOutEvent( e );
}

void PagesEdit::mousePressEvent( QMouseEvent * e )
{
    // if this click got the focus in, don't process the event
    if ( !m_eatClick )
        KLineEdit::mousePressEvent( e );
    m_eatClick = false;
}

void PagesEdit::wheelEvent( QWheelEvent * e )
{
    if ( e->delta() > 0 )
        m_miniBar->slotEmitNextPage();
    else
        m_miniBar->slotEmitPrevPage();
}


/** HoverButton **/

HoverButton::HoverButton( QWidget * parent )
    : QToolButton( parent )
{
    setAutoRaise(true);
    setFocusPolicy(Qt::NoFocus);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    KAcceleratorManager::setNoAccel( this );
}

#include "minibar.moc"
