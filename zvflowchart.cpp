/****************************************************************************
**                                                                         **
** Copyright (C) 2009-2014 Victor Zinkevich. All rights reserved.          **
** Contact: vicking@yandex.ru                                              **
**                                                                         **
** This file is part of the Algorithm Flowchart Editor project.            **
**                                                                         **
** This file may be used under the terms of the GNU                        **
** General Public License versions 2.0 or 3.0 as published by the Free     **
** Software Foundation and appearing in the file LICENSE included in       **
** the packaging of this file.                                             **
** You can find license at http://www.gnu.org/licenses/gpl.html            **
**                                                                         **
****************************************************************************/

#include "zvflowchart.h"
#include <QtGui>
#include <QDomDocument>
#include <QApplication>

QFlowChart::QFlowChart(QWidget *pObj /* = 0 */) : QWidget(pObj), fZoom(1)
{
  fBuffer = QString();
  fTargetPoint = QInsertionPoint();
  fStatus = Display;
  fActiveBlock = nullptr;
  fRoot = new QBlock();
  root()->setFlowChart(this);
  clear();
  setZoom(1);
}

QFlowChart::~QFlowChart()
{
  clear();
}

void QFlowChart::makeUndo()
{
  QString state = toString();
  undoStack.push(state);
  redoStack.clear();
  emit modified();
}
bool QFlowChart::canUndo() const
{
  return !undoStack.isEmpty();
}

bool QFlowChart::canRedo() const
{
  return !redoStack.isEmpty();
}

bool QFlowChart::canPaste() const
{
  QDomDocument doc;
  QClipboard *clp = QApplication::clipboard();
  QString aBuffer = clp->text();
  return doc.setContent(aBuffer, false);
}

void QFlowChart::makeChanged()
{
  emit changed();
}

void QFlowChart::undo()
{
  if (!undoStack.isEmpty())
  {
    QString state = toString();
    redoStack.push(state);
    state = undoStack.pop();
    fromString(state);
    deselectAll();
    emit changed();
  }
}


void QFlowChart::redo()
{
  if (!redoStack.isEmpty())
  {
    QString state = toString();
    undoStack.push(state);
    state = redoStack.pop();
    fromString(state);
    deselectAll();
    emit changed();
  }
}


void QFlowChart::clear()
{
    deselectAll();
    root()->clear();
    root()->attributes.clear();
    root()->setType("algorithm");
    QBlock *branch = new QBlock();
    branch->setType("branch");
    branch->isBranch = true;
    root()->append(branch);
    branch->setFlowChart(root()->flowChart());

//    fDocument->clear();
//    QDomProcessingInstruction xml = fDocument->createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\" stand-alone=\"yes\"");
//    fDocument->appendChild(xml);

//    QDomElement fRoot = fDocument->createElement("algorithm");
//    fDocument->appendChild(fRoot);
//    QDomElement branch = fDocument->createElement("branch");
//    fRoot.appendChild(branch);
    emit changed();
}

void QFlowChart::selectAll()
{
  fActiveBlock = root();
  emit changed();
  update();
}

void QFlowChart::deselectAll()
{
    fActiveBlock = 0;
    emit changed();
    update();
}

void QFlowChart::paintEvent(QPaintEvent *pEvent)
{
    QPainter canvas(this);
    pEvent->accept();
    canvas.setClipRect(pEvent->rect());
    canvas.setRenderHint(QPainter::Antialiasing, true);
    paintTo(&canvas);
}

void QFlowChart::paintTo(QPainter *canvas)
{
    if (root())
    {
      root()->paint(canvas);
      if (status() == Insertion)
      {
        QFlowChartStyle st = chartStyle();
        for (int i = 0; i < insertionPoints.size(); ++i)
        {
          QInsertionPoint ip = insertionPoints.at(i);
          QPointF p = ip.point();
          canvas->setPen(QPen(st.normalForeground(), 2 * zoom()));
          canvas->setBrush(st.normalForeground());
          canvas->drawEllipse(p, 3 * zoom(), 3 * zoom());
        }
        if (!targetPoint().isNull())
        {
          QPointF p = targetPoint().point();
          canvas->setPen(QPen(st.selectedBackground(), 2 * zoom()));
          canvas->setBrush(st.selectedBackground());
          canvas->drawEllipse(p, 7 * zoom(), 7 * zoom());
        }
      }
    }
}

QDomDocument QFlowChart::document() const
{
  QDomDocument doc("AFC");
  QDomElement r = root()->xmlNode(doc);
  doc.appendChild(r);
  return doc;
}

QBlock * QFlowChart::root() const
{
  return fRoot;
}

QBlock * QFlowChart::activeBlock() const
{
  return fActiveBlock;
}

void QFlowChart::setZoom(const double aZoom)
{
  fZoom = aZoom;
  realignObjects();
  if (status() == Insertion)
  {
    fTargetPoint = QInsertionPoint();
    regeneratePoints();
  }
  emit zoomChanged(aZoom);
}

void QFlowChart::setStatus(int aStatus)
{
  fStatus = aStatus;
  if (status() == Insertion)
  {
    setMouseTracking(true);
    regeneratePoints();
    update();
  }
  else
  {
    fTargetPoint = QInsertionPoint();
    setMouseTracking(false);
  }
  emit statusChanged();
  emit changed();
}

void QFlowChart::deleteActiveBlock()
{
  if (activeBlock())
  {
    makeUndo();
    QBlock *tmp = activeBlock();
    fActiveBlock = 0;
    deleteBlock(tmp);
    emit changed();
  }
}
void QFlowChart::realignObjects()
{
  if(root())
  {
    makeBackwardCompatibility();
    root()->adjustSize(zoom());
    root()->adjustPosition(0,0);
    resize(root()->width, root()->height);
    emit changed();
    update();
  }
}

void QFlowChart::makeBackwardCompatibility() {
    if(root()) {
        root()->makeBackwardCompatibility();
    }
}


void QFlowChart::setBuffer(const QString & aBuffer)
{
  QDomDocument doc;
  if(doc.setContent(aBuffer, false))
  {
    fBuffer = aBuffer;
  }
  else
  {
    fBuffer = QString();
  }
}

double QFlowChart::zoom() const
{
  return fZoom;
}

void QFlowChart::mousePressEvent(QMouseEvent *pEvent)
{
  if (status() == Selectable)
  {
    QBlock *block = root()->blockAt(pEvent->x(), pEvent->y());
    if (block)
    {
      if (block->isActive() && ((pEvent->modifiers() & Qt::ControlModifier) != 0))
      {
        if(activeBlock()->parent)
        {
          fActiveBlock = activeBlock()->parent;
        }
        else
        {
          fActiveBlock = block;
        }
      }
      else
      {
        if(block->parent !=0 && block->isBranch)
        {
          fActiveBlock = block->parent;
        }
        else
          fActiveBlock = block;

      }
      emit changed();
      update();
    }
  }
  else if(status() == Insertion)
  {
    QPoint mp = pEvent->pos();
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    if(!ip.isNull() && !buffer().isEmpty())
    {
      QDomDocument doc;
      if(doc.setContent(buffer(), false))
      {
        QDomElement algorithm = doc.firstChildElement("algorithm");
        QBlock *branch = ip.branch();
        if(branch)
        {
          makeUndo();
          branch->insertXmlTree(ip.index(), algorithm);
          realignObjects();
          regeneratePoints();
          fActiveBlock = 0;
          emit changed();
        }
      }
    }
    if (!multiInsert()) setStatus(Selectable);
  }
}

void QFlowChart::mouseDoubleClickEvent(QMouseEvent * event)
{
  if(status() == Selectable && event->modifiers() == Qt::NoModifier)
  {
    QBlock *block = root()->blockAt(event->x(), event->y());
    if (block)
    {
      emit changed();
      emit editBlock(block);
    }
  }
}

void QFlowChart::mouseMoveEvent(QMouseEvent *pEvent)
{
  if(status() == Insertion)
  {
    QPoint mp = pEvent->pos();
    QInsertionPoint ip = getNearistPoint(mp.x(), mp.y());
    fTargetPoint = ip;
    repaint();
  }
}

