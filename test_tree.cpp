// Test tree comment scheme
#include <QApplication>
#include <QFile>
#include <cstdio>
#include "pseudolangparser.h"
#define VMSG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QFile f("examples/tree_comment_test.apl");
    if (!f.open(QIODevice::ReadOnly)) { VMSG("FAIL: cannot open\n"); return 1; }
    QString code = QString::fromUtf8(f.readAll());
    QString err;
    QDomDocument doc = PseudoLangParser::parse(code, &err);
    if (!err.isEmpty()) VMSG("Warning: %s\n", qPrintable(err));
    // Count elements
    int comments=0, processes=0, ifs=0;
    QDomElement root = doc.documentElement();
    QDomElement branch = root.firstChildElement("branch");
    QDomElement child = branch.firstChildElement();
    while (!child.isNull()) {
        QString tag = child.tagName();
        if (tag == "process") processes++;
        else if (tag == "comment") comments++;
        else if (tag == "if") ifs++;
        child = child.nextSiblingElement();
    }
    VMSG("Top-level: processes=%d comments=%d ifs=%d\n", processes, comments, ifs);
    // Expect: 1 process, 1 if, 0 comments at top level (comments are inside if branches)
    VMSG("%s\n", qPrintable(doc.toString(2)));
    return 0;
}
