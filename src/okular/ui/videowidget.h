/***************************************************************************
 *   Copyright (C) 2008 by Pino Toscano <pino@kde.org>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _OKULAR_VIDEOWIDGET_H_
#define _OKULAR_VIDEOWIDGET_H_

#include <qwidget.h>

namespace Okular {
class Document;
class MovieAnnotation;
class NormalizedRect;
}

class VideoWidget : public QWidget
{
    Q_OBJECT
    public:
        VideoWidget( Okular::MovieAnnotation *movieann, Okular::Document *document, QWidget *parent = 0 );
        ~VideoWidget();

        void setNormGeometry( const Okular::NormalizedRect &rect );
        Okular::NormalizedRect normGeometry() const;

    public slots:
        void play();
        void pause();
        void stop();

    protected:
        /* reimp */ bool eventFilter( QObject * object, QEvent * event );
        /* reimp */ bool event( QEvent * event );
        /* reimp */ void resizeEvent( QResizeEvent * event );

    private:
        Q_PRIVATE_SLOT( d, void finished() )
        Q_PRIVATE_SLOT( d, void playOrPause() )

        // private storage
        class Private;
        Private *d;
};

#endif
