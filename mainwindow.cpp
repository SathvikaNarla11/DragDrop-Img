#include "arrow.h"
#include "customitem.h"
#include "customscene.h"
#include "customtextitem.h"
#include "mainwindow.h"

#include <QDomDocument>
#include <QtWidgets>
#include <QMimeData>

#include <QApplication>
#include <QMouseEvent>
#include <QPixmap>

#include <QByteArray>

const int InsertTextButton = 10;

MainWindow::MainWindow()
{
    currentFile = "";
    createActions();
    createToolBox();
    createMenus();

    scene = new CustomScene(itemMenu, this);
    scene->setSceneRect(QRectF(0, 0, 5000, 5000));
    connect(scene, SIGNAL(itemInserted(CustomItem*)),this, SLOT(itemInserted(CustomItem*)));
    connect(scene, SIGNAL(textInserted(QGraphicsTextItem*)),this, SLOT(textInserted(QGraphicsTextItem*)));
    connect(scene, SIGNAL(arrowInserted()),this, SLOT(backupUndostack()));
    connect(scene, SIGNAL(textChanged()), this, SLOT(backupUndostack()));
    connect(scene, SIGNAL(itemSelected(QGraphicsItem*)),this, SLOT(itemSelected(QGraphicsItem*)));
    connect(scene, SIGNAL(scaleChanging(int)),this, SLOT(sceneScaleZooming(int)));


    createToolbars();

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(toolBox);
    view = new CustomView(scene);
    layout->addWidget(view);

    connect(view, SIGNAL(needsUndoBackUp()), this, SLOT(backupUndostack()));

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    setCentralWidget(widget);
    setWindowTitle(tr("Demo Project"));
    setUnifiedTitleAndToolBarOnMac(true);

    undoStack.backup(QList<QGraphicsItem*>());
}

void MainWindow::backgroundButtonGroupClicked(QAbstractButton *button)
{
    QList<QAbstractButton *> buttons = backgroundButtonGroup->buttons();
    foreach (QAbstractButton *myButton, buttons)
    {
        if (myButton != button)
            button->setChecked(false);
    }
    QString text = button->text();

    if (text == tr("Blue Grid"))
        scene->setBackgroundBrush(QPixmap(":/Icon/background1.png"));
    else if (text == tr("White Grid"))
        scene->setBackgroundBrush(QPixmap(":/Icon/background2.png"));
    else if (text == tr("Gray Grid"))
        scene->setBackgroundBrush(QPixmap(":/Icon/background3.png"));
    else
        scene->setBackgroundBrush(QPixmap(":/Icon/background4.png"));

    scene->update();
    view->update();
}

void MainWindow::buttonGroupClicked(int id)
{
    QList<QAbstractButton *> buttons = buttonGroup->buttons();
    QAbstractButton* clickedButton = buttonGroup->button(id);

    // set other button unchecked
    foreach (QAbstractButton *button, buttons)
    {
        if (clickedButton != button)
            button->setChecked(false);
    }

    // simply set objButton unchecked if already checked
    if (!clickedButton->isChecked())
    {
        scene->setMode(CustomScene::Mode(pointerTypeGroup->checkedId()));
        return;
    }

    if (id == InsertTextButton)
    {
        scene->setMode(CustomScene::InsertText);
    }
    else
    {
        scene->setItemType(CustomItem::CustomType(id));
        scene->setMode(CustomScene::InsertItem);
    }
}

void MainWindow::newFile()
{
    scene->clear();
    setCurrentFile(QString());

}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("XML Files (*.xml)"));
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("Open File"), tr("Cannot open file %1:\n%2.").arg(fileName).arg(file.errorString()));
        return;
    }

    if (fileName.endsWith(".xml"))
    {
        QDomDocument doc;
        if (!doc.setContent(&file))
        {
            QMessageBox::warning(this, tr("Open File"), tr("Cannot parse file %1:\n%2.").arg(fileName).arg(file.errorString()));
            return;
        }
        QDomElement root = doc.documentElement();
        scene->loadFromXml(root);
    }

    setCurrentFile(fileName);

}

void MainWindow::save()
{
    if (currentFile.isEmpty())
    {
        saveAs();
    }
    else
    {
        QFile file(currentFile);
        if (!file.open(QIODevice::WriteOnly))
        {
            QMessageBox::warning(this, tr("Save File"), tr("Cannot save file %1:\n%2.").arg(currentFile).arg(file.errorString()));
            return;
        }

        if (currentFile.endsWith(".xml"))
        {
            QDomDocument doc;
            QDomElement root = doc.createElement("scene");
            doc.appendChild(root);
            scene->saveToXml(doc, root);
            QTextStream stream(&file);
            stream << doc.toString();
        }

        setCurrentFile(currentFile);
    }

}

