/*

Copyright (c) 2015, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include <QKeyEvent>

#include "ListWidget.h"

void ListWidget::keyPressEvent(QKeyEvent *event) {
    auto mod = event->modifiers();
    int key = event->key();

    if (mod == Qt::NoModifier && key == Qt::Key_Delete) {
        emit deletePressed();
        return;
    }

    QListWidget::keyPressEvent(event);
}


QList<QListWidgetItem *> ListWidget::selectedItems() const {
    auto selection = QListWidget::selectedItems();

    auto cmp = [this] (const QListWidgetItem *a, const QListWidgetItem *b) -> bool {
        return row(a) < row(b);
    };
    std::sort(selection.begin(), selection.end(), cmp);

    return selection;
}