QSize QFlowChart::sizeHint() const
{
  if (root())
  {
    // Never let the content size dominate the layout — scroll bars handle overflow.
    // A huge sizeHint (e.g. 5000×10000 at 500 % zoom) would starve the zoom panel.
    return QSize(qMin((int)root()->width, 1200),
                 qMin((int)root()->height, 800));
  }
  else return QSize();

}

void QFlowChart::deleteBlock(QBlock *aBlock)
{
  if (aBlock == root())
  {
    for(int i = 0; i < aBlock->items.size(); ++i)
    {
      aBlock->item(i)->clear();
    }
    realignObjects();
  }
  else if (aBlock->isBranch)
  {
    aBlock->clear();
    realignObjects();
  }
  else
  {
    delete aBlock;
    realignObjects();
  }
  emit changed();
}

QInsertionPoint QFlowChart::getNearistPoint(int x, int y) const
{
  QInsertionPoint result;
  if (insertionPoints.size() > 0)
  {
    result = insertionPoints.at(0);
    double len = calcLength(result.point(), QPointF(x, y));
    for (int i = 0; i < insertionPoints.size(); ++i)
    {
      QInsertionPoint ip = insertionPoints.at(i);
      double tmp = calcLength(ip.point(), QPointF(x, y));
      if (tmp < len)
      {
        result = ip;
        len = tmp;
      }
    }
  }
  return result;
}

void QFlowChart::regeneratePoints()
{
  insertionPoints.clear();
  if(root())
  {
    generatePoints(root());
  }
}

void QFlowChart::generatePoints(QBlock *aBlock)
{
  if(aBlock->isBranch)
  {
    double x = aBlock->x + aBlock->width / 2.0;
    for (int i = 0; i < aBlock->items.size(); ++i)
    {
      double y = aBlock->item(i)->y;
      QInsertionPoint p;
      p.setBranch(aBlock);
      p.setPoint(QPointF(x, y));
      p.setIndex(i);
      insertionPoints.append(p);
      generatePoints(aBlock->item(i));
    }
    QInsertionPoint p;
    p.setBranch(aBlock);
    if(aBlock->items.size() == 0)
      p.setPoint(QPointF(x, aBlock->y + aBlock->height / 2));
    else
      p.setPoint(QPointF(x, aBlock->y + aBlock->height));
    p.setIndex(aBlock->items.size());
    insertionPoints.append(p);
  }
  else
  {
    for (int i = 0; i < aBlock->items.size(); ++i)
    {
      generatePoints(aBlock->item(i));
    }
  }
}

double QFlowChart::calcLength(const QPointF & p1, const QPointF & p2)
{
  return (p1.x() - p2.x()) * (p1.x() - p2.x()) + (p1.y() - p2.y()) * (p1.y() - p2.y());
}
void QFlowChart::setChartStyle(const QFlowChartStyle & aStyle)
{
  fStyle = aStyle;
  emit changed();
  update();
}

void QFlowChart::drawBottomArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  QVector<QPointF> arrow;
  arrow << QPointF(aPoint.x() - aSize.width()/2.0, aPoint.y()-aSize.height()) <<
      aPoint << aPoint << QPointF(aPoint.x() + aSize.width()/2.0, aPoint.y()-aSize.height());
  canvas->drawLines(arrow);
}

QString QFlowChart::toString()
{
  return document().toString(2);
}

void QFlowChart::fromString(const QString & str)
{
  QDomDocument doc;
  if(doc.setContent(str, false))
  {
    root()->setXmlNode(doc.firstChildElement("algorithm"));
    realignObjects();
    emit changed();
  }
}

void QFlowChart::drawRightArrow(QPainter *canvas, const QPointF & aPoint, const QSizeF & aSize)
{
  QVector<QPointF> arrow;
  arrow << QPointF(aPoint.x() - aSize.width(), aPoint.y()-aSize.height()/2);
  arrow << aPoint;
  arrow << aPoint;
  arrow << QPointF(aPoint.x() - aSize.width(), aPoint.y()+aSize.height()/2);
  canvas->drawLines(arrow);
}


/******************************** QBlock ***********************************/


QBlock::QBlock()
{
  parent = 0;
  isBranch = false;
  attributes.clear();
  items.clear();
}

QBlock::~QBlock()
{
  if(parent != 0)
  {
    parent->remove(this);
    clear();
  }
}

void QBlock::makeBackwardCompatibility() {

    // it supports obsoletted attributes t1, t2, ..., t8
    // and converts to attribute vars with comma delemited values
    // versions before 0.9.7
    if(type() == "io" || type() == "ou") {
        QStringList sl;
        for(int i = 1; i <= 8; ++i) {
            QString attr = QString("t%1").arg(i);
            if(attributes.value(attr, "") != "")
                sl << attributes.value(attr, "");
            attributes.remove(attr);
        }
        if(!sl.empty()) {
            QString vars = sl.join(",");
            attributes.insert("vars", vars);
        }
    }

    if(type() == "algorithm") {
        attributes.insert("version", AFC_VERSION);
    }

    for(int i = 0; i < items.size(); ++i) {
        items.at(i)->makeBackwardCompatibility();
    }
}

//QBlock::QBlock(const QBlock & aBlock)
//{
//  x = aBlock.x;
//  y = aBlock.y;
//  width = aBlock.width;
//  height = aBlock.height;
//  branches = aBlock.branches;
//  attributes = aBlock.attributes;
//}

QBlock * QBlock::root()
{
  if (parent == 0)
  {
    return this;
  }
  else
  {
    return parent->root();
  }
}



int QBlock::index()
{

  if(parent == 0) return -1;
  else return parent->items.indexOf(this);
}

void QBlock::insert(int newIndex, QBlock *aBlock)
{
  if (aBlock->parent != 0)
  {
    aBlock->parent->remove(aBlock);
  }
  if (newIndex < 0 || newIndex >= items.size())
    items.append(aBlock);
  else
    items.insert(newIndex, aBlock);
  aBlock->parent = this;
  aBlock->setFlowChart(flowChart());
}

void QBlock::remove(QBlock *aBlock)
{
  items.removeAll(aBlock);
  aBlock->parent = 0;
  aBlock->setFlowChart(0);
}

void QBlock::append(QBlock *aBlock)
{
  insert(-1, aBlock);
}

void QBlock::deleteObject(int aIndex)
{
  QBlock *tmp = item(aIndex);
  if(tmp)
  {
    delete tmp;
  }
}


void QBlock::setItem(int aIndex, QBlock *aBlock)
{
  if(items.size() > aIndex && aIndex >= 0)
  {
    if (aBlock->parent != 0)
    {
      aBlock->parent->remove(aBlock);
    }
    QBlock *old = item(aIndex);
    old->parent = 0;
    items.replace(aIndex, aBlock);
  }
}

void QBlock::setFlowChart(QFlowChart * aFlowChart)
{
  fFlowChart = aFlowChart;
}

void QBlock::clear()
{
  while(!items.isEmpty())
  {
    deleteObject(0);
  }
  QString currentType = type();
  attributes.clear();
  setType(currentType);
  items.clear();
}

