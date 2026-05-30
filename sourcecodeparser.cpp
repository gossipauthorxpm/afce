#include "sourcecodeparser.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStack>

// ---------------------------------------------------------------------------
// stripLiterals — replace comments and string/char literals with spaces
// ---------------------------------------------------------------------------
QString SourceCodeParser::stripLiterals(const QString &code)
{
    QString result = code;
    const int len = result.length();
    QChar *data = result.data();
    bool inLineComment = false;
    bool inBlockComment = false;
    bool inString = false;
    bool inChar = false;

    for (int i = 0; i < len; ++i) {
        if (inLineComment) {
            if (data[i] == '\n')
                inLineComment = false;
            else
                data[i] = ' ';
            continue;
        }
        if (inBlockComment) {
            if (i + 1 < len && data[i] == '*' && data[i + 1] == '/') {
                data[i] = data[i + 1] = ' ';
                ++i;
                inBlockComment = false;
            } else {
                data[i] = ' ';
            }
            continue;
        }
        if (inString) {
            if (data[i] == '\\' && i + 1 < len) {
                data[i] = data[i + 1] = ' ';
                ++i;
            } else if (data[i] == '"') {
                data[i] = ' ';
                inString = false;
            } else {
                data[i] = ' ';
            }
            continue;
        }
        if (inChar) {
            if (data[i] == '\\' && i + 1 < len) {
                data[i] = data[i + 1] = ' ';
                ++i;
            } else if (data[i] == '\'') {
                data[i] = ' ';
                inChar = false;
            } else {
                data[i] = ' ';
            }
            continue;
        }
        // Not in any literal — check for start
        if (i + 1 < len && data[i] == '/' && data[i + 1] == '/') {
            data[i] = data[i + 1] = ' ';
            ++i;
            inLineComment = true;
        } else if (i + 1 < len && data[i] == '/' && data[i + 1] == '*') {
            data[i] = data[i + 1] = ' ';
            ++i;
            inBlockComment = true;
        } else if (data[i] == '"') {
            data[i] = ' ';
            inString = true;
        } else if (data[i] == '\'') {
            data[i] = ' ';
            inChar = true;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// matchBrace — find matching closing brace/paren accounting for nesting
// ---------------------------------------------------------------------------
int SourceCodeParser::matchBrace(const QString &code, int openPos, QChar open, QChar close)
{
    if (openPos >= code.length() || code[openPos] != open)
        return -1;
    int depth = 0;
    for (int i = openPos; i < code.length(); ++i) {
        if (code[i] == open) ++depth;
        else if (code[i] == close) {
            --depth;
            if (depth == 0)
                return i;
        }
    }
    return -1;
}

int SourceCodeParser::matchParen(const QString &code, int openPos)
{
    return matchBrace(code, openPos, '(', ')');
}

// ---------------------------------------------------------------------------
// isWordAt — check if a specific keyword appears as a whole word at position
// ---------------------------------------------------------------------------
bool SourceCodeParser::isWordAt(const QString &code, int pos, const char *word)
{
    if (pos > 0) {
        QChar prev = code[pos - 1];
        if (prev.isLetterOrNumber() || prev == '_')
            return false;
    }
    int wlen = static_cast<int>(qstrlen(word));
    if (pos + wlen > code.length())
        return false;
    for (int i = 0; i < wlen; ++i) {
        if (code[pos + i] != word[i])
            return false;
    }
    int next = pos + wlen;
    if (next < code.length()) {
        QChar nc = code[next];
        if (nc.isLetterOrNumber() || nc == '_')
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// extractConditionEnd — find the closing paren of a keyword's condition
// e.g. "if (x > 0) {" → returns position of ')'
// ---------------------------------------------------------------------------
int SourceCodeParser::extractConditionEnd(const QString &code, int startPos)
{
    // skip whitespace
    int i = startPos;
    while (i < code.length() && code[i].isSpace())
        ++i;
    if (i < code.length() && code[i] == '(')
        return matchParen(code, i);
    return -1;
}

// ---------------------------------------------------------------------------
// extractBody — extract the body of a compound statement
// Returns the position after the body, and stores body text in bodyOut.
// Handles both {...} blocks and single statements.
// ---------------------------------------------------------------------------
int SourceCodeParser::extractBody(const QString &code, int startPos, QString &bodyOut)
{
    int i = startPos;
    while (i < code.length() && code[i].isSpace())
        ++i;
    if (i >= code.length()) {
        bodyOut.clear();
        return i;
    }
    if (code[i] == '{') {
        int close = matchBrace(code, i, '{', '}');
        if (close < 0) {
            bodyOut.clear();
            return i;
        }
        // Extract content between { and }
        bodyOut = code.mid(i + 1, close - i - 1);
        return close + 1;
    }
    // Single statement — find end at ';' at depth 0
    int depth = 0;
    int end = i;
    while (end < code.length()) {
        if (code[end] == '{') ++depth;
        else if (code[end] == '}') --depth;
        else if (code[end] == ';' && depth == 0) {
            bodyOut = code.mid(i, end - i + 1);
            return end + 1;
        }
        ++end;
    }
    bodyOut = code.mid(i);
    return code.length();
}

// ---------------------------------------------------------------------------
// parseStatement — parse one statement and add blocks to the branch
// Returns true if a statement was recognized and handled.
// ---------------------------------------------------------------------------
bool SourceCodeParser::parseStatement(const QString &stmt, QDomDocument &doc, QDomElement &branch)
{
    // Original text (with comments) for content extraction
    const QString orig = stmt.trimmed();
    if (orig.isEmpty())
        return false;
    // Stripped version (comments removed) for keyword detection
    QString s = stripLiterals(orig).trimmed();

    // Helper: strip only C comments (// and /* */) from a string, keep strings intact
    auto cleanValue = [](QString val) -> QString {
        // Strip only /* */ block comments
        {
            QRegularExpression re("/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption);
            val.replace(re, QString());
        }
        // Strip // line comments (from // to end of line)
        {
            QRegularExpression re("//[^\n]*");
            val.replace(re, QString());
        }
        val = val.trimmed();
        // Remove redundant internal whitespace
        val.replace(QRegularExpression("\\s+"), QStringLiteral(" "));
        return val;
    };

    // Helper: extract a parenthesized block from the ORIGINAL text.
    struct ParenContent {
        QString content;
        int endPos;
        bool valid;
    };
    auto extractParenContent = [&](const QString &text, int searchFrom = 0) -> ParenContent {
        int pp = -1;
        for (int i = searchFrom; i < text.length(); ++i) {
            if (text[i] == '(') { pp = i; break; }
        }
        if (pp < 0) return {QString(), -1, false};
        int pe = matchParen(text, pp);
        if (pe < 0) return {QString(), -1, false};
        return {text.mid(pp + 1, pe - pp - 1), pe, true};
    };

    // ---------------------------------------------------------------
    // if (...) ... else ...
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "if")) {
        auto pc = extractParenContent(orig);
        if (!pc.valid) return false;
        QString cond = cleanValue(pc.content);

        QDomElement ifBlock = addBlock(doc, branch, "if", "cond", cond);
        QDomElement thenBranch = addBranch(doc, ifBlock);
        QDomElement elseBranch = addBranch(doc, ifBlock);

        // Extract then-body from original
        QString thenBody;
        int afterThen = extractBody(orig, pc.endPos + 1, thenBody);
        parseBlock(thenBody, doc, thenBranch);

        // Look for else in stripped version
        int searchPos = 0;
        // Map the position: find the then body end in stripped
        QString strippedAfter = s.mid(s.indexOf('('));
        int condEndS = extractConditionEnd(s, 2);
        if (condEndS < 0) condEndS = s.indexOf(')') + 1;
        else condEndS += 1;
        // Check for else in stripped
        int searchS = condEndS;
        {
            // find matching body end in stripped
            QString tmpBody;
            searchS = extractBody(s, condEndS, tmpBody);
        }
        while (searchS < s.length() && s[searchS].isSpace()) ++searchS;
        if (isWordAt(s, searchS, "else")) {
            int elsePosS = searchS + 4;
            while (elsePosS < s.length() && s[elsePosS].isSpace()) ++elsePosS;
            if (isWordAt(s, elsePosS, "if")) {
                QString rest = s.mid(elsePosS).trimmed();
                parseStatement(rest, doc, elseBranch);
            } else {
                QString elseBody;
                extractBody(orig, afterThen, elseBody);
                // We need the actual else body from orig...
                // find it by scanning orig from afterThen
                int eSearch = afterThen;
                while (eSearch < orig.length() && orig[eSearch].isSpace()) ++eSearch;
                if (isWordAt(orig, eSearch, "else")) {
                    int ePos = eSearch + 4;
                    while (ePos < orig.length() && orig[ePos].isSpace()) ++ePos;
                    extractBody(orig, ePos, elseBody);
                    parseBlock(elseBody, doc, elseBranch);
                }
            }
        }
        return true;
    }

    // ---------------------------------------------------------------
    // while (...) ...
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "while")) {
        auto pc = extractParenContent(orig);
        if (!pc.valid) return false;
        QString cond = cleanValue(pc.content);

        QDomElement whileBlock = addBlock(doc, branch, "pre", "cond", cond);
        QDomElement bodyBranch = addBranch(doc, whileBlock);

        QString body;
        extractBody(orig, pc.endPos + 1, body);
        parseBlock(body, doc, bodyBranch);
        return true;
    }

    // ---------------------------------------------------------------
    // do { ... } while (...);
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "do")) {
        int braceStart = -1;
        for (int i = 0; i < orig.length(); ++i) {
            if (orig[i] == '{') { braceStart = i; break; }
        }
        if (braceStart < 0) return false;
        int braceEnd = matchBrace(orig, braceStart, '{', '}');
        if (braceEnd < 0) return false;

        QString body = orig.mid(braceStart + 1, braceEnd - braceStart - 1);

        int whilePos = braceEnd + 1;
        while (whilePos < orig.length() && orig[whilePos].isSpace()) ++whilePos;
        if (!isWordAt(orig, whilePos, "while")) return false;
        int parenPos = whilePos + 5;
        while (parenPos < orig.length() && orig[parenPos].isSpace()) ++parenPos;
        auto pc = extractParenContent(orig, parenPos);
        if (!pc.valid) return false;
        QString cond = cleanValue(pc.content);

        QDomElement postBlock = addBlock(doc, branch, "post", "cond", cond);
        QDomElement bodyBranch = addBranch(doc, postBlock);
        parseBlock(body, doc, bodyBranch);
        return true;
    }

    // ---------------------------------------------------------------
    // for (...; ...; ...) ...
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "for")) {
        int parenPos = -1;
        for (int i = 0; i < orig.length(); ++i) {
            if (orig[i] == '(') { parenPos = i; break; }
        }
        if (parenPos < 0) return false;
        int parenEnd = matchParen(orig, parenPos);
        if (parenEnd < 0) return false;

        QString forSpec = orig.mid(parenPos + 1, parenEnd - parenPos - 1);
        QStringList parts;
        int last = 0;
        int depth = 0;
        for (int i = 0; i < forSpec.length(); ++i) {
            if (forSpec[i] == '(') ++depth;
            else if (forSpec[i] == ')') --depth;
            else if (forSpec[i] == ';' && depth == 0) {
                parts << forSpec.mid(last, i - last).trimmed();
                last = i + 1;
            }
        }
        parts << forSpec.mid(last).trimmed();
        while (parts.size() < 3) parts << QString();

        if (!parts[0].isEmpty()) {
            QString init = parts[0];
            if (init.contains('=')) {
                int eq = init.indexOf('=');
                QString dest = init.left(eq).trimmed();
                int space = dest.lastIndexOf(' ');
                if (space >= 0) dest = dest.mid(space + 1);
                while (dest.endsWith('*')) dest.chop(1);
                dest = dest.trimmed();
                addBlock(doc, branch, "assign", "dest", cleanValue(dest));
                QString src = cleanValue(init.mid(eq + 1));
                QDomElement assignEl = branch.lastChildElement("assign");
                if (!assignEl.isNull())
                    assignEl.setAttribute("src", src);
            } else {
                addBlock(doc, branch, "process", "text", cleanValue(init + ";"));
            }
        }

        QString cond = parts[1].isEmpty() ? "1" : cleanValue(parts[1]);
        QDomElement forBlock = addBlock(doc, branch, "pre", "cond", cond);
        QDomElement bodyBranch = addBranch(doc, forBlock);

        QString body;
        extractBody(orig, parenEnd + 1, body);
        parseBlock(body, doc, bodyBranch);

        if (!parts[2].isEmpty()) {
            addBlock(doc, bodyBranch, "process", "text", cleanValue(parts[2]));
        }
        return true;
    }

    // ---------------------------------------------------------------
    // scanf(...) → io
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "scanf")) {
        auto pc = extractParenContent(orig);
        if (!pc.valid) return false;
        QString args = pc.content;

        QStringList parts;
        int depth = 0;
        int last = 0;
        for (int i = 0; i < args.length(); ++i) {
            if (args[i] == '(') ++depth;
            else if (args[i] == ')') --depth;
            else if (args[i] == ',' && depth == 0) {
                parts << args.mid(last, i - last).trimmed();
                last = i + 1;
            }
        }
        parts << args.mid(last).trimmed();

        QStringList vars;
        for (int i = 1; i < parts.size(); ++i) {
            QString v = parts[i].trimmed();
            while (v.startsWith('&')) v = v.mid(1);
            while (v.startsWith('*')) v = v.mid(1);
            vars << v.trimmed();
        }
        addBlock(doc, branch, "io", "vars", cleanValue(vars.join(",")));
        return true;
    }

    // ---------------------------------------------------------------
    // printf(...) → ou
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "printf") || isWordAt(s, 0, "fprintf")) {
        auto pc = extractParenContent(orig);
        if (!pc.valid) return false;
        QString args = pc.content;
        addBlock(doc, branch, "ou", "vars", cleanValue(args));
        return true;
    }

    // ---------------------------------------------------------------
    // Simple assignment: var = expr;
    // ---------------------------------------------------------------
    if (orig.contains('=') && !orig.contains("==") && !orig.contains("!=")
        && !orig.contains("<=") && !orig.contains(">=")) {
        int eq = orig.indexOf('=');
        QString dest = orig.left(eq).trimmed();
        int space = dest.lastIndexOf(' ');
        if (space >= 0) dest = dest.mid(space + 1);
        while (dest.endsWith('*')) dest.chop(1);
        dest = dest.trimmed();
        if (!dest.isEmpty() && dest[0].isLetter()) {
            QString src = orig.mid(eq + 1).trimmed();
            while (src.endsWith(';')) src.chop(1);
            src = src.trimmed();
            addBlock(doc, branch, "assign", "dest", cleanValue(dest));
            QDomElement assignEl = branch.lastChildElement("assign");
            if (!assignEl.isNull())
                assignEl.setAttribute("src", cleanValue(src));
            return true;
        }
    }

    // ---------------------------------------------------------------
    // switch (...) { case ...: ... }
    // Convert to if-else chain
    // ---------------------------------------------------------------
    if (isWordAt(s, 0, "switch")) {
        auto pc = extractParenContent(orig);
        if (!pc.valid) return false;
        QString expr = pc.content.trimmed();

        int bodyStart = pc.endPos + 1;
        while (bodyStart < orig.length() && orig[bodyStart].isSpace()) ++bodyStart;
        if (bodyStart >= orig.length() || orig[bodyStart] != '{') return false;
        int bodyEnd = matchBrace(orig, bodyStart, '{', '}');
        if (bodyEnd < 0) return false;

        QString switchBody = orig.mid(bodyStart + 1, bodyEnd - bodyStart - 1);
        QString strippedBody = stripLiterals(switchBody);

        // Find all case labels in the stripped body
        struct CaseInfo {
            QString value;  // "case VALUE:" value, or "default"
            int start;      // position in original switchBody where case body starts
            int end;        // end position in original switchBody
        };
        QList<CaseInfo> cases;

        // Scan for case/default labels
        int scanPos = 0;
        int len = strippedBody.length();
        while (scanPos < len) {
            while (scanPos < len && (strippedBody[scanPos].isSpace() || strippedBody[scanPos] == ';'))
                ++scanPos;
            if (scanPos >= len) break;

            QString label;
            if (isWordAt(strippedBody, scanPos, "case")) {
                int valStart = scanPos + 4;
                while (valStart < len && strippedBody[valStart].isSpace()) ++valStart;
                int valEnd = valStart;
                while (valEnd < len && strippedBody[valEnd] != ':')
                    ++valEnd;
                if (valEnd >= len) break;
                label = switchBody.mid(valStart, valEnd - valStart).trimmed();
                scanPos = valEnd + 1; // skip ':'
            } else if (isWordAt(strippedBody, scanPos, "default")) {
                int colon = scanPos + 7;
                while (colon < len && strippedBody[colon].isSpace()) ++colon;
                if (colon >= len || strippedBody[colon] != ':') { ++scanPos; continue; }
                label = QStringLiteral("default");
                scanPos = colon + 1;
            } else {
                // Not a case label — find next label or end
                // We'll skip to the next case/default or end
                int nextLabel = len;
                int nextPos = scanPos;
                while (nextPos < len) {
                    if (isWordAt(strippedBody, nextPos, "case") || isWordAt(strippedBody, nextPos, "default")) {
                        nextLabel = nextPos;
                        break;
                    }
                    ++nextPos;
                }
                scanPos = nextLabel;
                continue;
            }

            if (!label.isEmpty()) {
                CaseInfo ci;
                ci.value = label;
                ci.start = scanPos;

                // Find end of this case (next case/default, or end of body)
                int nextPos = scanPos;
                while (nextPos < len) {
                    if (isWordAt(strippedBody, nextPos, "case") || isWordAt(strippedBody, nextPos, "default")) {
                        break;
                    }
                    ++nextPos;
                }
                ci.end = nextPos;
                cases.append(ci);
                scanPos = nextPos;
            }
        }

        // Build if-else chain from cases
        if (cases.isEmpty()) return true;

        // First case becomes the if; subsequent cases become else-if; default becomes last else
        QDomElement currentIf;
        QDomElement currentElseBranch;

        for (int ci = 0; ci < cases.size(); ++ci) {
            QString caseBody = switchBody.mid(cases[ci].start, cases[ci].end - cases[ci].start).trimmed();
            // Remove trailing break from case body (it's implicit in flowchart)
            // But keep as process if needed

            QString strippedCase = stripLiterals(caseBody);
            // Strip trailing 'break;'
            if (strippedCase.endsWith("break")) {
                int lastBreak = strippedCase.lastIndexOf("break");
                // Check it's really a statement boundary
                caseBody = caseBody.left(cases[ci].start + (lastBreak - strippedCase.left(cases[ci].start).length()));
                caseBody = caseBody.trimmed();
                if (caseBody.endsWith(';')) caseBody.chop(1);
                caseBody = caseBody.trimmed();
            }

            if (cases[ci].value == "default") {
                // Default case → final else branch
                if (currentElseBranch.isNull()) {
                    // No else branch yet — create if with just else
                    // This happens if default is the only case
                    QDomElement ifEl = addBlock(doc, branch, "if", "cond", "default");
                    currentElseBranch = addBranch(doc, ifEl);
                    if (!caseBody.isEmpty()) {
                        // We can still parse the default body
                        parseBlock(caseBody, doc, currentElseBranch);
                    }
                } else {
                    if (!caseBody.isEmpty())
                        parseBlock(caseBody, doc, currentElseBranch);
                }
            } else if (ci == 0) {
                // First case → "if (expr == value)"
                QString cond = cleanValue(expr) + " == " + cleanValue(cases[ci].value);
                currentIf = addBlock(doc, branch, "if", "cond", cond);
                QDomElement thenBr = addBranch(doc, currentIf);
                currentElseBranch = addBranch(doc, currentIf);
                if (!caseBody.isEmpty())
                    parseBlock(caseBody, doc, thenBr);
            } else {
                // Subsequent cases → nested if in else branch
                QString cond = cleanValue(expr) + " == " + cleanValue(cases[ci].value);
                QDomElement nestedIf = addBlock(doc, currentElseBranch, "if", "cond", cond);
                QDomElement thenBr = addBranch(doc, nestedIf);
                currentElseBranch = addBranch(doc, nestedIf);
                if (!caseBody.isEmpty())
                    parseBlock(caseBody, doc, thenBr);
            }
        }

        return true;
    }

    // Function call → predefined process
    {
        QRegularExpression funcRe(R"(^\s*(\w[\w\d]*)\s*\()");
        auto m = funcRe.match(s);
        if (m.hasMatch() && m.capturedStart() == 0) {
            // Extract the full call from original text (keep parens and args)
            QString callText = orig.trimmed();
            while (callText.endsWith(';')) callText.chop(1);
            callText = callText.trimmed();
            addBlock(doc, branch, "predefined", "text", cleanValue(callText));
            return true;
        }
    }

    // ---------------------------------------------------------------
    // Default: process block
    // ---------------------------------------------------------------
    // Strip any remaining comments from the statement text
    QString text = stripLiterals(s);
    while (text.endsWith(';')) text.chop(1);
    text = text.trimmed();
    if (!text.isEmpty())
        addBlock(doc, branch, "process", "text", text);
    return true;
}

