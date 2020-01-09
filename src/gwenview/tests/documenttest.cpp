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
// Qt
#include <QConicalGradient>
#include <QImage>
#include <QPainter>

// KDE
#include <kdebug.h>
#include <kio/netaccess.h>
#include <qtest_kde.h>

// Local
#include "../lib/abstractimageoperation.h"
#include "../lib/document/abstractdocumenteditor.h"
#include "../lib/document/documentfactory.h"
#include "../lib/imagemetainfomodel.h"
#include "../lib/imageutils.h"
#include "../lib/transformimageoperation.h"
#include "testutils.h"

#include <exiv2/exif.hpp>

#include "documenttest.moc"

QTEST_KDEMAIN( DocumentTest, GUI )

using namespace Gwenview;


static void waitUntilMetaInfoLoaded(Document::Ptr doc) {
	while (doc->loadingState() < Document::MetaInfoLoaded) {
		QTest::qWait(100);
	}
}


void DocumentTest::initTestCase() {
	qRegisterMetaType<KUrl>("KUrl");
}


void DocumentTest::init() {
	DocumentFactory::instance()->clearCache();
}


void DocumentTest::testLoad() {
	QFETCH(QString, fileName);
	QFETCH(QByteArray, expectedFormat);
	QFETCH(int, expectedKindInt);
	QFETCH(bool, expectedIsAnimated);
	QFETCH(QImage, expectedImage);
	MimeTypeUtils::Kind expectedKind = MimeTypeUtils::Kind(expectedKindInt);

	KUrl url = urlForTestFile(fileName);
	if (expectedKind != MimeTypeUtils::KIND_SVG_IMAGE) {
		QVERIFY2(!expectedImage.isNull(), "Could not load test image");
	}

	Document::Ptr doc = DocumentFactory::instance()->load(url);
	QSignalSpy spy(doc.data(), SIGNAL(isAnimatedUpdated()));
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QCOMPARE(doc->loadingState(), Document::Loaded);

	QCOMPARE(expectedKind, doc->kind());
	QCOMPARE(expectedIsAnimated, doc->isAnimated());
	QCOMPARE(spy.count(), doc->isAnimated() ? 1 : 0);
	if (doc->kind() == MimeTypeUtils::KIND_RASTER_IMAGE) {
		QCOMPARE(expectedImage, doc->image());
		QCOMPARE(expectedFormat, doc->format());
	}
}

