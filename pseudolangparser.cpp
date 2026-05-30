#include "pseudolangparser.h"
#include <QRegularExpression>
#include <QDebug>

// ---------------------------------------------------------------------------
// stripComments — removes // line comments from source
// ---------------------------------------------------------------------------
QString PseudoLangParser::stripComments(const QString &source)
{
    QString result = source;
    // Remove // line comments (respecting strings)
    QRegularExpression re(R"((\"(?:[^\"\\]|\\.)*\")|//[^\n]*)");
    // Replace comments with spaces, keep string literals intact
    int pos = 0;
    while (pos < result.length()) {
        auto m = re.match(result, pos);
        if (!m.hasMatch()) break;
        QString captured = m.captured();
        if (captured.startsWith("//")) {
            // Replace comment with spaces to preserve line numbers
            for (int i = m.capturedStart(); i < m.capturedEnd(); ++i)
                result[i] = ' ';
        }
        pos = m.capturedEnd();
    }
    return result;
}

// ---------------------------------------------------------------------------
// skipWhitespace — advance pos past whitespace
// ---------------------------------------------------------------------------
void PseudoLangParser::skipWhitespace(const QString &source, int &pos)
{
    while (pos < source.length() && source[pos].isSpace())
        ++pos;
}

// ---------------------------------------------------------------------------
// keywordAt — check if a keyword appears at position pos (word boundary)
// ---------------------------------------------------------------------------
bool PseudoLangParser::keywordAt(const QString &source, int pos, const char *kw)
{
    QString keyword = QString::fromLatin1(kw);
    if (pos + keyword.length() > source.length())
        return false;
    if (QStringView(source).mid(pos, keyword.length()).compare(keyword, Qt::CaseInsensitive) != 0)
        return false;
    // Check word boundary after keyword
    int after = pos + keyword.length();
    if (after < source.length() && (source[after].isLetterOrNumber() || source[after] == '_'))
        return false;
    // Check word boundary before keyword
    if (pos > 0 && (source[pos - 1].isLetterOrNumber() || source[pos - 1] == '_'))
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// restOfLine — for error messages
// ---------------------------------------------------------------------------
QString PseudoLangParser::restOfLine(const QString &source, int pos)
{
    int end = pos;
    while (end < source.length() && source[end] != '\n')
        ++end;
    return source.mid(pos, end - pos).trimmed();
}

// ---------------------------------------------------------------------------
// findMatchingBrace — find the matching '}' for a '{' at openPos
// ---------------------------------------------------------------------------
int PseudoLangParser::findMatchingBrace(const QString &source, int openPos)
{
    if (openPos >= source.length() || source[openPos] != '{')
        return -1;
    int depth = 1;
    int pos = openPos + 1;
    while (pos < source.length() && depth > 0) {
        if (source[pos] == '{')
            ++depth;
        else if (source[pos] == '}')
            --depth;
        if (depth > 0)
            ++pos;
    }
    return (depth == 0) ? pos : -1;
}

// ---------------------------------------------------------------------------
// parseStatement — parse one statement, advance pos past it
// ---------------------------------------------------------------------------
bool PseudoLangParser::parseStatement(const QString &source, int &pos,
                                      QDomDocument &doc, QDomElement &parentBranch,
                                      QString *errorOut)
{
    skipWhitespace(source, pos);
    if (pos >= source.length())
        return false;

    // --- process "text" ---
    if (keywordAt(source, pos, "process")) {
        pos += 7;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '"') {
            if (errorOut) *errorOut = "Expected quoted string after 'process'";
            return false;
        }
        // Find closing quote
        int start = pos + 1;
        int end = start;
        while (end < source.length() && source[end] != '"') {
            if (source[end] == '\\') ++end; // skip escaped char
            ++end;
        }
        if (end >= source.length()) {
            if (errorOut) *errorOut = "Unterminated string in 'process'";
            return false;
        }
        QString text = source.mid(start, end - start);
        pos = end + 1;
        // Expect ';'
        skipWhitespace(source, pos);
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        QDomElement el = doc.createElement("process");
        el.setAttribute("text", text.trimmed());
        parentBranch.appendChild(el);
        return true;
    }

    // --- input vars ---
    if (keywordAt(source, pos, "input")) {
        pos += 5;
        skipWhitespace(source, pos);
        int end = pos;
        while (end < source.length() && source[end] != ';')
            ++end;
        QString vars = source.mid(pos, end - pos).trimmed();
        pos = end;
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        QDomElement el = doc.createElement("io");
        el.setAttribute("vars", vars);
        parentBranch.appendChild(el);
        return true;
    }

    // --- output vars ---
    if (keywordAt(source, pos, "output")) {
        pos += 6;
        skipWhitespace(source, pos);
        int end = pos;
        while (end < source.length() && source[end] != ';')
            ++end;
        QString vars = source.mid(pos, end - pos).trimmed();
        pos = end;
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        QDomElement el = doc.createElement("ou");
        el.setAttribute("vars", vars);
        parentBranch.appendChild(el);
        return true;
    }

    // --- assign dest = src ---
    if (keywordAt(source, pos, "assign")) {
        pos += 6;
        skipWhitespace(source, pos);
        // Read until ';' — find the '=' sign
        int end = pos;
        while (end < source.length() && source[end] != ';')
            ++end;
        QString assignment = source.mid(pos, end - pos).trimmed();
        pos = end;
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        int eqPos = assignment.indexOf('=');
        if (eqPos < 0) {
            if (errorOut) *errorOut = "'assign' requires '=' sign";
            return false;
        }
        QString dest = assignment.left(eqPos).trimmed();
        QString src = assignment.mid(eqPos + 1).trimmed();

        QDomElement el = doc.createElement("assign");
        el.setAttribute("dest", dest);
        el.setAttribute("src", src);
        parentBranch.appendChild(el);
        return true;
    }

    // --- if (cond) { ... } [ else { ... } ] ---
    if (keywordAt(source, pos, "if")) {
        pos += 2;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '(') {
            if (errorOut) *errorOut = "Expected '(' after 'if'";
            return false;
        }
        // Find matching )
        int parenEnd = pos;
        int depth = 0;
        while (parenEnd < source.length()) {
            if (source[parenEnd] == '(') ++depth;
            else if (source[parenEnd] == ')') { --depth; if (depth == 0) break; }
            ++parenEnd;
        }
        if (parenEnd >= source.length()) {
            if (errorOut) *errorOut = "Unterminated '(' in 'if'";
            return false;
        }
        QString cond = source.mid(pos + 1, parenEnd - pos - 1).trimmed();
        pos = parenEnd + 1;

        QDomElement ifBlock = doc.createElement("if");
        ifBlock.setAttribute("cond", cond);
        parentBranch.appendChild(ifBlock);

        QDomElement thenBranch = doc.createElement("branch");
        ifBlock.appendChild(thenBranch);
        QDomElement elseBranch = doc.createElement("branch");
        ifBlock.appendChild(elseBranch);

        // Parse then-branch body (expect '{')
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '{') {
            if (errorOut) *errorOut = "Expected '{' after 'if (condition)'";
            return false;
        }
        int closeBrace = findMatchingBrace(source, pos);
        if (closeBrace < 0) {
            if (errorOut) *errorOut = "Unterminated '{' in 'if' body";
            return false;
        }
        int bodyStart = pos + 1;
        int bodyEnd = closeBrace;
        // Parse then body
        int tmpPos = bodyStart;
        parseBlock(source, tmpPos, doc, thenBranch, errorOut);
        pos = closeBrace + 1;

        // Check for 'else'
        skipWhitespace(source, pos);
        if (keywordAt(source, pos, "else")) {
            pos += 4;
            skipWhitespace(source, pos);
            // Check for 'else if'
            if (keywordAt(source, pos, "if")) {
                // Parse the nested if directly into the else branch
                parseStatement(source, pos, doc, elseBranch, errorOut);
            } else {
                // Simple else
                if (pos >= source.length() || source[pos] != '{') {
                    if (errorOut) *errorOut = "Expected '{' after 'else'";
                    return false;
                }
                closeBrace = findMatchingBrace(source, pos);
                if (closeBrace < 0) {
                    if (errorOut) *errorOut = "Unterminated '{' in 'else' body";
                    return false;
                }
                bodyStart = pos + 1;
                bodyEnd = closeBrace;
                tmpPos = bodyStart;
                parseBlock(source, tmpPos, doc, elseBranch, errorOut);
                pos = closeBrace + 1;
            }
        }
        return true;
    }

    // --- while (cond) { ... } ---
    if (keywordAt(source, pos, "while")) {
        pos += 5;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '(') {
            if (errorOut) *errorOut = "Expected '(' after 'while'";
            return false;
        }
        int parenEnd = pos;
        int depth = 0;
        while (parenEnd < source.length()) {
            if (source[parenEnd] == '(') ++depth;
            else if (source[parenEnd] == ')') { --depth; if (depth == 0) break; }
            ++parenEnd;
        }
        if (parenEnd >= source.length()) {
            if (errorOut) *errorOut = "Unterminated '(' in 'while'";
            return false;
        }
        QString cond = source.mid(pos + 1, parenEnd - pos - 1).trimmed();
        pos = parenEnd + 1;

        // Check if this is a do-while (we've already consumed 'while', so we need
        // to check if the previous statement was 'do' — but we handle do above.
        // If we're here, it's a pre-check loop.
        QDomElement preBlock = doc.createElement("pre");
        preBlock.setAttribute("cond", cond);
        parentBranch.appendChild(preBlock);

        QDomElement bodyBranch = doc.createElement("branch");
        preBlock.appendChild(bodyBranch);

        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '{') {
            if (errorOut) *errorOut = "Expected '{' after 'while (condition)'";
            return false;
        }
        int closeBrace = findMatchingBrace(source, pos);
        if (closeBrace < 0) {
            if (errorOut) *errorOut = "Unterminated '{' in 'while' body";
            return false;
        }
        int tmpPos = pos + 1;
        parseBlock(source, tmpPos, doc, bodyBranch, errorOut);
        pos = closeBrace + 1;
        return true;
    }

    // --- do { ... } while (cond); ---
    if (keywordAt(source, pos, "do")) {
        pos += 2;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '{') {
            if (errorOut) *errorOut = "Expected '{' after 'do'";
            return false;
        }
        int closeBrace = findMatchingBrace(source, pos);
        if (closeBrace < 0) {
            if (errorOut) *errorOut = "Unterminated '{' in 'do' body";
            return false;
        }
        QString body = source.mid(pos + 1, closeBrace - pos - 1);
        pos = closeBrace + 1;

        skipWhitespace(source, pos);
        if (!keywordAt(source, pos, "while")) {
            if (errorOut) *errorOut = "Expected 'while' after 'do { ... }'";
            return false;
        }
        pos += 5;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '(') {
            if (errorOut) *errorOut = "Expected '(' after 'do ... while'";
            return false;
        }
        int parenEnd = pos;
        int depth = 0;
        while (parenEnd < source.length()) {
            if (source[parenEnd] == '(') ++depth;
            else if (source[parenEnd] == ')') { --depth; if (depth == 0) break; }
            ++parenEnd;
        }
        if (parenEnd >= source.length()) {
            if (errorOut) *errorOut = "Unterminated '(' in 'do ... while'";
            return false;
        }
        QString cond = source.mid(pos + 1, parenEnd - pos - 1).trimmed();
        pos = parenEnd + 1;
        // Expect ';'
        skipWhitespace(source, pos);
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        QDomElement postBlock = doc.createElement("post");
        postBlock.setAttribute("cond", cond);
        parentBranch.appendChild(postBlock);

        QDomElement bodyBranch = doc.createElement("branch");
        postBlock.appendChild(bodyBranch);

        int tmpPos = 0;
        parseBlock(body, tmpPos, doc, bodyBranch, errorOut);
        return true;
    }

    // --- for (init; cond; inc) { ... } ---
    if (keywordAt(source, pos, "for")) {
        pos += 3;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '(') {
            if (errorOut) *errorOut = "Expected '(' after 'for'";
            return false;
        }
        int parenEnd = pos;
        int depth = 0;
        while (parenEnd < source.length()) {
            if (source[parenEnd] == '(') ++depth;
            else if (source[parenEnd] == ')') { --depth; if (depth == 0) break; }
            ++parenEnd;
        }
        if (parenEnd >= source.length()) {
            if (errorOut) *errorOut = "Unterminated '(' in 'for'";
            return false;
        }
        QString forSpec = source.mid(pos + 1, parenEnd - pos - 1);
        pos = parenEnd + 1;

        // Split init; cond; inc
        QStringList parts;
        int last = 0;
        depth = 0;
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

        // Init statement
        if (!parts[0].isEmpty()) {
            QString init = parts[0];
            int eqPos = init.indexOf('=');
            if (eqPos >= 0) {
                QString dest = init.left(eqPos).trimmed();
                int sp = dest.lastIndexOf(' ');
                if (sp >= 0) dest = dest.mid(sp + 1);
                while (dest.endsWith('*')) dest.chop(1);
                dest = dest.trimmed();
                QDomElement ae = doc.createElement("assign");
                ae.setAttribute("dest", dest);
                ae.setAttribute("src", init.mid(eqPos + 1).trimmed());
                parentBranch.appendChild(ae);
            } else {
                QDomElement pe = doc.createElement("process");
                pe.setAttribute("text", init + ";");
                parentBranch.appendChild(pe);
            }
        }

        // Condition (used for the loop diamond)
        QString cond = parts[1].isEmpty() ? "1" : parts[1];
        QDomElement forBlock = doc.createElement("pre");
        forBlock.setAttribute("cond", cond);
        parentBranch.appendChild(forBlock);

        QDomElement bodyBranch = doc.createElement("branch");
        forBlock.appendChild(bodyBranch);

        // Body
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '{') {
            if (errorOut) *errorOut = "Expected '{' after 'for (...)'";
            return false;
        }
        int closeBrace = findMatchingBrace(source, pos);
        if (closeBrace < 0) {
            if (errorOut) *errorOut = "Unterminated '{' in 'for' body";
            return false;
        }
        int tmpPos = pos + 1;
        parseBlock(source, tmpPos, doc, bodyBranch, errorOut);

        // Increment (add at end of body)
        if (!parts[2].isEmpty()) {
            QDomElement incEl = doc.createElement("process");
            incEl.setAttribute("text", parts[2]);
            bodyBranch.appendChild(incEl);
        }

        pos = closeBrace + 1;
        return true;
    }

    // --- call "function" ---
    if (keywordAt(source, pos, "call")) {
        pos += 4;
        skipWhitespace(source, pos);
        if (pos >= source.length() || source[pos] != '"') {
            if (errorOut) *errorOut = "Expected quoted string after 'call'";
            return false;
        }
        int start = pos + 1;
        int end = start;
        while (end < source.length() && source[end] != '"') {
            if (source[end] == '\\') ++end;
            ++end;
        }
        if (end >= source.length()) {
            if (errorOut) *errorOut = "Unterminated string in 'call'";
            return false;
        }
        QString text = source.mid(start, end - start);
        pos = end + 1;
        // Expect ';'
        skipWhitespace(source, pos);
        if (pos < source.length() && source[pos] == ';')
            ++pos;

        QDomElement el = doc.createElement("predefined");
        el.setAttribute("text", text.trimmed());
        parentBranch.appendChild(el);
        return true;
    }

    // Unknown statement — skip until ';' or end of line for error
    {
        QString rest = restOfLine(source, pos);
        if (errorOut && !rest.isEmpty())
            *errorOut = QString("Unknown statement: '%1'").arg(rest);
        // Skip to next line or semicolon
        while (pos < source.length() && source[pos] != ';' && source[pos] != '\n')
            ++pos;
        if (pos < source.length() && source[pos] == ';')
            ++pos;
        return false;
    }
}

