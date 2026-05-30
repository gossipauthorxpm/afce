#ifndef SOURCECODEPARSER_H
#define SOURCECODEPARSER_H

#include <QString>
#include <QDomDocument>

class SourceCodeParser
{
public:
    /// Parse C source code and produce AFCE flowchart XML
    static QDomDocument parse(const QString &sourceCode, QString *errorOut = nullptr);

private:
    // Remove comments and string literals (replace with spaces to keep positions)
    static QString stripLiterals(const QString &code);

    // Find matching closing brace/paren starting from openPos
    static int matchBrace(const QString &code, int openPos, QChar open, QChar close);
    static int matchParen(const QString &code, int openPos);

    // Extract a top-level keyword construct and its parenthesized condition
    // Returns the position right after the condition's closing paren
    static int extractConditionEnd(const QString &code, int startPos);

    // Check if a word starts at position pos (word boundary before)
    static bool isWordAt(const QString &code, int pos, const char *word);

    // Find the body of a compound statement (either {...} or a single statement)
    // Returns end position (exclusive), stores body text in bodyOut
    static int extractBody(const QString &code, int startPos, QString &bodyOut);

    // Parse a block of statements (function body, if body, loop body, etc.)
    static void parseBlock(const QString &code, QDomDocument &doc, QDomElement &parentBranch);

    // Parse a single statement and add it to the branch
    static bool parseStatement(const QString &stmt, QDomDocument &doc, QDomElement &branch);

    // Helper: create a block element and append to branch
    static QDomElement addBlock(QDomDocument &doc, QDomElement &branch,
                                const QString &type, const QString &attrName = QString(),
                                const QString &attrValue = QString());

    // Helper: create a branch element (for if/loop bodies)
    static QDomElement addBranch(QDomDocument &doc, QDomElement &parent);
};

#endif // SOURCECODEPARSER_H
