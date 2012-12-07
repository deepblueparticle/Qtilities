/****************************************************************************
**
** Copyright (c) 2009-2012, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "WidgetLoggerEngineFrontend.h"
#include "LoggingConstants.h"
#include "QtilitiesApplication.h"
#include "QtilitiesCoreGuiConstants.h"
#include "ActionProvider.h"
#include "ConfigurationWidget.h"

#include <Logger.h>

#include <QScrollBar>
#include <QMetaObject>
#include <QPrinter>
#include <QPrintDialog>
#include <QAbstractPrintDialog>
#include <QFileDialog>
#include <QAction>
#include <QShortcut>
#include <QKeySequence>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QPrintPreviewDialog>
#include <QApplication>
#include <QToolBar>
#include <QDockWidget>
#include <QTabBar>

using namespace Qtilities::Core;
using namespace Qtilities::CoreGui;
using namespace Qtilities::CoreGui::Icons;
using namespace Qtilities::CoreGui::Actions;
using namespace Qtilities::CoreGui::Constants;

struct Qtilities::CoreGui::MessagesPlainTextEditTabPrivateData {
    MessagesPlainTextEditTabPrivateData() : searchBoxWidget(0),
    txtLog(0),
    actionCopy(0),
    actionSelectAll(0),
    actionClear(0),
    actionPrint(0),
    actionPrintPDF(0),
    actionPrintPreview(0),
    actionSave(0),
    actionFind(0),
    actionSettings(0),
    central_widget(0) {}

    SearchBoxWidget* searchBoxWidget;
    QPlainTextEdit txtLog;
    QAction* actionCopy;
    QAction* actionSelectAll;
    QAction* actionClear;
    QAction* actionPrint;
    QAction* actionPrintPDF;
    QAction* actionPrintPreview;
    QAction* actionSave;
    QAction* actionFind;
    QAction* actionSettings;

    //! The IActionProvider interface implementation.
    ActionProvider* action_provider;
    //! The action toolbars list. Contains toolbars created for each category in the action provider.
    QList<QToolBar*> action_toolbars;
    //! The global meta type string used for this widget.
    QString global_meta_type;
    //! This is the widget for the plain text editor and search box widget.
    QWidget* central_widget;
};

Qtilities::CoreGui::MessagesPlainTextEditTab::MessagesPlainTextEditTab(QWidget *parent) : QMainWindow(parent)
{
    d = new MessagesPlainTextEditTabPrivateData;
    d->action_provider = new ActionProvider(this);

    // Setup search box widget:
    SearchBoxWidget::SearchOptions search_options = 0;
    search_options |= SearchBoxWidget::CaseSensitive;
    search_options |= SearchBoxWidget::WholeWordsOnly;
    search_options |= SearchBoxWidget::RegEx;
    d->searchBoxWidget = new SearchBoxWidget(search_options);
    d->searchBoxWidget->setWholeWordsOnly(false);
    connect(d->searchBoxWidget,SIGNAL(searchStringChanged(const QString)),SLOT(handleSearchStringChanged(QString)));
    connect(d->searchBoxWidget,SIGNAL(searchOptionsChanged()),SLOT(handle_FindNext()));
    connect(d->searchBoxWidget,SIGNAL(btnClose_clicked()),d->searchBoxWidget,SLOT(hide()));
    connect(d->searchBoxWidget,SIGNAL(btnFindNext_clicked()),SLOT(handle_FindNext()));
    connect(d->searchBoxWidget,SIGNAL(btnFindPrevious_clicked()),SLOT(handle_FindPrevious()));
    d->searchBoxWidget->setEditorFocus();
    d->searchBoxWidget->hide();

    // Setup the log widget:
    d->txtLog.setReadOnly(true);
    d->txtLog.setFont(QFont("Courier New"));
    d->txtLog.setMaximumBlockCount(1000);

    d->central_widget = new QWidget;

    // Setup the widget layout:
    if (d->central_widget->layout())
        delete d->central_widget->layout();

    QVBoxLayout* layout = new QVBoxLayout(d->central_widget);
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(&d->txtLog);
    layout->addWidget(d->searchBoxWidget);
    d->txtLog.show();
    setCentralWidget(d->central_widget);

    // Assign a default meta type for this widget:
    // We construct each action and then register it
    QString context_string = "MessagesPlainTextEditTab";
    int count = 0;
    context_string.append(QString("%1").arg(count));
    while (CONTEXT_MANAGER->hasContext(context_string)) {
        QString count_string = QString("%1").arg(count);
        context_string.chop(count_string.length());
        ++count;
        context_string.append(QString("%1").arg(count));
    }
    CONTEXT_MANAGER->registerContext(context_string);
    d->global_meta_type = context_string;
    setObjectName(context_string);

    // Construct actions only after global meta type was set.
    constructActions();
    QList<QtilitiesCategory> categories = d->action_provider->actionCategories();
    for (int i = 0; i < categories.count(); ++i) {
        QList<QAction*> action_list = d->action_provider->actions(IActionProvider::FilterHidden,categories.at(i));
        if (action_list.count() > 0) {
            QToolBar* new_toolbar = addToolBar(categories.at(i).toString());
            d->action_toolbars << new_toolbar;
            new_toolbar->addActions(action_list);
        }
    }

    d->txtLog.installEventFilter(this);
}

Qtilities::CoreGui::MessagesPlainTextEditTab::~MessagesPlainTextEditTab() {
    delete d;
}

bool Qtilities::CoreGui::MessagesPlainTextEditTab::eventFilter(QObject *object, QEvent *event) {
    if (object == &d->txtLog && event->type() == QEvent::FocusIn) {
        CONTEXT_MANAGER->setNewContext(d->global_meta_type,true);
    }
    return false;
}

bool Qtilities::CoreGui::MessagesPlainTextEditTab::setGlobalMetaType(const QString& meta_type) {
    // Check if this global meta type is allowed.
    if (CONTEXT_MANAGER->hasContext(meta_type))
        return false;

    d->global_meta_type = meta_type;
    return true;
}

QString Qtilities::CoreGui::MessagesPlainTextEditTab::globalMetaType() const {
    return d->global_meta_type;
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::appendMessage(const QString& message) {
    d->txtLog.appendHtml(message);
    d->txtLog.verticalScrollBar()->setValue(d->txtLog.verticalScrollBar()->maximum());
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_FindPrevious() {
    QTextDocument::FindFlags find_flags = 0;
    if (d->searchBoxWidget->caseSensitive())
        find_flags |= QTextDocument::FindCaseSensitively;
    if (d->searchBoxWidget->wholeWordsOnly())
        find_flags |= QTextDocument::FindWholeWords;
    find_flags |= QTextDocument::FindBackward;
    d->txtLog.find(d->searchBoxWidget->currentSearchString(),find_flags);
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handleSearchStringChanged(const QString& filter_string) {
    QTextDocument::FindFlags find_flags = 0;
    if (d->searchBoxWidget->wholeWordsOnly())
        find_flags |= QTextDocument::FindWholeWords;
    if (d->searchBoxWidget->caseSensitive())
        find_flags |= QTextDocument::FindCaseSensitively;

    d->txtLog.find(filter_string,find_flags | QTextDocument::FindBackward);
    d->txtLog.find(filter_string,find_flags);
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_FindNext() {
    QTextDocument::FindFlags find_flags = 0;
    if (d->searchBoxWidget->caseSensitive())
        find_flags |= QTextDocument::FindCaseSensitively;
    if (d->searchBoxWidget->wholeWordsOnly())
        find_flags |= QTextDocument::FindWholeWords;
    d->txtLog.find(d->searchBoxWidget->currentSearchString(),find_flags);
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_Save() {
    QString file_name = QFileDialog::getSaveFileName(this, tr("Save Log"),QtilitiesApplication::applicationSessionPath(),tr("Log File (*.log)"));

    if (file_name.isEmpty())
        return;

    QFile file(file_name);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << d->txtLog.toPlainText() << "\n";
    file.close();
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_Settings() {
    ConfigurationWidget* config_widget = qobject_cast<ConfigurationWidget*> (QtilitiesApplication::configWidget());
    if (config_widget) {
        config_widget->setActivePage(tr("Logging"));
        config_widget->show();
    }
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_Print() {
#ifndef QT_NO_PRINTER
     QPrinter printer;

     QPrintDialog *dialog = new QPrintDialog(&printer, this);
     dialog->setWindowTitle(tr("Print Current Log"));
     if (d->txtLog.textCursor().hasSelection())
         dialog->addEnabledOption(QAbstractPrintDialog::PrintSelection);
     if (dialog->exec() != QDialog::Accepted)
         return;

     delete dialog;
     d->txtLog.print(&printer);
#endif
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_PrintPDF() {
#ifndef QT_NO_PRINTER
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export PDF"), QString(), "*.pdf");
    if (!fileName.isEmpty()) {
        if (QFileInfo(fileName).completeSuffix().isEmpty())
            fileName.append(".pdf");
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        d->txtLog.document()->print(&printer);
    }
#endif
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_PrintPreview() {
#ifndef QT_NO_PRINTER
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, &d->txtLog);
    connect(&preview, SIGNAL(paintRequested(QPrinter *)), SLOT(printPreview(QPrinter *)));
    preview.exec();
#endif
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::printPreview(QPrinter *printer) {
#ifdef QT_NO_PRINTER
    Q_UNUSED(printer);
#else
    d->txtLog.print(printer);
#endif
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_Clear() {
    d->txtLog.clear();
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_Copy() {
    d->txtLog.copy();
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_SearchShortcut() {
    if (d->searchBoxWidget) {
        if (!d->searchBoxWidget->isVisible()) {
            d->searchBoxWidget->show();
            d->searchBoxWidget->setEditorFocus();
        }

        // We check if there is a selection in the user visible cursor. If so we set that as the search string.
        QTextCursor visible_cursor = d->txtLog.textCursor();
        if (visible_cursor.hasSelection()) {
            d->searchBoxWidget->setCurrentSearchString(visible_cursor.selectedText());
        }
    }
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::handle_SelectAll() {
    d->txtLog.selectAll();
}

void Qtilities::CoreGui::MessagesPlainTextEditTab::constructActions() {
    QList<int> context;
    context.push_front(CONTEXT_MANAGER->contextID(d->global_meta_type));

    ACTION_MANAGER->commandObserver()->startProcessingCycle();

    // ---------------------------
    // Save
    // ---------------------------
    d->actionSave = new QAction(QIcon(qti_icon_FILE_SAVE_16x16),tr("Save"),this);
    d->action_provider->addAction(d->actionSave,QtilitiesCategory(tr("Log")));
    connect(d->actionSave,SIGNAL(triggered()),SLOT(handle_Save()));
    Command* command = ACTION_MANAGER->registerAction(qti_action_FILE_SAVE,d->actionSave,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Copy
    // ---------------------------
    d->actionCopy = new QAction(QIcon(qti_icon_EDIT_COPY_16x16),tr("Copy"),this);
    d->action_provider->addAction(d->actionCopy,QtilitiesCategory(tr("Log")));
    connect(d->actionCopy,SIGNAL(triggered()),SLOT(handle_Copy()));
    command = ACTION_MANAGER->registerAction(qti_action_EDIT_COPY,d->actionCopy,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Select All
    // ---------------------------
    d->actionSelectAll = new QAction(QIcon(qti_icon_EDIT_SELECT_ALL_16x16),tr("Select All"),this);
    d->actionSelectAll->setEnabled(true);
    d->action_provider->addAction(d->actionSelectAll,QtilitiesCategory(tr("Log")));
    connect(d->actionSelectAll,SIGNAL(triggered()),SLOT(handle_SelectAll()));
    command = ACTION_MANAGER->registerAction(qti_action_EDIT_SELECT_ALL,d->actionSelectAll,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Clear
    // ---------------------------
    d->actionClear = new QAction(QIcon(qti_icon_EDIT_CLEAR_16x16),tr("Clear"),this);
    d->action_provider->addAction(d->actionClear,QtilitiesCategory(tr("Log")));
    connect(d->actionClear,SIGNAL(triggered()),SLOT(handle_Clear()));
    command = ACTION_MANAGER->registerAction(qti_action_EDIT_CLEAR,d->actionClear,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Find
    // ---------------------------
    d->actionFind = new QAction(QIcon(qti_icon_FIND_16x16),tr("Find"),this);
    //d->actionFind->setShortcut(QKeySequence(QKeySequence::Find));
    d->action_provider->addAction(d->actionFind,QtilitiesCategory(tr("Log")));
    connect(d->actionFind,SIGNAL(triggered()),SLOT(handle_SearchShortcut()));
    command = ACTION_MANAGER->registerAction(qti_action_EDIT_FIND,d->actionFind,context);
    // ---------------------------
    // Print
    // ---------------------------
    d->actionPrint = new QAction(QIcon(qti_icon_PRINT_16x16),tr("Print"),this);
    d->action_provider->addAction(d->actionPrint,QtilitiesCategory(tr("Print")));
    connect(d->actionPrint,SIGNAL(triggered()),SLOT(handle_Print()));
    command = ACTION_MANAGER->registerAction(qti_action_FILE_PRINT,d->actionPrint,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Print PDF
    // ---------------------------
    d->actionPrintPDF = new QAction(QIcon(qti_icon_PRINT_PDF_16x16),tr("Print PDF"),this);
    d->action_provider->addAction(d->actionPrintPDF,QtilitiesCategory(tr("Print")));
    connect(d->actionPrintPDF,SIGNAL(triggered()),SLOT(handle_PrintPDF()));
    command = ACTION_MANAGER->registerAction(qti_action_FILE_PRINT_PDF,d->actionPrintPDF,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Print Preview
    // ---------------------------
    d->actionPrintPreview = new QAction(QIcon(qti_icon_PRINT_PREVIEW_16x16),tr("Print Preview"),this);
    d->action_provider->addAction(d->actionPrintPreview,QtilitiesCategory(tr("Print")));
    connect(d->actionPrintPreview,SIGNAL(triggered()),SLOT(handle_PrintPreview()));
    command = ACTION_MANAGER->registerAction(qti_action_FILE_PRINT_PREVIEW,d->actionPrintPreview,context);
    command->setCategory(QtilitiesCategory("Editing"));
    // ---------------------------
    // Logger Settings
    // ---------------------------
    // We create the settings action only if there is a config page registered in QtilitiesApplication and
    // it has the Logging page as well.
    if (QtilitiesApplication::configWidget()) {
        ConfigurationWidget* config_widget = qobject_cast<ConfigurationWidget*> (QtilitiesApplication::configWidget());
        if (config_widget) {
            if (config_widget->hasPage(tr("Logging"))) {
                d->actionSettings = new QAction(QIcon(qti_icon_PROPERTY_16x16),tr("Logging Settings"),this);
                d->action_provider->addAction(d->actionSettings,QtilitiesCategory(tr("Log")));
                ACTION_MANAGER->registerAction(qti_action_FILE_SETTINGS,d->actionSettings,context);
                connect(d->actionSettings,SIGNAL(triggered()),SLOT(handle_Settings()));
            }
        }
    }

    // Add actions to text edit.
    d->txtLog.addAction(d->actionClear);
    d->txtLog.addAction(d->actionSave);
    QAction* sep1 = new QAction("",0);
    sep1->setSeparator(true);
    d->txtLog.addAction(sep1);
    d->txtLog.addAction(d->actionFind);
    QAction* sep2 = new QAction("",0);
    sep2->setSeparator(true);
    d->txtLog.addAction(sep2);
    //d->txtLog.addAction(d->actionSettings);
    d->txtLog.setContextMenuPolicy(Qt::ActionsContextMenu);

    ACTION_MANAGER->commandObserver()->endProcessingCycle(false);
}

QPlainTextEdit* Qtilities::CoreGui::MessagesPlainTextEditTab::plainTextEdit() const {
    return &d->txtLog;
}

void MessagesPlainTextEditTab::clear() {
    d->txtLog.clear();
}

// ----------------------------------------------
// WidgetLoggerEngineFrontend
// ----------------------------------------------


struct Qtilities::CoreGui::WidgetLoggerEngineFrontendPrivateData {
    WidgetLoggerEngineFrontendPrivateData() {}

    WidgetLoggerEngine::MessageDisplaysFlag                         message_displays_flag;
    QMap<WidgetLoggerEngine::MessageDisplaysFlag,QWidget*>          message_displays;
    QMap<WidgetLoggerEngine::MessageDisplaysFlag,QDockWidget*>      message_display_docks;
};

Qtilities::CoreGui::WidgetLoggerEngineFrontend::WidgetLoggerEngineFrontend(WidgetLoggerEngine::MessageDisplaysFlag message_displays_flag, QWidget *parent) : QMainWindow(parent)
{
    d = new WidgetLoggerEngineFrontendPrivateData;
    d->message_displays_flag = message_displays_flag;

    // When only one message display is present we don't create it as a tab widget.
    if (message_displays_flag == WidgetLoggerEngine::AllMessagesPlainTextEdit ||
            message_displays_flag == WidgetLoggerEngine::IssuesPlainTextEdit ||
            message_displays_flag == WidgetLoggerEngine::WarningsPlainTextEdit ||
            message_displays_flag == WidgetLoggerEngine::ErrorsPlainTextEdit) {

        MessagesPlainTextEditTab* new_tab = new MessagesPlainTextEditTab;
        d->message_displays[message_displays_flag] = new_tab;
        setCentralWidget(new_tab);
    } else {
        int info_index = -1;
        int issues_index = -1;
        int warning_index = -1;
        int error_index = -1;

        // Create needed tabs:
        if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit) {
            MessagesPlainTextEditTab* new_tab = new MessagesPlainTextEditTab;
            d->message_displays[WidgetLoggerEngine::AllMessagesPlainTextEdit] = new_tab;
            QDockWidget* new_dock = new QDockWidget("Messages");

            // We don't want users to be able to close the individual log
            // dock widgets since there is no way to get them back then
            // until the application is restarted.
            QDockWidget::DockWidgetFeatures features = new_dock->features();
            features &= ~QDockWidget::DockWidgetClosable;
            new_dock->setFeatures(features);

            d->message_display_docks[WidgetLoggerEngine::AllMessagesPlainTextEdit] = new_dock;
            connect(new_dock,SIGNAL(visibilityChanged(bool)),SLOT(handle_dockVisibilityChanged(bool)));
            new_dock->setWidget(new_tab);
            addDockWidget(Qt::BottomDockWidgetArea,new_dock);

            info_index = 0;
        }

        if (d->message_displays_flag & WidgetLoggerEngine::IssuesPlainTextEdit) {
            MessagesPlainTextEditTab* new_tab = new MessagesPlainTextEditTab;
            d->message_displays[WidgetLoggerEngine::IssuesPlainTextEdit] = new_tab;
            QDockWidget* new_dock = new QDockWidget("Issues");

            // We don't want users to be able to close the individual log
            // dock widgets since there is no way to get them back then
            // until the application is restarted.
            QDockWidget::DockWidgetFeatures features = new_dock->features();
            features &= ~QDockWidget::DockWidgetClosable;
            new_dock->setFeatures(features);

            d->message_display_docks[WidgetLoggerEngine::IssuesPlainTextEdit] = new_dock;
            connect(new_dock,SIGNAL(visibilityChanged(bool)),SLOT(handle_dockVisibilityChanged(bool)));
            new_dock->setWidget(new_tab);
            addDockWidget(Qt::BottomDockWidgetArea,new_dock);

            if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit)
                issues_index = 1;
            else
                issues_index = 0;
        }

        if (d->message_displays_flag & WidgetLoggerEngine::WarningsPlainTextEdit) {
            MessagesPlainTextEditTab* new_tab = new MessagesPlainTextEditTab;
            d->message_displays[WidgetLoggerEngine::WarningsPlainTextEdit] = new_tab;
            QDockWidget* new_dock = new QDockWidget("Warnings");

            // We don't want users to be able to close the individual log
            // dock widgets since there is no way to get them back then
            // until the application is restarted.
            QDockWidget::DockWidgetFeatures features = new_dock->features();
            features &= ~QDockWidget::DockWidgetClosable;
            new_dock->setFeatures(features);

            d->message_display_docks[WidgetLoggerEngine::WarningsPlainTextEdit] = new_dock;
            connect(new_dock,SIGNAL(visibilityChanged(bool)),SLOT(handle_dockVisibilityChanged(bool)));
            new_dock->setWidget(new_tab);
            addDockWidget(Qt::BottomDockWidgetArea,new_dock);

            if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit && d->message_displays_flag & WidgetLoggerEngine::IssuesPlainTextEdit)
                warning_index = 2;
            else if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit)
                warning_index = 1;
            else
                warning_index = 0;
        }

        if (d->message_displays_flag & WidgetLoggerEngine::ErrorsPlainTextEdit) {
            MessagesPlainTextEditTab* new_tab = new MessagesPlainTextEditTab;
            d->message_displays[WidgetLoggerEngine::ErrorsPlainTextEdit] = new_tab;
            QDockWidget* new_dock = new QDockWidget("Errors");

            // We don't want users to be able to close the individual log
            // dock widgets since there is no way to get them back then
            // until the application is restarted.
            QDockWidget::DockWidgetFeatures features = new_dock->features();
            features &= ~QDockWidget::DockWidgetClosable;
            new_dock->setFeatures(features);

            d->message_display_docks[WidgetLoggerEngine::ErrorsPlainTextEdit] = new_dock;
            connect(new_dock,SIGNAL(visibilityChanged(bool)),SLOT(handle_dockVisibilityChanged(bool)));
            new_dock->setWidget(new_tab);
            addDockWidget(Qt::BottomDockWidgetArea,new_dock);

            if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit && d->message_displays_flag & WidgetLoggerEngine::IssuesPlainTextEdit && d->message_displays_flag & WidgetLoggerEngine::WarningsPlainTextEdit)
                error_index = 3;
            else if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit && d->message_displays_flag & WidgetLoggerEngine::WarningsPlainTextEdit)
                error_index = 2;
            else if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit && d->message_displays_flag & WidgetLoggerEngine::IssuesPlainTextEdit)
                error_index = 2;
            else if (d->message_displays_flag & WidgetLoggerEngine::AllMessagesPlainTextEdit)
                error_index = 1;
            else if (d->message_displays_flag & WidgetLoggerEngine::WarningsPlainTextEdit)
                error_index = 1;
            else if (d->message_displays_flag & WidgetLoggerEngine::IssuesPlainTextEdit)
                error_index = 1;
            else
                error_index = 0;
        }

        for (int i = 1; i < d->message_display_docks.count(); ++i)
            tabifyDockWidget(d->message_display_docks.values().at(i-1),d->message_display_docks.values().at(i));

        QList<QTabBar *> tabList = findChildren<QTabBar *>();
        if (!tabList.isEmpty()) {
            QTabBar *tabBar = tabList.at(0);
            tabBar->setCurrentIndex(0);
            tabBar->setShape(QTabBar::RoundedSouth);

            if (info_index != -1)
                tabBar->setTabIcon(info_index,QIcon(qti_icon_INFO_12x12));
            if (issues_index != -1)
                tabBar->setTabIcon(issues_index,QIcon(qti_icon_WARNING_12x12));
            if (warning_index != -1)
                tabBar->setTabIcon(warning_index,QIcon(qti_icon_WARNING_12x12));
            if (error_index != -1)
                tabBar->setTabIcon(error_index,QIcon(qti_icon_ERROR_12x12));
        }
    }
}

Qtilities::CoreGui::WidgetLoggerEngineFrontend::~WidgetLoggerEngineFrontend() {
    delete d;
}

QPlainTextEdit* Qtilities::CoreGui::WidgetLoggerEngineFrontend::plainTextEdit(WidgetLoggerEngine::MessageDisplaysFlag message_display) const {
    if (d->message_displays.contains(message_display)) {
        MessagesPlainTextEditTab* plain_text_edit_tab = qobject_cast<MessagesPlainTextEditTab*> (d->message_displays[message_display]);
        if (plain_text_edit_tab)
            return plain_text_edit_tab->plainTextEdit();
    }
    return 0;
}

void WidgetLoggerEngineFrontend::appendMessage(const QString &message, Logger::MessageType message_type) {
    MessagesPlainTextEditTab* plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::AllMessagesPlainTextEdit);
    if (plain_text_edit_tab)
        plain_text_edit_tab->appendMessage(message);

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::IssuesPlainTextEdit);
    if (plain_text_edit_tab && (message_type & Logger::Warning || message_type & Logger::Error || message_type & Logger::Fatal))
        plain_text_edit_tab->appendMessage(message);

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::WarningsPlainTextEdit);
    if (plain_text_edit_tab && (message_type & Logger::Warning))
        plain_text_edit_tab->appendMessage(message);

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::ErrorsPlainTextEdit);
    if (plain_text_edit_tab && (message_type & Logger::Error || message_type & Logger::Fatal))
        plain_text_edit_tab->appendMessage(message);
}

void WidgetLoggerEngineFrontend::clear() {
    MessagesPlainTextEditTab* plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::AllMessagesPlainTextEdit);
    if (plain_text_edit_tab)
        plain_text_edit_tab->clear();

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::IssuesPlainTextEdit);
    if (plain_text_edit_tab)
        plain_text_edit_tab->clear();

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::WarningsPlainTextEdit);
    if (plain_text_edit_tab)
        plain_text_edit_tab->clear();

    plain_text_edit_tab = plainTextEditTab(WidgetLoggerEngine::ErrorsPlainTextEdit);
    if (plain_text_edit_tab)
        plain_text_edit_tab->clear();
}

MessagesPlainTextEditTab *WidgetLoggerEngineFrontend::plainTextEditTab(WidgetLoggerEngine::MessageDisplaysFlag message_display) {
    if (d->message_displays.contains(message_display) && d->message_displays_flag & message_display)
        return qobject_cast<MessagesPlainTextEditTab*> (d->message_displays[message_display]);

    return 0;
}

void WidgetLoggerEngineFrontend::handle_dockVisibilityChanged(bool visible) {
    QDockWidget* dock = qobject_cast<QDockWidget*> (sender());
    if (dock && visible) {
        MessagesPlainTextEditTab* front_end = qobject_cast<MessagesPlainTextEditTab*> (dock->widget());
        if (front_end) {
            CONTEXT_MANAGER->setNewContext(front_end->contextString(),true);
        }
    }
}

