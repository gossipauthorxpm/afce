# AFCE — Algorithm Flowchart Editor

AFCE is an algorithm flowchart editor with code generation and vector graphics. It has a simple graphical user interface written on Qt. This software is cross-platform and can be built under Microsoft Windows, GNU/Linux and MacOS.

AFCE allows you to create, edit, print and export flowcharts easily in a few minutes. Flowcharts can be exported to several graphical formats including SVG and PNG.

Also AFCE can generate source code based on the flowchart.

---

## 🇷🇺 AFCE Pseudo Language (APL) — текстовый псевдоязык для блок-схем

В проект добавлен **парсер псевдоязыка APL** (AFCE Pseudo Language) — PlantUML-подобного текстового формата для описания блок-схем, соответствующих **ГОСТ 19.701-90**.

### Как использовать

1. Нажмите кнопку **«Псевдоязык»** на панели инструментов (или *File → Import pseudo-language...*).
2. Вставьте APL-код в текстовое поле.
3. Нажмите OK — блок-схема построится автоматически.

### Пример APL-кода

```apl
@start_scheme
  input n;
  assign result = 1;

  if (n < 0) {
      output "Ошибка";
  } else {
      for (i = 1; i <= n; i++) {
          assign result = result * i;
      }
      output "Факториал: " + result;
  }
@stop_scheme
```

Полная документация — в файле [`PSEUDOLANG.md`](PSEUDOLANG.md).

### Типы блоков

| APL-команда | Элемент схемы | ГОСТ 19.701-90 |
|-------------|---------------|----------------|
| `process "текст"` | Прямоугольник | 3.2.1.1 Процесс |
| `call "func()"` | Прямоугольник с полосками | 3.2.2.1 Предопределённый процесс |
| `assign x = expr` | Прямоугольник с `:=` | 3.2.1.1 |
| `input vars` | Параллелограмм ввода | 3.1.2.5 Ручной ввод |
| `output vars` | Параллелограмм вывода | 3.1.2.8 Дисплей |
| `if (cond) { } else { }` | Ромб (Да/Нет) | 3.2.2.4 Решение |
| `while (cond) { }` | Ромб с возвратом | 3.2.2.6 Граница цикла |
| `do { } while (cond)` | Ромб с возвратом | 3.2.2.6 Граница цикла |
| `for (init; cond; inc) { }` | Ромб с возвратом | 3.2.2.6 Граница цикла |

### Импорт C-кода

Также доступен импорт C/C++ исходников (*File → Import C source...*). Парсер автоматически распознаёт:
- `if / else if / else`
- `while`, `do { } while`
- `for`
- `switch / case` (конвертируется в цепочку if-else)
- `scanf` → ввод, `printf` → вывод
- Присваивания
- Вызовы функций

---

## Что добавлено / изменено в этой версии

- **Псевдоязык APL** — новый парсер `pseudolangparser.cpp` / `.h`, преобразующий PlantUML-подобный текст в XML блок-схемы.
- **Документация APL** — [`PSEUDOLANG.md`](PSEUDOLANG.md) с полным описанием синтаксиса, примеров и соответствия ГОСТ.
- **Agent skill** — скилл для AI-агентов в `.agents/skills/afce-pseudo-language/`, позволяющий генерировать APL-код в контексте любых проектов.
- **Кнопка на тулбаре** — «Псевдоязык» на панели инструментов для быстрого вызова.
- **Пример** — `examples/dropLastUnit.apl` с APL-кодом удаления последнего элемента связного списка.
- **Русификация схем** — надписи BEGIN/END заменены на Начало/Конец, Yes/No на Да/Нет в соответствии с российской традицией ГОСТ.

---

## Build

```bash
qmake6 afce.pro
make -j$(nproc)
./afce
```

## Dependencies

- Qt 6 (widgets, xml, svg, printsupport)
- g++ (C++17)

## License

GNU General Public License version 2.0 or 3.0.
