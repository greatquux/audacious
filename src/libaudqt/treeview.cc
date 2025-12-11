/*
 * treeview.cc
 * Copyright 2018 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "treeview.h"
#include "libaudqt.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QProxyStyle>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/index.h>
#include <libaudcore/playlist.h>

namespace audqt
{

/*
 * On some platforms (mainly KDE), there is a feature where
 * clicking on icons makes them work like hyperlinks.  Unfortunately,
 * the way this is implemented is by making all QAbstractItemView
 * widgets behave in this way.
 *
 * In all situations, it makes no sense for audqt::TreeView
 * widgets to behave in this way.  So we override that feature
 * with a QProxyStyle.
 */
class TreeViewStyleOverrides : public QProxyStyle
{
public:
    TreeViewStyleOverrides()
    {
        setup_proxy_style(this);
    }

    int styleHint(StyleHint hint, const QStyleOption * option = nullptr,
                  const QWidget * widget = nullptr,
                  QStyleHintReturn * returnData = nullptr) const override
    {
        if (hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
            return 0;

        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption * option,
                       QPainter * painter,
                       const QWidget * widget) const override
    {
        // extend the drag-and-drop indicator line across all columns
        if (element == QStyle::PE_IndicatorItemViewItemDrop &&
            !option->rect.isNull() && widget)
        {
            QStyleOption opt(*option);
            opt.rect.setLeft(0);
            opt.rect.setWidth(widget->width());

            QProxyStyle::drawPrimitive(element, &opt, painter, widget);
            return;
        }

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

EXPORT TreeView::TreeView(QWidget * parent) : QTreeView(parent)
{
    auto style = new TreeViewStyleOverrides;
    style->setParent(this);
    setStyle(style);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint & pos) {
        QModelIndex idx = indexAt(mapFromGlobal(pos));
        if (!idx.isValid())
            return;

        int row = idx.row();

        QMenu menu;
        QAction * stop_after = menu.addAction(_("Stop After This Song"));
        connect(stop_after, &QAction::triggered, [this, row]() {
            on_stop_after_clicked(row);
        });

        menu.exec(pos);
    });
}

EXPORT TreeView::~TreeView() {}

EXPORT void TreeView::keyPressEvent(QKeyEvent * event)
{
    auto CtrlShiftAlt =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier;
    if (event->key() == Qt::Key_Delete && !(event->modifiers() & CtrlShiftAlt))
    {
        removeSelectedRows();
        return;
    }

    QTreeView::keyPressEvent(event);
}

EXPORT void TreeView::removeSelectedRows()
{
    // get all selected rows
    Index<int> rows;
    for (auto & idx : selectionModel()->selectedRows())
        rows.append(idx.row());

    // sort in reverse order
    rows.sort([](const int & a, const int & b) { return b - a; });

    // remove rows in reverse order
    auto m = model();
    for (int row : rows)
        m->removeRow(row);
}

void TreeView::on_stop_after_clicked(int row)
{
    Playlist playlist;
    
    if (m_get_playlist)
    {
        // Use the provided callback if available
        if (!m_get_playlist(row, playlist))
            return;
    }
    else
    {
        // Fall back to active playlist if no callback provided
        playlist = Playlist::active_playlist();
    }

    aud_drct_pl_set_stop_after(playlist.index(), row);
}

EXPORT void TreeView::setPlaylistContextMenu(
    bool (*getPlaylist)(int row, class Playlist & playlist_out))
{
    m_get_playlist = getPlaylist;
}

} // namespace audqt
