#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "customitem.h"

#include <QMainWindow>
#include "customview.h"
#include <QDomDocument>
#include <memory>
#include "undosystem.h"

class CustomScene;

#include <QAction>
#include <QToolBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QButtonGroup>
#include <QLineEdit>
#include <QGraphicsTextItem>
#include <QFont>
#include <QToolButton>
#include <QAbstractButton>
#include <QGraphicsView>
#include <QDragEnterEvent>
#include <QDragMoveEvent>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

private slots:
    void backgroundButtonGroupClicked(QAbstractButton *button);
    void buttonGroupClicked(int id);
    void newFile();
    void openFile();
    void save();
    void saveAs();
    void copyItem();
    void pasteItem();
    void cutItem();
    void deleteItem();
    void undo();
    void redo();
    void groupItems();
    void ungroupItems();

    void rectangleAreaItem();
    void rectanglePerimeterItem();
    void circleItems();
    void triangleItems();
    void diamondItems();
    void polygonItems();

    void pointerGroupClicked(int id);
    void bringToFront();
    void sendToBack();
    void itemInserted(CustomItem *item);
    void textInserted(QGraphicsTextItem *item);
    void backupUndostack();
    void currentFontChanged(const QFont &font);
    void fontSizeChanged(const QString &size);
    void sceneScaleChanged(const QString &scale);
    void sceneScaleZooming(int delta);

    void textColorChanged();
    void itemColorChanged();
    void lineColorChanged();
    void textButtonTriggered();
    void fillButtonTriggered();
    void lineButtonTriggered();
    void handleFontChange();
    void itemSelected(QGraphicsItem *item);
    void about();


private:
    void createToolBox();
    void createActions();
    void createMenus();
    void createToolbars();

    void setCurrentFile(const QString &fileName);
    QWidget *createBackgroundCellWidget(const QString &text, const QString &image);
    QWidget *createCellWidget(const QString &text, CustomItem::CustomType type);
    QMenu *createColorMenu(const char *slot, QColor defaultColor);
    QIcon createColorToolButtonIcon(const QString &image, QColor color);
    QIcon createColorIcon(QColor color);

    QList<QGraphicsItem*> cloneItems(QList<QGraphicsItem*> const& items);

    CustomScene *scene;
    QGraphicsView *view;
    QString currentFile;

    QList<QGraphicsItem*> pasteBoard;
    UndoSystem undoStack;

    QAction *newAction;
    QAction *openAction;
    QAction *saveAction;
    QAction *saveAsAction;
    QAction *exitAction;
    QAction *addAction;
    QAction *deleteAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *cutAction;
    QAction *undoAction;
    QAction *redoAction;

    QAction *toFrontAction;
    QAction *sendBackAction;
    QAction *groupAction;
    QAction *ungroupAction;

    QAction *aboutAction;

    QMenu *fileMenu;
    QMenu *itemMenu;
    QMenu *aboutMenu;

    QToolBar *textToolBar;
    QToolBar *editToolBar;
    QToolBar *colorToolBar;
    QToolBar *pointerToolbar;

    QComboBox *sceneScaleCombo;
    QComboBox *itemColorCombo;
    QComboBox *textColorCombo;
    QComboBox *fontSizeCombo;
    QFontComboBox *fontCombo;

    QToolBox *toolBox;
    QButtonGroup *buttonGroup;
    QButtonGroup *pointerTypeGroup;
    QButtonGroup *backgroundButtonGroup;
    QToolButton *fontColorToolButton;
    QToolButton *fillColorToolButton;
    QToolButton *lineColorToolButton;

//    QAction *rectangleAction;
    QAction *rectangleArea;
    QAction *rectanglePerimeter;
//    QAction *circleAction;
    QAction *circleArea;
    QAction *circleCircumference;
//    QAction *triangleAction;
    QAction *triangleArea;
    QAction *trianglePerimeter;
    QAction *polygonAction;
    QAction *diamondAction;

    QAction *boldAction;
    QAction *underlineAction;
    QAction *italicAction;
    QAction *textAction;
    QAction *fillAction;
    QAction *lineAction;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;


};

#endif // MAINWINDOW_H