void MainWindow::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As File"), "", tr("XML Files (*.xml)"));
    if (fileName.isEmpty())
        return;

    setCurrentFile(fileName);
    save();

}

void MainWindow::copyItem()
{
    foreach(QGraphicsItem* p, pasteBoard)
    {
        delete p;
    }
    pasteBoard = cloneItems(scene->selectedItems());
    qDebug() << pasteBoard.size();
}

void MainWindow::pasteItem()
{
    QList<QGraphicsItem*> pasteBoardCopy(cloneItems(pasteBoard));
    foreach(QGraphicsItem* p, scene->items()) p->setSelected(false);

    foreach(QGraphicsItem* item, pasteBoard)
    {
        if (item->type() != Arrow::Type)
        {
            item->setPos(item->scenePos() + QPointF(20, 20));
            item->setZValue(item->zValue() + 0.1);
        }
        scene->addItem(item);
        item->setSelected(true);
    }
    pasteBoard.swap(pasteBoardCopy);
    if (!scene->selectedItems().empty())
        undoStack.backup(cloneItems(scene->items()));
}

void MainWindow::cutItem()
{
    copyItem();
    deleteItem();
}

void MainWindow::deleteItem()
{
    bool needsBackup = !scene->selectedItems().empty();
    scene->deleteItems(scene->selectedItems());
    if (needsBackup)
        undoStack.backup(cloneItems(scene->items()));
}

void MainWindow::undo()
{
    if (undoStack.isEmpty()) return;

    scene->deleteItems(scene->items());
    QList<QGraphicsItem*> undoneItems = cloneItems(undoStack.undo());
    foreach(QGraphicsItem* item, undoneItems)
    {
        qDebug() << item << "--------- add item";
        scene->addItem(item);
    }


    foreach(QGraphicsItem* item, undoneItems)
    {
        if (item->type() == Arrow::Type)
            qgraphicsitem_cast<Arrow*>(item)->updatePosition();
    }
}

void MainWindow::redo()
{
    if (undoStack.isFull()) return;
    scene->deleteItems(scene->items());
    QList<QGraphicsItem*> redoneItems = cloneItems(undoStack.redo());
    foreach(QGraphicsItem* item, redoneItems)
    {
        scene->addItem(item);
    }

    foreach(QGraphicsItem* item, redoneItems)
    {
        if (item->type() == Arrow::Type)
            qgraphicsitem_cast<Arrow*>(item)->updatePosition();
    }
}

void MainWindow::groupItems()
{
    QGraphicsItemGroup* group = scene->createItemGroup(scene->selectedItems());
    group->setFlag(QGraphicsItem::ItemIsMovable, true);
    group->setFlag(QGraphicsItem::ItemIsSelectable, true);
    group->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    scene->addItem(group);
    backupUndostack();
}

void MainWindow::ungroupItems()
{
    foreach(QGraphicsItem* p, scene->selectedItems())
    {
        if (p->type() == QGraphicsItemGroup::Type)
        {
            scene->destroyItemGroup(qgraphicsitem_cast<QGraphicsItemGroup*>(p));
        }
    }
    backupUndostack();
}

void MainWindow::rectangleAreaItem()
{
    qDebug() << "Rectangle Action Triggerd";

    qDebug () << "Area of Reactangle : " << CustomItem::rectangleArea;

}
void MainWindow::rectanglePerimeterItem()
{
    qDebug() << "Rectangle Perimeter Action Triggerd";

    qDebug () << "Perimeter of Reactangle : " << CustomItem::rectanglePerimeter;

}

void MainWindow::circleItems()
{
    qDebug() << "Circle Action Triggerd";
    qDebug () << "Area of Circle : " << CustomItem::circleArea;
    qDebug () << "Circle Circumference : " << CustomItem::circleCircumference;

}

void MainWindow::triangleItems()
{
    qDebug () << "Area of Triangle : " << CustomItem::triangleArea;
    qDebug () << "Perimeter of Triangle : " << CustomItem::trianglePerimeter;
}

void MainWindow::polygonItems()
{
    qDebug() << "Polygon Action Triggerd";
}

void MainWindow::diamondItems()
{
    qDebug() << "Diamond Action Triggerd";
}

void MainWindow::pointerGroupClicked(int)
{
    foreach(QAbstractButton* b, buttonGroup->buttons())
    {
        b->setChecked(false);
    }
    scene->setMode(CustomScene::Mode(pointerTypeGroup->checkedId()));
}

