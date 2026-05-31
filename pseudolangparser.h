#ifndef PSEUDOLANGPARSER_H
#define PSEUDOLANGPARSER_H

#include <QString>
#include <QDomDocument>

/// Parser for AFCE Pseudo Language (APL) — a PlantUML-like notation
/// for describing block diagrams in accordance with GOST 19.701-90.
///
/// Converts pseudo-language text into the internal AFCE XML representation,
/// which can then be rendered as a flowchart.
///
/// Syntax overview:
/// @start_scheme
///   process "text";            — действие (прямоугольник)
///   input vars;                — ввод данных (параллелограмм)
///   output vars;               — вывод данных (параллелограмм)
///   assign dest = src;         — присваивание
///   if (cond) { ... } else { ... }  — ветвление (ромб)
///   while (cond) { ... }       — цикл с предусловием (ромб)
///   do { ... } while (cond);   — цикл с постусловием (ромб)
///   for (init; cond; inc) { ... }   — цикл for (шестиугольник)
///   call "function()";         — вызов подпрограммы
///   comment "text";            // комментарий по ГОСТ 19.701-90 п.3.4.3
///   data "text";               // данные (параллелограмм) ГОСТ 3.1.1.1
///   stored_data "text";        // запоминаемые данные (шестиугольник вогнутый) ГОСТ 3.1.1.2
///   document "text";           // документ (прямоугольник с волнистым низом) ГОСТ 3.1.2.4
///   manual_input "text";       // ручной ввод (трапеция) ГОСТ 3.1.2.5
///   display "text";            // дисплей (монитор) ГОСТ 3.1.2.8
///   manual_op "text";          // ручная операция (трапеция книзу) ГОСТ 3.2.2.2
///   parallel "text";           // параллельные действия (двойная черта) ГОСТ 3.2.2.5
///   connector "label";         // соединитель (кружок) ГОСТ 3.4.1
///   ellipsis;                   // пропуск (три точки) ГОСТ 3.4.4
///   ram "text";                // ОЗУ (прямоугольник с перегородкой) ГОСТ 3.1.2.1
///   seq_access "text";         // последовательный доступ (катушка) ГОСТ 3.1.2.2
///   direct_access "text";      // прямой доступ (цилиндр) ГОСТ 3.1.2.3
///   card "text";               // карта (прямоугольник со срезом) ГОСТ 3.1.2.6
///   paper_tape "text";         // бумажная лента (волнистая) ГОСТ 3.1.2.7
///   // comment                 // строчный комментарий
/// @stop_scheme
class PseudoLangParser
{
public:
    /// Parse pseudo-language text and produce AFCE flowchart XML
    static QDomDocument parse(const QString &source, QString *errorOut = nullptr);

private:
    // Strip comments from source
    static QString stripComments(const QString &source);

    // Find matching closing brace starting from openPos
    static int findMatchingBrace(const QString &source, int openPos);

    // Parse a block of statements between braces
    static bool parseBlock(const QString &source, int &pos, QDomDocument &doc,
                           QDomElement &parentBranch, QString *errorOut);

    // Parse a single statement at the given position
    static bool parseStatement(const QString &source, int &pos, QDomDocument &doc,
                               QDomElement &parentBranch, QString *errorOut);

    // Helper: skip whitespace and comments
    static void skipWhitespace(const QString &source, int &pos);

    // Helper: check if a keyword starts at position pos
    static bool keywordAt(const QString &source, int pos, const char *kw);

    // Helper: return the rest of the line (for error messages)
    static QString restOfLine(const QString &source, int pos);
};

#endif // PSEUDOLANGPARSER_H
