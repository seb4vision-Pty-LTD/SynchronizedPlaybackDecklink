#include "MainWindow.h"
#include <QDir>
#include <QFileInfoList>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QFrame>
#include <QTime>
#include <QDebug>
#include <QStatusBar>
#include <QByteArray>
#include <gst/video/videooverlay.h>

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent), updateTimer(nullptr), busTimer(nullptr), pipeline(nullptr), bus(nullptr), duration(-1), isPlaying(false)
{
    for (int i = 0; i < kChannelCount; i++) {
        videoWidgets[i] = nullptr;
        fileSrcs[i] = nullptr;
    }

    setupUI();

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateInterface);

    busTimer = new QTimer(this);
    busTimer->setInterval(10);
    connect(busTimer, &QTimer::timeout, this, &MainWindow::processBusMessages);
    busTimer->start();

    setupGStreamer();
    setWindowState(windowState() | Qt::WindowMaximized);
    
    const QString preferredRootDir = "C:/Users/sipho-mancam/Videos/Playlists";
    currentRootDir = QDir(preferredRootDir).exists() ? preferredRootDir : QDir::homePath();
    rootDirEdit->setText(currentRootDir);
    loadPlaylists(currentRootDir);
}

MainWindow::~MainWindow() {
    if (busTimer) {
        busTimer->stop();
    }

    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
}

void MainWindow::setupUI() {
    this->setWindowTitle("8-Channel Synchronized DeckLink Playout");
    this->resize(1600, 900);

    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    setCentralWidget(centralWidget);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, centralWidget);
    splitter->setChildrenCollapsible(false);
    mainLayout->addWidget(splitter);

    QWidget *controlsContainer = new QWidget(splitter);
    QVBoxLayout *layout = new QVBoxLayout(controlsContainer);
    layout->setContentsMargins(8, 8, 8, 8);

    layout->addWidget(new QLabel("Playlist Root Directory:"));
    QHBoxLayout *rootLayout = new QHBoxLayout();
    rootDirEdit = new QLineEdit(this);
    rootDirEdit->setReadOnly(true);
    selectRootBtn = new QPushButton("Browse...", this);
    connect(selectRootBtn, &QPushButton::clicked, this, &MainWindow::onSelectRootDirectory);
    rootLayout->addWidget(rootDirEdit, 1);
    rootLayout->addWidget(selectRootBtn);
    layout->addLayout(rootLayout);

    playlistCombo = new QComboBox(this);
    connect(playlistCombo, &QComboBox::currentTextChanged, this, &MainWindow::onPlaylistChanged);
    layout->addWidget(new QLabel("Select Playlist:"));
    layout->addWidget(playlistCombo);

    startBtn = new QPushButton("Start", this);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    layout->addWidget(startBtn);

    playPauseBtn = new QPushButton("Play", this);
    connect(playPauseBtn, &QPushButton::clicked, this, &MainWindow::onPlayPauseClicked);
    layout->addWidget(playPauseBtn);

    layout->addStretch();

    controlsContainer->setMinimumWidth(280);
    controlsContainer->setMaximumWidth(480);

    QWidget *playoutGridContainer = new QWidget(splitter);
    QGridLayout *gridLayout = new QGridLayout(playoutGridContainer);
    gridLayout->setContentsMargins(6, 6, 6, 6);
    gridLayout->setSpacing(6);

    auto createVideoCell = [&](int channelIndex, int row, int col) {
        QFrame *cell = new QFrame(playoutGridContainer);
        cell->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(4, 4, 4, 4);
        cellLayout->setSpacing(4);

        QLabel *title = new QLabel(QString("Channel %1").arg(channelIndex + 1), cell);
        cellLayout->addWidget(title);

        videoWidgets[channelIndex] = new QWidget(cell);
        videoWidgets[channelIndex]->setMinimumSize(220, 140);
        videoWidgets[channelIndex]->setAttribute(Qt::WA_NativeWindow, true);
        cellLayout->addWidget(videoWidgets[channelIndex], 1);

        gridLayout->addWidget(cell, row, col);
    };

    createVideoCell(0, 0, 0);
    createVideoCell(1, 0, 1);
    createVideoCell(2, 0, 2);
    createVideoCell(3, 1, 0);
    createVideoCell(4, 1, 1);
    createVideoCell(5, 1, 2);
    createVideoCell(6, 2, 0);
    createVideoCell(7, 2, 1);

    QWidget *emptyCell = new QWidget(playoutGridContainer);
    gridLayout->addWidget(emptyCell, 2, 2);

    for (int i = 0; i < 3; i++) {
        gridLayout->setColumnStretch(i, 1);
        gridLayout->setRowStretch(i, 1);
    }

    splitter->addWidget(controlsContainer);
    splitter->addWidget(playoutGridContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setSizes({320, 1280});

    timeLabel = new QLabel("00:00:00 / 00:00:00", this);
    seekSlider = new QSlider(Qt::Horizontal, this);
    connect(seekSlider, &QSlider::sliderMoved, this, &MainWindow::onSeekSliderMoved);

    // Transport controls sit at the bottom of the main window.
    statusBar()->addWidget(seekSlider, 1);
    statusBar()->addPermanentWidget(timeLabel);
}