void MainWindow::bringToFront()
{
    if (scene->selectedItems().isEmpty())
        return;

    QGraphicsItem *selectedItem = scene->selectedItems().first();
    QList<QGraphicsItem *> overlapItems = selectedItem->collidingItems();

    qreal zValue = 0;
    foreach (QGraphicsItem *item, overlapItems) {
        if (item->zValue() >= zValue && item->type() == CustomItem::Type)
            zValue = item->zValue() + 0.1;
    }
    selectedItem->setZValue(zValue);
    backupUndostack();
}

void MainWindow::sendToBack()
{
    if (scene->selectedItems().isEmpty())
        return;

    QGraphicsItem *selectedItem = scene->selectedItems().first();
    QList<QGraphicsItem *> overlapItems = selectedItem->collidingItems();

    qreal zValue = 0;
    foreach (QGraphicsItem *item, overlapItems) {
        if (item->zValue() <= zValue && item->type() == CustomItem::Type)
            zValue = item->zValue() - 0.1;
    }
    selectedItem->setZValue(zValue);
    backupUndostack();
}

void MainWindow::itemInserted(CustomItem *item)
{
    pointerTypeGroup->button(int(CustomScene::MoveItem))->setChecked(true);
    scene->setMode(CustomScene::Mode(pointerTypeGroup->checkedId()));
    buttonGroup->button(int(item->customType()))->setChecked(false);
    backupUndostack();
}

void MainWindow::textInserted(QGraphicsTextItem *)
{
    buttonGroup->button(InsertTextButton)->setChecked(false);
    scene->setMode(CustomScene::Mode(pointerTypeGroup->checkedId()));
}

void MainWindow::backupUndostack()
{
    undoStack.backup(cloneItems(scene->items()));
}

void MainWindow::currentFontChanged(const QFont &)
{
    handleFontChange();
}

void MainWindow::fontSizeChanged(const QString &)
{
    handleFontChange();
}

void MainWindow::sceneScaleChanged(const QString &scale)
{
    double newScale = scale.toDouble() / 100.0;
    QMatrix oldMatrix = view->matrix();
    view->resetMatrix();
    view->translate(oldMatrix.dx(), oldMatrix.dy());
    view->scale(newScale, newScale);
}

void MainWindow::sceneScaleZooming(int delta)
{
    int changingPercent = delta > 0 ? 10 : -10;
    QString comboText = sceneScaleCombo->currentText();
    int newScale = comboText.toInt() + changingPercent;

    if (newScale > 0 && newScale <= 200)
    {
        sceneScaleCombo->setCurrentText(QString().number(newScale));
    }
}

void MainWindow::textColorChanged()
{
    textAction = qobject_cast<QAction *>(sender());
    fontColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/textpointer.png",
                                                           qvariant_cast<QColor>(textAction->data())));
    textButtonTriggered();
    undoStack.backup(cloneItems(scene->items()));
}

void MainWindow::itemColorChanged()
{
    fillAction = qobject_cast<QAction *>(sender());
    fillColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/floodfill.png",
                                                           qvariant_cast<QColor>(fillAction->data())));
    fillButtonTriggered();
    undoStack.backup(cloneItems(scene->items()));
}

void MainWindow::lineColorChanged()
{
    lineAction = qobject_cast<QAction *>(sender());
    lineColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/linecolor.png",
                                                           qvariant_cast<QColor>(lineAction->data())));
    lineButtonTriggered();
}

void MainWindow::textButtonTriggered()
{
    scene->setTextColor(qvariant_cast<QColor>(textAction->data()));
}

void MainWindow::fillButtonTriggered()
{
    scene->setItemColor(qvariant_cast<QColor>(fillAction->data()));
}

void MainWindow::lineButtonTriggered()
{
    scene->setLineColor(qvariant_cast<QColor>(lineAction->data()));
}

void MainWindow::handleFontChange()
{
    QFont font = fontCombo->currentFont();
    font.setPointSize(fontSizeCombo->currentText().toInt());
    font.setWeight(boldAction->isChecked() ? QFont::Bold : QFont::Normal);
    font.setItalic(italicAction->isChecked());
    font.setUnderline(underlineAction->isChecked());

    scene->setFont(font);
}

void MainWindow::itemSelected(QGraphicsItem *item)
{
    CustomTextItem *textItem = qgraphicsitem_cast<CustomTextItem *>(item);

    QFont font = textItem->font();
    fontCombo->setCurrentFont(font);
    fontSizeCombo->setEditText(QString().setNum(font.pointSize()));
    boldAction->setChecked(font.weight() == QFont::Bold);
    italicAction->setChecked(font.italic());
    underlineAction->setChecked(font.underline());
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Demo Project"), tr("A drawing tool."));
}


