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
#ifndef STARTPAGE_H
#define STARTPAGE_H

// Qt
#include <QFrame>

// KDE

// Local

class QModelIndex;
class QPalette;
class QShowEvent;

class KFilePlacesModel;
class KUrl;

namespace Gwenview {

class GvCore;

class StartPagePrivate;
class StartPage : public QFrame {
	Q_OBJECT
public:
	StartPage(QWidget* parent, GvCore*);
	~StartPage();

	void applyPalette(const QPalette&);

Q_SIGNALS:
	void urlSelected(const KUrl& url);

protected:
	virtual void showEvent(QShowEvent*);

private Q_SLOTS:
	void slotListViewClicked(const QModelIndex& index);
	void showRecentFoldersViewContextMenu(const QPoint& pos);
	void slotTagViewClicked(const QModelIndex& index);
	void slotConfigChanged();

private:
	StartPagePrivate* const d;
};


} // namespace

#endif /* STARTPAGE_H */