void QBlock::adjustSize(const double aZoom)
{
  double clientWidth = 0, clientHeight = 0;
  if (isBranch)
  {
    // Первый проход: собираем размеры блоков и комментариев
    double lastNonCommentWidth = 0;
    bool insideIfElse = parent && parent->type() == "if";
    for (int i = 0; i < items.size(); ++i)
    {
      item(i)->adjustSize(aZoom);
      if (item(i)->type() == "comment") {
        // Центральные комментарии (вне if/else) занимают вертикальное место
        if (!insideIfElse) {
          clientHeight += item(i)->height;
        }
        continue;
      }
      if (clientWidth < item(i)->width) clientWidth = item(i)->width;
      clientHeight += item(i)->height;
      lastNonCommentWidth = item(i)->width;
    }
    // Второй проход: учитываем ширину комментариев ТОЛЬКО для if/else-ветвей
    // (на стороне — скобка расширяет ветку)
    for (int i = 0; i < items.size(); ++i)
    {
      if (item(i)->type() != "comment") continue;
      if (!insideIfElse) continue; // центральные не расширяют
      QBlock *target = nullptr;
      for (int j = i + 1; j < items.size(); ++j) {
        if (items.at(j)->type() != "comment") { target = items.at(j); break; }
      }
      if (!target) {
        for (int j = i - 1; j >= 0; --j) {
          if (items.at(j)->type() != "comment") { target = items.at(j); break; }
        }
      }
      if (!target) continue;
      double needed = target->width + 2 * (8 * aZoom + item(i)->width);
      if (needed > clientWidth) clientWidth = needed;
    }
    double minWidth = 180 * aZoom;
    double minHeight = 16 * aZoom;
    if (clientHeight < minHeight) clientHeight = minHeight;
    if (clientWidth < minWidth) clientWidth = minWidth;
    height = clientHeight;
    width = clientWidth;

  }
  else
  {
    for (int i = 0; i < items.size(); ++i)
    {

      item(i)->adjustSize(aZoom);
      if (clientHeight < item(i)->height) clientHeight = item(i)->height;
      clientWidth += item(i)->width;
    }
    /* поля по умолчанию */
    topMargin = 60 * aZoom;
    bottomMargin = 60 * aZoom;
    leftMargin = 10 * aZoom;
    rightMargin = 10 * aZoom;
    if (type() == "algorithm")
    {
      topMargin = 40 * aZoom;
      bottomMargin = 50 * aZoom;
    }
    else if (type() == "process" || type() == "assign")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;

      // Динамический размер на основе длины текста
      QString text;
      if (type() == "assign")
        text = QString("%1 := %2").arg(attributes.value("dest", ""), attributes.value("src", ""));
      else
        text = attributes.value("text", "");

      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      // Ширина текстовой области внутри блока (с учётом отступов 4px с каждой стороны)
      double availTextW = qMax(112.0 * aZoom, textWidth);
      int cpl = qMax(1, (int)(availTextW / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = qMax(textWidth + 16.0 * aZoom, 120.0 * aZoom);
      clientHeight = qMax(lines * lineH + 16.0 * aZoom, 60.0 * aZoom);
    }
    else if (type() == "predefined")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;

      // Функциональный блок (с перемычками): текст должен помещаться между ними
      QString text = attributes.value("text", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      // Текст рисуется между вертикальными перемычками (barOffset = b/6)
      // Доступная ширина = b - 2*b/6 = 2*b/3
      // Минимальная ширина clientWidth, чтобы текст вмещался между перемычками
      double minWForText = qMax(120.0 * aZoom, (textWidth + 8.0 * aZoom) * 1.5);
      double availTextW = minWForText * 2.0 / 3.0 - 8.0 * aZoom;
      int cpl = qMax(1, (int)(availTextW / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = minWForText;
      clientHeight = qMax(lines * lineH + 16.0 * aZoom, 60.0 * aZoom);
    }
    else if (type() == "io")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 20 * aZoom;
      rightMargin = 20 * aZoom;

      // Параллелограмм ввода: текст располагается с учётом скоса a/4
      QString text = attributes.value("vars", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      // В параллелограмме скос равен a/4, доступная ширина ≈ b - a/2
      int cpl = qMax(1, (int)((120.0 * aZoom - 30.0 * aZoom) / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = qMax(textWidth + 16.0 * aZoom, 120.0 * aZoom);
      clientHeight = qMax(lines * lineH + 16.0 * aZoom, 60.0 * aZoom);
    }
    else if (type() == "ou")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 20 * aZoom;
      rightMargin = 20 * aZoom;

      // Параллелограмм вывода
      QString text = attributes.value("vars", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      int cpl = qMax(1, (int)((120.0 * aZoom - 30.0 * aZoom) / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = qMax(textWidth + 16.0 * aZoom, 120.0 * aZoom);
      clientHeight = qMax(lines * lineH + 16.0 * aZoom, 60.0 * aZoom);
    }
    else if (type() == "if")
    {
      topMargin = 92 * aZoom;
      bottomMargin = 16 * aZoom;
    }
    else if (type() == "pre")
    {
      topMargin = 108 * aZoom;
      bottomMargin = 32 * aZoom;
    }
    else if (type() == "post")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 96 * aZoom;
    }
    else if (type() == "for")
    {
      topMargin = 108 * aZoom;
      bottomMargin = 32 * aZoom;
    }
    else if (type() == "comment")
    {
      // Комментарий по ГОСТ 19.701-90 п.3.4.3
      // Размер блока нужен для клика/выбора
      // Actual w/h will be adjusted in adjustPosition() based on target
      QString text = attributes.value("text", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(11 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double textHeight = fm.height();

      double bracketDepth = qMax(20.0 * aZoom, textHeight * 0.12);
      double pad = 3 * aZoom;

      topMargin = 0;
      bottomMargin = 0;
      leftMargin = 0;
      rightMargin = 0;
      // Полная ширина: скобка + зазор + текст (для любой стороны)
      clientWidth = bracketDepth + pad + textWidth + 2*pad;
      clientHeight = textHeight + 2*pad;
    }
    else if (type() == "data" || type() == "stored_data" || type() == "document" ||
             type() == "manual_input" || type() == "display" || type() == "manual_op" ||
             type() == "ram" || type() == "card" || type() == "paper_tape" ||
             type() == "seq_access" || type() == "direct_access")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;

      QString text = attributes.value("text", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      double availTextW = qMax(112.0 * aZoom, textWidth);
      int cpl = qMax(1, (int)(availTextW / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = qMax(textWidth + 16.0 * aZoom, 120.0 * aZoom);
      clientHeight = qMax(lines * lineH + 16.0 * aZoom, 60.0 * aZoom);
    }
    else if (type() == "parallel")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;

      QString text = attributes.value("text", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(13 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double lineH = fm.height();
      double avgW = qMax(1.0, fm.averageCharWidth());

      double availTextW = qMax(112.0 * aZoom, textWidth);
      int cpl = qMax(1, (int)(availTextW / avgW));
      int lines = qMax(1, (text.length() + cpl - 1) / cpl);

      clientWidth = qMax(textWidth + 16.0 * aZoom, 120.0 * aZoom);
      clientHeight = qMax(lines * lineH + 36.0 * aZoom, 70.0 * aZoom);
    }
    else if (type() == "connector")
    {
      topMargin = 16 * aZoom;
      bottomMargin = 10 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;

      QString text = attributes.value("text", "");
      QFont fnt("Tahoma");
      fnt.setPixelSize(11 * aZoom);
      QFontMetricsF fm(fnt);
      double textWidth = fm.horizontalAdvance(text);
      double textHeight = fm.height();

      double dia = qMax(textWidth, textHeight) + 16.0 * aZoom;
      clientWidth = qMax(dia, 40.0 * aZoom);
      clientHeight = qMax(dia, 40.0 * aZoom);
    }
    else if (type() == "ellipsis")
    {
      topMargin = 8 * aZoom;
      bottomMargin = 8 * aZoom;
      leftMargin = 10 * aZoom;
      rightMargin = 10 * aZoom;
      clientWidth = 80.0 * aZoom;
      clientHeight = 20.0 * aZoom;
    }

    width = leftMargin + clientWidth + rightMargin;
    height = topMargin + clientHeight + bottomMargin;
  }
}

void QBlock::adjustPosition(const double ox, const double oy)
{
  x = ox;
  y = oy;
  if (isBranch)
  {
    // ---- Pass 1: temporary positioning to determine extent (incl. comments) ----
    // Place non-comment blocks at preliminary centers and determine where each
    // comment would go, so we know the total width required.
    double cy = y;
    double maxRight = x + width;
    double minLeft  = x;
    for (int i = 0; i < items.size(); ++i)
    {
      if (items.at(i)->type() == "comment") {
        // Find target block (forward first, then backward)
        QBlock *target = nullptr;
        for (int j = i + 1; j < items.size(); ++j) {
            if (items.at(j)->type() != "comment") { target = items.at(j); break; }
        }
        if (!target) {
            for (int j = i - 1; j >= 0; --j) {
                if (items.at(j)->type() != "comment") { target = items.at(j); break; }
            }
        }

        double gap = 8 * zoom();
        double cw  = items.at(i)->width;
        // Predict target's center (blocks are centered in branch)
        double tgtX = target ? ox + (width - target->width) / 2 : ox;

        // Determine side (same logic as final pass below)
        bool placeRight = true;
        bool hasBranchIdx = false;
        {
          int elseDepth = 0;
          int branchIdx = 0;
          QBlock *node = this;
          while (node) {
              QBlock *p = node->parent;
              if (p && p->type() == "if") {
                  int idx = p->items.indexOf(node);
                  if (!hasBranchIdx) { branchIdx = idx; hasBranchIdx = true; }
                  else if (idx == 1) { elseDepth++; }
              }
              node = p;
          }
          if (!hasBranchIdx) {
              int commentIdx = 0;
              for (int j = 0; j < items.size(); ++j) {
                  if (items.at(j)->type() == "comment") {
                      if (items.at(j) == items.at(i)) break;
                      commentIdx++;
                  }
              }
              placeRight = (commentIdx % 2 == 0);
          } else if (branchIdx == 1) {
              placeRight = true;
          } else {
              placeRight = (elseDepth >= 2);
          }
        }

        if (!hasBranchIdx) {
          // Центральный комментарий (вне if/else): занимает вертикальное место,
          // не расширяет ветку
          cy += items.at(i)->height;
        } else if (target) {
          // if/else-комментарий: на стороне, расширяет ветку
          if (placeRight) {
              double rx = tgtX + target->width + gap + cw;
              if (rx > maxRight) maxRight = rx;
          } else {
              double lx = tgtX - gap - cw;
              if (lx < minLeft) minLeft = lx;
          }
        }
        continue;
      }
      cy += items.at(i)->height;
    }

    // Adjust branch bounds to encompass all comments
    if (minLeft < x)  x = minLeft;
    if (maxRight > x + width) width = maxRight - x;

    // ---- Pass 2: final positioning with corrected bounds ----
    cy = y;
    for (int i = 0; i < items.size(); ++i)
    {
      if (items.at(i)->type() == "comment") {
        double cw = items.at(i)->width;
        double ch = items.at(i)->height;

        // Determine if this comment is center-embedded or side
      bool isCenter = false;
      {
        QBlock *node = this;
          while (node) {
              QBlock *p = node->parent;
              if (p && p->type() == "if") { break; }
              node = p;
          }
          isCenter = (!node || !node->parent || node->parent->type() != "if");
        }

        if (isCenter) {
          // Центральный комментарий: как блок — по центру, разрывает линию
          items.at(i)->adjustPosition(x + (width - cw) / 2, cy);
          cy += ch;
        } else {
          // if/else-комментарий: скобка сбоку
          QBlock *target = nullptr;
          bool targetIsForward = false;
          for (int j = i + 1; j < items.size(); ++j) {
              if (items.at(j)->type() != "comment") { target = items.at(j); targetIsForward = true; break; }
          }
          if (!target) {
              for (int j = i - 1; j >= 0; --j) {
                  if (items.at(j)->type() != "comment") { target = items.at(j); break; }
              }
          }
          if (target) {
              double gap = 8 * zoom();

              double targetX, targetY, targetRight;
              if (targetIsForward) {
                  targetX     = x + (width - target->width) / 2;
                  targetY     = cy;
                  targetRight = targetX + target->width;
              } else {
                  targetX     = target->x;
                  targetY     = target->y;
                  targetRight = target->x + target->width;
              }

              bool placeRight = true;
              {
                int elseDepth = 0;
                bool hasBranchIdx = false;
                int branchIdx = 0;
                QBlock *node = this;
                while (node) {
                    QBlock *p = node->parent;
                    if (p && p->type() == "if") {
                        int idx = p->items.indexOf(node);
                        if (!hasBranchIdx) { branchIdx = idx; hasBranchIdx = true; }
                        else if (idx == 1) { elseDepth++; }
                    }
                    node = p;
                }
                if (!hasBranchIdx) {
                    int commentIdx = 0;
                    for (int j = 0; j < items.size(); ++j) {
                        if (items.at(j)->type() == "comment") {
                            if (items.at(j) == items.at(i)) break;
                            commentIdx++;
                        }
                    }
                    placeRight = (commentIdx % 2 == 0);
                } else if (branchIdx == 1) {
                    placeRight = true;
                } else {
                    placeRight = (elseDepth >= 2);
                }
              }

              if (placeRight) {
                  double bx = targetRight + gap;
                  double by = targetY + target->height / 2 - ch / 2;
                  items.at(i)->adjustPosition(bx, by);
              } else {
                  double bx = targetX - gap - cw;
                  double by = targetY + target->height / 2 - ch / 2;
                  items.at(i)->adjustPosition(bx, by);
              }
          } else {
              items.at(i)->adjustPosition(x, cy);
          }
        }
        continue;
      }
      item(i)->adjustPosition(x + (width - item(i)->width) / 2, cy);
      cy += item(i)->height;
    }
  }
  else
  {
    double cx = x + leftMargin;
    for (int i = 0; i < items.size(); ++i)
    {
      item(i)->adjustPosition(cx, y + topMargin);
      cx += item(i)->width;
    }
  }
}

void QBlock::drawCaption(QPainter *canvas, const QRectF & rect, const double zoomFactor, const QString & text)
{
  QFont font("Sans Serif");
  font.setPixelSize(13 * zoomFactor);
  font = QFont(font, canvas->device());
  QFontMetricsF fontMetrics(font);
  QRectF textRect = fontMetrics.boundingRect(text);
  double tx = rect.x() + rect.width() / 2 - textRect.width() / 2;
  double ty = rect.y() + rect.height() / 2 + fontMetrics.ascent() / 2;
  canvas->setFont(font);
//  canvas->drawRect(tx, ty - textRect.height(), textRect.width(), textRect.height());
  canvas->drawText(QPointF(tx, ty), text);

}

void QBlock::paint(QPainter *canvas, bool fontSizeInPoints) const
{
  if (flowChart())
  {
    QFlowChartStyle st = flowChart()->chartStyle();
    double hcenter = x + width / 2;
    /* в соответствии с ГОСТ 19.003-80 */
    double a = 60 * zoom();
    double b = 2 * a;
    double bottom = y + height;
    double lw = st.lineWidth() * zoom();

//    QFont font = flowChart()->font();
//    font.setPixelSize(13 * zoom());
//    flowChart()->setFont(font);

    QFont font("Tahoma");
    font.setWeight(QFont::Normal);
    if (fontSizeInPoints)
      font.setPointSizeF(10 * zoom());
    else
      font.setPixelSize(13 * zoom());
    font = QFont(font, canvas->device());
    canvas->setFont(font);

    if(flowChart()->status() == QFlowChart::Selectable && isActive())
    {
      canvas->setPen(QPen(st.selectedForeground(), lw));
      canvas->setBrush(QBrush(st.selectedBackground()));
    }
    else
    {
      canvas->setPen(QPen(st.normalForeground(), lw));
      canvas->setBrush(QBrush(st.normalBackground()));
    }

    QPen pen = canvas->pen();
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    canvas->setPen(pen);
    canvas->fillRect(QRectF(x, y, width, height), canvas->brush());

    if (isBranch)
    {
      /* отрисовка ветви — сегментами между блоками */
      /* Центральные комментарии (вне if/else) разрывают линию: */
      /* линия идёт от предыдущего блока до верха комментария, */
      /* затем от низа комментария до следующего блока. */
      bool insideIfElse = parent && parent->type() == "if";
      double lastY = y;
      bool hasBlock = false;
      for (int i = 0; i < items.size(); ++i)
      {
        QBlock *blk = items.at(i);
        // Боковые if/else-комментарии не влияют на линию
        if (blk->type() == "comment" && insideIfElse)
          continue;
        if (!hasBlock)
        {
          canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, blk->y));
          hasBlock = true;
        }
        else
        {
          canvas->drawLine(QLineF(hcenter, lastY, hcenter, blk->y));
        }
        lastY = blk->y + blk->height;
      }
      if (hasBlock)
        canvas->drawLine(QLineF(hcenter, lastY, hcenter, y + height + 0.5));
      else
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + height + 0.5));
    }
    else
    {
      if (type() == "algorithm")
      {
        /* алгоритм */
        QBlock *body = item(0);
        Q_ASSERT_X(body != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of algorithm is nul.");
        QRectF oval(hcenter - b/2, lw, b, a/2);
        canvas->drawRoundedRect(oval, a/4, a/4);
        canvas->drawText(oval, Qt::TextSingleLine | Qt::AlignHCenter | Qt::AlignVCenter, tr("Начало"));
//        drawCaption(canvas, oval, zoom(), tr("BEGIN"));
        canvas->drawLine(QLineF(hcenter, y + a/2+lw, hcenter, body->y+0.5));
        canvas->drawLine(QLineF(hcenter, body->y + body->height-0.5, hcenter, bottom - a/2-lw));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, bottom - a/2 - lw),
                                    QSize(6 * zoom(), 12 * zoom()));

        oval = QRectF(hcenter - b/2, bottom - a/2 - lw, b, a/2);
        canvas->drawRoundedRect(oval, a/4, a/4);
        canvas->drawText(oval, Qt::TextSingleLine | Qt::AlignHCenter | Qt::AlignVCenter, tr("Конец"));

      }
      else if(type() == "process")
      {
        /* процесс — с динамическим размером */
        double p_a = height - topMargin - bottomMargin;  // = clientHeight
        double p_b = width - leftMargin - rightMargin;    // = clientWidth
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QRectF rect(hcenter - p_b/2, y + 16 * zoom(), p_b, p_a);
        QRectF textRect(hcenter - p_b/2 + 4 * zoom(), y + 20 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawRect(rect);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+p_a, hcenter, bottom+0.5));
      }
      else if(type() == "predefined")
      {
        /* предопределённый процесс (ГОСТ 19.701-90) — с динамическим размером */
        double p_a = height - topMargin - bottomMargin;  // = clientHeight
        double p_b = width - leftMargin - rightMargin;    // = clientWidth
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QRectF rect(hcenter - p_b/2, y + 16 * zoom(), p_b, p_a);
        canvas->drawRect(rect);
        // vertical bars inside the rectangle (predefined process feature)
        double barOffset = p_b / 6;
        canvas->drawLine(QLineF(hcenter - p_b/2 + barOffset, y + 16 * zoom(),
                                hcenter - p_b/2 + barOffset, y + 16 * zoom() + p_a));
        canvas->drawLine(QLineF(hcenter + p_b/2 - barOffset, y + 16 * zoom(),
                                hcenter + p_b/2 - barOffset, y + 16 * zoom() + p_a));
        // Текст ТОЛЬКО между вертикальными перемычками (не наезжает на них)
        QRectF textRect(hcenter - p_b/2 + barOffset + 2 * zoom(), y + 20 * zoom(),
                        p_b - 2 * barOffset - 4 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+p_a, hcenter, bottom+0.5));
      }
      else if(type() == "assign")
      {
        /* присваивание — с динамическим размером */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QRectF rect(hcenter - p_b/2, y + 16 * zoom(), p_b, p_a);
        QRectF textRect(hcenter - p_b/2 + 4 * zoom(), y + 20 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawRect(rect);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere,
            QString("%1 := %2").arg(attributes.value("dest", ""), attributes.value("src", "")));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+p_a, hcenter, bottom+0.5));
      }
      else if(type() == "io")
      {
        /* ввод/вывод — с динамическим размером */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - p_b/2 + p_a/4, y + 16 * zoom());
        par[1] = QPointF(hcenter + p_b/2 + p_a/4, y + 16 * zoom());
        par[2] = QPointF(hcenter + p_b/2 - p_a/4, y + 16 * zoom() + p_a);
        par[3] = QPointF(hcenter - p_b/2 - p_a/4, y + 16 * zoom() + p_a);
        canvas->drawPolygon(par, 4);
        // Текст внутри параллелограмма (с учётом скоса)
        QRectF textRect(hcenter - p_b/2 + p_a/4 + 4 * zoom(), y + 20 * zoom(),
                        p_b - p_a/2 - 8 * zoom(), p_a - 8 * zoom());
        QStringList ls = attributes["vars"].split(",");
        QString text = ls.join(", ");
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, text);
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+p_a, hcenter, bottom+0.5));
      }
      else if(type() == "ou")
      {
        /* вывод — с динамическим размером */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        canvas->drawLine(QLineF(hcenter, y, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - p_b/2 + p_a/4, y + 16 * zoom());
        par[1] = QPointF(hcenter + p_b/2 + p_a/4, y + 16 * zoom());
        par[2] = QPointF(hcenter + p_b/2 - p_a/4, y + 16 * zoom() + p_a);
        par[3] = QPointF(hcenter - p_b/2 - p_a/4, y + 16 * zoom() + p_a);
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - p_b/2 + p_a/4 + 4 * zoom(), y + 20 * zoom(),
                        p_b - p_a/2 - 8 * zoom(), p_a - 8 * zoom());
        QStringList ls = attributes["vars"].split(",");
        QString text = ls.join(", ");
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, text);
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom()+p_a, hcenter, bottom+0.5));
      }
      else if(type() == "if")
      {
        /* ветвление */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, y + 16 * zoom() + a/2);
        par[1] = QPointF(hcenter      , y + 16 * zoom()      );
        par[2] = QPointF(hcenter + b/2, y + 16 * zoom() + a/2);
        par[3] = QPointF(hcenter      , y + 16 * zoom() + a  );
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - b/2 + a/4 + 20, y + 16 * zoom()+4 + a/8, b - a/2 - 40, a - 8 - a/4);
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. left branch of IF is nul.");
        QBlock *right = item(1);
        Q_ASSERT_X(right != 0, "QBlock::paint()" ,"item(1) == 0. i.e. right branch of IF is nul.");
        // левая линия
        QPointF line[3];
        line[0] = QPointF(hcenter - b/2, y + 16 * zoom() + a/2);
        line[1] = QPointF(left->x+left->width/2, y + 16 * zoom() + a/2);
        line[2] = QPointF(left->x+left->width/2, left->y);
        canvas->drawPolyline(line, 3);

        canvas->drawText(QPointF(hcenter - b/2 - 24*zoom(), y + 12 * zoom() + a/2), tr("Да"));

        // правая линия
        line[0] = QPointF(hcenter + b/2, y + 16 * zoom() + a/2);
        line[1] = QPointF(right->x+right->width/2, y + 16 * zoom() + a/2);
        line[2] = QPointF(right->x+right->width/2, right->y);
        canvas->drawPolyline(line, 3);
        canvas->drawText(QPointF(hcenter + b/2 +5*zoom(), y + 12 * zoom() + a/2), tr("Нет"));

        // соединение
        QPointF collector[4];
        collector[0] = QPointF(left->x + left->width / 2, left->y+left->height);
        collector[1] = QPointF(left->x + left->width / 2, bottom - 8*zoom());
        collector[2] = QPointF(right->x + right->width / 2, bottom - 8*zoom());
        collector[3] = QPointF(right->x + right->width / 2, right->y+right->height);
        canvas->drawPolyline(collector, 4);
        canvas->drawLine(QLineF(hcenter, bottom-8*zoom(), hcenter, bottom+0.5));
      }
      else if(type() == "pre")
      {
        /* цикл с предусловием */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 32 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 32 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, y + 32 * zoom() + a/2);
        par[1] = QPointF(hcenter      , y + 32 * zoom()      );
        par[2] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        par[3] = QPointF(hcenter      , y + 32 * zoom() + a  );
        canvas->drawPolygon(par, 4);
        QRectF rect(hcenter - b/2, y + 32 * zoom(), b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of PRE-loop is nul.");
        canvas->drawLine(QLineF(hcenter,y + 32 * zoom() + a,hcenter,left->y));

//        // правая линия
        QPointF line[5];
        line[0] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        line[1] = QPointF(x + width - 5*zoom(), y + 32 * zoom() + a/2);
        line[2] = QPointF(x + width - 5*zoom(), bottom - 4 * zoom());
        line[3] = QPointF(hcenter, bottom - 4 * zoom());
        line[4] = QPointF(hcenter, bottom+0.5);
        canvas->drawPolyline(line, 5);
        canvas->drawText(QPointF(hcenter + 4*zoom(), y + 44 * zoom() + a), tr("Да"));
        canvas->drawText(QPointF(hcenter + b/2 +5*zoom(), y + 28 * zoom() + a/2), tr("Нет"));

        // соединение
        QPointF collector[5];
        collector[0] = QPointF(hcenter, left->y+left->height);
        collector[1] = QPointF(hcenter, bottom - 28*zoom());
        collector[2] = QPointF(x + 5*zoom(), bottom - 28*zoom());
        collector[3] = QPointF(x + 5*zoom(), y + 8*zoom());
        collector[4] = QPointF(hcenter, y + 8*zoom());
        canvas->drawPolyline(collector, 5);
        QFlowChart::drawRightArrow(canvas, collector[4],
                                    QSize(12 * zoom(), 6 * zoom()));

      }
      else if(type() == "post")
      {
        /* цикл с постусловием */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of POST-loop is nul.");
        // верх ромба с входяящей стрелкой
        double top = left->y+left->height + 16 * zoom();

        canvas->drawLine(QLineF(hcenter,left->y+left->height,hcenter,top));

        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, top),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - b/2, top + a/2);
        par[1] = QPointF(hcenter      , top      );
        par[2] = QPointF(hcenter + b/2, top + a/2);
        par[3] = QPointF(hcenter      , top + a  );
        canvas->drawPolygon(par, 4);
        QRectF rect(hcenter - b/2, top, b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1?").arg(attributes.value("cond", "")));

        canvas->drawText(QPointF(hcenter - b/2 - 24*zoom(), top - 4* zoom() + a/2), tr("Да"));
        canvas->drawText(QPointF(hcenter  +4*zoom(), top + 16 * zoom() + a), tr("Нет"));

        // соединение
        QPointF collector[4];
        collector[0] = QPointF(hcenter - b/2, top + a/2);
        collector[1] = QPointF(x + 5*zoom(), top + a/2);
        collector[2] = QPointF(x + 5*zoom(), y + 8*zoom());
        collector[3] = QPointF(hcenter, y + 8*zoom());
        canvas->drawPolyline(collector, 4);
        QFlowChart::drawRightArrow(canvas, collector[3],
                                    QSize(12 * zoom(), 6 * zoom()));

        // выход
        canvas->drawLine(QLineF(hcenter, top + a, hcenter, bottom+0.5));
      }
      else if(type() == "for")
      {
        /* цикл FOR */
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 32 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 32 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF hex[6];
        hex[0] = QPointF(hcenter - b/2, y + 32 * zoom() + a/2);
        hex[1] = QPointF(hcenter - a/2, y + 32 * zoom()      );
        hex[2] = QPointF(hcenter + a/2, y + 32 * zoom()      );
        hex[3] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        hex[4] = QPointF(hcenter + a/2, y + 32 * zoom() + a  );
        hex[5] = QPointF(hcenter - a/2, y + 32 * zoom() + a  );
        canvas->drawPolygon(hex, 6);

        QRectF rect(hcenter - b/2, y + 32 * zoom(), b, a);
        canvas->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, QString("%1 := %2...%3").arg(attributes.value("var", ""), attributes.value("from", ""), attributes.value("to", "")));
        QBlock *left = item(0);
        Q_ASSERT_X(left != 0, "QBlock::paint()" ,"item(0) == 0. i.e. body of PRE-loop is nul.");
        canvas->drawLine(QLineF(hcenter,y + 32 * zoom() + a,hcenter,left->y));

