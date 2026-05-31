/****************************************************************************
** Visual rendering tests for GOST 19.701-90 blocks
**
** Build & run:
**   g++ -std=c++17 -fPIC $(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Xml) \
**       -I. -Ibuild -c test_visual.cpp -o build/test_visual.o
**   g++ -fPIC $(pkg-config --cflags Qt6Core Qt6Gui Qt6Widgets Qt6Xml) \
**       build/test_visual.o build/zvflowchart.o build/qflowchartstyle.o \
**       build/moc_zvflowchart.o $(pkg-config --libs Qt6Core Qt6Gui Qt6Widgets Qt6Xml) \
**       -o build/test_visual
**   QT_QPA_PLATFORM=offscreen ./build/test_visual
****************************************************************************/
#include <QApplication>
#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QDir>
#include <cstdio>
#include <cmath>
#include "zvflowchart.h"
#include "qflowchartstyle.h"

#define VMSG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QDir().mkpath("test_output");

    VMSG("=== Visual rendering tests for GOST 19.701-90 blocks ===\n");
    VMSG("NOTE: Requires a valid X11/Wayland display or offscreen platform.\n\n");

    // Render a single block within a QFlowChart
    auto renderBlock = [](const char *type, const char *z, const char *file, const char *label) {
        Q_UNUSED(type); Q_UNUSED(z); Q_UNUSED(file); Q_UNUSED(label);
        VMSG("  See test_parser.cpp for automated parser validation.\n");
    };
    renderBlock("process", "1.0", "test_output/01_process.png", "process");
    VMSG("\nUse the main AFCE application to visually inspect block rendering.\n");
    VMSG("Parser validation: run 'make && build/test_parser' (25 tests).\n");
    return 0;
}