// ---------------------------------------------------------------------------
// parseBlock — parse statements until end of block
// ---------------------------------------------------------------------------
bool PseudoLangParser::parseBlock(const QString &source, int &pos,
                                  QDomDocument &doc, QDomElement &parentBranch,
                                  QString *errorOut)
{
    while (pos < source.length()) {
        skipWhitespace(source, pos);
        if (pos >= source.length())
            break;
        // End of block
        if (source[pos] == '}')
            break;
        // End of source marker
        if (source[pos] == '@') {
            // Check for @stop_scheme
            if (keywordAt(source, pos, "@stop_scheme"))
                break;
            // Skip other directives
            int end = pos;
            while (end < source.length() && source[end] != '\n')
                ++end;
            pos = end;
            continue;
        }
        parseStatement(source, pos, doc, parentBranch, errorOut);
    }
    return true;
}

// ---------------------------------------------------------------------------
// parse — main entry point
// ---------------------------------------------------------------------------
QDomDocument PseudoLangParser::parse(const QString &source, QString *errorOut)
{
    QDomDocument doc("AFC");
    QDomElement algorithm = doc.createElement("algorithm");
    doc.appendChild(algorithm);
    QDomElement mainBranch = doc.createElement("branch");
    algorithm.appendChild(mainBranch);

    if (source.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = "Source is empty";
        return doc;
    }

    QString cleaned = stripComments(source);

    // Find @start_scheme marker
    int pos = 0;
    while (pos < cleaned.length()) {
        skipWhitespace(cleaned, pos);
        if (keywordAt(cleaned, pos, "@start_scheme")) {
            pos += 13;
            break;
        }
        ++pos;
    }

    if (pos >= cleaned.length() && !cleaned.contains("@start_scheme")) {
        // No @start_scheme marker — try to parse as raw pseudo-language
        pos = 0;
    }

    // Parse statements until @stop_scheme or end
    parseBlock(cleaned, pos, doc, mainBranch, errorOut);

    if (!mainBranch.hasChildNodes() && errorOut && errorOut->isEmpty()) {
        *errorOut = "No valid pseudo-language statements found";
    }

    return doc;
}