//        // правая линия
        QPointF line[5];
        line[0] = QPointF(hcenter + b/2, y + 32 * zoom() + a/2);
        line[1] = QPointF(x + width - 5*zoom(), y + 32 * zoom() + a/2);
        line[2] = QPointF(x + width - 5*zoom(), bottom - 4 * zoom());
        line[3] = QPointF(hcenter, bottom - 4 * zoom());
        line[4] = QPointF(hcenter, bottom+0.5);
        canvas->drawPolyline(line, 5);

        // соединение
        QPointF collector[5];
        collector[0] = QPointF(hcenter, left->y+left->height);
        collector[1] = QPointF(hcenter, bottom - 28*zoom());
        collector[2] = QPointF(x + 5*zoom(), bottom - 28*zoom());
        collector[3] = QPointF(x + 5*zoom(), y + 32*zoom() + a/2);
        collector[4] = QPointF(hcenter - b/2, y + 32*zoom() + a/2);
        canvas->drawPolyline(collector, 5);
        QFlowChart::drawRightArrow(canvas, collector[4],
                                    QSize(12 * zoom(), 6 * zoom()));

      }
      else if(type() == "comment")
      {
        /* комментарий по ГОСТ 19.701-90 п.3.4.3
         *
         * Справа от блока:  block ─ ─ ─ ─┬─ text
         *                                │
         *                                └─
         * Слева от блока:   text ─┬─ ─ ─ ─ ─ block
         *                         │
         *                         └─
         * Высота скобки = высоте блока.
         * Положение (x,y) устанавливается в adjustPosition().
         *
         * Центральные комментарии (вне if/else) — встроены в поток,
         * разрывают линию, отображаются как текст по центру.
         */
        QFont commentFont("Tahoma");
        commentFont.setWeight(QFont::Normal);
        if (fontSizeInPoints)
          commentFont.setPointSizeF(9 * zoom());
        else
          commentFont.setPixelSize(11 * zoom());
        canvas->setFont(commentFont);

        QString commentText = attributes.value("text", "");
        QFontMetricsF cfm(commentFont);
        double textWidth = cfm.horizontalAdvance(commentText);
        double textHeight = cfm.height();
        double pad = 3 * zoom();

        bool isCenterComment = parent && parent->parent &&
                                parent->parent->type() != "if";

        if (isCenterComment) {
          // Центральный комментарий: встроен в поток, разрывает линию
          QPen bgPen = canvas->pen();
          bgPen.setStyle(Qt::DashLine);
          canvas->setPen(bgPen);
          canvas->drawRoundedRect(QRectF(x + pad, y + pad, width - 2*pad,
                                         height - 2*pad), 3*zoom(), 3*zoom());
          canvas->setPen(QPen(st.normalForeground(), lw));
          canvas->drawText(QRectF(x, y, width, height),
                           Qt::AlignCenter | Qt::TextWrapAnywhere, commentText);
        } else {
        // Определяем блок, к которому относится комментарий
        QBlock *target = nullptr;
        if (parent) {
            int myIndex = -1;
            for (int i = 0; i < parent->items.size(); ++i) {
                if (parent->items.at(i) == this) { myIndex = i; break; }
            }
            if (myIndex >= 0) {
                for (int i = myIndex + 1; i < parent->items.size(); ++i) {
                    if (parent->items.at(i)->type() != "comment") { target = parent->items.at(i); break; }
                }
                if (!target) {
                    for (int i = myIndex - 1; i >= 0; --i) {
                        if (parent->items.at(i)->type() != "comment") { target = parent->items.at(i); break; }
                    }
                }
            }
        }
        if (!target) return;

        double targetMidY = target->y + target->height / 2;
        double gap = 8 * zoom();

        // Определяем, с какой стороны блок: по положению комментария
        bool placeRight = (x > target->x);
        double bracketH = target->height; // высота скобки = высоте блока
        double bracketDepth = qMax(20.0 * zoom(), bracketH * 0.12); // длина ── (не менее 20px)

        QPen dashPen = canvas->pen();
        dashPen.setStyle(Qt::DashLine);
        QPen solidPen = canvas->pen();
        solidPen.setStyle(Qt::SolidLine);

        if (placeRight) {
            // ─ ─ ─ ─┬─ text
            //         │
            //         └─
            double bracketY = targetMidY - bracketH / 2;
            double bracketX = target->x + target->width + gap;
            double rhsX = bracketX + bracketDepth;

            // Пунктир от блока к скобке
            canvas->setPen(dashPen);
            canvas->drawLine(QLineF(target->x + target->width, targetMidY,
                                    bracketX, targetMidY));

            // Скобка [
            canvas->setPen(solidPen);
            // Вертикаль (левая сторона [)
            canvas->drawLine(QLineF(bracketX, bracketY, bracketX, bracketY + bracketH));
            // Верх
            canvas->drawLine(QLineF(bracketX, bracketY, rhsX, bracketY));
            // Низ
            canvas->drawLine(QLineF(bracketX, bracketY + bracketH, rhsX, bracketY + bracketH));

            // Текст справа от скобки
            double textX = rhsX + pad;
            double textY = targetMidY - textHeight / 2;
            QRectF tr(textX, textY, textWidth + 2*pad, textHeight);
            canvas->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter, commentText);
        } else {
            // text ─┬─ ─ ─ ─ ─
            //        │
            //        └─
            double bracketY = targetMidY - bracketH / 2;
            double bracketX = target->x - gap;
            double lhsX = bracketX - bracketDepth;

            canvas->setPen(dashPen);
            canvas->drawLine(QLineF(bracketX, targetMidY, target->x, targetMidY));

            canvas->setPen(solidPen);
            canvas->drawLine(QLineF(bracketX, bracketY, bracketX, bracketY + bracketH));
            canvas->drawLine(QLineF(lhsX, bracketY, bracketX, bracketY));
            canvas->drawLine(QLineF(lhsX, bracketY + bracketH, bracketX, bracketY + bracketH));

            double textX = lhsX - pad - textWidth;
            double textY = targetMidY - textHeight / 2;
            QRectF tr(textX, textY, textWidth + 2*pad, textHeight);
            canvas->drawText(tr, Qt::AlignRight | Qt::AlignVCenter, commentText);
        }
      } // закрывает else (isCenterComment)
      }
      else if(type() == "data")
      {
        /* данные — параллелограмм с наклоном вправо */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, y + 16 * zoom()));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, y + 16 * zoom()),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        // Наклон вправо: верхняя грань смещена влево, нижняя — вправо
        par[0] = QPointF(hcenter - p_b/2 - p_a/4, y + 16 * zoom());
        par[1] = QPointF(hcenter + p_b/2 - p_a/4, y + 16 * zoom());
        par[2] = QPointF(hcenter + p_b/2 + p_a/4, y + 16 * zoom() + p_a);
        par[3] = QPointF(hcenter - p_b/2 + p_a/4, y + 16 * zoom() + p_a);
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - p_b/2 - p_a/4 + 4 * zoom(), y + 20 * zoom(),
                        p_b + p_a/2 - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, y + 16 * zoom() + p_a, hcenter, bottom+0.5));
      }
      else if(type() == "stored_data")
      {
        /* запоминаемые данные — шестиугольник с вогнутыми боковыми сторонами */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;
        double indent = p_b * 0.12;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPainterPath path;
        path.moveTo(leftX, topY);
        path.lineTo(rightX, topY);
        // Правая сторона — вогнутая внутрь
        path.cubicTo(rightX - indent, topY + p_a/3,
                     rightX - indent, topY + 2*p_a/3,
                     rightX, bottomY);
        path.lineTo(leftX, bottomY);
        // Левая сторона — вогнутая внутрь
        path.cubicTo(leftX + indent, bottomY - p_a/3,
                     leftX + indent, topY + p_a/3,
                     leftX, topY);
        path.closeSubpath();
        canvas->drawPath(path);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "document")
      {
        /* документ — прямоугольник с волнистым низом */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPainterPath path;
        path.moveTo(leftX, topY);
        path.lineTo(rightX, topY);
        path.lineTo(rightX, bottomY);
        // Волнистый низ: 6 сегментов (3 волны)
        double waveW = p_b / 6.0;
        double waveA = 6 * zoom();
        for (int i = 0; i < 6; ++i) {
            double sx = rightX - i * waveW;
            double ex = rightX - (i + 1) * waveW;
            double midY = (i % 2 == 0) ? bottomY + waveA : bottomY - waveA;
            path.quadTo((sx + ex) / 2, midY, ex, bottomY);
        }
        path.closeSubpath();
        canvas->drawPath(path);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "manual_input")
      {
        /* ручной ввод — трапеция со скошенным верхом (левая сторона короче) */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;
        double slant = p_b * 0.2;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPointF par[4];
        par[0] = QPointF(leftX + slant, topY);   // верх-левый (смещён вправо)
        par[1] = QPointF(rightX, topY);           // верх-правый
        par[2] = QPointF(rightX, bottomY);        // низ-правый
        par[3] = QPointF(leftX, bottomY);         // низ-левый
        canvas->drawPolygon(par, 4);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "display")
      {
        /* дисплей — параллелограмм (ГОСТ 19.701-90 п.3.1.2.8) */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double topY = y + 16 * zoom();

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));
        QPointF par[4];
        par[0] = QPointF(hcenter - p_b/2 + p_a/4, topY);                       // верх-левый
        par[1] = QPointF(hcenter + p_b/2 + p_a/4, topY);                       // верх-правый
        par[2] = QPointF(hcenter + p_b/2 - p_a/4, topY + p_a);                 // низ-правый
        par[3] = QPointF(hcenter - p_b/2 - p_a/4, topY + p_a);                 // низ-левый
        canvas->drawPolygon(par, 4);
        QRectF textRect(hcenter - p_b/2 + p_a/4 + 4 * zoom(), topY + 4 * zoom(),
                        p_b - p_a/2 - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, topY + p_a, hcenter, bottom+0.5));
      }
      else if(type() == "manual_op")
      {
        /* ручная операция — трапеция, сужающаяся книзу */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;
        double slant = p_b * 0.15;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPointF par[4];
        par[0] = QPointF(leftX, topY);              // верх-левый
        par[1] = QPointF(rightX, topY);             // верх-правый
        par[2] = QPointF(rightX - slant, bottomY);  // низ-правый (уже)
        par[3] = QPointF(leftX + slant, bottomY);   // низ-левый (уже)
        canvas->drawPolygon(par, 4);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "parallel")
      {
        /* параллельные действия — две горизонтальные линии */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        double barThick = 6 * zoom();
        double barY1 = topY + p_a * 0.25;
        double barY2 = topY + p_a * 0.75;

        canvas->fillRect(QRectF(leftX, barY1 - barThick/2, p_b, barThick), canvas->brush());
        canvas->drawRect(QRectF(leftX, barY1 - barThick/2, p_b, barThick));
        canvas->fillRect(QRectF(leftX, barY2 - barThick/2, p_b, barThick), canvas->brush());
        canvas->drawRect(QRectF(leftX, barY2 - barThick/2, p_b, barThick));

        QRectF textRect(leftX + 4 * zoom(), barY1 + barThick/2 + 2 * zoom(),
                        p_b - 8 * zoom(), barY2 - barY1 - barThick - 4 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));

        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "connector")
      {
        /* соединитель — маленький кружок с текстом внутри */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double topY = y + 16 * zoom();
        double bottomY = topY + p_a;
        double r = qMin(p_b, p_a) / 2;
        QRectF circle(hcenter - r, topY + p_a/2 - r, 2*r, 2*r);

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));
        canvas->drawEllipse(circle);
        canvas->drawText(circle, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, topY + p_a/2 + r, hcenter, bottom+0.5));
      }
      else if(type() == "ellipsis")
      {
        /* пропуск — три точки на горизонтальной линии */
        double topY = y + 8 * zoom();
        double p_a = height - topMargin - bottomMargin;
        double bottomY = topY + p_a;
        double midY = (topY + bottomY) / 2;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        // Горизонтальная линия
        double lineX1 = x + 10 * zoom();
        double lineX2 = x + width - 10 * zoom();
        canvas->drawLine(QLineF(lineX1, midY, lineX2, midY));

        // Три точки на линии
        double dotSpacing = 10 * zoom();
        double dotR = 2.5 * zoom();
        for (int i = -1; i <= 1; ++i) {
            canvas->drawEllipse(QPointF(hcenter + i * dotSpacing, midY), dotR, dotR);
        }

        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "ram")
      {
        /* ОЗУ — прямоугольник, разделённый вертикальной линией */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QRectF rect(leftX, topY, p_b, p_a);
        canvas->drawRect(rect);
        // Вертикальная разделительная линия посередине
        canvas->drawLine(QLineF(hcenter, topY, hcenter, bottomY));

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "seq_access")
      {
        /* последовательный доступ — круг с хвостиком (Q-образный) */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double topY = y + 16 * zoom();
        double bottomY = topY + p_a;
        double cx = hcenter;
        double cy = topY + p_a/2;
        double r = qMin(p_b, p_a) / 2;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        // Круг
        canvas->drawEllipse(QPointF(cx, cy), r, r);

        // Хвостик (из правой нижней части круга, загибается вверх)
        QPainterPath tail;
        tail.moveTo(cx + r * 0.6, cy + r * 0.6);
        tail.cubicTo(cx + r * 1.4, cy + r * 0.8,
                     cx + r * 1.4, cy - r * 0.4,
                     cx + r * 0.5, cy - r * 0.7);
        canvas->drawPath(tail);

        QRectF textRect(cx - r + 4 * zoom(), topY + 4 * zoom(),
                        2*r - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "direct_access")
      {
        /* прямой доступ — цилиндр на боку */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;
        double bulge = p_a * 0.15;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPainterPath path;
        path.moveTo(leftX, topY);
        path.lineTo(rightX, topY);
        // Правая выпуклая сторона
        path.cubicTo(rightX + bulge, topY + p_a/3,
                     rightX + bulge, topY + 2*p_a/3,
                     rightX, bottomY);
        path.lineTo(leftX, bottomY);
        // Левая выпуклая сторона
        path.cubicTo(leftX - bulge, bottomY - p_a/3,
                     leftX - bulge, topY + p_a/3,
                     leftX, topY);
        path.closeSubpath();
        canvas->drawPath(path);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "card")
      {
        /* карта — прямоугольник со срезанным левым верхним углом */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;
        double cut = p_b * 0.12;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPointF card[5];
        card[0] = QPointF(leftX + cut, topY);    // после среза
        card[1] = QPointF(rightX, topY);          // верх-правый
        card[2] = QPointF(rightX, bottomY);       // низ-правый
        card[3] = QPointF(leftX, bottomY);        // низ-левый
        card[4] = QPointF(leftX, topY + cut);     // левый, начало среза
        canvas->drawPolygon(card, 5);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }
      else if(type() == "paper_tape")
      {
        /* бумажная лента — прямоугольник с волнистыми верхом и низом */
        double p_a = height - topMargin - bottomMargin;
        double p_b = width - leftMargin - rightMargin;
        double leftX = hcenter - p_b/2;
        double topY = y + 16 * zoom();
        double rightX = hcenter + p_b/2;
        double bottomY = topY + p_a;

        canvas->drawLine(QLineF(hcenter, y-0.5, hcenter, topY));
        QFlowChart::drawBottomArrow(canvas, QPointF(hcenter, topY),
                                    QSize(6 * zoom(), 12 * zoom()));

        QPainterPath path;
        double waveW = p_b / 6.0;
        double waveA = 6 * zoom();

        // Верхняя сторона (слева направо) — волнистая
        path.moveTo(leftX, topY);
        for (int i = 0; i < 6; ++i) {
            double sx = leftX + i * waveW;
            double ex = leftX + (i + 1) * waveW;
            double midY = (i % 2 == 0) ? topY - waveA : topY + waveA;
            path.quadTo((sx + ex) / 2, midY, ex, topY);
        }
        // Правая сторона вниз
        path.lineTo(rightX, bottomY);
        // Нижняя сторона (справа налево) — волнистая
        for (int i = 0; i < 6; ++i) {
            double sx = rightX - i * waveW;
            double ex = rightX - (i + 1) * waveW;
            double midY = (i % 2 == 0) ? bottomY + waveA : bottomY - waveA;
            path.quadTo((sx + ex) / 2, midY, ex, bottomY);
        }
        path.closeSubpath();
        canvas->drawPath(path);

        QRectF textRect(leftX + 4 * zoom(), topY + 4 * zoom(),
                        p_b - 8 * zoom(), p_a - 8 * zoom());
        canvas->drawText(textRect, Qt::AlignCenter | Qt::TextWrapAnywhere, attributes.value("text", ""));
        canvas->drawLine(QLineF(hcenter, bottomY, hcenter, bottom+0.5));
      }

    }
    //canvas->drawText(x+8, y+12, type());
    for(int i = 0; i < items.size(); ++i)
    {
      item(i)->paint(canvas, fontSizeInPoints);
    }
  }
}

