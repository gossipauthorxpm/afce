@start_scheme
  process "addTreeNode(root, data)";
  if (root == NULL) {
      comment "Дерево пусто — создаём корневой узел";
      process "Создать новый узел (malloc)";
      process "Присвоить данные, left=right=NULL";
      process "return createTreeNode(data)";
  } else {
      if (data.id < root->data.id) {
          comment "ID меньше — вставка в левое поддерево";
          process "root->left = addTreeNode(root->left, data)";
      } else if (data.id > root->data.id) {
          comment "ID больше — вставка в правое поддерево";
          process "root->right = addTreeNode(root->right, data)";
      } else {
          comment "ID совпадает — дубликат не добавляется";
          process "Вывести предупреждение: ID уже существует";
      }
      process "return root";
  }
@stop_scheme
