@start_scheme
  // Тест всех типов блоков ГОСТ 19.701-90
  process "Прямоугольник (процесс)";
  call "Подпрограмма()";
  assign result = 42;
  input x, y;
  output "Результат: " + result;

  // Символы данных (3.1)
  data "Данные (параллелограмм)";
  stored_data "Запоминаемые данные";
  document "Документ";
  manual_input "Ручной ввод";
  display "Дисплей";

  // Символы процесса (3.2)
  manual_op "Ручная операция";
  parallel "Параллельные действия";

  // Носители данных (3.1.2)
  ram "ОЗУ";
  seq_access "Магнитная лента";
  direct_access "Магнитный диск";
  card "Перфокарта";
  paper_tape "Бумажная лента";

  // Специальные символы (3.4)
  connector "A";
  comment "Это пояснительный комментарий";

  if (x > 0) {
      process "Ветка Да";
  } else {
      process "Ветка Нет";
  }

  while (i < 10) {
      process "Тело while";
  }

  do {
      process "Тело do-while";
  } while (done != true);

  for (i = 0; i < n; i++) {
      process "Тело for";
  }

  ellipsis;
@stop_scheme