double QBlock::zoom() const
{
  if (flowChart())
  {
    return flowChart()->zoom();
  }
  else
  {
    return 1;
  }
}

QBlock * QBlock::blockAt(int px, int py)
{
  QRectF rect(x, y, width, height);
  if (!rect.contains(px, py)) return 0;
  else
  {
    for (int i = 0; i < items.size(); ++i)
    {
      QBlock *tmp = item(i)->blockAt(px, py);
      if (tmp) return tmp;
    }
    return this;
  }
}

QDomElement QBlock::xmlNode(QDomDocument & doc) const
{
  QDomElement self = doc.createElement(type());
  QList<QString> sl = attributes.keys();
  for (int i = 0; i < sl.size(); ++i)
  {
    if (sl.at(i) != "type")
    {
      self.setAttribute(sl.at(i), attributes.value(sl.at(i), QString()));
    }
  }


  for (int i = 0; i < items.size(); ++i)
  {
    QDomElement child = item(i)->xmlNode(doc);
    self.appendChild(child);
  }
  return self;
}

void QBlock::setXmlNode(const QDomElement & node)
{
  clear();
  setType(node.nodeName());
  QDomNamedNodeMap attrs = node.attributes();
  for (int i = 0; i < attrs.size(); ++i)
  {
    QDomAttr da = attrs.item(i).toAttr();
    if(da.name() != "type")
    {
      attributes.insert(da.name(), da.value());
    }
  }
  if (type() == "branch") isBranch = true;
  else isBranch = false;
  QDomNodeList children = node.childNodes();
  for(int i = 0; i < children.size(); ++i)
  {
    if (children.at(i).isElement())
    {
      QDomElement child = children.at(i).toElement();
      QBlock *block = new QBlock();
      block->setFlowChart(flowChart());
      block->setXmlNode(child);
      append(block);
    }
  }
}

void QBlock::insertXmlTree(int aIndex, const QDomElement & algorithm)
{
  if (isBranch)
  {
    QDomElement branch = algorithm.firstChildElement("branch");
    if(!branch.isNull())
    {
      QDomNodeList children = branch.childNodes();
      int ind = aIndex;
      for(int i = 0; i < children.size(); ++i)
      {
        if (children.at(i).isElement())
        {
          QDomElement child = children.at(i).toElement();
          QBlock *block = new QBlock();
          block->setFlowChart(flowChart());
          block->setXmlNode(child);
          insert(ind, block);
          ind++;
        }
      }
    }
  }
}

bool QBlock::isActive() const
{
  if(flowChart())
  {
    if (flowChart()->activeBlock() == this) return true;
    else if (parent)
    {
      return parent->isActive();
    }
    else
    {
      return false;
    }

  }
  else
    return false;
}
