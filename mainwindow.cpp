#include "mainwindow.h"
#include "./ui_mainwindow.h"

void Worker::doWork()
{
    CreateDirectory(OutputPath.toStdWString().c_str(), NULL);
    if (BoolTimer)
    {
        while (StopFlag == false)
        {
            FilesNum = FindFiles();
            QThread::sleep(Timer.hour() * 60 * 60 + Timer.minute() * 60 + Timer.second());
        }
        StopFlag = false;
    }
    else
    {
        FilesNum = FindFiles();
    }
    emit workFinished();
}

int Worker::FindFiles()
{
    QString SearchPath = InputPath + "\\" + FileMask;
    WIN32_FIND_DATA findFileData;
    HANDLE hFile, hFind = FindFirstFile(SearchPath.toStdWString().c_str(), &findFileData);
    int FilesCount = 0;

    do {
        const std::wstring Filename = findFileData.cFileName;
        if (Filename != L"." && Filename != L"..")
        {
            //Tries to open file without sharing == it is not opened
            hFile = CreateFileW((InputPath.toStdWString() + L"\\" + Filename).c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE && GetLastError() != ERROR_SHARING_VIOLATION)
            {
                FilesCount += ModifyFile(hFile, OutputPath.toStdWString() + L"\\" + Filename);
                if (DeleteInputFiles == true)
                {
                    CloseHandle(hFile);
                    DeleteFile((InputPath.toStdWString() + L"\\" + Filename).c_str());
                }
            }
            CloseHandle(hFile);
        }
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
    return FilesCount;
}

int Worker::ModifyFile(HANDLE InFile, const std::wstring OutputFilename)
{
    uint64_t buffer;
    DWORD bytesRead = 1;
    HANDLE OutFile;

    if (ReWriteCopies == true)
        OutFile = CreateFile(OutputFilename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    else
    {
        //Try default name
        if ((OutFile = CreateFile(OutputFilename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
        {
            std::wstring baseName = OutputFilename;
            std::wstring extension;

            size_t lastDotPos = OutputFilename.find_last_of(L".");
            if (lastDotPos != std::wstring::npos) {
                baseName = OutputFilename.substr(0, lastDotPos);
                extension = OutputFilename.substr(lastDotPos);
            }

            int index = 1;
            std::wstring newOutputFilename = OutputFilename;

            //Find new index
            while ((OutFile = CreateFile(newOutputFilename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
            {
                newOutputFilename = baseName + L" (" + std::to_wstring(index) + L")" + extension;
                index++;
            }
        }
    }
    if (OutFile == INVALID_HANDLE_VALUE)
    {
        qDebug() << "Error";
        return 0;
    }

    while (bytesRead > 0)
    {
        ReadFile(InFile, &buffer, sizeof(buffer), &bytesRead, NULL);
        if (MainOperation == OR)
        {
            buffer |= MainConstant;
        }
        else if (MainOperation == XOR)
        {
            buffer ^= MainConstant;
        }
        else if (MainOperation == AND)
        {
            buffer &= MainConstant;
        }
        else if (MainOperation == NOT)
        {
            buffer = ~buffer;
        }
        WriteFile(OutFile, &buffer, bytesRead, NULL, NULL);
    }
    CloseHandle(OutFile);
    return 1;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), workerThread(new QThread(this)), worker(new Worker)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    worker->Timer.setHMS(0, 0, 0);

    //Validator for ConstantInput to input only hex numbers
    QRegularExpression hexRegex("^(0x)?[0-9A-Fa-f]{1,16}$");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(hexRegex, this);
    ui->ConstantInput->setValidator(validator);

    //One function for "OR", "XOR", "AND", "NOT" buttons
    connect(ui->ORButton, &QRadioButton::clicked, this, &MainWindow::SetOperation);
    connect(ui->XORButton, &QRadioButton::clicked, this, &MainWindow::SetOperation);
    connect(ui->ANDButton, &QRadioButton::clicked, this, &MainWindow::SetOperation);
    connect(ui->NOTButton, &QRadioButton::clicked, this, &MainWindow::SetOperation);

    //Group RadioButtons
    QButtonGroup *RewriteSaveGroup = new QButtonGroup;
    QButtonGroup *OperationGroup = new QButtonGroup;
    QButtonGroup *TimerGroup = new QButtonGroup;

    RewriteSaveGroup->addButton(ui->RewriteButton);
    RewriteSaveGroup->addButton(ui->SaveButton);

    OperationGroup->addButton(ui->ORButton);
    OperationGroup->addButton(ui->XORButton);
    OperationGroup->addButton(ui->ANDButton);
    OperationGroup->addButton(ui->NOTButton);

    connect(worker, &Worker::workFinished, this, &MainWindow::ThreadFinished);

    TimerGroup->addButton(ui->TimerButton);
    TimerGroup->addButton(ui->ExactLaunchButton);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_TimerButton_toggled(bool checked)
{
    ui->TimerCount->setEnabled(checked);
    worker->BoolTimer = checked;
    LaunchCheck();
}


void MainWindow::on_ConstantInput_editingFinished()
{
    if (ui->ConstantInput->text() == "" || ui->ConstantInput->text() == "0" || ui->ConstantInput->text() == "0x")
        worker->MainConstant = 0;
    else
        worker->MainConstant = ui->ConstantInput->text().toULongLong(nullptr, 16);
    LaunchCheck();
}

void MainWindow::SetOperation()
{
    if (ui->ORButton->isChecked())
        worker->MainOperation = worker->OR;
    else if (ui->XORButton->isChecked())
        worker->MainOperation = worker->XOR;
    else if (ui->ANDButton->isChecked())
        worker->MainOperation = worker->AND;
    else if (ui->NOTButton->isChecked())
        worker->MainOperation = worker->NOT;
    LaunchCheck();
}

void MainWindow::on_DeleteCheckBox_stateChanged(int arg1)
{
    worker->DeleteInputFiles = arg1; //2 - true; 0 - false
}

void MainWindow::on_RewriteButton_toggled(bool checked)
{
    worker->ReWriteCopies = checked;
    LaunchCheck();
}

void MainWindow::on_TimerCount_userTimeChanged(const QTime &time)
{
    worker->Timer = time;
    LaunchCheck();
}

void MainWindow::on_DirectoryInput_editingFinished()
{
    worker->InputPath = ui->DirectoryInput->text();
    LaunchCheck();
}

void MainWindow::on_FileMaskInput_editingFinished()
{
    worker->FileMask = ui->FileMaskInput->text();
    LaunchCheck();
}

void MainWindow::on_OutputDirectoryInput_editingFinished()
{
    worker->OutputPath = ui->OutputDirectoryInput->text();
    LaunchCheck();
}

//Checks all condition to enable launch button
void MainWindow::LaunchCheck()
{
    if (worker->InputPath != "")
    {
        if (worker->OutputPath != "")
        {
            if (ui->RewriteButton->isChecked() || ui->SaveButton->isChecked())
            {
                if (ui->ConstantInput->text() != "")
                {
                    if (worker->MainOperation != worker->NONE)
                    {
                        if (ui->ExactLaunchButton->isChecked()
                        || !(worker->Timer.second() == 0 && worker->Timer.minute() == 0 && worker->Timer.hour() == 0))
                        {
                            ui->LaunchButton->setEnabled(true);
                            return;
                        }
                    }
                }
            }
        }
    }
    ui->LaunchButton->setEnabled(false);
}

void MainWindow::on_StopButton_clicked()
{
    worker->StopFlag = true;
}

//Start background thread to modify files
void MainWindow::on_LaunchButton_clicked()
{
    int Ret;
    QString Msg;
    if (worker->FileMask == "")
    {
        Msg = "Маска файлов не была задана!\nБудут изменены все файлы в папке!";
        Ret = QMessageBox::warning(this, "Warning", Msg, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (Ret == QMessageBox::Cancel)
            return;
        worker->FileMask = "*";
    }
    if (worker->DeleteInputFiles == true)
    {
        Msg = "Входные файлы будут удалены!";
        Ret = QMessageBox::critical(this, "Critical", Msg, QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (Ret == QMessageBox::Cancel)
            return;
    }
    if (worker->BoolTimer)
    {
        worker->moveToThread(workerThread);
        connect(workerThread, &QThread::started, worker, &Worker::doWork);
        workerThread->start();
        ui->StopButton->setEnabled(true);
        ui->statusbar->showMessage("Working by timer");
    }
    else
    {
        worker->doWork();
    }
}

void MainWindow::ThreadFinished()
{
    ui->StopButton->setEnabled(false);
    ui->statusbar->showMessage(QString(std::to_string(worker->FilesNum).c_str()) + " files modified");
}
