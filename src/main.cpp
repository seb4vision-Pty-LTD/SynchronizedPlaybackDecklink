#include <QApplication>
#include "MainWindow.h"
#include <gst/gst.h>

int main(int argc, char *argv[]) {
    // Prefer software decode for H.264 to avoid D3D decoder caps negotiation failures.
    // g_setenv("GST_PLUGIN_FEATURE_RANK", "d3d11h264dec:0,d3d12h264dec:0", TRUE);

    // Initialize GStreamer before Qt
    gst_init(&argc, &argv);
    
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}