void MainWindow::createToolBox()
{
    buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(false);
    connect(buttonGroup, SIGNAL(buttonClicked(int)), this, SLOT(buttonGroupClicked(int)));
    QGridLayout *layout = new QGridLayout;

    QWidget *itemWidget = new QWidget;
    itemWidget->setLayout(layout);

    for (int i = 0; i < 7; ++i)
    {
        QToolButton *imageButton = new QToolButton;
        imageButton->setCheckable(true);
        buttonGroup->addButton(imageButton);
        QGridLayout *imageLayout = new QGridLayout;
        switch(i){
        case 0:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_black.png"))));
            imageLayout->addWidget(new QLabel("Tractor Black"), 1, 0, Qt::AlignCenter);
            break;
        case 1:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_ok.png"))));
            imageLayout->addWidget(new QLabel("Tractor Ok"), 1, 0, Qt::AlignCenter);
            break;
        case 2:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_On_Field.png"))));
            imageLayout->addWidget(new QLabel("Tractor On Field"), 1, 0, Qt::AlignCenter);
            break;
        case 3:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_orange.png"))));
            imageLayout->addWidget(new QLabel("Tractor Orange"), 1, 0, Qt::AlignCenter);
            break;
        case 4:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_red.png"))));
            imageLayout->addWidget(new QLabel("Tractor Red"), 1, 0, Qt::AlignCenter);
            break;
        case 5:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_transperant.png"))));
            imageLayout->addWidget(new QLabel("Tractor Transperant"), 1, 0, Qt::AlignCenter);
            break;
        case 6:
            imageButton->setIcon(QIcon(QPixmap(QString(":/Icon/tractor_yellow.png"))));
            imageLayout->addWidget(new QLabel("Tractor Yellow"), 1, 0, Qt::AlignCenter);
            break;
        }
        imageButton->setIconSize(QSize(50, 50));
        imageLayout->addWidget(imageButton, 0, 0, Qt::AlignHCenter);
        QWidget *imageWidget = new QWidget;
        imageWidget->setLayout(imageLayout);
        layout->addWidget(imageWidget, 0 + i / 2, i % 2);

        imageButton->setAcceptDrops(false);
        imageButton->installEventFilter(this);

    }

    layout->addWidget(createCellWidget(tr("OutPut"), CustomItem::Output), 8, 0);
    layout->addWidget(createCellWidget(tr("Rectangle"), CustomItem::Rectangle),8, 1);
    layout->addWidget(createCellWidget(tr("Triangle"), CustomItem::Triangle), 9, 0);
    layout->addWidget(createCellWidget(tr("Circle"), CustomItem::Circle), 9, 1);
    layout->addWidget(createCellWidget(tr("Polygon"), CustomItem::Io), 10, 0);
    layout->addWidget(createCellWidget(tr("Diamond"), CustomItem::Diamond), 10, 1);

    QToolButton *textButton = new QToolButton;
    textButton->setCheckable(true);
    buttonGroup->addButton(textButton, InsertTextButton);
    textButton->setIcon(QIcon(QPixmap(":/Icon/textpointer.png")));
    textButton->setIconSize(QSize(50, 50));
    QGridLayout *textLayout = new QGridLayout;
    textLayout->addWidget(textButton, 0, 0, Qt::AlignHCenter);
    textLayout->addWidget(new QLabel(tr("Text")), 1, 0, Qt::AlignCenter);
    QWidget *textWidget = new QWidget;
    textWidget->setLayout(textLayout);
    layout->addWidget(textWidget, 7, 0);

    layout->setRowStretch(7, 10);
    layout->setColumnStretch(2, 10);


    layout->setRowStretch(6, 10);
    layout->setColumnStretch(2, 10);

    backgroundButtonGroup = new QButtonGroup(this);
    connect(backgroundButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)),this, SLOT(backgroundButtonGroupClicked(QAbstractButton*)));

    QGridLayout *backgroundLayout = new QGridLayout;
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("Blue Grid"),":/Icon/background1.png"), 0, 0);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("White Grid"),":/Icon/background2.png"), 0, 1);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("Gray Grid"), ":/Icon/background3.png"), 1, 0);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("No Grid"),":/Icon/background4.png"), 1, 1);

    backgroundLayout->setRowStretch(2, 10);
    backgroundLayout->setColumnStretch(2, 10);

    QWidget *backgroundWidget = new QWidget;
    backgroundWidget->setLayout(backgroundLayout);

    toolBox = new QToolBox;
    toolBox->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored));
    toolBox->setMinimumWidth(itemWidget->sizeHint().width());
    toolBox->addItem(itemWidget, tr("Basic Flowchart Shapes"));
    toolBox->addItem(backgroundWidget, tr("Backgrounds"));
}

