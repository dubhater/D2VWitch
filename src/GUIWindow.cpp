/*

Copyright (c) 2016, John Smith

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


#include <atomic>
#include <unordered_set>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include <QDateTime>
#include <QFileInfo>
#include <QLibrary>
#include <QMimeData>
#include <QThread>

#include <QFileDialog>
#include <QGroupBox>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "Audio.h"
#include "Bullshit.h"
#include "GUIWindow.h"
#include "ScrollArea.h"

#ifdef _WIN32
#include <windows.h>
#endif


// For QMetaObject::invokeMethod.
Q_DECLARE_METATYPE(int64_t)

// For connect.
Q_DECLARE_METATYPE(D2V)


enum DataRoles {
    FileNameSuffix = Qt::UserRole,
    StreamIndex,
    DecoderOpened
};


static QString removeExtension(const QString &file_name) {
    QString chopped = file_name;

    int last_dot = chopped.lastIndexOf('.');
    if (last_dot > -1)
        chopped.resize(last_dot);

    return chopped;
};


static void updateProgress(int64_t current_position, int64_t total_size, void *data) {
    GUIWindow *window = (GUIWindow *)data;

    QMetaObject::invokeMethod(window, "updateProgress", Qt::QueuedConnection, Q_ARG(int64_t, current_position), Q_ARG(int64_t, total_size));
}


void GUIWindow::updateProgress(int64_t current_position, int64_t total_size) {
    progress_bar->setValue((int)(current_position * 10000 / total_size));
}


static void logMessage(const std::string &msg, void *data) {
    GUIWindow *window = (GUIWindow *)data;

    QMetaObject::invokeMethod(window, "logMessage", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdString(msg)));
}


void GUIWindow::logMessage(const QString &msg) {
    QString time = QDateTime::currentDateTime().toString(QStringLiteral("[yyyyMMdd hh:mm:ss] "));
    log_edit->appendPlainText(time + msg);
    log_edit->appendPlainText(QString());
}


void GUIWindow::clearUserInterface() {
    d2v_edit->clear();
    for (auto *button : video_group->buttons())
        video_group->removeButton(button);
    video_list->clear();
    audio_list->clear();
}


void GUIWindow::maybeEnableEngageButton() {
    start_stop_button->setEnabled(container_okay && output_okay && video_okay);
}


void GUIWindow::setContainerError(bool error) {
    container_okay = !error;
    QString style_sheet;
    QString tool_tip;

    if (error) {
        style_sheet = QStringLiteral("QListWidget { background: red }");
        tool_tip = QStringLiteral("Unsupported container format. Only raw MPEG 1/2 elementary streams, program streams, transport streams, and PVA streams are supported."); /// this message should be shared with the CLI.
    }

    input_list->setStyleSheet(style_sheet);
    input_list->setToolTip(tool_tip);

    maybeEnableEngageButton();
}


void GUIWindow::setOutputError(bool folder_error, bool empty_d2v) {
    output_okay = !folder_error && !empty_d2v;
    QString style_sheet;
    QString tool_tip;

    if (folder_error || empty_d2v) {
        style_sheet = QStringLiteral("QLineEdit, QListWidget { background: red }");
        if (folder_error)
            tool_tip = QStringLiteral("Selected folder is not writable.");
        else
            tool_tip = QStringLiteral("No d2v file is specified.");
    }

    d2v_edit->setStyleSheet(style_sheet);
    d2v_edit->setToolTip(tool_tip);

    maybeEnableEngageButton();
}


void GUIWindow::setVideoError(QRadioButton *button, bool error) {
    QString style_sheet;
    QString tool_tip;

    if (error) {
        style_sheet = QStringLiteral("QRadioButton { background: red }");
        tool_tip = QStringLiteral("Unsupported video format. Only MPEG 1 and MPEG 2 are supported."); /// this message should be shared with the CLI.
    }

    button->setStyleSheet(style_sheet);
    button->setToolTip(tool_tip);
}


void GUIWindow::setAudioError(const std::vector<int> &failed_decoders) {
    audio_okay = !failed_decoders.size();
    QString style_sheet;
    QString tool_tip;

    if (failed_decoders.size()) {
        style_sheet = QStringLiteral("QListWidget { background: red }");
        tool_tip = QStringLiteral("Failed to open decoder for track");
        if (failed_decoders.size() == 1)
            tool_tip += QStringLiteral(" %1.").arg(failed_decoders[0], 0, 16);
        else {
            tool_tip += QStringLiteral("s ");
            for (size_t i = 0; i < failed_decoders.size() - 1; i++)
                tool_tip += QStringLiteral("%1, ").arg(failed_decoders[i], 0, 16);
            tool_tip += QStringLiteral("%1.").arg(failed_decoders.back(), 0, 16);
        }
    }

    audio_list->setStyleSheet(style_sheet);
    audio_list->setToolTip(tool_tip);

    maybeEnableEngageButton();
}


void GUIWindow::inputFilesUpdated() {
    if (!fake_file.open()) {
        errorPopup(fake_file.getError());

        fake_file.close();
        fake_file.clear();
        input_list->clear();

        return;
    }

    if (!f.initFormat(fake_file)) {
        errorPopup(f.getError());

        f.cleanup();
        fake_file.close();
        fake_file.clear();
        input_list->clear();

        return;
    }

    setContainerError(D2V::getStreamType(f.fctx->iformat->name) == D2V::UNSUPPORTED_STREAM);

    d2v_edit->setText(QString::fromStdString(suggestD2VName(fake_file[0].name)));

    for (unsigned i = 0; i < f.fctx->nb_streams; i++) {
        if (f.fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            const char *type = "unknown";
            const AVCodecDescriptor *desc = avcodec_descriptor_get(f.fctx->streams[i]->codec->codec_id);
            if (desc)
                type = desc->long_name ? desc->long_name : desc->name;

            int width, height;
            if (av_opt_get_image_size(f.fctx->streams[i]->codec, "video_size", 0, &width, &height) < 0)
                width = height = -1;

            const char *pixel_format = av_get_pix_fmt_name(f.fctx->streams[i]->codec->pix_fmt);
            if (!pixel_format)
                pixel_format = "unknown";

            char track_text[161] = { 0 };
            snprintf(track_text, 160, "%x, %s, %dx%d, %s", f.fctx->streams[i]->id, type, width, height, pixel_format);

            QListWidgetItem *item = new QListWidgetItem;
            item->setData(StreamIndex, i);
//            item->setData(DecoderOpened, f.initVideoCodec(i));
            video_list->addItem(item);
            QRadioButton *track_radio = new QRadioButton(track_text);
            track_radio->setFocusPolicy(Qt::NoFocus);
//            setVideoError(track_radio, !D2V::isSupportedVideoCodecID(f.fctx->streams[i]->codec->codec_id));
            video_group->addButton(track_radio, i);
            video_list->setItemWidget(item, track_radio);
        } else if (f.fctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            QString path = removeExtension(d2v_edit->text());

            QString suffix = QString::fromStdString(suggestAudioTrackSuffix(f.fctx->streams[i]));

            QListWidgetItem *item = new QListWidgetItem(path + suffix);
            item->setData(FileNameSuffix, suffix);
            item->setData(StreamIndex, i);
            item->setData(DecoderOpened, f.initAudioCodec(i));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
            audio_list->addItem(item);
        }
    }

    if (video_list->count())
        ((QRadioButton *)video_list->itemWidget(video_list->item(0)))->setChecked(true);
}


void GUIWindow::startIndexing() {
    QStringList existing_files;

    QString file_name = d2v_edit->text();

    if (QFileInfo::exists(file_name))
        existing_files.push_back(file_name);

    for (int i = 0; i < audio_list->count(); i++) {
        QListWidgetItem *item = audio_list->item(i);

        if (item->checkState() == Qt::Checked) {
            file_name = item->text();

            if (QFileInfo::exists(file_name))
                existing_files.push_back(file_name);
        }
    }

    if (existing_files.size()) {
        QMessageBox::StandardButton button = QMessageBox::warning(this, QStringLiteral("Overwrite output?"), QStringLiteral("The following files already exist. Would you like to overwrite them?\n\n%1").arg(existing_files.join('\n')), QMessageBox::Yes | QMessageBox::No);

        if (button == QMessageBox::No) {
            enableInterface(true);
            return;
        }
    }


    f.deselectAllStreams();

    int video_index = video_group->checkedId();
    AVStream *video_stream = f.fctx->streams[video_index];
    video_stream->discard = AVDISCARD_DEFAULT;

    for (int i = 0; i < audio_list->count(); i++) {
        QListWidgetItem *item = audio_list->item(i);

        if (item->checkState() == Qt::Checked) {
            int audio_index = item->data(StreamIndex).toInt();
            f.fctx->streams[audio_index]->discard = AVDISCARD_DEFAULT;
        }
    }

    d2v_file = openFile(d2v_edit->text().toUtf8().constData(), "wb");
    if (!d2v_file) {
        errorPopup(QStringLiteral("Failed to open d2v file '%1' for writing: %2").arg(d2v_edit->text()).arg(strerror(errno)));

        enableInterface(true);

        return;
    }

    for (int i = 0; i < audio_list->count(); i++) {
        QListWidgetItem *item = audio_list->item(i);

        if (item->checkState() == Qt::Checked) {
            int stream_index = item->data(StreamIndex).toInt();
            file_name = item->text();

            if (codecIDRequiresWave64(f.fctx->streams[stream_index]->codec->codec_id)) {
                std::string error;

                AVFormatContext *w64_ctx = openWave64(file_name.toStdString(), f.fctx->streams[stream_index]->codec, error);
                if (!w64_ctx) {
                    errorPopup(error);

                    fclose(d2v_file);
                    closeAudioFiles(audio_files, f.fctx);

                    enableInterface(true);

                    return;
                }

                audio_files.insert({ f.fctx->streams[stream_index]->index, w64_ctx });
            } else {
                FILE *file = openFile(file_name.toUtf8().constData(), "wb");
                if (!file) {
                    errorPopup(QStringLiteral("Failed to open audio file '%1' for writing: %2").arg(file_name).arg(strerror(errno)));

                    fclose(d2v_file);
                    closeAudioFiles(audio_files, f.fctx);

                    enableInterface(true);

                    return;
                }

                audio_files.insert({ f.fctx->streams[stream_index]->index, file });
            }
        }
    }

    logMessage(QStringLiteral("Started indexing whole video."));


    QThread *worker_thread = new QThread;
    IndexingWorker *worker = new IndexingWorker(d2v_edit->text(), d2v_file, audio_files, &fake_file, &f, video_stream, (D2V::ColourRange)range_group->checkedId(), use_relative_paths_check->isChecked(), this);
    worker->moveToThread(worker_thread);

    connect(worker_thread, &QThread::started, worker, &IndexingWorker::process);
    connect(worker, &IndexingWorker::finished, this, &GUIWindow::indexingFinished);
    connect(worker, &IndexingWorker::finished, worker_thread, &QThread::quit);
    connect(worker, &IndexingWorker::finished, worker, &IndexingWorker::deleteLater);
    connect(worker_thread, &QThread::finished, worker_thread, &QThread::deleteLater);

    worker_thread->start();
}


void GUIWindow::startDemuxing() {
    int start_gop_frame = d2v.getGOPStartFrame(range_start);
    if (start_gop_frame > 0 && d2v.isOpenGOP(start_gop_frame))
        start_gop_frame = d2v.getGOPStartFrame(start_gop_frame - 1);

    int64_t start_gop_position = d2v.getGOPStartPosition(start_gop_frame);

    int end_gop_frame = d2v.getNextGOPStartFrame(range_end);

    int64_t end_gop_position = d2v.getNextGOPStartPosition(range_end);

    video_file_name = QStringLiteral("%1.%2-%3.m2v").arg(removeExtension(d2v_edit->text())).arg(start_gop_frame).arg(end_gop_frame - 1);

    QStringList existing_files;

    if (QFileInfo::exists(video_file_name))
        existing_files.push_back(video_file_name);

    QString demuxed_d2v = QString::fromStdString(suggestD2VName(video_file_name.toStdString()));

    if (QFileInfo::exists(demuxed_d2v))
        existing_files.push_back(demuxed_d2v);

    if (existing_files.size()) {
        QMessageBox::StandardButton button = QMessageBox::warning(this, QStringLiteral("Overwrite output?"), QStringLiteral("The following files already exist. Would you like to overwrite them?\n\n%1").arg(existing_files.join('\n')), QMessageBox::Yes | QMessageBox::No);

        if (button == QMessageBox::No) {
            enableInterface(true);
            return;
        }
    }

    video_file = openFile(video_file_name.toUtf8().constData(), "wb");
    if (!video_file) {
        errorPopup(QStringLiteral("Failed to open video file '%1' for writing: %2").arg(video_file_name).arg(strerror(errno)));

        enableInterface(true);

        return;
    }

    logMessage(QStringLiteral("Started demuxing video range %1-%2 (%3 and %4 additional frames).").arg(start_gop_frame).arg(end_gop_frame - 1).arg(range_start - start_gop_frame).arg(end_gop_frame - 1 - range_end));

    QThread *worker_thread = new QThread;
    DemuxingWorker *worker = new DemuxingWorker(d2v, video_file, start_gop_position, end_gop_position);
    worker->moveToThread(worker_thread);

    connect(worker_thread, &QThread::started, worker, &DemuxingWorker::process);
    connect(worker, &DemuxingWorker::finished, this, &GUIWindow::demuxingFinished);
    connect(worker, &DemuxingWorker::finished, worker_thread, &QThread::quit);
    connect(worker, &DemuxingWorker::finished, worker, &DemuxingWorker::deleteLater);
    connect(worker_thread, &QThread::finished, worker_thread, &QThread::deleteLater);

    worker_thread->start();
}


extern std::atomic_bool stop_processing;


GUIWindow::GUIWindow(QSettings &_settings, QWidget *parent)
    : QMainWindow(parent)
    , container_okay(false)
    , output_okay(false)
    , video_okay(false)
    , audio_okay(false)
    , vsapi(nullptr)
    , vscore(nullptr)
    , vsnode(nullptr)
    , vsframe(nullptr)
    , settings(_settings)
{
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<D2V>();


    setAcceptDrops(true);


    setWindowTitle("D2V Witch v" PACKAGE_VERSION);


    input_list = new ListWidget(this);
    input_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QPushButton *add_button = new QPushButton("&Add files", this);
    QPushButton *remove_button = new QPushButton("&Remove files", this);
    QPushButton *move_up_button = new QPushButton("Move &up", this);
    QPushButton *move_down_button = new QPushButton("Move &down", this);
    use_relative_paths_check = new QCheckBox("Use relative paths", this);
    use_relative_paths_check->setChecked(settings.value(KEY_USE_RELATIVE_PATHS, KEY_DEFAULT_USE_RELATIVE_PATHS).toBool());
    use_relative_paths_check->setToolTip("The paths written in the d2v file will be relative to the location of the d2v file.");

    d2v_edit = new QLineEdit(this);
    QPushButton *d2v_button = new QPushButton("&Browse", this);

    video_list = new QListWidget(this);
    video_group = new QButtonGroup(this);

    video_demux_check = new QCheckBox("Demu&x (part of) the selected video track", this);

    QGroupBox *range_box = new QGroupBox("Input colour range", this);
    range_group = new QButtonGroup(this);
    range_group->addButton(new QRadioButton("Limited (&TV)", this), D2V::ColourRangeLimited);
    range_group->button(D2V::ColourRangeLimited)->setChecked(true);
    range_group->addButton(new QRadioButton("Full (&PC)", this), D2V::ColourRangeFull);

    audio_list = new QListWidget(this);
    audio_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    indexing_page = new QWidget(this);


    video_frame_label = new QLabel;
    video_frame_label->setAlignment(Qt::AlignCenter);

    ScrollArea *video_frame_scroll = new ScrollArea;
    video_frame_scroll->setFrameShape(QFrame::NoFrame);
    video_frame_scroll->setFocusPolicy(Qt::NoFocus);
    video_frame_scroll->setAlignment(Qt::AlignCenter);
    video_frame_scroll->setWidgetResizable(true);
    video_frame_scroll->setWidget(video_frame_label);

    QPushButton *range_start_button = new QPushButton(QStringLiteral("&["), this);
    QPushButton *range_end_button = new QPushButton(QStringLiteral("&]"), this);

    range_label = new QLabel(this);

    video_frame_spin = new QSpinBox(this);

    video_frame_slider = new QSlider(Qt::Horizontal);
    video_frame_slider->setTracking(false);

    demuxing_page = new QWidget(this);


    log_edit = new QPlainTextEdit(this);
    log_edit->setReadOnly(true);
    log_edit->setFont(QFont(QStringLiteral("Monospace")));

    progress_bar = new QProgressBar(this);
    progress_bar->setMaximum(10000);
    start_stop_button = new QPushButton("&Engage", this);
    start_stop_button->setEnabled(false);

    container_widget = new QStackedWidget(this);


    QMenuBar *bar = menuBar();

    QMenu *file_menu = bar->addMenu(QStringLiteral("&File"));
    QMenu *help_menu = bar->addMenu(QStringLiteral("&Help"));

    QAction *open_action = new QAction(QStringLiteral("&Open video files"), this);
    open_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));

    QAction *quit_action = new QAction(QStringLiteral("&Quit"), this);
    quit_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));;

    QAction *about_action = new QAction(QStringLiteral("&About D2V Witch"), this);

    QAction *aboutqt_action = new QAction(QStringLiteral("About &Qt"), this);


    statusBar()->addWidget(new QLabel(QStringLiteral("Help the author with a coffee, maybe: <a href=\"https://ko-fi.com/bitterblue\">https://ko-fi.com/bitterblue</a>")));


    connect(input_list, &ListWidget::deletePressed, remove_button, &QPushButton::click);

    connect(add_button, &QPushButton::clicked, [this] () {
        QStringList file_names = QFileDialog::getOpenFileNames(this, "Open video files");

        if (!file_names.size())
            return;

        fake_file.close();

        if (container_widget->currentWidget() == demuxing_page) {
            input_list->clear();
            fake_file.clear();

            container_widget->setCurrentWidget(indexing_page);
        }

        for (int i = 0; i < file_names.size(); i++) {
            input_list->addItem(file_names[i]);
            fake_file.push_back(file_names[i].toStdString());
        }


        clearUserInterface();

        inputFilesUpdated();
    });

    connect(remove_button, &QPushButton::clicked, [this] () {
        auto selection = input_list->selectedItems();

        if (!selection.size())
            return;

        fake_file.close();

        for (int i = selection.size() - 1; i >= 0; i--) {
            fake_file.erase(fake_file.begin() + input_list->row(selection[i]));
            delete selection[i];
        }


        clearUserInterface();

        if (!input_list->count()) {
            setContainerError(false);
            return;
        }

        inputFilesUpdated();
    });

    connect(move_up_button, &QPushButton::clicked, [this] () {
        auto selection = input_list->selectedItems();

        if (!selection.size())
            return;

        fake_file.close();

        for (int i = 0; i < selection.size(); i++) {
            int row = input_list->row(selection[i]);

            if (row == 0)
                return;

            std::swap(fake_file[row], fake_file[row - 1]);
            input_list->insertItem(row, input_list->takeItem(row - 1));
        }


        clearUserInterface();

        inputFilesUpdated();
    });

    connect(move_down_button, &QPushButton::clicked, [this] () {
        auto selection = input_list->selectedItems();

        if (!selection.size())
            return;

        fake_file.close();

        for (int i = selection.size() - 1; i >= 0; i--) {
            int row = input_list->row(selection[i]);

            if (row == input_list->count() - 1)
                return;

            std::swap(fake_file[row], fake_file[row + 1]);
            input_list->insertItem(row, input_list->takeItem(row + 1));
        }


        clearUserInterface();

        inputFilesUpdated();
    });

    connect(use_relative_paths_check, &QCheckBox::toggled, [this] (bool checked) {
        settings.setValue(KEY_USE_RELATIVE_PATHS, checked);
    });

    connect(d2v_edit, &QLineEdit::textChanged, [this] (const QString &text) {
        for (int i = 0; i < audio_list->count(); i++)
            audio_list->item(i)->setText(text + audio_list->item(i)->data(FileNameSuffix).toString());

        QFileInfo d2v_info(text);
        QFileInfo dir_info(d2v_info.path());
        setOutputError(!(dir_info.exists() && dir_info.isDir() && dir_info.isWritable()), (text.isEmpty() || d2v_info.isDir()) && input_list->count());
    });

    connect(d2v_button, &QPushButton::clicked, [this] () {
        QString file_name = QFileDialog::getSaveFileName(this, "Save d2v file", QString(), QString(), nullptr, QFileDialog::DontConfirmOverwrite);

        if (file_name.isEmpty())
            return;

        d2v_edit->setText(file_name);
    });

    connect(video_group, static_cast<void (QButtonGroup::*)(int, bool)>(&QButtonGroup::buttonToggled), [this] (int id, bool checked) {
        if (checked) {
            video_okay = D2V::isSupportedVideoCodecID(f.fctx->streams[id]->codec->codec_id) && f.initVideoCodec(id);
            maybeEnableEngageButton();
        }

        setVideoError((QRadioButton *)video_group->button(id), !video_okay && checked);
    });

    connect(audio_list, &QListWidget::itemChanged, [this] (QListWidgetItem *) {
        std::vector<int> failed_decoders;

        for (int i = 0; i < audio_list->count(); i++) {
            QListWidgetItem *item = audio_list->item(i);
            bool decoder_okay = (item->checkState() == Qt::Unchecked) || item->data(DecoderOpened).toBool();
            if (!decoder_okay)
                failed_decoders.push_back(f.fctx->streams[item->data(StreamIndex).toInt()]->id);
        }

        setAudioError(failed_decoders);
    });


    connect(range_start_button, &QPushButton::clicked, [this] () {
        range_start = video_frame_spin->value();

        updateRangeLabel();
    });

    connect(range_end_button, &QPushButton::clicked, [this] () {
        range_end = video_frame_spin->value();

        updateRangeLabel();
    });

    connect(video_frame_spin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GUIWindow::displayFrame);

    connect(video_frame_slider, &QSlider::valueChanged, this, &GUIWindow::displayFrame);


    connect(start_stop_button, &QPushButton::clicked, [this] () {
        bool working = !container_widget->isEnabled();

        /// maybe disable this button until it's safe to click it again

        if (working) {
            stop_processing = true;
        }

        enableInterface(working);

        if (!working) {
            if (container_widget->currentWidget() == indexing_page)
                startIndexing();
            else if (container_widget->currentWidget() == demuxing_page)
                startDemuxing();
        }
    });

    connect(open_action, &QAction::triggered, add_button, &QPushButton::click);

    connect(quit_action, &QAction::triggered, this, &GUIWindow::close);

    connect(about_action, &QAction::triggered, [this] () {
        unsigned lavf = avformat_version();
        unsigned lavc = avcodec_version();
        unsigned lavu = avutil_version();

        QMessageBox msgbox(this);

        msgbox.setText(QStringLiteral("<a href='https://github.com/dubhater/D2VWitch'>https://github.com/dubhater/D2VWitch</a>"));

        QString about = QStringLiteral(
            "Copyright (c) 2016, John Smith\n"
            "\n"
            "Permission to use, copy, modify, and/or distribute this software for "
            "any purpose with or without fee is hereby granted, provided that the "
            "above copyright notice and this permission notice appear in all copies.\n"
            "\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL "
            "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED "
            "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR "
            "BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES "
            "OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, "
            "WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, "
            "ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS "
            "SOFTWARE.\n"
            "\n"
            "D2V Witch version: %1\n"
            "libavformat version: %2.%3.%4\n"
            "libavcodec version: %5.%6.%7\n"
            "libavutil version: %8.%9.%10\n"
            "\n"
            "libavformat configuration:\n"
            "%11\n"
            "\n"
            "libavcodec configuration:\n"
            "%12\n"
            "\n"
            "libavutil configuration:\n"
            "%13\n"
        );
        about = about.arg(PACKAGE_VERSION);
        about = about.arg((lavf >> 16) & 0xff).arg((lavf >> 8) & 0xff).arg(lavf & 0xff);
        about = about.arg((lavc >> 16) & 0xff).arg((lavc >> 8) & 0xff).arg(lavc & 0xff);
        about = about.arg((lavu >> 16) & 0xff).arg((lavu >> 8) & 0xff).arg(lavu & 0xff);
        about = about.arg(avformat_configuration());
        about = about.arg(avcodec_configuration());
        about = about.arg(avutil_configuration());

        msgbox.setDetailedText(about);

        msgbox.setWindowTitle(QStringLiteral("About D2V Witch"));

        msgbox.setStandardButtons(QMessageBox::Ok);
        msgbox.setDefaultButton(QMessageBox::Ok);
        msgbox.setEscapeButton(QMessageBox::Ok);

        msgbox.exec();
    });

    connect(aboutqt_action, &QAction::triggered, [this] () {
        QMessageBox::aboutQt(this);
    });


    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(input_list);
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(add_button);
    hbox->addWidget(remove_button);
    hbox->addWidget(move_up_button);
    hbox->addWidget(move_down_button);
    hbox->addWidget(use_relative_paths_check);
    hbox->addStretch(1);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(d2v_edit);
    hbox->addWidget(d2v_button);
    vbox->addLayout(hbox);

    hbox = new QHBoxLayout;
    hbox->addWidget(video_list);
    QVBoxLayout *vbox2 = new QVBoxLayout;
    vbox2->addWidget(range_group->button(D2V::ColourRangeLimited));
    vbox2->addWidget(range_group->button(D2V::ColourRangeFull));
    vbox2->addStretch(1);
    range_box->setLayout(vbox2);
    hbox->addWidget(range_box);
    vbox->addLayout(hbox);
    vbox->addWidget(video_demux_check);

    vbox->addWidget(audio_list);

    vbox->setContentsMargins(0, 0, 0, 0);
    indexing_page->setLayout(vbox);


    vbox = new QVBoxLayout;
    vbox->addWidget(video_frame_scroll);

    hbox = new QHBoxLayout;
    hbox->addWidget(range_start_button);
    hbox->addWidget(range_end_button);
    hbox->addWidget(range_label);
    hbox->addWidget(video_frame_spin);
    hbox->addWidget(video_frame_slider);
    vbox->addLayout(hbox);

    vbox->setContentsMargins(0, 0, 0, 0);
    demuxing_page->setLayout(vbox);

    container_widget->addWidget(indexing_page);
    container_widget->addWidget(demuxing_page);

    vbox = new QVBoxLayout;
    vbox->addWidget(container_widget);

    vbox->addWidget(log_edit);

    hbox = new QHBoxLayout;
    hbox->addWidget(progress_bar);
    hbox->addWidget(start_stop_button);
    vbox->addLayout(hbox);


    QWidget *central_widget = new QWidget(this);

    central_widget->setLayout(vbox);

    setCentralWidget(central_widget);


    file_menu->addAction(open_action);
    file_menu->addSeparator();
    file_menu->addAction(quit_action);

    help_menu->addAction(about_action);
    help_menu->addAction(aboutqt_action);


    // Fix broken tab order. It's unclear why video_frame_slider isn't in this position already,
    // since it is constructed after video_frame_spin.
    QWidget::setTabOrder(video_frame_spin, video_frame_slider);


    if (settings.contains(KEY_GEOMETRY))
        restoreGeometry(settings.value(KEY_GEOMETRY).toByteArray());


    initialiseVapourSynth();
}


GUIWindow::~GUIWindow() {
    freeVapourSynth();
}


void GUIWindow::errorPopup(const std::string &msg) {
    errorPopup(QString::fromStdString(msg));
}


void GUIWindow::errorPopup(const QString &msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}


void GUIWindow::indexingFinished(D2V new_d2v) {
    audio_files.clear();

    if (!d2v.getNumFrames())
        d2v = new_d2v;

    D2V::ProcessingResult result = new_d2v.getResult();

    if (result == D2V::ProcessingFinished) {
        logMessage(QStringLiteral("Finished writing d2v file %1.\n").arg(QString::fromStdString(new_d2v.getD2VFileName())));

        progress_bar->setValue(progress_bar->maximum());


        if (video_demux_check->isChecked()) {
            int num_frames = d2v.getNumFrames();

            video_frame_spin->setMaximum(num_frames - 1);
            video_frame_slider->setMaximum(num_frames - 1);

            range_start = 0;
            range_end = num_frames - 1;
            updateRangeLabel();

            createVapourSynthFilterChain();

            container_widget->setCurrentWidget(demuxing_page);
        }
    } else if (result == D2V::ProcessingError) {
        errorPopup(new_d2v.getError());
    } else if (result == D2V::ProcessingCancelled) {
        logMessage(QStringLiteral("Indexing cancelled by user."));
    }

    enableInterface(true);

    if (!f.seek(0))
        logMessage(QString::fromStdString(f.getError()));
}


void GUIWindow::demuxingFinished(D2V new_d2v) {
    D2V::ProcessingResult result = new_d2v.getResult();

    if (result == D2V::ProcessingFinished) {
        logMessage(QStringLiteral("Finished writing video file %1.").arg(video_file_name));

        progress_bar->setValue(progress_bar->maximum());

        demuxed_fake_file.close();
        demuxed_fake_file.clear();

        demuxed_fake_file.push_back(video_file_name.toStdString());

        if (!demuxed_fake_file.open()) {
            errorPopup(demuxed_fake_file.getError());

            enableInterface(true);

            if (!f.seek(0))
                logMessage(QString::fromStdString(f.getError()));

            return;
        }

        demuxed_f.cleanup();

        if (!demuxed_f.initFormat(demuxed_fake_file)) {
            errorPopup(demuxed_f.getError());

            enableInterface(true);

            if (!f.seek(0))
                logMessage(QString::fromStdString(f.getError()));

            return;
        }

        AVStream *video_stream = demuxed_f.fctx->streams[0];

        QString new_d2v_name = QString::fromStdString(suggestD2VName(video_file_name.toStdString()));
        FILE *new_d2v_file = openFile(new_d2v_name.toUtf8().constData(), "wb");
        if (!new_d2v_file) {
            errorPopup(QStringLiteral("Failed to open d2v file '%1' for writing: %2").arg(new_d2v_name).arg(strerror(errno)));

            enableInterface(true);

            if (!f.seek(0))
                logMessage(QString::fromStdString(f.getError()));

            return;
        }

        logMessage(QStringLiteral("Started indexing the demuxed video %1.").arg(video_file_name));


        QThread *worker_thread = new QThread;
        IndexingWorker *worker = new IndexingWorker(new_d2v_name, new_d2v_file, D2V::AudioFilesMap(), &demuxed_fake_file, &demuxed_f, video_stream, (D2V::ColourRange)range_group->checkedId(), use_relative_paths_check->isChecked(), this);
        worker->moveToThread(worker_thread);

        connect(worker_thread, &QThread::started, worker, &IndexingWorker::process);
        connect(worker, &IndexingWorker::finished, this, &GUIWindow::indexingFinished);
        connect(worker, &IndexingWorker::finished, worker_thread, &QThread::quit);
        connect(worker, &IndexingWorker::finished, worker, &IndexingWorker::deleteLater);
        connect(worker_thread, &QThread::finished, worker_thread, &QThread::deleteLater);

        worker_thread->start();

        if (!f.seek(0))
            logMessage(QString::fromStdString(f.getError()));

        return;
    } else if (result == D2V::ProcessingError) {
        errorPopup(new_d2v.getError());
    } else if (result == D2V::ProcessingCancelled) {
        logMessage(QStringLiteral("Video demuxing cancelled by user."));
    }

    enableInterface(true);

    if (!f.seek(0))
        logMessage(QString::fromStdString(f.getError()));
}


void GUIWindow::displayFrame(int n) {
    video_frame_spin->blockSignals(true);
    video_frame_spin->setValue(n);
    video_frame_spin->blockSignals(false);

    video_frame_slider->blockSignals(true);
    video_frame_slider->setValue(n);
    video_frame_slider->blockSignals(false);

    if (!vsnode)
        return;

    std::vector<char> error(1024);

    const VSFrameRef *frame = vsapi->getFrame(n, vsnode, error.data(), error.size());

    if (!frame) {
        logMessage(QStringLiteral("Failed to retrieve frame number %1. Error message: %2").arg(n).arg(error.data()));
        return;
    }

    const uint8_t *ptr = vsapi->getReadPtr(frame, 0);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    int stride = vsapi->getStride(frame, 0);
    QPixmap pixmap = QPixmap::fromImage(QImage(ptr, width, height, stride, QImage::Format_RGB32).mirrored(false, true));

    video_frame_label->setPixmap(pixmap);
    // Must free the frame only after replacing the pixmap.
    vsapi->freeFrame(vsframe);
    vsframe = frame;
}


void GUIWindow::updateRangeLabel() {
    range_label->setText(QStringLiteral("Demux range %1-%2").arg(range_start).arg(range_end));
}


void GUIWindow::initialiseVapourSynth() {
#define THEREFORE ", therefore video preview is not available."

    QLibrary libvs;

#ifdef _WIN32
    libvs.setFileName(QStringLiteral("vapoursynth"));
    if (!libvs.load()) {
        HKEY hKey;
        LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\VapourSynth", 0, KEY_READ, &hKey);
        if (lRes == ERROR_SUCCESS) {
            const wchar_t *value_name = L"VapourSynthDLL";
            DWORD dwBufferSize;
            ULONG nError = RegQueryValueExW(hKey, value_name, 0, nullptr, nullptr, &dwBufferSize);
            if (nError == ERROR_SUCCESS) {
                std::vector<wchar_t> buffer(dwBufferSize / sizeof(wchar_t));

                nError = RegQueryValueExW(hKey, value_name, 0, nullptr, (LPBYTE)buffer.data(), &dwBufferSize);
                if (nError == ERROR_SUCCESS)
                    libvs.setFileName(QString::fromWCharArray(buffer.data(), buffer.size()));
            }
            RegCloseKey(hKey);
        }
    }
#else
    libvs.setFileName(QStringLiteral("libvapoursynth"));
#endif

    if (!libvs.isLoaded() && !libvs.load()) {
        logMessage(QStringLiteral("Could not load %1" THEREFORE).arg(libvs.fileName()));

        return;
    }

    typedef const VSAPI * (* getVapourSynthAPIFunction)(int version);

    const char *function_name =
        #if defined(_WIN32) && !defined(_WIN64)
            "_"
        #endif
            "getVapourSynthAPI"
        #if defined(_WIN32) && !defined(_WIN64)
            "@4"
        #endif
            ;

    getVapourSynthAPIFunction getVapourSynthAPIAddr = (getVapourSynthAPIFunction)libvs.resolve(function_name);

    if (!getVapourSynthAPIAddr) {
        logMessage(QStringLiteral("Could not find function %1 in %2" THEREFORE).arg(function_name).arg(libvs.fileName()));

        return;
    }

    vsapi = getVapourSynthAPIAddr((3 << 16) | 0); // API 3.0 is enough at the moment.
    if (!vsapi) {
        logMessage(QStringLiteral("Could not obtain the VSAPI pointer" THEREFORE));

        return;
    }

    vscore = vsapi->createCore(0);
    if (!vscore) {
        logMessage(QStringLiteral("Could not create VapourSynth core object" THEREFORE));

        vsapi = nullptr;
        return;
    }

#define D2VSOURCE_ID    "com.sources.d2vsource"
#define RESIZE_ID       "com.vapoursynth.resize"
#define STD_ID          "com.vapoursynth.std"

    const char *required_plugins[] = {
        D2VSOURCE_ID,
        RESIZE_ID,
        STD_ID,
        nullptr
    };
    for (int i = 0; required_plugins[i]; i++) {
        if (!vsapi->getPluginById(required_plugins[i], vscore)) {
            logMessage(QStringLiteral("VapourSynth plugin %1 is not loaded" THEREFORE).arg(required_plugins[i]));

            vsapi->freeCore(vscore);
            vscore = nullptr;
            vsapi = nullptr;
            return;
        }
    }
}


void GUIWindow::freeVapourSynth() {
    if (!vsapi)
        return;

    video_frame_label->setPixmap(QPixmap());
    vsapi->freeFrame(vsframe);
    vsframe = nullptr;

    vsapi->freeNode(vsnode);
    vsnode = nullptr;

    vsapi->freeCore(vscore);
    vscore = nullptr;

    vsapi = nullptr;
}


void GUIWindow::createVapourSynthFilterChain() {
    if (!vsapi)
        return;

    VSPlugin *d2vsource_plugin = vsapi->getPluginById(D2VSOURCE_ID, vscore);
    VSPlugin *resize_plugin = vsapi->getPluginById(RESIZE_ID, vscore);
    VSPlugin *std_plugin = vsapi->getPluginById(STD_ID, vscore);

#undef D2VSOURCE_ID
#undef RESIZE_ID
#undef STD_ID

    VSMap *args = vsapi->createMap();

    const std::string &d2v_name = d2v_edit->text().toStdString();
    vsapi->propSetData(args, "input", d2v_name.c_str(), d2v_name.size(), paReplace);
    vsapi->propSetInt(args, "rff", 0, paReplace);

    VSMap *ret = vsapi->invoke(d2vsource_plugin, "Source", args);
    if (vsapi->getError(ret)) {
        logMessage(QStringLiteral("Failed to invoke d2v.Source" THEREFORE " Error message: %1").arg(vsapi->getError(ret)));

        vsapi->freeMap(ret);
        vsapi->freeMap(args);

        return;
    }
    vsnode = vsapi->propGetNode(ret, "clip", 0, nullptr);
    vsapi->freeMap(ret);
    vsapi->clearMap(args);

    vsapi->propSetNode(args, "clip", vsnode, paReplace);
    vsapi->freeNode(vsnode);
    vsapi->propSetInt(args, "format", pfCompatBGR32, paReplace);
    vsapi->propSetData(args, "matrix_in_s", "709", -1, paReplace);
    vsapi->propSetInt(args, "prefer_props", 1, paReplace);

    ret = vsapi->invoke(resize_plugin, "Bicubic", args);
    if (vsapi->getError(ret)) {
        logMessage(QStringLiteral("Failed to invoke resize.Bicubic" THEREFORE " Error message: %1").arg(vsapi->getError(ret)));

        vsapi->freeMap(ret);
        vsapi->freeMap(args);

        return;
    }
    vsnode = vsapi->propGetNode(ret, "clip", 0, nullptr);
    vsapi->freeMap(ret);
    vsapi->clearMap(args);

    vsapi->propSetNode(args, "clip", vsnode, paReplace);
    vsapi->freeNode(vsnode);

    ret = vsapi->invoke(std_plugin, "Cache", args);
    if (vsapi->getError(ret)) {
        logMessage(QStringLiteral("Failed to invoke std.Cache" THEREFORE " Error message: %1").arg(vsapi->getError(ret)));

        vsapi->freeMap(ret);
        vsapi->freeMap(args);

        return;
    }
    vsnode = vsapi->propGetNode(ret, "clip", 0, nullptr);
    vsapi->freeMap(ret);
    vsapi->freeMap(args);


    displayFrame(0);

#undef THEREFORE
}


void GUIWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}


void GUIWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();

    QStringList paths;
    paths.reserve(urls.size());

    for (int i = 0; i < urls.size(); i++)
        if (urls[i].isLocalFile())
            paths.push_back(urls[i].toLocalFile());

    if (!paths.size())
        return;

    paths.sort();

    fake_file.close();

    for (int i = 0; i < paths.size(); i++) {
        input_list->addItem(paths[i]);
        fake_file.push_back(paths[i].toStdString());
    }


    clearUserInterface();

    inputFilesUpdated();

    event->acceptProposedAction();
}


void GUIWindow::enableInterface(bool enable) {
    container_widget->setEnabled(enable);
    menuBar()->setEnabled(enable);
    start_stop_button->setText(enable ? "&Engage" : "Canc&el");
}


void GUIWindow::closeEvent(QCloseEvent *event) {
    settings.setValue(KEY_GEOMETRY, saveGeometry());

    QMainWindow::closeEvent(event);
}


IndexingWorker::IndexingWorker(const QString &_d2v_file_name, FILE *_d2v_file, const D2V::AudioFilesMap &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, D2V::ColourRange _input_range, bool _use_relative_paths, GUIWindow *_window)
    : d2v(_d2v_file_name.toStdString(), _d2v_file, _audio_files, _fake_file, _f, _video_stream, _input_range, _use_relative_paths, ::updateProgress, _window, ::logMessage, _window)
{

}


void IndexingWorker::process() {
    d2v.index();

    emit finished(d2v);
}


DemuxingWorker::DemuxingWorker(const D2V &_d2v, FILE *_video_file, int64_t _start_gop_position, int64_t _end_gop_position)
    : d2v(_d2v)
    , video_file(_video_file)
    , start_gop_position(_start_gop_position)
    , end_gop_position(_end_gop_position)
{

}


void DemuxingWorker::process() {
    d2v.demuxVideo(video_file, start_gop_position, end_gop_position);

    emit finished(d2v);
}
