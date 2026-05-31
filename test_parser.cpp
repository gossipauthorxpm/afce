/****************************************************************************
**                                                                         **
** Test program for GOST 19.701-90 block parsing and rendering            **
**                                                                         **
****************************************************************************/

#include <QtCore>
#include <QtGui>
#include <QApplication>
#include <cstdio>
#include "pseudolangparser.h"
#include "zvflowchart.h"

// Use fprintf for test output since qDebug may be filtered
#define TEST_MSG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)

// Helper to load test APL from a file
static QString readTestFile(const QString &path)
{
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString::fromUtf8(f.readAll());
    return QString();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    int passed = 0;
    int failed = 0;

    // =====================================================================
    // TEST 0: Load test file
    // =====================================================================
    TEST_MSG("\n=== TEST 0: Load test_all_blocks.apl ===\n");
    QString testFile = readTestFile("examples/test_all_blocks.apl");
    if (testFile.isEmpty()) {
        TEST_MSG("  FAIL: Cannot read test file\n");
        failed++;
    } else {
        TEST_MSG("  OK Test file loaded (%ld bytes)\n", (long)testFile.length());
        passed++;
    }

    // =====================================================================
    // TEST 1: Parse all block types from APL test file
    // =====================================================================
    TEST_MSG("\n=== TEST 1: Parse all GOST block types from file ===\n");

    QString error;
    QDomDocument doc = PseudoLangParser::parse(testFile, &error);

    if (!error.isEmpty()) {
        TEST_MSG("  FAIL: Parser error: %s\n", qPrintable(error));
        failed++;
    } else {
        QDomElement root = doc.documentElement();
        if (root.tagName() != "algorithm") {
            TEST_MSG("  FAIL: Root element is not 'algorithm', got '%s'\n", qPrintable(root.tagName()));
            failed++;
        } else {
            QDomElement branch = root.firstChildElement("branch");
            if (branch.isNull()) {
                TEST_MSG("  FAIL: No branch element found\n");
                failed++;
            } else {
                int totalElements = 0;
                QDomElement child = branch.firstChildElement();
                while (!child.isNull()) {
                    totalElements++;
                    child = child.nextSiblingElement();
                }
                TEST_MSG("  OK Parsed %d elements from test file\n", totalElements);
                passed++;
            }
        }
    }

    // =====================================================================
    // TEST 2: Parse each block type individually
    // =====================================================================
    TEST_MSG("\n=== TEST 2: Individual block type parsing ===\n");

    struct BlockTest {
        const char *apl;
        const char *expectedType;
        const char *expectedAttr;
        const char *expectedValue;
    };

    BlockTest tests[] = {
        {"process \"test\";", "process", "text", "test"},
        {"call \"func()\";", "predefined", "text", "func()"},
        {"assign x = 42;", "assign", "dest", "x"},
        {"input a, b;", "io", "vars", "a, b"},
        {"output result;", "ou", "vars", "result"},
        {"data \"DATA\";", "data", "text", "DATA"},
        {"stored_data \"STORED\";", "stored_data", "text", "STORED"},
        {"document \"DOC\";", "document", "text", "DOC"},
        {"manual_input \"MI\";", "manual_input", "text", "MI"},
        {"display \"DISP\";", "display", "text", "DISP"},
        {"manual_op \"MO\";", "manual_op", "text", "MO"},
        {"parallel \"PAR\";", "parallel", "text", "PAR"},
        {"connector \"A\";", "connector", "text", "A"},
        {"ellipsis;", "ellipsis", "text", "..."},
        {"ram \"RAM\";", "ram", "text", "RAM"},
        {"seq_access \"TAPE\";", "seq_access", "text", "TAPE"},
        {"direct_access \"DISK\";", "direct_access", "text", "DISK"},
        {"card \"CARD\";", "card", "text", "CARD"},
        {"paper_tape \"PT\";", "paper_tape", "text", "PT"},
        {"comment \"CMT\";", "comment", "text", "CMT"},
    };

    for (unsigned t = 0; t < sizeof(tests)/sizeof(tests[0]); ++t) {
        QString code = QString("@start_scheme\n  %1\n@stop_scheme").arg(tests[t].apl);
        error.clear();
        QDomDocument d = PseudoLangParser::parse(code, &error);
        if (!error.isEmpty()) {
            TEST_MSG("  FAIL [%s] Parse error: %s\n", tests[t].expectedType, qPrintable(error));
            failed++;
        } else {
            QDomElement el = d.documentElement().firstChildElement("branch").firstChildElement();
            if (el.tagName() != tests[t].expectedType) {
                TEST_MSG("  FAIL [%s] Wrong type: got '%s'\n", tests[t].expectedType, qPrintable(el.tagName()));
                failed++;
            } else if (el.attribute(tests[t].expectedAttr) != tests[t].expectedValue) {
                TEST_MSG("  FAIL [%s] attr '%s': got '%s' expected '%s'\n",
                    tests[t].expectedType, tests[t].expectedAttr,
                    qPrintable(el.attribute(tests[t].expectedAttr)), tests[t].expectedValue);
                failed++;
            } else {
                TEST_MSG("  OK [%s]\n", tests[t].expectedType);
                passed++;
            }
        }
    }

    // =====================================================================
    // TEST 3: Parse conditional structures
    // =====================================================================
    TEST_MSG("\n=== TEST 3: Parse conditional structures ===\n");

    {
        QString code = "@start_scheme\n  if (x > 0) {\n    process \"pos\";\n  } else {\n    process \"neg\";\n  }\n@stop_scheme\n";
        error.clear();
        doc = PseudoLangParser::parse(code, &error);
        if (!error.isEmpty()) {
            TEST_MSG("  FAIL: If-else parse error: %s\n", qPrintable(error));
            failed++;
        } else {
            QDomElement ifEl = doc.documentElement().firstChildElement("branch").firstChildElement("if");
            if (ifEl.isNull()) {
                TEST_MSG("  FAIL: No 'if' element found\n");
                failed++;
            } else if (ifEl.attribute("cond") != "x > 0") {
                TEST_MSG("  FAIL: if condition wrong, got: '%s'\n", qPrintable(ifEl.attribute("cond")));
                failed++;
            } else {
                TEST_MSG("  OK If-else parsed, condition = '%s'\n", qPrintable(ifEl.attribute("cond")));
                passed++;
            }
        }
    }

    // =====================================================================
    // TEST 4: Parse loop structures
    // =====================================================================
    TEST_MSG("\n=== TEST 4: Parse loop structures ===\n");

    {
        QString code = "@start_scheme\n  while (i < 10) {\n    process \"body\";\n  }\n  do {\n    process \"body\";\n  } while (done);\n  for (i = 0; i < n; i++) {\n    process \"body\";\n  }\n@stop_scheme\n";
        error.clear();
        doc = PseudoLangParser::parse(code, &error);
        if (!error.isEmpty()) {
            TEST_MSG("  FAIL: Loop parse error: %s\n", qPrintable(error));
            failed++;
        } else {
            int loopCount = 0;
            QDomElement child = doc.documentElement().firstChildElement("branch").firstChildElement();
            while (!child.isNull()) {
                if (child.tagName() == "pre" || child.tagName() == "post") {
                    loopCount++;
                }
                child = child.nextSiblingElement();
            }
            if (loopCount == 3) {
                TEST_MSG("  OK 3 loops parsed correctly\n");
                passed++;
            } else {
                TEST_MSG("  FAIL: Expected 3 loops, got %d\n", loopCount);
                failed++;
            }
        }
    }

    // =====================================================================
    // TEST 5: XML round-trip of block types
    // =====================================================================
    TEST_MSG("\n=== TEST 5: XML round-trip ===\n");

    {
        QDomDocument td("afce");
        QDomElement alg = td.createElement("algorithm");
        td.appendChild(alg);
        QDomElement br = td.createElement("branch");
        alg.appendChild(br);

        QStringList allTypes = {
            "process", "predefined", "assign", "io", "ou",
            "data", "stored_data", "document", "manual_input", "display",
            "manual_op", "parallel", "connector", "ram",
            "seq_access", "direct_access", "card", "paper_tape",
            "ellipsis", "comment", "if", "pre", "post", "for"
        };

        for (const QString &type : allTypes) {
            QDomElement el = td.createElement(type);
            el.setAttribute("text", "test");
            br.appendChild(el);
        }

        QString xml = td.toString(2);
        QDomDocument restored;
        restored.setContent(xml);

        int restoredCount = 0;
        QDomElement ch = restored.documentElement().firstChildElement("branch").firstChildElement();
        while (!ch.isNull()) {
            restoredCount++;
            ch = ch.nextSiblingElement();
        }

        if (restoredCount == allTypes.size()) {
            TEST_MSG("  OK Round-trip: %d elements preserved\n", restoredCount);
            passed++;
        } else {
            TEST_MSG("  FAIL Round-trip: expected %ld, got %d\n", (long)allTypes.size(), restoredCount);
            failed++;
        }
    }

    // =====================================================================
    // Summary
    // =====================================================================
    TEST_MSG("\n========================================\n");
    TEST_MSG("Test results: PASSED: %d FAILED: %d\n", passed, failed);
    TEST_MSG("========================================\n");

    return (failed > 0) ? 1 : 0;
}
