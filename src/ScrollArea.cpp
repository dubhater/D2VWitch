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


#include <QMouseEvent>
#include <QScrollBar>

#include "ScrollArea.h"


void ScrollArea::mousePressEvent(QMouseEvent *e) {
    if (e->buttons() == Qt::LeftButton) {
        old_mouse_position = e->globalPos();
    } else {
        QScrollArea::mousePressEvent(e);
    }
}


void ScrollArea::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() == Qt::LeftButton) {
        QPoint new_mouse_position = e->globalPos();
        QPoint diff = new_mouse_position - old_mouse_position;
        old_mouse_position = new_mouse_position;

        QScrollBar *hbar = horizontalScrollBar();
        hbar->setValue(hbar->value() - diff.x());

        QScrollBar *vbar = verticalScrollBar();
        vbar->setValue(vbar->value() - diff.y());
    } else {
        QScrollArea::mouseMoveEvent(e);
    }
}
