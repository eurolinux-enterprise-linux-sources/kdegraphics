/***************************************************************************
 *   Copyright (C) 2004-2008 by Albert Astals Cid <aacid@kde.org>          *
 *   Copyright (C) 2004 by Enrico Ros <eros.kde@email.it>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _OKULAR_GENERATOR_PDF_H_
#define _OKULAR_GENERATOR_PDF_H_

#define UNSTABLE_POPPLER_QT4

#include <poppler-qt4.h>

#include <qbitarray.h>
#include <qpointer.h>
#include <qthread.h>

#include <okular/core/document.h>
#include <okular/core/generator.h>
#include <okular/interfaces/configinterface.h>
#include <okular/interfaces/printinterface.h>
#include <okular/interfaces/saveinterface.h>

namespace Okular {
class ObjectRect;
}

class PDFOptionsPage;
class PDFPixmapGeneratorThread;

/**
 * @short A generator that builds contents from a PDF document.
 *
 * All Generator features are supported and implented by this one.
 * Internally this holds a reference to xpdf's core objects and provides
 * contents generation using the PDFDoc object and a couple of OutputDevices
 * called Okular::OutputDev and Okular::TextDev (both defined in gp_outputdev.h).
 *
 * For generating page contents we tell PDFDoc to render a page and grab
 * contents from out OutputDevs when rendering finishes.
 *
 * Background asynchronous contents providing is done via a QThread inherited
 * class defined at the bottom of the file.
 */
class PDFGenerator : public Okular::Generator, public Okular::ConfigInterface, public Okular::PrintInterface, public Okular::SaveInterface
{
    Q_OBJECT
    Q_INTERFACES( Okular::ConfigInterface )
    Q_INTERFACES( Okular::PrintInterface )
    Q_INTERFACES( Okular::SaveInterface )

    public:
        PDFGenerator( QObject *parent, const QVariantList &args );
        virtual ~PDFGenerator();

        // [INHERITED] load a document and fill up the pagesVector
        bool loadDocument( const QString & fileName, QVector<Okular::Page*> & pagesVector );
        bool loadDocumentFromData( const QByteArray & fileData, QVector<Okular::Page*> & pagesVector );
        void loadPages(QVector<Okular::Page*> &pagesVector, int rotation=-1, bool clear=false);
        // [INHERITED] document information
        const Okular::DocumentInfo * generateDocumentInfo();
        const Okular::DocumentSynopsis * generateDocumentSynopsis();
        Okular::FontInfo::List fontsForPage( int page );
        const QList<Okular::EmbeddedFile*> * embeddedFiles() const;
        PageSizeMetric pagesSizeMetric() const { return Points; }

        // [INHERITED] document information
        bool isAllowed( Okular::Permission permission ) const;

        // [INHERITED] perform actions on document / pages
        bool canGeneratePixmap() const;
        void generatePixmap( Okular::PixmapRequest * request );

        // [INHERITED] print page using an already configured kprinter
        bool print( QPrinter& printer );

        // [INHERITED] reply to some metadata requests
        QVariant metaData( const QString & key, const QVariant & option ) const;

        // [INHERITED] reparse configuration
        bool reparseConfig();
        void addPages( KConfigDialog * );

        // [INHERITED] text exporting
        Okular::ExportFormat::List exportFormats() const;
        bool exportTo( const QString &fileName, const Okular::ExportFormat &format );

        // [INHERITED] print interface
        QWidget* printConfigurationWidget() const;

        // [INHERITED] save interface
        bool supportsOption( SaveOption ) const;
        bool save( const QString &fileName, SaveOptions options );

    protected:
        bool doCloseDocument();
        Okular::TextPage* textPage( Okular::Page *page );

    protected slots:
        void requestFontData(const Okular::FontInfo &font, QByteArray *data);

    private slots:
        // (async related) receive data from the generator thread
        void threadFinished();

    private:
        bool init(QVector<Okular::Page*> & pagesVector, const QString &walletKey);

        // friend class to access private document related variables
        friend class PDFPixmapGeneratorThread;

        // create the document synopsis hieracy
        void addSynopsisChildren( QDomNode * parentSource, QDomNode * parentDestination );
        // fetch annotations from the pdf file and add they to the page
        void addAnnotations( Poppler::Page * popplerPage, Okular::Page * page );
        // fetch the transition information and add it to the page
        void addTransition( Poppler::Page * popplerPage, Okular::Page * page );
        // fetch the form fields and add them to the page
        void addFormFields( Poppler::Page * popplerPage, Okular::Page * page );
        // load the source references from a pdfsync file
        void loadPdfSync( const QString & fileName, QVector<Okular::Page*> & pagesVector );

        Okular::TextPage * abstractTextPage(const QList<Poppler::TextBox*> &text, double height, double width, int rot);

        bool setAAOptions();

        // poppler dependant stuff
        Poppler::Document *pdfdoc;

        // asynchronous generation related stuff
        PDFPixmapGeneratorThread * generatorThread;

        // misc variables for document info and synopsis caching
        bool ready;
        Okular::PixmapRequest * pixmapRequest;
        bool docInfoDirty;
        Okular::DocumentInfo docInfo;
        bool docSynopsisDirty;
        Okular::DocumentSynopsis docSyn;
        mutable bool docEmbeddedFilesDirty;
        mutable QList<Okular::EmbeddedFile*> docEmbeddedFiles;
        int nextFontPage;
        double dpiX;
        double dpiY;

        QBitArray rectsGenerated;

        QPointer<PDFOptionsPage> pdfOptionsPage;
};


/**
 * @short A thread that builds contents for PDFGenerator in the background.
 *
 */
class PDFPixmapGeneratorThread : public QThread
{
    public:
        PDFPixmapGeneratorThread( PDFGenerator * generator );
        ~PDFPixmapGeneratorThread();

        // set the request to the thread (it will be reparented)
        void startGeneration( Okular::PixmapRequest * request );
        // end generation
        void endGeneration();

        Okular::PixmapRequest *request() const;

        // methods for getting contents from the GUI thread
        QImage * takeImage() const;
        QList<Poppler::TextBox*> takeText();
        QLinkedList< Okular::ObjectRect * > takeObjectRects() const;

    private:
        // can't be called from the outside (but from startGeneration)
        void run();

        class PPGThreadPrivate * d;
};

#endif

/* kate: replace-tabs on; indent-width 4; */
