/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 2009 John Tapsell <john.tapsell@kde.org>
    SPDX-FileCopyrightText: 2018 Fabian Vogt <fabian@ritter-vogt.de>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#include "scripting.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

#include "ksysguardprocesslist.h"
#include "../processcore/processes.h"

#include <KDesktopFile>
#include <KLocalizedString>
#include <KStandardAction>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QVBoxLayout>

#if WEBENGINE_SCRIPTING_ENABLED
#include <QWebChannel>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <qtwebenginewidgetsversion.h>
#endif

#if WEBENGINE_SCRIPTING_ENABLED
class RemoteUrlInterceptor : public QWebEngineUrlRequestInterceptor
{
public:
    RemoteUrlInterceptor(QObject *parent)
        : QWebEngineUrlRequestInterceptor(parent)
    {
    }
    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        // Block non-GET/HEAD requests
        if (!QStringList({QStringLiteral("GET"), QStringLiteral("HEAD")}).contains(QString::fromLatin1(info.requestMethod())))
            info.block(true);

        // Block remote URLs
        if (!QStringList({QStringLiteral("blob"), QStringLiteral("data"), QStringLiteral("file")}).contains(info.requestUrl().scheme()))
            info.block(true);
    }
};
#endif

class ScriptingHtmlDialog : public QDialog
{
public:
    ScriptingHtmlDialog(QWidget *parent)
        : QDialog(parent)
    {
        QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
        buttonBox->setStandardButtons(QDialogButtonBox::Close);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

#if WEBENGINE_SCRIPTING_ENABLED
        QVBoxLayout *layout = new QVBoxLayout;
        layout->addWidget(&m_webView);
        layout->addWidget(buttonBox);
        setLayout(layout);
        layout->setContentsMargins(0, 0, 0, 0);
        m_webView.settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false);
        m_webView.page()->profile()->setUrlRequestInterceptor(new RemoteUrlInterceptor(this));
#endif
    }
#if WEBENGINE_SCRIPTING_ENABLED
    QWebEngineView *webView()
    {
        return &m_webView;
    }

protected:
    QWebEngineView m_webView;
#endif
};

ProcessObject::ProcessObject(ProcessModel *model, int pid)
{
    mModel = model;
    mPid = pid;
}

bool ProcessObject::fileExists(const QString &filename)
{
    QFileInfo fileInfo(filename);
    return fileInfo.exists();
}
QString ProcessObject::readFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QTextStream stream(&file);
    QString contents = stream.readAll();
    file.close();
    return contents;
}

