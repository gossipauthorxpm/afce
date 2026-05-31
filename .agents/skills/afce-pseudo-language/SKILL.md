---
name: afce-pseudo-language
description: Generate AFCE Pseudo Language (APL) code — a PlantUML-like notation for describing block diagrams that comply with GOST 19.701-90. Use this when a user wants to visualize a function or algorithm as a flowchart suitable for the AFCE editor.
---

# AFCE Pseudo Language (APL) Skill

Use this skill when you need to produce a textual representation of a block diagram (flowchart) that can be imported into the **AFCE** (Algorithm Flowchart Editor) application. The output is a text file with `.apl` extension using the AFCE Pseudo Language syntax.

## When to Use

- A user asks you to "draw a block diagram" or "make a flowchart" for a piece of code.
- A user wants to visualize an algorithm in **GOST 19.701-90** notation.
- A user mentions AFCE, `pseudolangparser`, or the APL format.
- A user wants to convert a C/C++ function into a flowchart description.

## Quick Reference

APL syntax is inspired by PlantUML but uses C-style braces `{ }` and requires semicolons `;` after each statement.

```
@start_scheme
  process "action text";
  input variable1, variable2;
  output result;
  assign x = expression;
  if (condition) {
      process "then-branch";
  } else {
      process "else-branch";
  }
  while (condition) {
      process "loop body";
  }
  do {
      process "loop body";
  } while (condition);
  for (init; condition; increment) {
      process "loop body";
  }
  call "function_name()";
  comment "пояснительный текст";
  data "Данные";
  stored_data "Запоминаемые данные";
  document "Документ";
  manual_input "Ручной ввод";
  display "Дисплей";
  manual_op "Ручная операция";
  parallel "Параллельные действия";
  connector "A";
  ellipsis;
  ram "ОЗУ";
  seq_access "Лента";
  direct_access "Диск";
  card "Карта";
  paper_tape "Лента";
@stop_scheme
```

## Rule: Always produce `.apl` files

When generating APL code, always produce it in a file with `.apl` extension. Include both the raw `.apl` file AND a readable description of the resulting flowchart.

## Block Types (GOST 19.701-90)

### Символы процесса (Process symbols, §3.2)

| Command | Shape | GOST | Purpose |
|---------|-------|------|---------|
| `process "text"` | Rectangle | 3.2.1.1 | General action / operation |
| `call "func()"` | Rectangle with side bars | 3.2.2.1 | Subroutine / predefined process |
| `assign x = expr` | Rectangle with `:=` | 3.2.1.1 | Variable assignment |
| `if (cond) { } else { }` | Diamond | 3.2.2.4 | Branching — labeled "Да"/"Нет" |
| `while (cond) { }` | Diamond with loop-back | 3.2.2.6 | Pre-condition loop |
| `do { } while (cond)` | Diamond with loop-back | 3.2.2.6 | Post-condition loop |
| `for (i; c; i++) { }` | Hexagon with loop-back | 3.2.2.6 | Counter loop |
| `manual_op "text"` | Trapezoid (tapering down) | 3.2.2.2 | Manual operation (human-operated) |
| `parallel "text"` | Double-bar | 3.2.2.5 | Parallel actions / fork-join |

### Символы данных (Data symbols, §3.1)

| Command | Shape | GOST | Purpose |
|---------|-------|------|---------|
| `input vars` | Parallelogram (slanted) | 3.1.2.5 | Data input (keyboard, etc.) |
| `output vars` | Parallelogram (slanted) | 3.1.2.8 | Data output (display, etc.) |
| `data "text"` | Parallelogram (right-slant) | 3.1.1.1 | General data |
| `stored_data "text"` | Hexagon (concave) | 3.1.1.2 | Stored data (database) |
| `document "text"` | Rectangle with wavy bottom | 3.1.2.4 | Document / report |
| `manual_input "text"` | Trapezoid (slanted top) | 3.1.2.5 | Manual input (keyboard) |
| `display "text"` | Parallelogram | 3.1.2.8 | Display / screen output |
| `ram "text"` | Rectangle with partition | 3.1.2.1 | RAM (random-access memory) |
| `seq_access "text"` | Circle with tail (tape) | 3.1.2.2 | Sequential access (magnetic tape) |
| `direct_access "text"` | Cylinder (disk) | 3.1.2.3 | Direct access (magnetic disk) |
| `card "text"` | Rectangle with cut corner | 3.1.2.6 | Punch card |
| `paper_tape "text"` | Rectangle with wavy edges | 3.1.2.7 | Paper tape |

