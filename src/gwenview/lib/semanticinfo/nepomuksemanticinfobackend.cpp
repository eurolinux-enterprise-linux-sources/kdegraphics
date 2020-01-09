// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2008 Aurélien Gâteau <agateau@kde.org>

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
// Self
#include "nepomuksemanticinfobackend.moc"

// Qt
#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>

// KDE
#include <kdebug.h>
#include <kurl.h>

// Nepomuk
#include <nepomuk/global.h>
#include <nepomuk/resource.h>
#include <nepomuk/resourcemanager.h>
#include <nepomuk/tag.h>

#include <Soprano/Vocabulary/Xesam>

// Local

namespace Gwenview {

struct Task {
	Task(const KUrl& url): mUrl(url) {}
	virtual ~Task() {}
	virtual void execute() = 0;

	KUrl mUrl;
};


struct RetrieveTask : public Task {
	RetrieveTask(NepomukSemanticInfoBackEnd* backEnd, const KUrl& url)
	: Task(url), mBackEnd(backEnd) {}

	virtual void execute() {
		QString urlString = mUrl.url();
		Nepomuk::Resource resource(urlString, Soprano::Vocabulary::Xesam::File());

		SemanticInfo semanticInfo;
		semanticInfo.mRating = resource.rating();
		semanticInfo.mDescription = resource.description();
		Q_FOREACH(const Nepomuk::Tag& tag, resource.tags()) {
			semanticInfo.mTags << tag.resourceUri().toString();
		}
		mBackEnd->emitSemanticInfoRetrieved(mUrl, semanticInfo);
	}

	NepomukSemanticInfoBackEnd* mBackEnd;
};


struct StoreTask : public Task {
	StoreTask(const KUrl& url, const SemanticInfo& semanticInfo)
	: Task(url), mSemanticInfo(semanticInfo) {}

	virtual void execute() {
		QString urlString = mUrl.url();
		Nepomuk::Resource resource(urlString, Soprano::Vocabulary::Xesam::File());
		resource.setRating(mSemanticInfo.mRating);
		resource.setDescription(mSemanticInfo.mDescription);
		QList<Nepomuk::Tag> tags;
		Q_FOREACH(const SemanticInfoTag& uri, mSemanticInfo.mTags) {
			tags << Nepomuk::Tag(uri);
		}
		resource.setTags(tags);
	}

	SemanticInfo mSemanticInfo;
};


typedef QQueue<Task*> TaskQueue;

class SemanticInfoThread : public QThread {
public:
	SemanticInfoThread()
	: mDeleting(false)
	{}

	~SemanticInfoThread() {
		{
			QMutexLocker locker(&mMutex);
			mDeleting = true;
		}
		// Notify the thread so that it doesn't stay blocked waiting for mQueueNotEmpty
		mQueueNotEmpty.wakeAll();

		wait();
		qDeleteAll(mTaskQueue);
	}


	void enqueueTask(Task* task) {
		{
			QMutexLocker locker(&mMutex);
			mTaskQueue.enqueue(task);
		}
		mQueueNotEmpty.wakeAll();
	}


	virtual void run() {
		while (true) {
			Task* task;
			{
				QMutexLocker locker(&mMutex);
				if (mDeleting) {
					return;
				}
				if (mTaskQueue.isEmpty()) {
					mQueueNotEmpty.wait(&mMutex);
				}
				if (mDeleting) {
					return;
				}
				task = mTaskQueue.dequeue();
			}
			task->execute();
			delete task;
		}
	}


private:
	TaskQueue mTaskQueue;
	QMutex mMutex;
	QWaitCondition mQueueNotEmpty;
	bool mDeleting;
};


struct NepomukSemanticInfoBackEndPrivate {
	SemanticInfoThread mThread;
	TagSet mAllTags;
};


NepomukSemanticInfoBackEnd::NepomukSemanticInfoBackEnd(QObject* parent)
: AbstractSemanticInfoBackEnd(parent)
, d(new NepomukSemanticInfoBackEndPrivate) {
	// FIXME: Check it returns 0
	Nepomuk::ResourceManager::instance()->init();
	d->mThread.start();
}


NepomukSemanticInfoBackEnd::~NepomukSemanticInfoBackEnd() {
	delete d;
}


TagSet NepomukSemanticInfoBackEnd::allTags() const {
	if (d->mAllTags.empty()) {
		const_cast<NepomukSemanticInfoBackEnd*>(this)->refreshAllTags();
	}
	return d->mAllTags;
}


void NepomukSemanticInfoBackEnd::refreshAllTags() {
	d->mAllTags.clear();
	Q_FOREACH(const Nepomuk::Tag& tag, Nepomuk::Tag::allTags()) {
		d->mAllTags << tag.resourceUri().toString();
	}
}


void NepomukSemanticInfoBackEnd::storeSemanticInfo(const KUrl& url, const SemanticInfo& semanticInfo) {
	StoreTask* task = new StoreTask(url, semanticInfo);
	d->mThread.enqueueTask(task);
}


void NepomukSemanticInfoBackEnd::retrieveSemanticInfo(const KUrl& url) {
	RetrieveTask* task = new RetrieveTask(this, url);
	d->mThread.enqueueTask(task);
}


void NepomukSemanticInfoBackEnd::emitSemanticInfoRetrieved(const KUrl& url, const SemanticInfo& semanticInfo) {
	emit semanticInfoRetrieved(url, semanticInfo);
}


QString NepomukSemanticInfoBackEnd::labelForTag(const SemanticInfoTag& uri) const {
	Nepomuk::Tag tag(uri);
	if (!tag.exists()) {
		kError() << "No tag for uri" << uri << ". This should not happen!";
		return QString();
	}
	return tag.label();
}


SemanticInfoTag NepomukSemanticInfoBackEnd::tagForLabel(const QString& label) {
	Nepomuk::Tag tag(label);
	SemanticInfoTag uri;
	if (tag.exists()) {
		uri = tag.resourceUri().toString();
	} else {
		// Not found, create the tag
		tag.setLabel(label);
		uri = tag.resourceUri().toString();
		d->mAllTags << uri;
		emit tagAdded(uri, label);
	}
	return uri;
}


} // namespace