void MainWindow::onSelectRootDirectory() {
    const QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        "Select Playlist Root Directory",
        currentRootDir.isEmpty() ? QDir::homePath() : currentRootDir
    );

    if (selectedDir.isEmpty()) {
        return;
    }

    currentRootDir = selectedDir;
    rootDirEdit->setText(currentRootDir);
    loadPlaylists(currentRootDir);
}

void MainWindow::setupGStreamer() {
    // We construct a single pipeline string containing 8 parallel branches.
    // uridecodebin allows direct URI assignment for each channel source.
    QString pipelineStr;

    for (int i = 0; i < kChannelCount; i++) {
        pipelineStr += QString(
            "uridecodebin name=src_%1 ! queue max-size-buffers=3 ! tee name=t_%1 "
            "t_%1. ! queue max-size-buffers=3 ! deinterlace ! interlace field-pattern=1:1 top-field-first=true ! videoconvert ! video/x-raw,format=UYVY,interlace-mode=interleaved,field-order=top-field-first ! decklinkvideosink device-number=%1 mode=1080i50 "
            "t_%1. ! queue max-size-buffers=3 ! deinterlace ! videoconvert ! video/x-raw,format=BGRA,interlace-mode=progressive ! d3d11videosink name=gui_sink_%1 sync=false "
        ).arg(i);
    }

    for (int i = 0; i < kChannelCount; i++) {
        fileSrcs[i] = nullptr;
    }

    GError *error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.toUtf8().constData(), &error);
    if (error) {
        qWarning() << "Pipeline creation failed:" << error->message;
        g_clear_error(&error);
        pipeline = nullptr;
        return;
    }

    // Extract uridecodebin elements to set channel URIs later.
    for (int i = 0; i < kChannelCount; i++) {
        QString srcName = QString("src_%1").arg(i);
        fileSrcs[i] = gst_bin_get_by_name(GST_BIN(pipeline), srcName.toUtf8().constData());
        if (fileSrcs[i]) {
            g_object_set(fileSrcs[i], "expose-all-streams", FALSE, nullptr);
        }
    }

    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
}

void MainWindow::setWindowHandles() {
    if (!pipeline) {
        return;
    }

    // Map the Qt widget WId to the GStreamer autovideosinks
    for (int i = 0; i < kChannelCount; i++) {
        QString sinkName = QString("gui_sink_%1").arg(i);
        GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), sinkName.toUtf8().constData());

        if (sink) {
            if (GST_IS_VIDEO_OVERLAY(sink)) {
                gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), (guintptr)videoWidgets[i]->winId());
            }
            gst_object_unref(sink);
        }
    }
}

void MainWindow::loadPlaylists(const QString &rootDir) {
    QDir dir(rootDir);
    playlistCombo->clear();
    if (!dir.exists()) {
        return;
    }

    QStringList directories = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    playlistCombo->addItems(directories);
}