// ---------------------------------------------------------------------------
// parseBlock — parse a sequence of statements into a branch
// ---------------------------------------------------------------------------
void SourceCodeParser::parseBlock(const QString &code, QDomDocument &doc, QDomElement &parentBranch)
{
    QString stripped = stripLiterals(code);

    // Walk through and find top-level statements
    int i = 0;
    int len = stripped.length();
    while (i < len) {
        // skip whitespace and semicolons
        while (i < len && (stripped[i].isSpace() || stripped[i] == ';'))
            ++i;
        if (i >= len) break;

        // if (...) ... else ... (handled as a complete chain)
        if (isWordAt(stripped, i, "if")) {
            // Find the end of the entire if-else chain by parsing stripped code.
            int scanPos = i + 2; // past 'if'
            // Skip condition
            while (scanPos < len && stripped[scanPos].isSpace()) ++scanPos;
            if (scanPos < len && stripped[scanPos] == '(')
                scanPos = matchParen(stripped, scanPos) + 1;
            else { ++i; continue; }

            // Skip then-body
            QString thenBody;
            scanPos = extractBody(code, scanPos, thenBody);

            // Look for else chain
            while (scanPos < code.length()) {
                int ws = scanPos;
                while (ws < code.length() && code[ws].isSpace()) ++ws;
                if (!isWordAt(code, ws, "else")) break;
                int afterElse = ws + 4;
                while (afterElse < code.length() && code[afterElse].isSpace()) ++afterElse;
                if (isWordAt(code, afterElse, "if")) {
                    // else if - skip its condition and body
                    int eif = afterElse + 2;
                    while (eif < code.length() && code[eif].isSpace()) ++eif;
                    if (eif < code.length() && code[eif] == '(')
                        eif = matchParen(code, eif) + 1;
                    else break;
                    QString eifBody;
                    scanPos = extractBody(code, eif, eifBody);
                } else {
                    // simple else - skip its body and stop
                    QString elseBody;
                    scanPos = extractBody(code, afterElse, elseBody);
                    break;
                }
            }

            // Extract the full if-else chain from original code
            QString ifChain = code.mid(i, scanPos - i);
            parseStatement(ifChain, doc, parentBranch);
            i = scanPos;
            continue;
        }

        // do { ... } while (...);
        if (isWordAt(stripped, i, "do")) {
            int braceStart = -1;
            for (int j = i; j < len; ++j) {
                if (stripped[j] == '{') { braceStart = j; break; }
            }
            if (braceStart >= 0) {
                int braceEnd = matchBrace(stripped, braceStart, '{', '}');
                if (braceEnd >= 0) {
                    QString body = code.mid(braceStart + 1, braceEnd - braceStart - 1);
                    int whilePos = braceEnd + 1;
                    while (whilePos < len && stripped[whilePos].isSpace())
                        ++whilePos;
                    if (isWordAt(stripped, whilePos, "while")) {
                        int parenPos = whilePos + 5;
                        while (parenPos < len && stripped[parenPos].isSpace())
                            ++parenPos;
                        int condEnd = matchParen(stripped, parenPos);
                        if (condEnd >= 0) {
                            QString cond = stripLiterals(code.mid(parenPos + 1, condEnd - parenPos - 1)).trimmed();
                                            QDomElement postEl = addBlock(doc, parentBranch, "post", "cond", cond);
                            QDomElement bodyBr = addBranch(doc, postEl);
                            parseBlock(body, doc, bodyBr);
                            // Find the end of the statement
                            int stmtEnd = condEnd + 1;
                            while (stmtEnd < len && stripped[stmtEnd].isSpace())
                                ++stmtEnd;
                            if (stmtEnd < len && stripped[stmtEnd] == ';')
                                ++stmtEnd;
                            i = stmtEnd;
                            continue;
                        }
                    }
                }
            }
        }

        // while (...) ...
        if (isWordAt(stripped, i, "while")) {
            int parenPos = i + 5;
            while (parenPos < len && stripped[parenPos].isSpace())
                ++parenPos;
            int condEnd = matchParen(stripped, parenPos);
            if (condEnd >= 0) {
                // Check it's not a do-while (do should have been caught above)
                QString cond = stripLiterals(code.mid(parenPos + 1, condEnd - parenPos - 1)).trimmed();
                QString body;
                int afterBody = extractBody(code, condEnd + 1, body);
                QDomElement whileEl = addBlock(doc, parentBranch, "pre", "cond", cond);
                QDomElement bodyBr = addBranch(doc, whileEl);
                parseBlock(body, doc, bodyBr);
                i = afterBody;
                continue;
            }
        }

        // for (...) ...
        if (isWordAt(stripped, i, "for")) {
            int parenPos = i + 3;
            while (parenPos < len && stripped[parenPos].isSpace())
                ++parenPos;
            int parenEnd = matchParen(stripped, parenPos);
            if (parenEnd >= 0) {
                QString forSpec = code.mid(parenPos + 1, parenEnd - parenPos - 1);
                // Split by ; at depth 0
                QStringList parts;
                int last = 0;
                int depth = 0;
                for (int j = 0; j < forSpec.length(); ++j) {
                    if (forSpec[j] == '(') ++depth;
                    else if (forSpec[j] == ')') --depth;
                    else if (forSpec[j] == ';' && depth == 0) {
                        parts << forSpec.mid(last, j - last).trimmed();
                        last = j + 1;
                    }
                }
                parts << forSpec.mid(last).trimmed();
                while (parts.size() < 3) parts << QString();

                // Init statement
                if (!parts[0].isEmpty()) {
                    QString init = parts[0];
                    if (init.contains('=')) {
                        int eq = init.indexOf('=');
                        QString dest = init.left(eq).trimmed();
                        int sp = dest.lastIndexOf(' ');
                        if (sp >= 0) dest = dest.mid(sp + 1);
                        while (dest.endsWith('*')) dest.chop(1);
                        addBlock(doc, parentBranch, "assign", "dest", dest.trimmed());
                        QDomElement ae = parentBranch.lastChildElement("assign");
                        if (!ae.isNull())
                            ae.setAttribute("src", init.mid(eq + 1).trimmed());
                    } else {
                        addBlock(doc, parentBranch, "process", "text", init + ";");
                    }
                }

                QString cond = parts[1].isEmpty() ? "1" : stripLiterals(parts[1]).trimmed();
                QDomElement forEl = addBlock(doc, parentBranch, "pre", "cond", cond);
                QDomElement bodyBr = addBranch(doc, forEl);

                QString body;
                int afterBody = extractBody(code, parenEnd + 1, body);
                parseBlock(body, doc, bodyBr);

                // Increment
                if (!parts[2].isEmpty())
                    addBlock(doc, bodyBr, "process", "text", parts[2]);

                i = afterBody;
                continue;
            }
        }

        // switch (...) { ... } — collect the entire block for parseStatement
        if (isWordAt(stripped, i, "switch")) {
            int scanPos = i + 6; // past 'switch'
            while (scanPos < len && stripped[scanPos].isSpace()) ++scanPos;
            if (scanPos < len && stripped[scanPos] == '(')
                scanPos = matchParen(stripped, scanPos) + 1;
            else { ++i; continue; }
            while (scanPos < len && stripped[scanPos].isSpace()) ++scanPos;
            if (scanPos < len && stripped[scanPos] == '{') {
                int braceEnd = matchBrace(stripped, scanPos, '{', '}');
                if (braceEnd > 0) {
                    scanPos = braceEnd + 1;
                    QString switchChain = code.mid(i, scanPos - i);
                    parseStatement(switchChain, doc, parentBranch);
                    i = scanPos;
                    continue;
                }
            }
        }

        // Find end of current statement at depth 0.
        int depth = 0;
        int end = i;
        int braceStart = -1;
        while (end < len) {
            if (stripped[end] == '{') {
                if (depth == 0) {
                    // Function or block definition at top level.
                    // Add the function signature as a label/process block,
                    // then parse the body separately.
                    braceStart = end;
                    int braceEnd = matchBrace(stripped, end, '{', '}');
                    if (braceEnd > 0) {
                        // Function signature (everything before the brace)
                        QString signature = code.mid(i, end - i).trimmed();
                        if (!signature.isEmpty())
                            addBlock(doc, parentBranch, "process", "text", signature);
                        // Function body
                        QString funcBody = code.mid(end + 1, braceEnd - end - 1);
                        parseBlock(funcBody, doc, parentBranch);
                        i = braceEnd + 1;
                        break;
                    }
                }
                ++depth;
                ++end;
            } else if (stripped[end] == '}') {
                --depth;
                if (depth < 0) break; // end of enclosing block
                ++end;
            } else if (stripped[end] == ';' && depth == 0) {
                ++end;
                break;
            } else {
                ++end;
            }
        }

        if (braceStart < 0) {
            // Simple statement ending with ';'
            QString stmtText = code.mid(i, end - i).trimmed();
            if (!stmtText.isEmpty())
                parseStatement(stmtText, doc, parentBranch);
            i = end;
        }

        // Skip any extra whitespace/semicolons
        while (i < len && (stripped[i].isSpace() || stripped[i] == ';'))
            ++i;
    }
}