Scripting::Scripting(KSysGuardProcessList *parent)
    : QWidget(parent)
    , mProcessList(parent)
{
    mScriptingHtmlDialog = nullptr;
    loadContextMenu();
}
void Scripting::runScript(const QString &path, const QString &name)
{
    // Record the script name and path for use in the script helper functions
    mScriptPath = path;
    mScriptName = name;

#if WEBENGINE_SCRIPTING_ENABLED
    QUrl fileName = QUrl::fromLocalFile(path + QStringLiteral("index.html"));
    if (!mScriptingHtmlDialog) {
        mScriptingHtmlDialog = new ScriptingHtmlDialog(this);
        mWebChannel = new QWebChannel(mScriptingHtmlDialog);
        connect(mScriptingHtmlDialog, &QDialog::rejected, this, &Scripting::stopAllScripts);
        // Only show after page loaded to allow for layouting
        mScriptingHtmlDialog->connect(mScriptingHtmlDialog->webView(), &QWebEngineView::loadFinished, mScriptingHtmlDialog, &ScriptingHtmlDialog::show);

        QAction *refreshAction = new QAction(QStringLiteral("refresh"), mScriptingHtmlDialog);
        refreshAction->setShortcut(QKeySequence::Refresh);
        connect(refreshAction, &QAction::triggered, this, &Scripting::refreshScript);
        mScriptingHtmlDialog->addAction(refreshAction);

        QAction *zoomInAction = KStandardAction::zoomIn(this, SLOT(zoomIn()), mScriptingHtmlDialog);
        mScriptingHtmlDialog->addAction(zoomInAction);

        QAction *zoomOutAction = KStandardAction::zoomOut(this, SLOT(zoomOut()), mScriptingHtmlDialog);
        mScriptingHtmlDialog->addAction(zoomOutAction);
    }

    // Make the process information available to the script
    QWebEngineProfile *profile = mScriptingHtmlDialog->webView()->page()->profile();
    QFile webChannelJsFile(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
    webChannelJsFile.open(QIODevice::ReadOnly);
    QString webChannelJs = QString::fromUtf8(webChannelJsFile.readAll());

    /* Warning: Awful hack ahead!
     * WebChannel does not allow synchronous calls so we need to make
     * asynchronous calls synchronous.
     * The conversion is achieved by caching the result of all readFile
     * and fileExists calls and restarting the script on every result until
     * all requests can be fulfilled synchronously.
     * Another challenge is that WebEngine does not support reading
     * files from /proc over file:// (they are always empty) so we need
     * to keep using the ProcessObject helper methods.
     */
    webChannelJs.append(QStringLiteral(R"JS(
new QWebChannel(window.qt.webChannelTransport, function(channel) {
    window.process = channel.objects.process;
    window.process.realReadFile = window.process.readFile;
    window.process.realFileExists = window.process.fileExists;
    var files = {}; // Map of all read files. null means does not exist
    window.process.fileExists = function(name, cb) {
        if(cb) return window.process.realFileExists(name, cb);
        if (files[name] === null)
            return false; // Definitely does not exist
        if (typeof(files[name]) == 'string')
            return true; // Definitely exists

        window.process.realFileExists(name, function(r) {
            if(!r) {
                files[name] = null;
                refresh();
                return;
            }
            window.process.realReadFile(name, function(r) {
                files[name] = r;
                refresh();
            });
        });

        return true; // Might exist
    };
    window.process.readFile = function(name,cb) {
        if(cb) return window.process.realReadFile(name, cb);
        if (typeof(files[name]) == 'string')
            return files[name]; // From cache

        window.process.fileExists(name); // Fill the cache
        return '';
    };
    refresh && refresh();
});)JS"));

    QWebEngineScript webChannelScript;
    webChannelScript.setSourceCode(webChannelJs);
    webChannelScript.setName(QStringLiteral("qwebchannel.js"));
    webChannelScript.setWorldId(QWebEngineScript::MainWorld);
    webChannelScript.setInjectionPoint(QWebEngineScript::DocumentCreation);
    webChannelScript.setRunsOnSubFrames(false);

    profile->scripts()->insert(webChannelScript);

    // Inject a style sheet that follows system colors, otherwise we might end up with black text on dark gray background
    const QString styleSheet =
        QStringLiteral(
            "body { background: %1; color: %2; }"
            "a { color: %3; }"
            "a:visited { color: %4; } ")
            .arg(palette().window().color().name(), palette().text().color().name(), palette().link().color().name(), palette().linkVisited().color().name());

    QString styleSheetJs = QStringLiteral(
                               "\nvar node = document.createElement('style');"
                               "node.innerHTML = '%1';"
                               "document.body.appendChild(node);")
                               .arg(styleSheet);

    QWebEngineScript styleSheetScript;
    styleSheetScript.setSourceCode(styleSheetJs);
    styleSheetScript.setName(QStringLiteral("stylesheet.js"));
    styleSheetScript.setWorldId(QWebEngineScript::MainWorld);
    styleSheetScript.setInjectionPoint(QWebEngineScript::DocumentReady);
    styleSheetScript.setRunsOnSubFrames(false);

    profile->scripts()->insert(styleSheetScript);

    setupJavascriptObjects();

    mScriptingHtmlDialog->webView()->load(fileName);
#else
    QMessageBox::critical(this,
                          i18n("QtWebEngineWidgets not available"),
                          i18n("KSysGuard library was compiled without QtWebEngineWidgets, please contact your distribution."));
#endif
}
#if WEBENGINE_SCRIPTING_ENABLED
void Scripting::zoomIn()
{
    QWebEngineView *webView = mScriptingHtmlDialog->webView();
    webView->setZoomFactor(webView->zoomFactor() * 1.1);
}
void Scripting::zoomOut()
{
    QWebEngineView *webView = mScriptingHtmlDialog->webView();
    if (webView->zoomFactor() > 0.1) // Prevent it getting too small
        webView->setZoomFactor(webView->zoomFactor() / 1.1);
}

void Scripting::refreshScript()
{
    // Call any refresh function, if it exists
    mProcessList->processModel()->update(0, KSysGuard::Processes::XMemory);
    mProcessObject->anythingChanged();
    if (mScriptingHtmlDialog && mScriptingHtmlDialog->webView() && mScriptingHtmlDialog->webView()->page()) {
        mScriptingHtmlDialog->webView()->page()->runJavaScript(QStringLiteral("refresh && refresh();"));
    }
}
void Scripting::setupJavascriptObjects()
{
    mProcessList->processModel()->update(0, KSysGuard::Processes::XMemory);
    mProcessObject = new ProcessObject(mProcessList->processModel(), mPid);
    mWebChannel->registerObject(QStringLiteral("process"), mProcessObject);
    mScriptingHtmlDialog->webView()->page()->setWebChannel(mWebChannel);
}
#endif
void Scripting::stopAllScripts()
{
    if (mScriptingHtmlDialog)
        mScriptingHtmlDialog->deleteLater();
    mScriptingHtmlDialog = nullptr;
    mProcessObject = nullptr;
    mScriptPath.clear();
    mScriptName.clear();
}
void Scripting::loadContextMenu()
{
    // Clear any existing actions
    qDeleteAll(mActions);
    mActions.clear();

    QStringList scripts;
    const QStringList dirs =
        QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("ksysguard/scripts/"), QStandardPaths::LocateDirectory);
    for (const QString &dir : dirs) {
        QDirIterator it(dir, QStringList() << QStringLiteral("*.desktop"), QDir::NoFilter, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            scripts.append(it.next());
        }
    }

    for (const QString &script : scripts) {
        KDesktopFile desktopFile(script);
        if (!desktopFile.name().isEmpty() && !desktopFile.noDisplay()) {
            QAction *action = new QAction(desktopFile.readName(), this);
            action->setToolTip(desktopFile.readComment());
            action->setIcon(QIcon(desktopFile.readIcon()));
            QString scriptPath = script;
            scriptPath.truncate(scriptPath.lastIndexOf(QLatin1Char('/')));
            action->setProperty("scriptPath", QString(scriptPath + QLatin1Char('/')));
            connect(action, &QAction::triggered, this, &Scripting::runScriptSlot);
            mProcessList->addAction(action);
            mActions << action;
        }
    }
}

void Scripting::runScriptSlot()
{
    QAction *action = static_cast<QAction *>(sender());
    // All the files for the script should be in the scriptPath
    QString path = action->property("scriptPath").toString();

    QList<KSysGuard::Process *> selectedProcesses = mProcessList->selectedProcesses();
    if (selectedProcesses.isEmpty())
        return;
    mPid = selectedProcesses[0]->pid();

    runScript(path, action->text());
}