void MainWindow::onPlaylistChanged(const QString &dirName) {
    // Rebuild the playback graph for every playlist switch to avoid stale uridecodebin links.
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }
    if (bus) {
        gst_object_unref(bus);
        bus = nullptr;
    }
    setupGStreamer();
    if (!pipeline) {
        return;
    }

    if (updateTimer) {
        updateTimer->stop();
    }

    QDir playlistDir(currentRootDir + "/" + dirName);
    QStringList filters;
    filters << "*.mp4";
    QFileInfoList files = playlistDir.entryInfoList(filters, QDir::Files, QDir::Name);
    if (files.isEmpty()) {
        qWarning() << "No .mp4 files found in playlist:" << playlistDir.absolutePath();
        return;
    }

    // Load up to 8 files from the directory.
    for (int i = 0; i < kChannelCount; i++) {
        if (fileSrcs[i]) {
            const int fileIndex = i < files.size() ? i : files.size() - 1;
            const QString absPath = files[fileIndex].absoluteFilePath();
            const QByteArray nativePath = QDir::toNativeSeparators(absPath).toUtf8();
            GError *uriError = nullptr;
            gchar *uri = gst_filename_to_uri(nativePath.constData(), &uriError);

            if (!uri) {
                qWarning() << "Failed to convert file path to URI:" << absPath;
                if (uriError) {
                    qWarning() << "URI conversion error:" << uriError->message;
                    g_clear_error(&uriError);
                }
                continue;
            }

            g_object_set(fileSrcs[i], "uri", uri, nullptr);
            g_free(uri);
        }
    }

    // Must set window handles before going to PAUSED/PLAYING
    setWindowHandles();

    // Pre-roll the pipeline
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
    }
    duration = -1;
    isPlaying = false;
    playPauseBtn->setText("Play");
}

void MainWindow::onPlayPauseClicked() {
    if (!pipeline) return;

    if (isPlaying) {
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
        updateTimer->stop();
        playPauseBtn->setText("Play");
    } else {
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        updateTimer->start(100); // 100ms updates
        playPauseBtn->setText("Pause");
    }
    isPlaying = !isPlaying;
}

void MainWindow::onStartClicked() {
    if (!pipeline) {
        return;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (updateTimer) {
        updateTimer->start(100);
    }
    isPlaying = true;
    playPauseBtn->setText("Pause");
}

void MainWindow::onSeekSliderMoved(int position) {
    if (!pipeline) return;

    // position is percentage (0-100), map it to nanoseconds
    if (duration > 0) {
        gint64 seekPos = (gint64)((position / 100.0) * duration);
        gst_element_seek_simple(pipeline, GST_FORMAT_TIME, 
                               (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 
                               seekPos);
    }
}

void MainWindow::updateInterface() {
    if (!pipeline) return;

    processBusMessages();

    gint64 pos = 0;
    
    // Fetch duration once if not already grabbed
    if (duration == -1) {
        gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration);
    }

    if (gst_element_query_position(pipeline, GST_FORMAT_TIME, &pos)) {
        // Only update slider if user isn't actively dragging it
        if (!seekSlider->isSliderDown() && duration > 0) {
            int sliderVal = (int)((double)pos / duration * 100.0);
            seekSlider->setValue(sliderVal);
        }

        // Update Time Label
        QTime currentTime(0, 0, 0);
        currentTime = currentTime.addMSecs(pos / 1000000);
        
        QTime totalTime(0, 0, 0);
        if (duration > 0) totalTime = totalTime.addMSecs(duration / 1000000);

        timeLabel->setText(currentTime.toString("hh:mm:ss") + " / " + totalTime.toString("hh:mm:ss"));
    }
}

void MainWindow::processBusMessages() {
    if (!bus) {
        return;
    }

    GstMessage *msg = nullptr;
    const GstMessageType types = static_cast<GstMessageType>(
        GST_MESSAGE_ERROR |
        GST_MESSAGE_EOS |
        GST_MESSAGE_WARNING
    );

    while ((msg = gst_bus_pop_filtered(bus, types)) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_EOS:
                playPauseBtn->setText("Play");
                isPlaying = false;
                if (updateTimer) {
                    updateTimer->stop();
                }
                if (pipeline) {
                    gst_element_set_state(pipeline, GST_STATE_READY);
                }
                break;

            case GST_MESSAGE_ERROR: {
                GError *err = nullptr;
                gchar *debugInfo = nullptr;
                gst_message_parse_error(msg, &err, &debugInfo);
                qWarning() << "GStreamer error from" << GST_OBJECT_NAME(msg->src) << ":" << (err ? err->message : "Unknown error");
                if (debugInfo) {
                    qWarning() << "Debug details:" << debugInfo;
                }
                if (err) {
                    g_error_free(err);
                }
                g_free(debugInfo);
                break;
            }

            case GST_MESSAGE_WARNING: {
                GError *warn = nullptr;
                gchar *debugInfo = nullptr;
                gst_message_parse_warning(msg, &warn, &debugInfo);
                qWarning() << "GStreamer warning from" << GST_OBJECT_NAME(msg->src) << ":" << (warn ? warn->message : "Unknown warning");
                if (warn) {
                    g_error_free(warn);
                }
                g_free(debugInfo);
                break;
            }

            default:
                break;
        }

        gst_message_unref(msg);
    }
}