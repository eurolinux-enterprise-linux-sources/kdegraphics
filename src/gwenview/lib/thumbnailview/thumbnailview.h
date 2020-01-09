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
#ifndef THUMBNAILVIEW_H
#define THUMBNAILVIEW_H

#include <lib/gwenviewlib_export.h>

// Qt
#include <QListView>

// KDE
#include <kurl.h>

class KFileItem;
class KFileItemList;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QPixmap;

namespace Gwenview {

class AbstractThumbnailViewHelper;

class ThumbnailViewPrivate;
class GWENVIEWLIB_EXPORT ThumbnailView : public QListView {
	Q_OBJECT
public:
	ThumbnailView(QWidget* parent);
	~ThumbnailView();

	void setThumbnailViewHelper(AbstractThumbnailViewHelper* helper);

	AbstractThumbnailViewHelper* thumbnailViewHelper() const;

	/**
	 * Returns the thumbnail size.
	 */
	int thumbnailSize() const;

	QPixmap thumbnailForIndex(const QModelIndex&);

	/**
	 * Returns true if the document pointed by the index has been modified
	 * inside Gwenview.
	 */
	bool isModified(const QModelIndex&) const;

	/**
	 * Generate thumbnail for @a index.
	 */
	void generateThumbnailForIndex(const QModelIndex& index);

	virtual void setModel(QAbstractItemModel* model);

	/**
	 * Publish this method so that delegates can call it.
	 */
	using QListView::scheduleDelayedItemsLayout;

Q_SIGNALS:
	/**
	 * It seems we can't use the 'activated()' signal for now because it does
	 * not know about KDE single vs doubleclick settings. The indexActivated()
	 * signal replaces it.
	 */
	void indexActivated(const QModelIndex&);
	void urlListDropped(const KUrl::List& lst, const KUrl& destination);

	void thumbnailSizeChanged(int);

	/**
	 * Emitted whenever selectionChanged() is called.
	 * This signal is suffixed with "Signal" because
	 * QAbstractItemView::selectionChanged() is a slot.
	 */
	void selectionChangedSignal(const QItemSelection&, const QItemSelection&);

	/**
	 * Forward some signals from model, so that the delegate can use them
	 */
	void rowsRemovedSignal(const QModelIndex& parent, int start, int end);

	void rowsInsertedSignal(const QModelIndex& parent, int start, int end);


public Q_SLOTS:
	/**
	 * Sets the thumbnail size, in pixels.
	 */
	void setThumbnailSize(int pixel);

	void scrollToSelectedIndex();

protected:
	virtual void dragEnterEvent(QDragEnterEvent*);

	virtual void dragMoveEvent(QDragMoveEvent*);

	virtual void dropEvent(QDropEvent*);

	virtual void keyPressEvent(QKeyEvent*);

	virtual void resizeEvent(QResizeEvent*);

	virtual void scrollContentsBy(int dx, int dy);

	virtual void showEvent(QShowEvent*);

protected Q_SLOTS:
	virtual void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end);
	virtual void rowsInserted(const QModelIndex& parent, int start, int end);
	virtual void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

private Q_SLOTS:
	void showContextMenu();
	void emitIndexActivatedIfNoModifiers(const QModelIndex&);
	void setThumbnail(const KFileItem&, const QPixmap&, const QSize&);
	void setBrokenThumbnail(const KFileItem&);

	void generateThumbnailsForVisibleItems();
	void smoothNextThumbnail();

private:
	friend class ThumbnailViewPrivate;
	ThumbnailViewPrivate * const d;
};

} // namespace

#endif /* THUMBNAILVIEW_H */
