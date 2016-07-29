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


#ifndef D2V_WITCH_GUIWINDOW_H
#define D2V_WITCH_GUIWINDOW_H

#include <QtWidgets>

#include <VapourSynth.h>

#include "D2V.h"
#include "FakeFile.h"
#include "FFMPEG.h"


class GUIWindow : public QMainWindow {
    Q_OBJECT

    FakeFile fake_file;
    FFMPEG f;
    FILE *d2v_file;
    D2V::AudioFilesMap audio_files;

    QString video_file_name;
    FILE *video_file;
    FakeFile demuxed_fake_file;
    FFMPEG demuxed_f;


    bool container_okay;
    bool output_okay;
    bool video_okay;
    bool audio_okay;

    const VSAPI *vsapi;
    VSCore *vscore;
    VSNodeRef *vsnode;
    const VSFrameRef *vsframe;

    int range_start;
    int range_end;

    D2V d2v;

    // Widgets

    QListWidget *input_list;
    QLineEdit *d2v_edit;
    QListWidget *video_list;
    QButtonGroup *video_group;
    QCheckBox *video_demux_check;
    QButtonGroup *range_group;
    QListWidget *audio_list;
    QLabel *video_frame_label;
    QSpinBox *video_frame_spin;
    QLabel *range_label;
    QSlider *video_frame_slider;
    QPlainTextEdit *log_edit;
    QProgressBar *progress_bar;
    QPushButton *start_stop_button;
    QWidget *indexing_page;
    QWidget *demuxing_page;
    QStackedWidget *container_widget;


    // Methods

    void clearUserInterface();
    void maybeEnableEngageButton();
    void setContainerError(bool error);
    void setOutputError(bool folder_error, bool empty_d2v);
    void setVideoError(QRadioButton *button, bool error);
    void setAudioError(const std::vector<int> &failed_decoders);
    bool isSupportedVideoCodecID(AVCodecID id);
    void inputFilesUpdated();
    void startIndexing();
    void startDemuxing();
    void errorPopup(const std::string &msg);
    void errorPopup(const QString &msg);
    void indexingFinished(D2V new_d2v);
    void demuxingFinished(D2V new_d2v);
    void displayFrame(int n);
    void updateRangeLabel();
    void initialiseVapourSynth();
    void freeVapourSynth();
    void createVapourSynthFilterChain();

public:
    explicit GUIWindow(QWidget *parent = 0);
    ~GUIWindow();

public slots:
    void updateProgress(int64_t current_position, int64_t total_size);
    void logMessage(const QString &msg);
};


class IndexingWorker : public QObject {
    Q_OBJECT

public:
    IndexingWorker(const QString &_d2v_file_name, FILE *_d2v_file, const D2V::AudioFilesMap &_audio_files, FakeFile *_fake_file, FFMPEG *_f, AVStream *_video_stream, D2V::ColourRange _input_range, GUIWindow *_window);

public slots:
    void process();

signals:
    void finished(D2V d2v);

private:
    D2V d2v;
};


class DemuxingWorker : public QObject {
    Q_OBJECT

public:
    DemuxingWorker(const D2V &_d2v, FILE *_video_file, int64_t _start_gop_position, int64_t _end_gop_position);

public slots:
    void process();

signals:
    void finished(D2V d2v);

private:
    D2V d2v;
    FILE *video_file;
    int64_t start_gop_position;
    int64_t end_gop_position;
};

#endif // D2V_WITCH_GUIWINDOW_H
