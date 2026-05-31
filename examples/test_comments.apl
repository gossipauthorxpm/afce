// ================================================================
// Тест комментариев по ГОСТ 19.701-90 п.3.4.3
// ================================================================

@start_scheme
  process "addTreeNode(root, data)";

  if (root == NULL) {
      // Комментарий ПЕРЕД блоком
      comment "Дерево пустое — создаём корневой узел";
      process "return createTreeNode(data)";
  } else if (data.id < root->data.id) {
      comment "ID меньше — добавляем в левое поддерево";
      process "root->left = addTreeNode(root->left, data)";
  } else if (data.id > root->data.id) {
      comment "ID больше — добавляем в правое поддерево";
      process "root->right = addTreeNode(root->right, data)";
  } else {
      comment "ID совпадает — не добавляем дубликат";
      process "ID совпадает — не добавляем дубликат";
  }

  process "return root";
@stop_scheme