### Специальные символы (Special symbols, §3.4)

| Command | Shape | GOST | Purpose |
|---------|-------|------|---------|
| `comment "text"` | Bracket `[` + dashed line | 3.4.3 | Annotation / пояснение |
| `connector "label"` | Circle | 3.4.1 | Connector (page/section break) |
| `ellipsis` | Three dots `...` | 3.4.4 | Omission / continuation marker |

## Syntax Rules

1. Every statement must end with `;`.
2. Block bodies use `{ }` — always required for `if`, `else`, `while`, `do`, `for`.
3. Line comments are `//` (single line only).
4. `comment "text";` adds a GOST 19.701-90 annotation. Inside **if/else** branches it is drawn as a bracket `[` on the side; in the **main flow** it is an inline block that breaks the vertical line.
5. Keywords are case-insensitive (`PROCESS "x"` works).
6. String literals use double quotes `"..."`.
7. The `@start_scheme` / `@stop_scheme` markers are optional but recommended.
8. `else if` chains are supported naturally: `if (a) { } else if (b) { } else { }`.

## Dynamic Block Sizing

All blocks resize automatically based on their text content:
- **process**, **assign**, **predefined**, **io**, **ou**, **data**, **stored_data**, **document**, **manual_input**, **display**, **manual_op**, **parallel**, **ram**, **seq_access**, **direct_access**, **card**, **paper_tape**: width and height adapt to text length
- **predefined** (call): text placed strictly between vertical side bars
- **comment**: in **if/else** branches — side annotation (bracket `[`, does not expand branch vertically); in **center flow** — inline block (breaks the line, occupies vertical space, centered)
- **connector** and **ellipsis**: sized to fit label
- If text is too long, add `comment "...";` per GOST 3.4.3

## Converting from C/C++ Code

When converting a function to APL:

1. Replace `if (...) { ... } else { ... }` → `if (condition) { ... } else { ... }`
2. Replace `while (...) { ... }` → `while (condition) { ... }`
3. Replace `do { ... } while (...)` → `do { ... } while (condition);`
4. Replace `for (...; ...; ...) { ... }` → `for (init; cond; inc) { ... }`
5. Replace function calls → `call "function()";`
6. Replace `scanf(...)` → `input variables;`
7. Replace `printf(...)` → `output text;`
8. Replace assignments `x = y;` → `assign x = y;`
9. Replace other statements → `process "text representation";`
10. If text is too long for a block, append `comment "explanation";` after it

Focus on the **control flow logic** — you don't need to translate every single line literally. Group related operations into descriptive `process` blocks.

## Output Format

Always produce two things:
1. **An `.apl` file** containing the APL code
2. **A short explanation** of what the flowchart does, describing each major block

## Example: Converting a C Function

If given this C function:

```c
int factorial(int n) {
    if (n < 0) return -1;
    int result = 1;
    for (int i = 1; i <= n; i++) {
        result *= i;
    }
    return result;
}
```

Produce:

```apl
@start_scheme
  input n;
  
  if (n < 0) {
      process "return -1 (error)";
  } else {
      assign result = 1;
      for (i = 1; i <= n; i++) {
          assign result = result * i;
      }
      output "Result: " + result;
  }
@stop_scheme
```

### Using comments for long text (GOST 19.701-90, p. 3.4.3):

The comment is drawn as a `[` bracket connected to the block by a dashed line:
```
[Блок] - - - -[ Текст комментария ]
```

```apl
@start_scheme
  process "Сложная операция";
  comment "Детальное описание того, что делает эта операция";
@stop_scheme
```

## Full Reference

See the bundled `PSEUDOLANG.md` file in this skill's directory for complete documentation, including all block types, real-world examples (factorial calculation, bubble sort, `dropLastUnit`), and a comparison with PlantUML.