void MainWindow::createActions()
{
    newAction = new QAction(tr("N&ew"), this);
    newAction->setShortcuts(QKeySequence::New);
    newAction->setStatusTip(tr("Ctrl+N"));
    connect(newAction, SIGNAL(triggered()), this, SLOT(newFile()));

    openAction = new QAction(tr("&Open"), this);
    openAction->setShortcuts(QKeySequence::Open);
    openAction->setStatusTip(tr("Ctrl+O"));
    connect(openAction, SIGNAL(triggered()), this, SLOT(openFile()));

    saveAction = new QAction(tr("S&ave"), this);
    saveAction->setShortcuts(QKeySequence::Save);
    saveAction->setStatusTip(tr("Ctrl+S"));
    connect(saveAction, SIGNAL(triggered()), this, SLOT(save()));

    saveAsAction = new QAction(tr("S&ave As"), this);
    saveAsAction->setShortcuts(QKeySequence::SaveAs);
    saveAsAction->setShortcut(tr("Ctrl+Shift+S"));
    connect(saveAsAction, SIGNAL(triggered()), this, SLOT(saveAs()));

    toFrontAction = new QAction(QIcon(":/Icon/bringtofront.png"), tr("Bring to &Front"), this);
    toFrontAction->setShortcut(tr("Ctrl+F"));
    toFrontAction->setStatusTip(tr("Bring item to front"));
    connect(toFrontAction, SIGNAL(triggered()), this, SLOT(bringToFront()));

    sendBackAction = new QAction(QIcon(":/Icon/sendtoback.png"), tr("Send to &Back"), this);
    sendBackAction->setShortcut(tr("Ctrl+T"));
    sendBackAction->setStatusTip(tr("Send item to back"));
    connect(sendBackAction, SIGNAL(triggered()), this, SLOT(sendToBack()));

    deleteAction = new QAction(QIcon(":/Icon/delete.png"), tr("&Delete"), this);
    deleteAction->setShortcut(tr("Delete"));
    deleteAction->setStatusTip(tr("Delete item"));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteItem()));

    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Quit"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    boldAction = new QAction(tr("Bold"), this);
    boldAction->setCheckable(true);
    QPixmap pixmap(":/Icon/bold.png");
    boldAction->setIcon(QIcon(pixmap));
    boldAction->setShortcut(tr("Ctrl+B"));
    connect(boldAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    italicAction = new QAction(QIcon(":/Icon/italic.png"), tr("Italic"), this);
    italicAction->setCheckable(true);
    italicAction->setShortcut(tr("Ctrl+I"));
    connect(italicAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    underlineAction = new QAction(QIcon(":/Icon/underline.png"), tr("Underline"), this);
    underlineAction->setCheckable(true);
    underlineAction->setShortcut(tr("Ctrl+U"));
    connect(underlineAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    aboutAction = new QAction(tr("A&bout"), this);
    aboutAction->setShortcut(tr("F1"));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    copyAction = new QAction(QIcon(":/Icon/copy.png"), tr("C&opy"), this);
    copyAction->setShortcut(tr("Ctrl+C"));
    copyAction->setStatusTip(tr("Copy items"));
    connect(copyAction, SIGNAL(triggered()), this, SLOT(copyItem()));

    pasteAction = new QAction(QIcon(":/Icon/paste.png"), tr("P&aste"), this);
    pasteAction->setShortcut(tr("Ctrl+V"));
    pasteAction->setStatusTip(tr("Paste items"));
    connect(pasteAction, SIGNAL(triggered()), this, SLOT(pasteItem()));

    cutAction = new QAction(QIcon(":/Icon/cut.png"), tr("C&ut"), this);
    cutAction->setShortcut(tr("Ctrl+X"));
    cutAction->setStatusTip(tr("Cut items"));
    connect(cutAction, SIGNAL(triggered()), this, SLOT(cutItem()));

    undoAction = new QAction(QIcon(":Icon/undo.png"), tr("U&ndo"), this);
    undoAction->setShortcut(tr("Ctrl+Z"));
    undoAction->setStatusTip(tr("Undo last operation"));
    connect(undoAction, SIGNAL(triggered()), this, SLOT(undo()));

    redoAction = new QAction(QIcon(":Icon/redo.png"), tr("R&edo"), this);
    redoAction->setShortcut(tr("Ctrl+Shift+Z"));
    redoAction->setStatusTip(tr("Redo last operation"));
    connect(redoAction, SIGNAL(triggered()), this, SLOT(redo()));

    groupAction = new QAction(QIcon(":Icon/group.png"), tr("G&roup"), this);
    groupAction->setStatusTip(tr("Group graphic items "));
    connect(groupAction, SIGNAL(triggered()), this, SLOT(groupItems()));

    ungroupAction = new QAction(QIcon(":Icon/ungroup.png"), tr("U&ngroup"), this);
    ungroupAction->setStatusTip(tr("Ungroup graphic items"));
    connect(ungroupAction, SIGNAL(triggered()), this, SLOT(ungroupItems()));

    rectangleArea = new QAction (tr("&Rectangle Area"), this);
    connect(rectangleArea, SIGNAL(triggered()), this, SLOT(rectangleAreaItem()));

    rectanglePerimeter = new QAction (tr("&Rectangle Perimeter"), this);
    connect(rectanglePerimeter, SIGNAL(triggered()), this, SLOT(rectanglePerimeterItem()));

    circleArea = new QAction(tr("&Circle Area"), this);
    connect(circleArea, SIGNAL(triggered()), this, SLOT(circleItems()));

    circleCircumference = new QAction(tr("&Circle Circumference"), this);
    connect(circleCircumference, SIGNAL(triggered()), this, SLOT(circleItems()));

    triangleArea = new QAction(tr("&Triangle Area"), this);
    connect(triangleArea, SIGNAL(triggered()), this, SLOT(triangleItems()));

    trianglePerimeter = new QAction(tr("&Triangle Perimeter"), this);
    connect(trianglePerimeter, SIGNAL(triggered()), this, SLOT(triangleItems()));

    diamondAction = new QAction(tr("&Diamond"), this);
    connect(diamondAction, SIGNAL(triggered()), this, SLOT(diamondItems()));

    polygonAction = new QAction(tr("&Polygon"), this);
    connect(polygonAction, SIGNAL(triggered()), this, SLOT(polygonItems()));

}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction);
    fileMenu->addAction(openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction);
    fileMenu->addAction(saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    itemMenu = menuBar()->addMenu(tr("&Edit"));

    QMenu *propertyMenu = itemMenu->addMenu(tr("Reactangle"));
    propertyMenu->addAction(rectangleArea);
    propertyMenu->addAction(rectanglePerimeter);
    propertyMenu->addSeparator();

    propertyMenu = itemMenu->addMenu(tr("&Circle"));
    propertyMenu->addAction(circleArea);
    propertyMenu->addAction(circleCircumference);
    propertyMenu->addSeparator();

    propertyMenu = itemMenu->addMenu(tr("&Triangle"));
    propertyMenu->addAction(triangleArea);
    propertyMenu->addAction(trianglePerimeter);
    propertyMenu->addSeparator();

    propertyMenu->addAction(diamondAction);
    propertyMenu->addAction(polygonAction);

    itemMenu->addAction(copyAction);
    itemMenu->addAction(cutAction);
    itemMenu->addAction(pasteAction);
    itemMenu->addAction(deleteAction);
    itemMenu->addSeparator();
    itemMenu->addAction(undoAction);
    itemMenu->addAction(redoAction);
    itemMenu->addSeparator();
    itemMenu->addAction(groupAction);
    itemMenu->addAction(ungroupAction);
    itemMenu->addSeparator();
    itemMenu->addAction(toFrontAction);
    itemMenu->addAction(sendBackAction);
    itemMenu->addSeparator();


    aboutMenu = menuBar()->addMenu(tr("&Help"));
    aboutMenu->addAction(aboutAction);
}

void MainWindow::createToolbars()
{
    editToolBar = addToolBar(tr("Edit"));
    editToolBar->addAction(copyAction);
    editToolBar->addAction(cutAction);
    editToolBar->addAction(pasteAction);
    editToolBar->addAction(deleteAction);
    editToolBar->addAction(undoAction);
    editToolBar->addAction(redoAction);
    editToolBar->addAction(toFrontAction);
    editToolBar->addAction(sendBackAction);
    editToolBar->addAction(groupAction);
    editToolBar->addAction(ungroupAction);
    removeToolBar(editToolBar);
    addToolBar(Qt::LeftToolBarArea, editToolBar);
    editToolBar->show();

    fontCombo = new QFontComboBox();
    connect(fontCombo, SIGNAL(currentFontChanged(QFont)),this, SLOT(currentFontChanged(QFont)));
    fontCombo->setEditable(false);

    fontSizeCombo = new QComboBox;
    fontSizeCombo->setEditable(true);
    for (int i = 12; i < 30; i = i + 2)
        fontSizeCombo->addItem(QString().setNum(i));
    QIntValidator *validator = new QIntValidator(2, 64, this);
    fontSizeCombo->setValidator(validator);
    connect(fontSizeCombo, SIGNAL(currentIndexChanged(QString)), this, SLOT(fontSizeChanged(QString)));

    fontColorToolButton = new QToolButton;
    fontColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    fontColorToolButton->setMenu(createColorMenu(SLOT(textColorChanged()), Qt::black));
    textAction = fontColorToolButton->menu()->defaultAction();
    fontColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/textpointer.png", Qt::black));
    fontColorToolButton->setAutoFillBackground(true);
    connect(fontColorToolButton, SIGNAL(clicked()), this, SLOT(textButtonTriggered()));

    fillColorToolButton = new QToolButton;
    fillColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    fillColorToolButton->setMenu(createColorMenu(SLOT(itemColorChanged()), Qt::white));
    fillAction = fillColorToolButton->menu()->defaultAction();
    fillColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/floodfill.png", Qt::white));
    connect(fillColorToolButton, SIGNAL(clicked()), this, SLOT(fillButtonTriggered()));


    lineColorToolButton = new QToolButton;
    lineColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    lineColorToolButton->setMenu(createColorMenu(SLOT(lineColorChanged()), Qt::black));
    lineAction = lineColorToolButton->menu()->defaultAction();
    lineColorToolButton->setIcon(createColorToolButtonIcon(":/Icon/linecolor.png", Qt::black));
    connect(lineColorToolButton, SIGNAL(clicked()), this, SLOT(lineButtonTriggered()));

    textToolBar = addToolBar(tr("Font"));
    textToolBar->addWidget(fontCombo);
    textToolBar->addWidget(fontSizeCombo);
    textToolBar->addAction(boldAction);
    textToolBar->addAction(italicAction);
    textToolBar->addAction(underlineAction);

    colorToolBar = addToolBar(tr("Color"));
    colorToolBar->addWidget(fontColorToolButton);
    colorToolBar->addWidget(fillColorToolButton);
    colorToolBar->addWidget(lineColorToolButton);

    QToolButton *pointerButton = new QToolButton;
    pointerButton->setCheckable(true);
    pointerButton->setChecked(true);
    pointerButton->setIcon(QIcon(":/Icon/pointer.png"));
    QToolButton *linePointerButton = new QToolButton;
    linePointerButton->setCheckable(true);
    linePointerButton->setIcon(QIcon(":/Icon/linepointer.png"));

    pointerTypeGroup = new QButtonGroup(this);
    pointerTypeGroup->addButton(pointerButton, int(CustomScene::MoveItem));
    pointerTypeGroup->addButton(linePointerButton, int(CustomScene::InsertLine));
    connect(pointerTypeGroup, SIGNAL(buttonClicked(int)),this, SLOT(pointerGroupClicked(int)));

    sceneScaleCombo = new QComboBox;
    QStringList scales;
    scales << tr("50") << tr("75") << tr("100") << tr("125") << tr("150");
    sceneScaleCombo->addItems(scales);
    sceneScaleCombo->setCurrentIndex(2);
    sceneScaleCombo->setEditable(true);
    QIntValidator *scaleValidator = new QIntValidator(1, 200, this);
    sceneScaleCombo->setValidator(scaleValidator);
    connect(sceneScaleCombo, SIGNAL(currentTextChanged(QString)), this, SLOT(sceneScaleChanged(QString)));
    QLabel* percentLabel = new QLabel(tr("%"), this);

    pointerToolbar = addToolBar(tr("Pointer type"));
    pointerToolbar->addWidget(pointerButton);
    pointerToolbar->addWidget(linePointerButton);
    pointerToolbar->addWidget(sceneScaleCombo);
    pointerToolbar->addWidget(percentLabel);

}

void MainWindow::setCurrentFile(const QString &fileName)
{
    currentFile = fileName;
    setWindowModified(false);
    setWindowFilePath(currentFile.isEmpty() ? tr("untitled.xml") : currentFile);

}

QWidget *MainWindow::createBackgroundCellWidget(const QString &text, const QString &image)
{
    QToolButton *button = new QToolButton;
    button->setText(text);
    button->setIcon(QIcon(image));
    button->setIconSize(QSize(50, 50));
    button->setCheckable(true);
    backgroundButtonGroup->addButton(button);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(button, 0, 0, Qt::AlignHCenter);
    layout->addWidget(new QLabel(text), 1, 0, Qt::AlignCenter);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    return widget;
}

QWidget *MainWindow::createCellWidget(const QString &text, CustomItem::CustomType type)
{

    CustomItem item(type, itemMenu);
    QIcon icon(item.image());

    QToolButton *button = new QToolButton;
    button->setIcon(icon);
    button->setIconSize(QSize(50, 50));
    button->setCheckable(true);
    buttonGroup->addButton(button, int(type));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(button, 0, 0, Qt::AlignHCenter);
    layout->addWidget(new QLabel(text), 1, 0, Qt::AlignCenter);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    return widget;
}

QMenu *MainWindow::createColorMenu(const char *slot, QColor defaultColor)
{
    QList<QColor> colors;
    colors << Qt::black << Qt::white << Qt::red << Qt::blue << Qt::yellow;
    QStringList names;
    names << tr("black") << tr("white") << tr("red") << tr("blue")
          << tr("yellow");

    QMenu *colorMenu = new QMenu(this);
    for (int i = 0; i < colors.count(); i++)
    {
        QAction *action = new QAction(names.at(i), this);
        action->setData(colors.at(i));
        action->setIcon(createColorIcon(colors.at(i)));
        connect(action, SIGNAL(triggered()), this, slot);
        colorMenu->addAction(action);
        if (colors.at(i) == defaultColor)
            colorMenu->setDefaultAction(action);
    }
    return colorMenu;
}

QIcon MainWindow::createColorToolButtonIcon(const QString &imageFile, QColor color)
{
    QPixmap pixmap(50, 80);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QPixmap image(imageFile);
    // Draw icon centred horizontally on button.
    QRect target(4, 0, 42, 43);
    QRect source(0, 0, 42, 43);
    painter.fillRect(QRect(0, 60, 50, 80), color);
    painter.drawPixmap(target, image, source);

    return QIcon(pixmap);
}

QIcon MainWindow::createColorIcon(QColor color)
{
    QPixmap pixmap(20, 20);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(0, 0, 20, 20), color);

    return QIcon(pixmap);
}

