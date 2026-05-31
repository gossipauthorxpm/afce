// ================================================================
// Псевдокод на AFCE Pseudo Language (APL) для функции dropLastUnit
// ================================================================
// Вставьте этот код через меню File → Import pseudo-language...
// и нажмите OK для построения блок-схемы.
// ================================================================

@start_scheme
  process "Проверка: *collection == NULL?";

  if (*collection == NULL) {
      process "return (коллекция пуста)";
  } else if ((*collection)->nextDelivery == NULL) {
      process "free((*collection)->delivery)";
      process "free(*collection)";
      process "*collection = NULL";
      process "return";
  } else {
      process "temp = *collection";

      while (temp->nextDelivery->nextDelivery != NULL) {
          process "temp = temp->nextDelivery";
      }

      // Освобождение памяти — длинные операции требуют пояснения
      process "Освободить память последнего узла";
      comment "free(temp->nextDelivery->delivery) и free(temp->nextDelivery)";
      process "temp->nextDelivery = NULL";
  }
@stop_scheme