// ---------------------------------------------------------------------------
// parse — main entry point: C source → AFCE QDomDocument
// ---------------------------------------------------------------------------
QDomDocument SourceCodeParser::parse(const QString &sourceCode, QString *errorOut)
{
    QDomDocument doc("AFC");
    QDomElement algorithm = doc.createElement("algorithm");
    doc.appendChild(algorithm);
    QDomElement mainBranch = doc.createElement("branch");
    algorithm.appendChild(mainBranch);

    if (sourceCode.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = "Source code is empty";
        return doc;
    }

    // Parse directly — parseBlock handles function signatures + bodies
    // as single top-level statements.
    parseBlock(sourceCode, doc, mainBranch);

    if (!mainBranch.hasChildNodes()) {
        if (errorOut && errorOut->isEmpty())
            *errorOut = "Could not parse any recognizable C constructs";
    }

    return doc;
}

// ---------------------------------------------------------------------------
// addBlock — helper to create a block element
// ---------------------------------------------------------------------------
QDomElement SourceCodeParser::addBlock(QDomDocument &doc, QDomElement &branch,
                                       const QString &type, const QString &attrName,
                                       const QString &attrValue)
{
    QDomElement el = doc.createElement(type);
    if (!attrName.isEmpty())
        el.setAttribute(attrName, attrValue);
    branch.appendChild(el);
    return el;
}

// ---------------------------------------------------------------------------
// addBranch — helper to create a branch element
// ---------------------------------------------------------------------------
QDomElement SourceCodeParser::addBranch(QDomDocument &doc, QDomElement &parent)
{
    QDomElement br = doc.createElement("branch");
    parent.appendChild(br);
    return br;
}