#define NEW_ROW(fileName, format, kind, isAnimated) QTest::newRow(fileName) << fileName << QByteArray(format) << int(kind) << isAnimated << QImage(pathForTestFile(fileName))
void DocumentTest::testLoad_data() {
	QTest::addColumn<QString>("fileName");
	QTest::addColumn<QByteArray>("expectedFormat");
	QTest::addColumn<int>("expectedKindInt");
	QTest::addColumn<bool>("expectedIsAnimated");
	QTest::addColumn<QImage>("expectedImage");

	NEW_ROW("test.png",
	        "png", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("160216_no_size_before_decoding.eps",
	        "eps", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("160382_corrupted.jpeg",
	        "jpeg", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("test.svg",
	        "", MimeTypeUtils::KIND_SVG_IMAGE, false);
	// FIXME: Test svgz
	NEW_ROW("1x10k.png",
	        "png", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("1x10k.jpg",
	        "jpeg", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("4frames.gif",
	        "gif", MimeTypeUtils::KIND_RASTER_IMAGE, true);
	NEW_ROW("1frame.gif",
	        "gif", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("185523_1frame_with_graphic_control_extension.gif",
	        "gif", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("test.xcf",
	        "xcf", MimeTypeUtils::KIND_RASTER_IMAGE, false);
	NEW_ROW("188191_does_not_load.tga",
	        "tga", MimeTypeUtils::KIND_RASTER_IMAGE, false);
}
#undef NEW_ROW

void DocumentTest::testLoadTwoPasses() {
	KUrl url = urlForTestFile("test.png");
	QImage image;
	bool ok = image.load(url.toLocalFile());
	QVERIFY2(ok, "Could not load 'test.png'");
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	waitUntilMetaInfoLoaded(doc);
	QVERIFY2(doc->image().isNull(), "Image shouldn't have been loaded at this time");
	QCOMPARE(doc->format().data(), "png");
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QCOMPARE(image, doc->image());
}

void DocumentTest::testLoadEmpty() {
	KUrl url = urlForTestFile("empty.png");
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	while (doc->loadingState() <= Document::KindDetermined) {
		QTest::qWait(100);
	}
	QCOMPARE(doc->loadingState(), Document::LoadingFailed);
}

#define NEW_ROW(fileName) QTest::newRow(fileName) << fileName
void DocumentTest::testLoadDownSampled_data() {
	QTest::addColumn<QString>("fileName");

	NEW_ROW("orient6.jpg");
	NEW_ROW("1x10k.jpg");
}
#undef NEW_ROW

void DocumentTest::testLoadDownSampled() {
	// Note: for now we only support down sampling on jpeg, do not use test.png
	// here
	QFETCH(QString, fileName);
	KUrl url = urlForTestFile(fileName);
	QImage image;
	bool ok = image.load(url.toLocalFile());
	QVERIFY2(ok, "Could not load test image");
	Document::Ptr doc = DocumentFactory::instance()->load(url);

	QSignalSpy downSampledImageReadySpy(doc.data(), SIGNAL(downSampledImageReady()));
	QSignalSpy loadingFailedSpy(doc.data(), SIGNAL(loadingFailed(const KUrl&)));
	QSignalSpy loadedSpy(doc.data(), SIGNAL(loaded(const KUrl&)));
	bool ready = doc->prepareDownSampledImageForZoom(0.2);
	QVERIFY2(!ready, "There should not be a down sampled image at this point");

	while (downSampledImageReadySpy.count() == 0 && loadingFailedSpy.count() == 0 && loadedSpy.count() == 0) {
		QTest::qWait(100);
	}
	QImage downSampledImage = doc->downSampledImageForZoom(0.2);
	QVERIFY2(!downSampledImage.isNull(), "Down sampled image should not be null");

	QSize expectedSize = doc->size() / 4;
	if (expectedSize.isEmpty()) {
		expectedSize = image.size();
	}
	QCOMPARE(downSampledImage.size(), expectedSize);
}

/**
 * Down sampling is not supported on png. We should get a complete image
 * instead.
 */
void DocumentTest::testLoadDownSampledPng() {
	KUrl url = urlForTestFile("test.png");
	QImage image;
	bool ok = image.load(url.toLocalFile());
	QVERIFY2(ok, "Could not load test image");
	Document::Ptr doc = DocumentFactory::instance()->load(url);

	LoadingStateSpy stateSpy(doc);
	connect(doc.data(), SIGNAL(loaded(const KUrl&)), &stateSpy, SLOT(readState()));

	bool ready = doc->prepareDownSampledImageForZoom(0.2);
	QVERIFY2(!ready, "There should not be a down sampled image at this point");

	doc->waitUntilLoaded();

	QCOMPARE(stateSpy.mCallCount, 1);
	QCOMPARE(stateSpy.mState, Document::Loaded);
}

void DocumentTest::testLoadRemote() {
	QString testTarGzPath = pathForTestFile("test.tar.gz");
	KUrl url;
	url.setProtocol("tar");
	url.setPath(testTarGzPath + "/test.png");

	QVERIFY2(KIO::NetAccess::exists(url, KIO::NetAccess::SourceSide, 0), "test archive not found");

	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QImage image = doc->image();
	QCOMPARE(image.width(), 300);
	QCOMPARE(image.height(), 200);
}

void DocumentTest::testLoadAnimated() {
	KUrl srcUrl = urlForTestFile("40frames.gif");
	Document::Ptr doc = DocumentFactory::instance()->load(srcUrl);
	QSignalSpy spy(doc.data(), SIGNAL(imageRectUpdated(const QRect&)));
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QVERIFY(doc->isAnimated());

	// Test we receive only one imageRectUpdated() until animation is started
	// (the imageRectUpdated() is triggered by the loading of the first image)
	QTest::qWait(1000);
	QCOMPARE(spy.count(), 1);

	// Test we now receive some imageRectUpdated()
	doc->startAnimation();
	QTest::qWait(1000);
	int count = spy.count();
	doc->stopAnimation();
	QVERIFY2(count > 0, "No imageRectUpdated() signal received");

	// Test we do not receive imageRectUpdated() anymore
	QTest::qWait(1000);
	QCOMPARE(count, spy.count());

	// Start again, we should receive imageRectUpdated() again
	doc->startAnimation();
	QTest::qWait(1000);
	QVERIFY2(spy.count() > count, "No imageRectUpdated() signal received after restarting");
}

void DocumentTest::testSaveRemote() {
	KUrl srcUrl = urlForTestFile("test.png");
	Document::Ptr doc = DocumentFactory::instance()->load(srcUrl);
	doc->loadFullImage();
	doc->waitUntilLoaded();

	KUrl dstUrl;
	dstUrl.setProtocol("trash");
	dstUrl.setPath("/test.png");
	QVERIFY(!doc->save(dstUrl, "png"));

	if (qgetenv("REMOTE_SFTP_TEST").isEmpty()) {
		kWarning() << "*** Define the environment variable REMOTE_SFTP_TEST to try saving an image to sftp://localhost/tmp/test.png";
	} else {
		dstUrl.setProtocol("sftp");
		dstUrl.setHost("localhost");
		dstUrl.setPath("/tmp/test.png");
		QVERIFY(doc->save(dstUrl, "png"));
	}
}

/**
 * Check that deleting a document while it is loading does not crash
 */
void DocumentTest::testDeleteWhileLoading() {
	{
		KUrl url = urlForTestFile("test.png");
		QImage image;
		bool ok = image.load(url.toLocalFile());
		QVERIFY2(ok, "Could not load 'test.png'");
		Document::Ptr doc = DocumentFactory::instance()->load(url);
	}
	DocumentFactory::instance()->clearCache();
	// Wait two seconds. If the test fails we will get a segfault while waiting
	QTest::qWait(2000);
}

void DocumentTest::testLoadRotated() {
	KUrl url = urlForTestFile("orient6.jpg");
	QImage image;
	bool ok = image.load(url.toLocalFile());
	QVERIFY2(ok, "Could not load 'orient6.jpg'");
	QMatrix matrix = ImageUtils::transformMatrix(ROT_90);
	image = image.transformed(matrix);

	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QCOMPARE(image, doc->image());
}

/**
 * Checks that asking the DocumentFactory the same document twice in a row does
 * not load it twice
 */
void DocumentTest::testMultipleLoads() {
	KUrl url = urlForTestFile("orient6.jpg");
	Document::Ptr doc1 = DocumentFactory::instance()->load(url);
	Document::Ptr doc2 = DocumentFactory::instance()->load(url);

	QCOMPARE(doc1.data(), doc2.data());
}

void DocumentTest::testSave() {
	KUrl url = urlForTestFile("orient6.jpg");
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();

	KUrl destUrl = urlForTestOutputFile("result.png");
	QVERIFY(doc->save(destUrl, "png"));
	QCOMPARE(doc->format().data(), "png");

	QVERIFY2(doc->loadingState() == Document::Loaded,
		"Document is supposed to finish loading before saving"
		);
	
	QImage image("result.png", "png");
	QCOMPARE(doc->image(), image);
}

void DocumentTest::testLosslessSave() {
	KUrl url1 = urlForTestFile("orient6.jpg");
	Document::Ptr doc = DocumentFactory::instance()->load(url1);
	doc->loadFullImage();

	KUrl url2 = urlForTestOutputFile("orient1.jpg");
	QVERIFY(doc->save(url2, "jpeg"));

	QImage image1;
	QVERIFY(image1.load(url1.toLocalFile()));

	QImage image2;
	QVERIFY(image2.load(url2.toLocalFile()));

	QCOMPARE(image1, image2);
}

void DocumentTest::testLosslessRotate() {
	// Generate test image
	QImage image1(200, 96, QImage::Format_RGB32);
	{
		QPainter painter(&image1);
		QConicalGradient gradient(QPointF(100, 48), 100);
		gradient.setColorAt(0, Qt::white);
		gradient.setColorAt(1, Qt::blue);
		painter.fillRect(image1.rect(), gradient);
	}

	KUrl url1 = urlForTestOutputFile("lossless1.jpg");
	QVERIFY(image1.save(url1.toLocalFile(), "jpeg"));

	// Load it as a Gwenview document
	Document::Ptr doc = DocumentFactory::instance()->load(url1);
	doc->loadFullImage();
	doc->waitUntilLoaded();

	// Rotate one time
	QVERIFY(doc->editor());
	doc->editor()->applyTransformation(ROT_90);

	// Save it
	KUrl url2 = urlForTestOutputFile("lossless2.jpg");
	doc->save(url2, "jpeg");

	// Load the saved image
	doc = DocumentFactory::instance()->load(url2);
	doc->loadFullImage();
	doc->waitUntilLoaded();

	// Rotate the other way
	QVERIFY(doc->editor());
	doc->editor()->applyTransformation(ROT_270);
	doc->save(url2, "jpeg");

	// Compare the saved images
	QVERIFY(image1.load(url1.toLocalFile()));
	QImage image2;
	QVERIFY(image2.load(url2.toLocalFile()));

	QCOMPARE(image1, image2);
}

void DocumentTest::testModify() {
	class TestOperation : public AbstractImageOperation {
	public:
		void redo() {
			QImage image(10, 10, QImage::Format_ARGB32);
			image.fill(QColor(Qt::white).rgb());
			document()->editor()->setImage(image);
		}
	};
	QSignalSpy spy(DocumentFactory::instance(), SIGNAL(modifiedDocumentListChanged()));

	KUrl url = urlForTestFile("orient6.jpg");
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();
	QVERIFY(!doc->isModified());
	QCOMPARE(spy.count(), 0);

	QVERIFY(doc->editor());
	TestOperation* op = new TestOperation;
	op->setDocument(doc);
	doc->undoStack()->push(op);
	QVERIFY(doc->isModified());
	QCOMPARE(spy.count(), 1);
	QList<KUrl> lst = DocumentFactory::instance()->modifiedDocumentList();
	QCOMPARE(lst.count(), 1);
	QCOMPARE(lst.first(), url);

	KUrl destUrl = urlForTestOutputFile("modify.png");
	QVERIFY(doc->save(destUrl, "png"));

	// Wait a bit because save() will clear the undo stack when back to the
	// event loop
	QTest::qWait(100);
	QVERIFY(!doc->isModified());
	QCOMPARE(spy.count(), 2);
	QVERIFY(DocumentFactory::instance()->modifiedDocumentList().isEmpty());
}

void DocumentTest::testMetaInfoJpeg() {
	KUrl url = urlForTestFile("orient6.jpg");
	Document::Ptr doc = DocumentFactory::instance()->load(url);

	// We cleared the cache, so the document should not be loaded
	Q_ASSERT(doc->loadingState() <= Document::KindDetermined);

	// Wait until we receive the metaInfoUpdated() signal
	QSignalSpy metaInfoUpdatedSpy(doc.data(), SIGNAL(metaInfoUpdated()));
	while (metaInfoUpdatedSpy.count() == 0) {
		QTest::qWait(100);
	}

	// Extract an exif key
	QString value = doc->metaInfo()->getValueForKey("Exif.Image.Make");
	QCOMPARE(value, QString::fromUtf8("Canon"));
}


void DocumentTest::testMetaInfoBmp() {
	KUrl url = urlForTestOutputFile("metadata.bmp");
	const int width = 200;
	const int height = 100;
	QImage image(width, height, QImage::Format_ARGB32);
	image.fill(Qt::black);
	image.save(url.toLocalFile(), "BMP");

	Document::Ptr doc = DocumentFactory::instance()->load(url);
	QSignalSpy metaInfoUpdatedSpy(doc.data(), SIGNAL(metaInfoUpdated()));
	waitUntilMetaInfoLoaded(doc);

	Q_ASSERT(metaInfoUpdatedSpy.count() >= 1);

	QString value = doc->metaInfo()->getValueForKey("General.ImageSize");
	QString expectedValue = QString("%1x%2").arg(width).arg(height);
	QCOMPARE(value, expectedValue);
}


void DocumentTest::testForgetModifiedDocument() {
	QSignalSpy spy(DocumentFactory::instance(), SIGNAL(modifiedDocumentListChanged()));
	DocumentFactory::instance()->forget(KUrl("file://does/not/exist.png"));
	QCOMPARE(spy.count(), 0);

	// Generate test image
	QImage image1(200, 96, QImage::Format_RGB32);
	{
		QPainter painter(&image1);
		QConicalGradient gradient(QPointF(100, 48), 100);
		gradient.setColorAt(0, Qt::white);
		gradient.setColorAt(1, Qt::blue);
		painter.fillRect(image1.rect(), gradient);
	}

	KUrl url = urlForTestOutputFile("testForgetModifiedDocument.png");
	QVERIFY(image1.save(url.toLocalFile(), "png"));

	// Load it as a Gwenview document
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	doc->loadFullImage();
	doc->waitUntilLoaded();

	// Modify it
	TransformImageOperation* op = new TransformImageOperation(ROT_90);
	op->setDocument(doc);
	doc->undoStack()->push(op);

	QCOMPARE(spy.count(), 1);

	QList<KUrl> lst = DocumentFactory::instance()->modifiedDocumentList();
	QCOMPARE(lst.length(), 1);
	QCOMPARE(lst.first(), url);

	// Forget it
	DocumentFactory::instance()->forget(url);

	QCOMPARE(spy.count(), 2);
	lst = DocumentFactory::instance()->modifiedDocumentList();
	QVERIFY(lst.isEmpty());
}


void DocumentTest::testModifiedAndSavedSignals() {
	TransformImageOperation* op;

	KUrl url = urlForTestFile("orient6.jpg");
	Document::Ptr doc = DocumentFactory::instance()->load(url);
	QSignalSpy modifiedSpy(doc.data(), SIGNAL(modified(const KUrl&)));
	QSignalSpy savedSpy(doc.data(), SIGNAL(saved(const KUrl&)));
	doc->loadFullImage();
	doc->waitUntilLoaded();

	QCOMPARE(modifiedSpy.count(), 0);
	QCOMPARE(savedSpy.count(), 0);

	op = new TransformImageOperation(ROT_90);
	op->setDocument(doc);
	doc->undoStack()->push(op);
	QCOMPARE(modifiedSpy.count(), 1);

	op = new TransformImageOperation(ROT_90);
	op->setDocument(doc);
	doc->undoStack()->push(op);
	QCOMPARE(modifiedSpy.count(), 2);

	doc->undoStack()->undo();
	QCOMPARE(modifiedSpy.count(), 3);

	doc->undoStack()->undo();
	QCOMPARE(savedSpy.count(), 1);
}
