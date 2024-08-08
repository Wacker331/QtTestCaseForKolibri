#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QDebug>
#include <QButtonGroup>
#include <QThread>
#include <QMessageBox>
#include <windows.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Worker : public QObject {
    Q_OBJECT
public slots:
    void doWork();

signals:
    void workFinished();

    friend class MainWindow;
protected:
    enum operation
    {
        NONE,
        OR,
        XOR,
        AND,
        NOT
    }MainOperation = NONE;
    uint64_t MainConstant = 0x0;
    QString InputPath = "";
    QString OutputPath = "";
    QString FileMask = "";
    bool DeleteInputFiles = false, ReWriteCopies = false, BoolTimer = false, StopFlag = false;
    QTime Timer;
    int FilesNum = 0;

    int FindFiles();
    int ModifyFile(HANDLE hFile, const std::wstring OutputFilename);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_TimerButton_toggled(bool checked);

    void on_ConstantInput_editingFinished();
    void SetOperation();

    void on_DeleteCheckBox_stateChanged(int arg1);

    void on_RewriteButton_toggled(bool checked);

    void on_TimerCount_userTimeChanged(const QTime &time);

    void on_DirectoryInput_editingFinished();

    void on_FileMaskInput_editingFinished();

    void on_OutputDirectoryInput_editingFinished();

    void on_StopButton_clicked();

    void on_LaunchButton_clicked();

private:
    Ui::MainWindow *ui;

    void LaunchCheck();
    void ThreadFinished();

    QThread *workerThread;
    Worker *worker;
};
#endif // MAINWINDOW_H