QList<QGraphicsItem*> MainWindow::cloneItems(const QList<QGraphicsItem*>& items)
{
    QHash<QGraphicsItem*, QGraphicsItem*> copyMap;
    foreach (QGraphicsItem* item, items)
    {
        if (item->type() == CustomItem::Type)
        {
            copyMap[item] = qgraphicsitem_cast<CustomItem*>(item)->clone();
        }
        else if (item->type() == CustomTextItem::Type)
        {
            copyMap[item] = qgraphicsitem_cast<CustomTextItem*>(item)->clone();
        }
    }
    foreach (QGraphicsItem* item, items)
    {
        if (item->type() == Arrow::Type)
        {
            Arrow* arrow = qgraphicsitem_cast<Arrow*>(item);
            CustomItem* copiedStartItem = qgraphicsitem_cast<CustomItem*>(copyMap.value(arrow->startItem(), nullptr));
            CustomItem* copiedEndItem = qgraphicsitem_cast<CustomItem*>(copyMap.value(arrow->endItem(), nullptr));

            if (copiedStartItem == nullptr || copiedEndItem == nullptr) continue;

            Arrow* newArrow = new Arrow(copiedStartItem, copiedEndItem, nullptr);
            newArrow->setColor(arrow->getColor());

            copiedStartItem->addArrow(newArrow);
            copiedEndItem->addArrow(newArrow);
            newArrow->setZValue(-1000.0);

            copyMap[item] = newArrow;
        }
    }
    return copyMap.values();
}


bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QToolButton *button = qobject_cast<QToolButton *>(watched);
            if (button)
            {
                QMimeData *mimeData = new QMimeData;
                mimeData->setText(button->text());

                QPixmap pixmap = button->icon().pixmap(button->iconSize());

                QDrag *drag = new QDrag(this);
                drag->setMimeData(mimeData);
                drag->setPixmap(pixmap);
                drag->setHotSpot(mouseEvent->pos());

                drag->exec(Qt::CopyAction);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-image"))
    {
        event->acceptProposedAction();
    }
}
void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasText())
    {
        QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap(QString(":/Icon/%1.png").arg(event->mimeData()->text().toLower().replace(" ", "_"))));
        item->setPos(event->pos());
        scene->addItem(item);
        event->acceptProposedAction();
    }
}
