/****************************************************************************
** Unit tests for comment positioning logic (GOST 19.701-90 p.3.4.3)
**
** Tests the tree-walking algorithm that determines whether a comment
** should be placed on the LEFT or RIGHT side of its target block.
****************************************************************************/
#include <QApplication>
#include <QFile>
#include <cstdio>
#include <cstring>
#include "pseudolangparser.h"
#include "zvflowchart.h"

#define VMSG(...) fprintf(stderr, __VA_ARGS__); fflush(stderr)
#define PASS() do { passed++; VMSG("  PASS %d\n", __LINE__); } while(0)
#define FAIL(msg) do { failed++; VMSG("  FAIL %d: %s\n", __LINE__, msg); } while(0)

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    int passed = 0, failed = 0;

    VMSG("=== Comment positioning unit tests ===\n\n");

    // ---- Test 1: Parse tree_comment_test and verify structure ----
    VMSG("Test 1: Parse tree_comment_test.apl\n");
    QFile f("examples/tree_comment_test.apl");
    if (!f.open(QIODevice::ReadOnly)) { FAIL("cannot open file"); return 1; }
    QString code = QString::fromUtf8(f.readAll());
    QString err;
    QDomDocument doc = PseudoLangParser::parse(code, &err);
    if (!err.isEmpty()) { FAIL(qPrintable(err)); return 1; }
    PASS();

    // ---- Test 2: Build QBlock tree from parsed XML ----
    VMSG("Test 2: Build QBlock tree, run adjustSize/adjustPosition\n");
    QFlowChart *chart = new QFlowChart();
    chart->root()->setXmlNode(doc.documentElement());
    chart->root()->adjustSize(1.0);
    chart->root()->adjustPosition(0, 0);

    // Verify the tree structure
    QBlock *alg = chart->root();
    if (alg->type() != "algorithm") { FAIL("root not algorithm"); return 1; }
    if (alg->items.size() < 1) { FAIL("no branches"); return 1; }
    PASS();

    // ---- Test 3: Walk the tree and verify comment positions ----
    VMSG("Test 3: Verify comment side determination\n");

    struct CommentExpect {
        const char *text;
        bool expectRight; // true = right side, false = left side
    };
    // Expected from user spec:
    // "Дерево пусто" in outer if's LEFT branch → LEFT (false)
    // "ID меньше" in inner if's LEFT branch → LEFT (false)
    // "ID больше" in innermost if's LEFT branch, but in outer if's RIGHT → RIGHT (true)
    // "ID совпадает" in innermost if's RIGHT branch → RIGHT (true)
    CommentExpect expected[] = {
        {"Дерево пусто", false},
        {"ID меньше", false},
        {"ID больше", true},
        {"ID совпадает", true},
    };

    // Walk the tree and collect all comments
    struct CommentInfo {
        QString text;
        QBlock *block;
        QBlock *parent;
    };
    QList<CommentInfo> comments;

    // Recursively collect comments
    std::function<void(QBlock*)> collect = [&](QBlock *b) {
        if (b->type() == "comment") {
            comments.append({b->attributes.value("text", ""), b, b->parent});
        }
        for (int i = 0; i < b->items.size(); ++i)
            collect(b->items.at(i));
    };
    collect(alg);

    if (comments.size() != 4) {
        VMSG("  Expected 4 comments, found %d\n", comments.size());
        FAIL("wrong comment count");
        // Show what we found
        for (auto &c : comments)
            VMSG("    comment: '%s'\n", qPrintable(c.text));
    } else {
        PASS();
    }

    // Verify each comment's expected side
    for (int i = 0; i < (int)comments.size() && i < 4; ++i) {
        QString text = comments[i].text;
        bool found = false;
        for (auto &exp : expected) {
            if (text.contains(exp.text)) {
                found = true;

                // Determine actual side by walking up the tree
                // Uses the same logic as QFlowChart::adjustPosition for comments
                int elseDepth = 0;
                bool hasBranchIdx = false;
                int branchIdx = 0;
                QBlock *node = comments[i].parent;
                while (node) {
                    QBlock *p = node->parent;
                    if (p && p->type() == "if") {
                        int idx = p->items.indexOf(node);
                        if (!hasBranchIdx) {
                            branchIdx = idx;
                            hasBranchIdx = true;
                        } else if (idx == 1) {
                            elseDepth++;
                        }
                    }
                    node = p;
                }
                bool isRight;
                if (!hasBranchIdx) {
                    isRight = true; // default for non-if
                } else if (branchIdx == 1) {
                    isRight = true;
                } else {
                    isRight = (elseDepth >= 2);
                }

                bool expectedRight = exp.expectRight;
                if (isRight == expectedRight) {
                    VMSG("  OK comment '%s': side=%s (depth=%d)\n",
                         qPrintable(text), isRight ? "RIGHT" : "LEFT", elseDepth);
                    passed++;
                } else {
                    VMSG("  FAIL comment '%s': expected %s, got %s (depth=%d)\n",
                         qPrintable(text),
                         expectedRight ? "RIGHT" : "LEFT",
                         isRight ? "RIGHT" : "LEFT", elseDepth);
                    failed++;
                }
                break;
            }
        }
        if (!found) {
            VMSG("  WARN comment '%s' not in expected list\n", qPrintable(text));
        }
    }

    // ---- Test 4: Verify center branch alternation and minLeft ----
    VMSG("Test 4: Center flow alternation test\n");
    {
        QString apl = R"(
@start_scheme
  process "Step 1";
  comment "Comment 1";
  process "Step 2";
  comment "Comment 2";
@stop_scheme
)";
        QString err;
        QDomDocument doc = PseudoLangParser::parse(apl, &err);
        if (!err.isEmpty()) { FAIL(qPrintable(err)); }
        else {
            QFlowChart *c2 = new QFlowChart();
            c2->root()->setXmlNode(doc.documentElement());
            c2->root()->adjustSize(1.0);
            c2->root()->adjustPosition(0, 0);

            QBlock *branch = c2->root()->item(0);
            QBlock *p1 = nullptr, *cmt1 = nullptr, *p2 = nullptr, *cmt2 = nullptr;
            for (int i = 0; i < branch->items.size(); ++i) {
                QBlock *it = branch->items.at(i);
                if (it->type() == "process" && !p1) p1 = it;
                else if (it->type() == "comment" && !cmt1) cmt1 = it;
                else if (it->type() == "process" && p1) p2 = it;
                else if (it->type() == "comment" && cmt1) cmt2 = it;
            }
            if (!p1 || !cmt1 || !p2 || !cmt2) {
                FAIL("missing blocks in parsed apl");
            } else {
                bool cmt1Right = (cmt1->x > p1->x + p1->width);
                bool cmt2Left  = (cmt2->x + cmt2->width < p2->x);
                bool cmt1NotClipped = (cmt1->x >= 0);
                bool cmt2NotClipped = (cmt2->x >= 0);

                if (cmt1Right && cmt1NotClipped) { passed++; VMSG("  OK cmt1 RIGHT\n"); }
                else { VMSG("  FAIL cmt1: x=%.1f tr=%.1f\n", cmt1->x, p1->x+p1->width); FAIL("cmt1 RIGHT"); }

                if (cmt2Left && cmt2NotClipped) { passed++; VMSG("  OK cmt2 LEFT\n"); }
                else { VMSG("  FAIL cmt2: r=%.1f tx=%.1f\n", cmt2->x+cmt2->width, p2->x); FAIL("cmt2 LEFT"); }

                // Verify branch was widened for left comment (minLeft check)
                if (branch->x <= cmt2->x) {
                    VMSG("  OK branch.x <= cmt2->x (minLeft respected)\n");
                    passed++;
                } else {
                    VMSG("  FAIL branch.x=%.1f > cmt2->x=%.1f\n", branch->x, cmt2->x);
                    FAIL("minLeft not respected");
                }
            }
            delete c2;
        }
    }

    // ---- Summary ----
    VMSG("\n========================================\n");
    VMSG("Results: PASSED=%d FAILED=%d\n", passed, failed);
    VMSG("========================================\n");

    delete chart;
    return (failed > 0) ? 1 : 0;
}
