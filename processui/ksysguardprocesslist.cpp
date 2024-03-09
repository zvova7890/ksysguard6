/*
    KSysGuard, the KDE System Guard

    SPDX-FileCopyrightText: 1999-2001 Chris Schlaeger <cs@kde.org>
    SPDX-FileCopyrightText: 2006-2007 John Tapsell <john.tapsell@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

*/

#include "ksysguardprocesslist.h"

#include "config-ksysguard.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDBusMetaType>
#include <QDialog>
#include <QHeaderView>
#include <QHideEvent>
#include <QIcon>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSet>
#include <QShowEvent>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolTip>
#include <QtDBus>

#include <signal.h> //For SIGTERM

#include <KAuth/Action>
#include <KDialogJobUiDelegate>
#include <KGlobalAccel>
#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KWindowSystem>
#include <KX11Extras>
#include <klocalizedstring.h>
#include <kmessagebox.h>

#include "processdetails/ProcessDetailsDialog.h"
#include "ReniceDlg.h"
#include "../processcore/process_attribute.h"
#include "../processcore/process_controller.h"
#include "scripting.h"
#include "ui_ProcessWidgetUI.h"

#include <sys/types.h>
#include <unistd.h>

// Trolltech have a testing class for classes that inherit QAbstractItemModel.  If you want to run with this run-time testing enabled, put the modeltest.* files
// in this directory and uncomment the next line #define DO_MODELCHECK
#ifdef DO_MODELCHECK
#include "modeltest.h"
#endif
class ProgressBarItemDelegate : public QStyledItemDelegate
{
public:
    ProgressBarItemDelegate(QObject *parent)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        QStyleOptionViewItem option = opt;
        initStyleOption(&option, index);

        float percentage = index.data(ProcessModel::PercentageRole).toFloat();
        auto history = index.data(ProcessModel::PercentageHistoryRole).value<QVector<ProcessModel::PercentageHistoryEntry>>();
        if (percentage >= 0 || history.size() > 1)
            drawPercentageDisplay(painter, option, percentage, history);
        else
            QStyledItemDelegate::paint(painter, option, index);
    }

private:
    inline void
    drawPercentageDisplay(QPainter *painter, QStyleOptionViewItem &option, float percentage, const QVector<ProcessModel::PercentageHistoryEntry> &history) const
    {
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        const QRect &rect = option.rect;

        const int HIST_MS_PER_PX = 100; // 100 ms = 1 px -> 1 s = 10 px
        bool hasHistory = history.size() > 1;
        // Make sure that more than one entry is visible
        if (hasHistory) {
            int width = (history.crbegin()->timestamp - (history.crbegin() + 1)->timestamp) / HIST_MS_PER_PX;
            hasHistory = width < rect.width();
        }

        // draw the background
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;

        // Now draw our percentage thingy
        int size = qMin(int(percentage * rect.height()), rect.height());
        if (size > 2) { // make sure the line will have a width of more than 1 pixel
            painter->setPen(Qt::NoPen);
            QColor color = option.palette.color(cg, QPalette::Link);
            color.setAlpha(33);

            painter->fillRect(rect.x(), rect.y() + rect.height() - size, rect.width(), size, color);
        }

        // Draw the history graph
        if (hasHistory) {
            QColor color = option.palette.color(cg, QPalette::Link);
            color.setAlpha(66);
            painter->setPen(Qt::NoPen);

            QPainterPath path;
            // From right to left
            path.moveTo(rect.right(), rect.bottom());

            int xNow = rect.right();
            auto now = history.constLast();
            int height = qMin(int(rect.height() * now.value), rect.height());
            path.lineTo(xNow, rect.bottom() - height);

            for (int index = history.size() - 2; index >= 0 && xNow > rect.left(); --index) {
                auto next = history.at(index);
                int width = (now.timestamp - next.timestamp) / HIST_MS_PER_PX;
                int xNext = qMax(xNow - width, rect.left());

                now = next;
                xNow = xNext;
                int height = qMin(int(rect.height() * now.value), rect.height());

                path.lineTo(xNow, rect.bottom() - height);
            }

            path.lineTo(xNow, rect.bottom());
            path.lineTo(rect.right(), rect.bottom());

            painter->fillPath(path, color);
        }

        // draw the text
        if (!option.text.isEmpty()) {
            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &option, option.widget);

            if (option.state & QStyle::State_Selected) {
                painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
            } else {
                painter->setPen(option.palette.color(cg, QPalette::Text));
            }

            painter->setFont(option.font);
            QTextOption textOption;
            textOption.setWrapMode(QTextOption::ManualWrap);
            textOption.setTextDirection(option.direction);
            textOption.setAlignment(QStyle::visualAlignment(option.direction, option.displayAlignment));

            painter->drawText(textRect, option.text, textOption);
        }

        // draw the focus rect
        if (option.state & QStyle::State_HasFocus) {
            QStyleOptionFocusRect o;
            o.QStyleOption::operator=(option);
            o.rect = style->subElementRect(QStyle::SE_ItemViewItemFocusRect, &option, option.widget);
            o.state |= QStyle::State_KeyboardFocusChange;
            o.state |= QStyle::State_Item;
            QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
            o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected) ? QPalette::Highlight : QPalette::Window);
            style->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, option.widget);
        }
    }
};

struct KSysGuardProcessListPrivate {
    KSysGuardProcessListPrivate(KSysGuardProcessList *q, const QString &hostName)
        : mModel(q, hostName)
        , mFilterModel(q)
        , mUi(new Ui::ProcessWidget())
        , mProcessContextMenu(nullptr)
        , mUpdateTimer(nullptr)
        , mToolsMenu(new QMenu(q))
    {
        mScripting = nullptr;
        mNeedToExpandInit = false;
        mNumItemsSelected = -1;
        mResortCountDown =
            2; // The items added initially will be already sorted, but without CPU info.  On the second refresh we will have CPU usage, so /then/ we can resort
        renice = new QAction(i18np("Set Priority...", "Set Priority...", 1), q);
        renice->setShortcut(Qt::Key_F8);
        selectParent = new QAction(i18n("Jump to Parent Process"), q);

        selectTracer = new QAction(i18n("Jump to Process Debugging This One"), q);
        window = new QAction(i18n("Show Application Window"), q);
        processDetails = new QAction(i18nc("@action:inmenu", "Detailed Information..."), q);
        resume = new QAction(QIcon::fromTheme(QStringLiteral("media-playback-start")), i18n("Resume Stopped Process"), q);
        terminate = new QAction(i18np("End Process", "End Processes", 1), q);
        terminate->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
        terminate->setShortcut(Qt::Key_Delete);
        kill = new QAction(i18np("Forcibly Kill Process", "Forcibly Kill Processes", 1), q);
        kill->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
        kill->setShortcut(Qt::SHIFT | Qt::Key_Delete);

        sigStop = new QAction(i18n("Suspend (STOP)"), q);
        sigCont = new QAction(i18n("Continue (CONT)"), q);
        sigHup = new QAction(i18n("Hangup (HUP)"), q);
        sigInt = new QAction(i18n("Interrupt (INT)"), q);
        sigTerm = new QAction(i18n("Terminate (TERM)"), q);
        sigKill = new QAction(i18n("Kill (KILL)"), q);
        sigUsr1 = new QAction(i18n("User 1 (USR1)"), q);
        sigUsr2 = new QAction(i18n("User 2 (USR2)"), q);

        // Set up '/' as a shortcut to jump to the quick search text widget
        jumpToSearchFilter = new QAction(i18n("Focus on Quick Search"), q);
        jumpToSearchFilter->setShortcuts(QList<QKeySequence>() << QKeySequence::Find << '/');
    }

    ~KSysGuardProcessListPrivate()
    {
        delete mUi;
        mUi = nullptr;
    }

    /** The number rows and their children for the given parent in the mFilterModel model */
    int totalRowCount(const QModelIndex &parent) const;

    /** Helper function to setup 'action' with the given pids */
    void setupKAuthAction(KAuth::Action &action, const QList<long long> &pids) const;

    /** fire a timer event if we are set to use our internal timer*/
    void fireTimerEvent();

    /** The process model.  This contains all the data on all the processes running on the system */
    ProcessModel mModel;

    /** The process filter.  The mModel is connected to this, and this filter model connects to the view.  This lets us
     *  sort the view and filter (by using the combo box or the search line)
     */
    ProcessFilter mFilterModel;

    KSysGuard::ProcessController *mProcessController = nullptr;

    /** The graphical user interface for this process list widget, auto-generated by Qt Designer */
    Ui::ProcessWidget *mUi;

    /** The context menu when you right click on a process */
    QMenu *mProcessContextMenu;

    /** A timer to call updateList() every mUpdateIntervalMSecs.
     *  NULL is mUpdateIntervalMSecs is <= 0. */
    QTimer *mUpdateTimer;

    /** The time to wait, in milliseconds, between updating the process list */
    int mUpdateIntervalMSecs;

    /** Number of items that are selected */
    int mNumItemsSelected;

    /** Class to deal with the scripting. NULL if scripting is disabled */
    Scripting *mScripting;

    /** A counter to mark when to resort, so that we do not resort on every update */
    int mResortCountDown;

    bool mNeedToExpandInit;

    QAction *renice;
    QAction *terminate;
    QAction *kill;
    QAction *selectParent;
    QAction *selectTracer;
    QAction *jumpToSearchFilter;
    QAction *window;
    QAction *processDetails;
    QAction *resume;
    QAction *sigStop;
    QAction *sigCont;
    QAction *sigHup;
    QAction *sigInt;
    QAction *sigTerm;
    QAction *sigKill;
    QAction *sigUsr1;
    QAction *sigUsr2;

    QMenu *mToolsMenu;

    QPointer<ProcessDetailsDialog> processDetailsDialog;
};

KSysGuardProcessList::KSysGuardProcessList(QWidget *parent, const QString &hostName)
    : QWidget(parent)
    , d(new KSysGuardProcessListPrivate(this, hostName))
{
    qRegisterMetaType<QList<long long>>();
    qDBusRegisterMetaType<QList<long long>>();

    d->mProcessController = new KSysGuard::ProcessController(this);
    d->mProcessController->setWindow(windowHandle());

    d->mUpdateIntervalMSecs = 0; // Set process to not update manually by default
    d->mUi->setupUi(this);
    d->mFilterModel.setSourceModel(&d->mModel);
    d->mUi->treeView->setModel(&d->mFilterModel);
#ifdef DO_MODELCHECK
    new ModelTest(&d->mModel, this);
#endif
    d->mUi->treeView->setItemDelegate(new ProgressBarItemDelegate(d->mUi->treeView));

    d->mUi->treeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d->mUi->treeView->header(), &QWidget::customContextMenuRequested, this, &KSysGuardProcessList::showColumnContextMenu);

    d->mProcessContextMenu = new QMenu(d->mUi->treeView);
    d->mUi->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(d->mUi->treeView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showProcessContextMenu(QPoint)));

    d->mUi->treeView->header()->setSectionsClickable(true);
    d->mUi->treeView->header()->setSortIndicatorShown(true);
    d->mUi->treeView->header()->setCascadingSectionResizes(false);
    connect(d->mUi->btnKillProcess, &QAbstractButton::clicked, this, &KSysGuardProcessList::killSelectedProcesses);
    connect(d->mUi->txtFilter, &QLineEdit::textChanged, this, &KSysGuardProcessList::filterTextChanged);
    connect(d->mUi->cmbFilter, SIGNAL(currentIndexChanged(int)), this, SLOT(setStateInt(int)));
    connect(d->mUi->treeView, &QTreeView::expanded, this, &KSysGuardProcessList::expandAllChildren);
    connect(d->mUi->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &KSysGuardProcessList::selectionChanged);
    connect(&d->mFilterModel, &QAbstractItemModel::rowsInserted, this, &KSysGuardProcessList::rowsInserted);
    connect(&d->mFilterModel, &QAbstractItemModel::rowsRemoved, this, &KSysGuardProcessList::processListChanged);
    setMinimumSize(sizeHint());

    d->mFilterModel.setFilterKeyColumn(-1);

    /*  Hide various columns by default, to reduce information overload */
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingVmSize);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingNiceness);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingTty);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingStartTime);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingNoNewPrivileges);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingCommand);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingPid);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingCPUTime);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingIoRead);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingIoWrite);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingXMemory);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingCGroup);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingMACContext);
    d->mUi->treeView->header()->hideSection(ProcessModel::HeadingVmPSS);
    // NOTE!  After this is all setup, the settings for the header are restored
    // from the user's last run.  (in restoreHeaderState)
    // So making changes here only affects the default settings.  To
    // test changes temporarily, comment out the lines in restoreHeaderState.
    // When you are happy with the changes and want to commit, increase the
    // value of PROCESSHEADERVERSION.  This will force the header state
    // to be reset back to the defaults for all users.
    d->mUi->treeView->header()->resizeSection(ProcessModel::HeadingCPUUsage, d->mUi->treeView->header()->sectionSizeHint(ProcessModel::HeadingCPUUsage));
    d->mUi->treeView->header()->resizeSection(ProcessModel::HeadingMemory, d->mUi->treeView->header()->sectionSizeHint(ProcessModel::HeadingMemory));
    d->mUi->treeView->header()->resizeSection(ProcessModel::HeadingSharedMemory,
                                              d->mUi->treeView->header()->sectionSizeHint(ProcessModel::HeadingSharedMemory));
    d->mUi->treeView->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    d->mUi->treeView->header()->setStretchLastSection(true);

    // Process names can have mixed case. Make the filter case insensitive.
    d->mFilterModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    d->mFilterModel.setSortCaseSensitivity(Qt::CaseInsensitive);

    d->mUi->txtFilter->installEventFilter(this);
    d->mUi->treeView->installEventFilter(this);

    d->mUi->treeView->setDragEnabled(true);
    d->mUi->treeView->setDragDropMode(QAbstractItemView::DragOnly);

    auto extraAttributes = d->mModel.extraAttributes();
    for (int i = 0; i < extraAttributes.count(); ++i) {
        auto attribute = extraAttributes.at(i);
        if (!attribute->isVisibleByDefault()) {
            d->mUi->treeView->header()->hideSection(ProcessModel::HeadingPluginStart + i);
        }
    }

    // Sort by username by default
    d->mUi->treeView->sortByColumn(ProcessModel::HeadingUser, Qt::AscendingOrder);

    // Add all the actions to the main widget, and get all the actions to call actionTriggered when clicked
    QList<QAction *> actions;
    actions << d->renice << d->kill << d->terminate << d->selectParent << d->selectTracer << d->window;
    actions << d->processDetails << d->jumpToSearchFilter << d->resume << d->sigStop << d->sigCont;
    actions << d->sigHup << d->sigInt << d->sigTerm << d->sigKill << d->sigUsr1 << d->sigUsr2;

    for (QAction *action : actions) {
        addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            actionTriggered(action);
        });
    }

    retranslateUi();

    d->mUi->btnKillProcess->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    d->mUi->btnKillProcess->setToolTip(
        i18n("<qt>End the selected process. Warning - you may lose unsaved work.<br>Right click on a process to send other signals.<br>See What's This for "
             "technical information."));

    auto addByDesktopName = [this](const QString &desktopName) {
        auto kService = KService::serviceByDesktopName(desktopName);
        if (kService) {
            auto action = new QAction(QIcon::fromTheme(kService->icon()), kService->name(), this);

            connect(action, &QAction::triggered, this, [this, kService](bool) {
                auto *job = new KIO::ApplicationLauncherJob(kService);
                job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window()));
                job->start();
            });
            d->mToolsMenu->addAction(action);
        }
    };

    addByDesktopName(QStringLiteral("org.kde.konsole"));

    const QString ksysguardDesktopName = QStringLiteral("org.kde.ksysguard");
    // The following expression is true when the libksysguard process list is _not_ embedded in KSysGuard.
    // Only then we add KSysGuard to the menu
    if (qApp->desktopFileName() != ksysguardDesktopName) {
        addByDesktopName(ksysguardDesktopName);
    }

    addByDesktopName(QStringLiteral("org.kde.plasma-systemmonitor"));
    addByDesktopName(QStringLiteral("org.kde.ksystemlog"));
    addByDesktopName(QStringLiteral("org.kde.kinfocenter"));
    addByDesktopName(QStringLiteral("org.kde.filelight"));
    addByDesktopName(QStringLiteral("org.kde.sweeper"));
    addByDesktopName(QStringLiteral("org.kde.kmag"));
    addByDesktopName(QStringLiteral("htop"));

    // Add the xkill functionality...
    auto killWindowAction = new QAction(QIcon::fromTheme(QStringLiteral("document-close")), i18nc("@action:inmenu", "Kill a Window"), this);
    // Find shortcut of xkill functionality which is defined in KWin
    const auto killWindowShortcutList = KGlobalAccel::self()->globalShortcut(QStringLiteral("kwin"), QStringLiteral("Kill Window"));
    killWindowAction->setShortcuts(killWindowShortcutList);
    // We don't use xkill directly but the method in KWin which allows to press Esc to abort.
    auto killWindowKwinMethod = new QDBusInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"));
    // If KWin is not the window manager, then we disable the entry:
    if (!killWindowKwinMethod->isValid()) {
        killWindowAction->setEnabled(false);
    }
    connect(killWindowAction, &QAction::triggered, this, [this, killWindowKwinMethod](bool) {
        // with DBus call, always use the async method.
        // Otherwise it could wait up to 30 seconds in certain situations.
        killWindowKwinMethod->asyncCall(QStringLiteral("killWindow"));
    });
    d->mToolsMenu->addAction(killWindowAction);

    d->mUi->btnTools->setMenu(d->mToolsMenu);
}

KSysGuardProcessList::~KSysGuardProcessList()
{
    delete d;
}

QTreeView *KSysGuardProcessList::treeView() const
{
    return d->mUi->treeView;
}

QLineEdit *KSysGuardProcessList::filterLineEdit() const
{
    return d->mUi->txtFilter;
}

ProcessFilter::State KSysGuardProcessList::state() const
{
    return d->mFilterModel.filter();
}
void KSysGuardProcessList::setStateInt(int state)
{
    setState((ProcessFilter::State)state);
    d->mUi->treeView->scrollTo(d->mUi->treeView->currentIndex());
}
void KSysGuardProcessList::setState(ProcessFilter::State state)
{ // index is the item the user selected in the combo box
    d->mFilterModel.setFilter(state);
    d->mModel.setSimpleMode((state != ProcessFilter::AllProcessesInTreeForm));
    d->mUi->cmbFilter->setCurrentIndex((int)state);
    if (isVisible())
        expandInit();
}
void KSysGuardProcessList::filterTextChanged(const QString &newText)
{
    d->mFilterModel.setFilterRegularExpression(newText.trimmed());
    if (isVisible())
        expandInit();
    d->mUi->btnKillProcess->setEnabled(d->mUi->treeView->selectionModel()->hasSelection());
    d->mUi->treeView->scrollTo(d->mUi->treeView->currentIndex());
}

int KSysGuardProcessList::visibleProcessesCount() const
{
    // This assumes that all the visible rows are processes.  This is true currently, but might not be
    // true if we add support for showing threads etc
    if (d->mModel.isSimpleMode())
        return d->mFilterModel.rowCount();
    return d->totalRowCount(QModelIndex());
}

int KSysGuardProcessListPrivate::totalRowCount(const QModelIndex &parent) const
{
    int numRows = mFilterModel.rowCount(parent);
    int total = numRows;
    for (int i = 0; i < numRows; ++i) {
        QModelIndex index = mFilterModel.index(i, 0, parent);
        // if it has children add the total
        if (mFilterModel.hasChildren(index))
            total += totalRowCount(index);
    }
    return total;
}

void KSysGuardProcessListPrivate::setupKAuthAction(KAuth::Action &action, const QList<long long> &pids) const
{
    action.setHelperId(QStringLiteral("org.kde.ksysguard.processlisthelper"));

    const int processCount = pids.count();
    for (int i = 0; i < processCount; i++) {
        action.addArgument(QStringLiteral("pid%1").arg(i), pids[i]);
    }
    action.addArgument(QStringLiteral("pidcount"), processCount);
}
void KSysGuardProcessList::selectionChanged()
{
    int numSelected = d->mUi->treeView->selectionModel()->selectedRows().size();
    if (numSelected == d->mNumItemsSelected)
        return;
    d->mNumItemsSelected = numSelected;
    d->mUi->btnKillProcess->setEnabled(numSelected != 0);

    d->renice->setText(i18np("Set Priority...", "Set Priority...", numSelected));
    d->kill->setText(i18np("Forcibly Kill Process", "Forcibly Kill Processes", numSelected));
    d->terminate->setText(i18ncp("Context menu", "End Process", "End Processes", numSelected));
}
void KSysGuardProcessList::showProcessContextMenu(const QModelIndex &index)
{
    if (!index.isValid())
        return;
    QRect rect = d->mUi->treeView->visualRect(index);
    QPoint point(rect.x() + rect.width() / 4, rect.y() + rect.height() / 2);
    showProcessContextMenu(point);
}
void KSysGuardProcessList::showProcessContextMenu(const QPoint &point)
{
    d->mProcessContextMenu->clear();

    const QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
    const int numProcesses = selectedIndexes.size();

    if (numProcesses == 0) {
        // No processes selected, so no process context menu

        // Check just incase we have no columns visible.  In which case show the column context menu
        // so that users can unhide columns if there are no columns visible
        for (int i = 0; i < d->mFilterModel.columnCount(); ++i) {
            if (!d->mUi->treeView->header()->isSectionHidden(i))
                return;
        }
        showColumnContextMenu(point);
        return;
    }

    QModelIndex realIndex = d->mFilterModel.mapToSource(selectedIndexes.at(0));
    KSysGuard::Process *process = reinterpret_cast<KSysGuard::Process *>(realIndex.internalPointer());

    d->mProcessContextMenu->addAction(d->renice);
    QMenu *signalMenu = d->mProcessContextMenu->addMenu(i18n("Send Signal"));
    signalMenu->addAction(d->sigStop);
    signalMenu->addAction(d->sigCont);
    signalMenu->addAction(d->sigHup);
    signalMenu->addAction(d->sigInt);
    signalMenu->addAction(d->sigTerm);
    signalMenu->addAction(d->sigKill);
    signalMenu->addAction(d->sigUsr1);
    signalMenu->addAction(d->sigUsr2);

    if (numProcesses == 1 && process->parentPid() > 1) {
        // As a design decision, I do not show the 'Jump to parent process' option when the
        // parent is just 'init'.

        KSysGuard::Process *parent_process = d->mModel.getProcess(process->parentPid());
        if (parent_process) { // it should not be possible for this process to not exist, but check just incase
            QString parent_name = parent_process->name();
            d->selectParent->setText(i18n("Jump to Parent Process (%1)", parent_name));
            d->mProcessContextMenu->addAction(d->selectParent);
        }
    }

    if (numProcesses == 1 && process->tracerpid() >= 0) {
        // If the process is being debugged, offer to select it
        d->mProcessContextMenu->addAction(d->selectTracer);
    }

    if (numProcesses == 1 && !d->mModel.data(realIndex, ProcessModel::WindowIdRole).isNull()) {
        d->mProcessContextMenu->addAction(d->window);
    }

    if (numProcesses == 1) {
        const QFileInfo procDirFileInfo(QStringLiteral("/proc/%1").arg(process->pid()));
        if (procDirFileInfo.exists() && procDirFileInfo.isReadable() && procDirFileInfo.isDir()) {
            d->mProcessContextMenu->addAction(d->processDetails);
        }
    }

    if (numProcesses == 1 && process->status() == KSysGuard::Process::Stopped) {
        // If the process is stopped, offer to resume it
        d->mProcessContextMenu->addAction(d->resume);
    }

    if (numProcesses == 1 && d->mScripting) {
        for (QAction *action : d->mScripting->actions()) {
            d->mProcessContextMenu->addAction(action);
        }
    }
    d->mProcessContextMenu->addSeparator();
    d->mProcessContextMenu->addAction(d->terminate);
    if (numProcesses == 1 && process->timeKillWasSent().isValid())
        d->mProcessContextMenu->addAction(d->kill);

    d->mProcessContextMenu->popup(d->mUi->treeView->viewport()->mapToGlobal(point));
}
void KSysGuardProcessList::actionTriggered(QObject *object)
{
    if (!isVisible()) // Ignore triggered actions if we are not visible!
        return;
    // Reset the text back to normal
    d->selectParent->setText(i18n("Jump to Parent Process"));
    QAction *result = qobject_cast<QAction *>(object);
    if (result == nullptr) {
        // Escape was pressed. Do nothing.
    } else if (result == d->renice) {
        reniceSelectedProcesses();
    } else if (result == d->terminate) {
        sendSignalToSelectedProcesses(SIGTERM, true);
    } else if (result == d->kill) {
        sendSignalToSelectedProcesses(SIGKILL, true);
    } else if (result == d->selectParent) {
        QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
        int numProcesses = selectedIndexes.size();
        if (numProcesses == 0)
            return; // No processes selected
        QModelIndex realIndex = d->mFilterModel.mapToSource(selectedIndexes.at(0));
        KSysGuard::Process *process = reinterpret_cast<KSysGuard::Process *>(realIndex.internalPointer());
        if (process)
            selectAndJumpToProcess(process->parentPid());
    } else if (result == d->selectTracer) {
        QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
        int numProcesses = selectedIndexes.size();
        if (numProcesses == 0)
            return; // No processes selected
        QModelIndex realIndex = d->mFilterModel.mapToSource(selectedIndexes.at(0));
        KSysGuard::Process *process = reinterpret_cast<KSysGuard::Process *>(realIndex.internalPointer());
        if (process)
            selectAndJumpToProcess(process->tracerpid());
    } else if (result == d->window) {
        QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
        int numProcesses = selectedIndexes.size();
        if (numProcesses == 0)
            return; // No processes selected
        for (const QModelIndex &index : selectedIndexes) {
            QModelIndex realIndex = d->mFilterModel.mapToSource(index);
            QVariant widVar = d->mModel.data(realIndex, ProcessModel::WindowIdRole);
            if (!widVar.isNull()) {
                int wid = widVar.toInt();
                KX11Extras::activateWindow(wid);
            }
        }
    } else if (result == d->processDetails) {
        const QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
        if (selectedIndexes.size() != 1) {
            return;
        }
        const QModelIndex mappedIndex = d->mFilterModel.mapToSource(selectedIndexes.first());
        Q_ASSERT(mappedIndex.isValid());
        if (!d->processDetailsDialog) {
            d->processDetailsDialog = new ProcessDetailsDialog(this);
        }
        d->processDetailsDialog->setModelIndex(mappedIndex);
        d->processDetailsDialog->show();
        d->processDetailsDialog->raise();
    } else if (result == d->jumpToSearchFilter) {
        d->mUi->txtFilter->setFocus();
    } else {
        int sig;
        if (result == d->resume || result == d->sigCont)
            sig = SIGCONT; // Despite the function name, this sends a signal, rather than kill it.  Silly unix :)
        else if (result == d->sigStop)
            sig = SIGSTOP;
        else if (result == d->sigHup)
            sig = SIGHUP;
        else if (result == d->sigInt)
            sig = SIGINT;
        else if (result == d->sigTerm)
            sig = SIGTERM;
        else if (result == d->sigKill)
            sig = SIGKILL;
        else if (result == d->sigUsr1)
            sig = SIGUSR1;
        else if (result == d->sigUsr2)
            sig = SIGUSR2;
        else
            return;
        sendSignalToSelectedProcesses(sig, false);
    }
}

void KSysGuardProcessList::selectAndJumpToProcess(int pid)
{
    KSysGuard::Process *process = d->mModel.getProcess(pid);
    if (!process)
        return;
    QModelIndex sourceIndex = d->mModel.getQModelIndex(process, 0);
    QModelIndex filterIndex = d->mFilterModel.mapFromSource(sourceIndex);
    if (!filterIndex.isValid() && !d->mUi->txtFilter->text().isEmpty()) {
        // The filter is preventing us from finding the parent.  Clear the filter
        //(It could also be the combo box - should we deal with that case as well?)
        d->mUi->txtFilter->clear();
        filterIndex = d->mFilterModel.mapFromSource(sourceIndex);
    }
    d->mUi->treeView->clearSelection();
    d->mUi->treeView->setCurrentIndex(filterIndex);
    d->mUi->treeView->scrollTo(filterIndex, QAbstractItemView::PositionAtCenter);
}

void KSysGuardProcessList::showColumnContextMenu(const QPoint &point)
{
    QMenu menu;

    QAction *action;

    int num_headings = d->mFilterModel.columnCount();

    int index = d->mUi->treeView->header()->logicalIndexAt(point);
    if (index >= 0) {
        bool anyOtherVisibleColumns = false;
        for (int i = 0; i < num_headings; ++i) {
            if (i != index && !d->mUi->treeView->header()->isSectionHidden(i)) {
                anyOtherVisibleColumns = true;
                break;
            }
        }
        if (anyOtherVisibleColumns) {
            // selected a column.  Give the option to hide it
            action = new QAction(&menu);
            action->setData(-index - 1); // We set data to be negative (and minus 1) to hide a column, and positive to show a column
            action->setText(i18n("Hide Column '%1'", d->mFilterModel.headerData(index, Qt::Horizontal, Qt::DisplayRole).toString()));
            menu.addAction(action);
            if (d->mUi->treeView->header()->sectionsHidden()) {
                menu.addSeparator();
            }
        }
    }

    if (d->mUi->treeView->header()->sectionsHidden()) {
        for (int i = 0; i < num_headings; ++i) {
            if (d->mUi->treeView->header()->isSectionHidden(i)) {
#if !HAVE_XRES
                if (i == ProcessModel::HeadingXMemory)
                    continue;
#endif
                action = new QAction(&menu);
                action->setText(i18n("Show Column '%1'", d->mFilterModel.headerData(i, Qt::Horizontal, Qt::DisplayRole).toString()));
                action->setData(i); // We set data to be negative (and minus 1) to hide a column, and positive to show a column
                menu.addAction(action);
            }
        }
    }
    QAction *actionAuto = nullptr;
    QAction *actionKB = nullptr;
    QAction *actionMB = nullptr;
    QAction *actionGB = nullptr;
    QAction *actionPercentage = nullptr;
    QAction *actionShowCmdlineOptions = nullptr;
    QAction *actionShowTooltips = nullptr;
    QAction *actionNormalizeCPUUsage = nullptr;

    QAction *actionIoCharacters = nullptr;
    QAction *actionIoSyscalls = nullptr;
    QAction *actionIoActualCharacters = nullptr;
    QAction *actionIoShowRate = nullptr;
    bool showIoRate = false;
    if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite)
        showIoRate = d->mModel.ioInformation() == ProcessModel::BytesRate || d->mModel.ioInformation() == ProcessModel::SyscallsRate
            || d->mModel.ioInformation() == ProcessModel::ActualBytesRate;

    if (index == ProcessModel::HeadingVmSize || index == ProcessModel::HeadingMemory || index == ProcessModel::HeadingXMemory
        || index == ProcessModel::HeadingSharedMemory || index == ProcessModel::HeadingVmPSS
        || ((index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite) && d->mModel.ioInformation() != ProcessModel::Syscalls)) {
        // If the user right clicks on a column that contains a memory size, show a toggle option for displaying
        // the memory in different units.  e.g.  "2000 k" or "2 m"
        menu.addSeparator()->setText(i18n("Display Units"));
        QActionGroup *unitsGroup = new QActionGroup(&menu);
        /* Automatic (human readable)*/
        actionAuto = new QAction(&menu);
        actionAuto->setText(i18n("Mixed"));
        actionAuto->setCheckable(true);
        menu.addAction(actionAuto);
        unitsGroup->addAction(actionAuto);
        /* Kilobytes */
        actionKB = new QAction(&menu);
        actionKB->setText((showIoRate) ? i18n("Kilobytes per second") : i18n("Kilobytes"));
        actionKB->setCheckable(true);
        menu.addAction(actionKB);
        unitsGroup->addAction(actionKB);
        /* Megabytes */
        actionMB = new QAction(&menu);
        actionMB->setText((showIoRate) ? i18n("Megabytes per second") : i18n("Megabytes"));
        actionMB->setCheckable(true);
        menu.addAction(actionMB);
        unitsGroup->addAction(actionMB);
        /* Gigabytes */
        actionGB = new QAction(&menu);
        actionGB->setText((showIoRate) ? i18n("Gigabytes per second") : i18n("Gigabytes"));
        actionGB->setCheckable(true);
        menu.addAction(actionGB);
        unitsGroup->addAction(actionGB);
        ProcessModel::Units currentUnit;
        if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite) {
            currentUnit = d->mModel.ioUnits();
        } else {
            actionPercentage = new QAction(&menu);
            actionPercentage->setText(i18n("Percentage"));
            actionPercentage->setCheckable(true);
            menu.addAction(actionPercentage);
            unitsGroup->addAction(actionPercentage);
            currentUnit = d->mModel.units();
        }
        switch (currentUnit) {
        case ProcessModel::UnitsAuto:
            actionAuto->setChecked(true);
            break;
        case ProcessModel::UnitsKB:
            actionKB->setChecked(true);
            break;
        case ProcessModel::UnitsMB:
            actionMB->setChecked(true);
            break;
        case ProcessModel::UnitsGB:
            actionGB->setChecked(true);
            break;
        case ProcessModel::UnitsPercentage:
            actionPercentage->setChecked(true);
            break;
        default:
            break;
        }
        unitsGroup->setExclusive(true);
    } else if (index == ProcessModel::HeadingName) {
        menu.addSeparator();
        actionShowCmdlineOptions = new QAction(&menu);
        actionShowCmdlineOptions->setText(i18n("Display command line options"));
        actionShowCmdlineOptions->setCheckable(true);
        actionShowCmdlineOptions->setChecked(d->mModel.isShowCommandLineOptions());
        menu.addAction(actionShowCmdlineOptions);
    } else if (index == ProcessModel::HeadingCPUUsage) {
        menu.addSeparator();
        actionNormalizeCPUUsage = new QAction(&menu);
        actionNormalizeCPUUsage->setText(i18n("Divide CPU usage by number of CPUs"));
        actionNormalizeCPUUsage->setCheckable(true);
        actionNormalizeCPUUsage->setChecked(d->mModel.isNormalizedCPUUsage());
        menu.addAction(actionNormalizeCPUUsage);
    }

    if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite) {
        menu.addSeparator()->setText(i18n("Displayed Information"));
        QActionGroup *ioInformationGroup = new QActionGroup(&menu);
        actionIoCharacters = new QAction(&menu);
        actionIoCharacters->setText(i18n("Characters read/written"));
        actionIoCharacters->setCheckable(true);
        menu.addAction(actionIoCharacters);
        ioInformationGroup->addAction(actionIoCharacters);
        actionIoSyscalls = new QAction(&menu);
        actionIoSyscalls->setText(i18n("Number of Read/Write operations"));
        actionIoSyscalls->setCheckable(true);
        menu.addAction(actionIoSyscalls);
        ioInformationGroup->addAction(actionIoSyscalls);
        actionIoActualCharacters = new QAction(&menu);
        actionIoActualCharacters->setText(i18n("Bytes actually read/written"));
        actionIoActualCharacters->setCheckable(true);
        menu.addAction(actionIoActualCharacters);
        ioInformationGroup->addAction(actionIoActualCharacters);

        actionIoShowRate = new QAction(&menu);
        actionIoShowRate->setText(i18n("Show I/O rate"));
        actionIoShowRate->setCheckable(true);
        actionIoShowRate->setChecked(showIoRate);
        menu.addAction(actionIoShowRate);

        switch (d->mModel.ioInformation()) {
        case ProcessModel::Bytes:
        case ProcessModel::BytesRate:
            actionIoCharacters->setChecked(true);
            break;
        case ProcessModel::Syscalls:
        case ProcessModel::SyscallsRate:
            actionIoSyscalls->setChecked(true);
            break;
        case ProcessModel::ActualBytes:
        case ProcessModel::ActualBytesRate:
            actionIoActualCharacters->setChecked(true);
            break;
        default:
            break;
        }
    }

    menu.addSeparator();
    actionShowTooltips = new QAction(&menu);
    actionShowTooltips->setCheckable(true);
    actionShowTooltips->setChecked(d->mModel.isShowingTooltips());
    actionShowTooltips->setText(i18n("Show Tooltips"));
    menu.addAction(actionShowTooltips);

    QAction *result = menu.exec(d->mUi->treeView->header()->mapToGlobal(point));
    if (!result)
        return; // Menu cancelled
    if (result == actionAuto) {
        if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite)
            d->mModel.setIoUnits(ProcessModel::UnitsAuto);
        else
            d->mModel.setUnits(ProcessModel::UnitsAuto);
        return;
    } else if (result == actionKB) {
        if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite)
            d->mModel.setIoUnits(ProcessModel::UnitsKB);
        else
            d->mModel.setUnits(ProcessModel::UnitsKB);
        return;
    } else if (result == actionMB) {
        if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite)
            d->mModel.setIoUnits(ProcessModel::UnitsMB);
        else
            d->mModel.setUnits(ProcessModel::UnitsMB);
        return;
    } else if (result == actionGB) {
        if (index == ProcessModel::HeadingIoRead || index == ProcessModel::HeadingIoWrite)
            d->mModel.setIoUnits(ProcessModel::UnitsGB);
        else
            d->mModel.setUnits(ProcessModel::UnitsGB);
        return;
    } else if (result == actionPercentage) {
        d->mModel.setUnits(ProcessModel::UnitsPercentage);
        return;
    } else if (result == actionShowCmdlineOptions) {
        d->mModel.setShowCommandLineOptions(actionShowCmdlineOptions->isChecked());
        return;
    } else if (result == actionNormalizeCPUUsage) {
        d->mModel.setNormalizedCPUUsage(actionNormalizeCPUUsage->isChecked());
        return;
    } else if (result == actionShowTooltips) {
        d->mModel.setShowingTooltips(actionShowTooltips->isChecked());
        return;
    } else if (result == actionIoCharacters) {
        d->mModel.setIoInformation((showIoRate) ? ProcessModel::BytesRate : ProcessModel::Bytes);
        return;
    } else if (result == actionIoSyscalls) {
        d->mModel.setIoInformation((showIoRate) ? ProcessModel::SyscallsRate : ProcessModel::Syscalls);
        return;
    } else if (result == actionIoActualCharacters) {
        d->mModel.setIoInformation((showIoRate) ? ProcessModel::ActualBytesRate : ProcessModel::ActualBytes);
        return;
    } else if (result == actionIoShowRate) {
        showIoRate = actionIoShowRate->isChecked();
        switch (d->mModel.ioInformation()) {
        case ProcessModel::Bytes:
        case ProcessModel::BytesRate:
            d->mModel.setIoInformation((showIoRate) ? ProcessModel::BytesRate : ProcessModel::Bytes);
            break;
        case ProcessModel::Syscalls:
        case ProcessModel::SyscallsRate:
            d->mModel.setIoInformation((showIoRate) ? ProcessModel::SyscallsRate : ProcessModel::Syscalls);
            break;
        case ProcessModel::ActualBytes:
        case ProcessModel::ActualBytesRate:
            d->mModel.setIoInformation((showIoRate) ? ProcessModel::ActualBytesRate : ProcessModel::ActualBytes);
            break;
        default:
            break;
        }
    }

    int i = result->data().toInt();
    // We set data to be negative to hide a column, and positive to show a column
    if (i < 0) {
        auto index = -1 - i;
        d->mUi->treeView->hideColumn(index);
    } else {
        d->mUi->treeView->showColumn(i);
        updateList();
        d->mUi->treeView->resizeColumnToContents(i);
        d->mUi->treeView->resizeColumnToContents(d->mFilterModel.columnCount());
    }
    menu.deleteLater();
}

void KSysGuardProcessList::expandAllChildren(const QModelIndex &parent)
{
    // This is called when the user expands a node.  This then expands all of its
    // children.  This will trigger this function again recursively.
    QModelIndex sourceParent = d->mFilterModel.mapToSource(parent);
    for (int i = 0; i < d->mModel.rowCount(sourceParent); i++) {
        d->mUi->treeView->expand(d->mFilterModel.mapFromSource(d->mModel.index(i, 0, sourceParent)));
    }
}

void KSysGuardProcessList::rowsInserted(const QModelIndex &parent, int start, int end)
{
    if (d->mModel.isSimpleMode() || parent.isValid()) {
        Q_EMIT processListChanged();
        return; // No tree or not a root node - no need to expand init
    }
    disconnect(&d->mFilterModel, &QAbstractItemModel::rowsInserted, this, &KSysGuardProcessList::rowsInserted);
    // It is a root node that we just inserted - expand it
    bool expanded = false;
    for (int i = start; i <= end; i++) {
        QModelIndex index = d->mFilterModel.index(i, 0, QModelIndex());
        if (!d->mUi->treeView->isExpanded(index)) {
            if (!expanded) {
                disconnect(d->mUi->treeView, &QTreeView::expanded, this, &KSysGuardProcessList::expandAllChildren);
                expanded = true;
            }
            d->mUi->treeView->expand(index);
            d->mNeedToExpandInit = true;
        }
    }
    if (expanded)
        connect(d->mUi->treeView, &QTreeView::expanded, this, &KSysGuardProcessList::expandAllChildren);
    connect(&d->mFilterModel, &QAbstractItemModel::rowsInserted, this, &KSysGuardProcessList::rowsInserted);
    Q_EMIT processListChanged();
}

void KSysGuardProcessList::expandInit()
{
    if (d->mModel.isSimpleMode())
        return; // No tree - no need to expand init

    bool expanded = false;
    for (int i = 0; i < d->mFilterModel.rowCount(QModelIndex()); i++) {
        QModelIndex index = d->mFilterModel.index(i, 0, QModelIndex());
        if (!d->mUi->treeView->isExpanded(index)) {
            if (!expanded) {
                disconnect(d->mUi->treeView, &QTreeView::expanded, this, &KSysGuardProcessList::expandAllChildren);
                expanded = true;
            }

            d->mUi->treeView->expand(index);
        }
    }
    if (expanded)
        connect(d->mUi->treeView, &QTreeView::expanded, this, &KSysGuardProcessList::expandAllChildren);
}

void KSysGuardProcessList::hideEvent(QHideEvent *event) // virtual protected from QWidget
{
    // Stop updating the process list if we are hidden
    if (d->mUpdateTimer)
        d->mUpdateTimer->stop();
    // stop any scripts running, to save on memory
    if (d->mScripting)
        d->mScripting->stopAllScripts();

    QWidget::hideEvent(event);
}

void KSysGuardProcessList::showEvent(QShowEvent *event) // virtual protected from QWidget
{
    // Start updating the process list again if we are shown again
    updateList();
    QHeaderView *header = d->mUi->treeView->header();
    d->mUi->treeView->sortByColumn(header->sortIndicatorSection(), header->sortIndicatorOrder());

    QWidget::showEvent(event);
}

void KSysGuardProcessList::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        d->mModel.retranslateUi();
        d->mUi->retranslateUi(this);
        retranslateUi();
    }
    QWidget::changeEvent(event);
}
void KSysGuardProcessList::retranslateUi()
{
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::AllProcesses, QIcon::fromTheme(QStringLiteral("view-process-all")));
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::AllProcessesInTreeForm, QIcon::fromTheme(QStringLiteral("view-process-all-tree")));
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::SystemProcesses, QIcon::fromTheme(QStringLiteral("view-process-system")));
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::UserProcesses, QIcon::fromTheme(QStringLiteral("view-process-users")));
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::OwnProcesses, QIcon::fromTheme(QStringLiteral("view-process-own")));
    d->mUi->cmbFilter->setItemIcon(ProcessFilter::ProgramsOnly, QIcon::fromTheme(QStringLiteral("view-process-all")));
}

void KSysGuardProcessList::updateList()
{
    if (isVisible()) {
        KSysGuard::Processes::UpdateFlags updateFlags = KSysGuard::Processes::StandardInformation;
        if (!d->mUi->treeView->isColumnHidden(ProcessModel::HeadingIoRead) || !d->mUi->treeView->isColumnHidden(ProcessModel::HeadingIoWrite))
            updateFlags |= KSysGuard::Processes::IOStatistics;
        if (!d->mUi->treeView->isColumnHidden(ProcessModel::HeadingXMemory))
            updateFlags |= KSysGuard::Processes::XMemory;
        // Updating VmPSS every call results in ~4x CPU load on my machine, so do it less often
        if (!d->mUi->treeView->isColumnHidden(ProcessModel::HeadingVmPSS) && d->mResortCountDown <= 1)
            updateFlags |= KSysGuard::Processes::Smaps;
        d->mModel.update(d->mUpdateIntervalMSecs, updateFlags);
        if (d->mUpdateTimer)
            d->mUpdateTimer->start(d->mUpdateIntervalMSecs);
        Q_EMIT updated();
        if (QToolTip::isVisible() && qApp->topLevelAt(QCursor::pos()) == window()) {
            QWidget *w = d->mUi->treeView->viewport();
            if (w->geometry().contains(d->mUi->treeView->mapFromGlobal(QCursor::pos()))) {
                QHelpEvent event(QEvent::ToolTip, w->mapFromGlobal(QCursor::pos()), QCursor::pos());
                qApp->notify(w, &event);
            }
        }
        if (--d->mResortCountDown <= 0) {
            d->mResortCountDown = 2; // resort every second time
            // resort now
            QHeaderView *header = d->mUi->treeView->header();
            d->mUi->treeView->sortByColumn(header->sortIndicatorSection(), header->sortIndicatorOrder());
        }
        if (d->mNeedToExpandInit) {
            expandInit();
            d->mNeedToExpandInit = false;
        }
    }
}

int KSysGuardProcessList::updateIntervalMSecs() const
{
    return d->mUpdateIntervalMSecs;
}

void KSysGuardProcessList::setUpdateIntervalMSecs(int intervalMSecs)
{
    if (intervalMSecs == d->mUpdateIntervalMSecs)
        return;

    d->mUpdateIntervalMSecs = intervalMSecs;
    if (intervalMSecs <= 0) { // no point keep the timer around if we aren't updating automatically
        delete d->mUpdateTimer;
        d->mUpdateTimer = nullptr;
        return;
    }

    if (!d->mUpdateTimer) {
        // intervalMSecs is a valid time, so set up a timer
        d->mUpdateTimer = new QTimer(this);
        d->mUpdateTimer->setSingleShot(true);
        connect(d->mUpdateTimer, &QTimer::timeout, this, &KSysGuardProcessList::updateList);
        if (isVisible())
            d->mUpdateTimer->start(d->mUpdateIntervalMSecs);
    } else
        d->mUpdateTimer->setInterval(d->mUpdateIntervalMSecs);
}

bool KSysGuardProcessList::reniceProcesses(const QList<long long> &pids, int niceValue)
{
    auto result = d->mProcessController->setPriority(pids, niceValue);
    if (result == KSysGuard::ProcessController::Result::Success) {
        updateList();
        return true;
    } else if (result == KSysGuard::ProcessController::Result::Error) {
        KMessageBox::error(this,
                           i18n("You do not have the permission to renice the process and there "
                                "was a problem trying to run as root."));
    }
    return true;
}

QList<KSysGuard::Process *> KSysGuardProcessList::selectedProcesses() const
{
    QList<KSysGuard::Process *> processes;
    QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
    for (int i = 0; i < selectedIndexes.size(); ++i) {
        KSysGuard::Process *process = reinterpret_cast<KSysGuard::Process *>(d->mFilterModel.mapToSource(selectedIndexes.at(i)).internalPointer());
        processes << process;
    }
    return processes;
}

void KSysGuardProcessList::reniceSelectedProcesses()
{
    QList<long long> pids;
    QPointer<ReniceDlg> reniceDlg;
    {
        QList<KSysGuard::Process *> processes = selectedProcesses();
        QStringList selectedAsStrings;

        if (processes.isEmpty()) {
            KMessageBox::error(this, i18n("You must select a process first."));
            return;
        }

        int sched = -2;
        int iosched = -2;
        for (KSysGuard::Process *process : processes) {
            pids << process->pid();
            selectedAsStrings << d->mModel.getStringForProcess(process);
            if (sched == -2)
                sched = (int)process->scheduler();
            else if (sched != -1 && sched != (int)process->scheduler())
                sched = -1; // If two processes have different schedulers, disable the cpu scheduler stuff
            if (iosched == -2)
                iosched = (int)process->ioPriorityClass();
            else if (iosched != -1 && iosched != (int)process->ioPriorityClass())
                iosched = -1; // If two processes have different schedulers, disable the cpu scheduler stuff
        }
        int firstPriority = processes.first()->niceLevel();
        int firstIOPriority = processes.first()->ioniceLevel();

        bool supportsIoNice = d->mModel.processController()->supportsIoNiceness();
        if (!supportsIoNice) {
            iosched = -2;
            firstIOPriority = -2;
        }
        reniceDlg = new ReniceDlg(d->mUi->treeView, selectedAsStrings, firstPriority, sched, firstIOPriority, iosched);
        if (reniceDlg->exec() == QDialog::Rejected) {
            delete reniceDlg;
            return;
        }
    }

    // Because we've done into ReniceDlg, which calls processEvents etc, our processes list is no
    // longer valid

    QList<long long> renicePids;
    QList<long long> changeCPUSchedulerPids;
    QList<long long> changeIOSchedulerPids;
    for (long long pid : pids) {
        KSysGuard::Process *process = d->mModel.getProcess(pid);
        if (!process)
            continue;

        switch (reniceDlg->newCPUSched) {
        case -2:
        case -1: // Invalid, not changed etc.
            break; // So do nothing
        case KSysGuard::Process::Other:
        case KSysGuard::Process::Fifo: // Don't know if some other
                                       // system uses SCHED_FIFO
                                       // with niceness. Linux
                                       // doesn't
        case KSysGuard::Process::Batch:
            if (reniceDlg->newCPUSched != (int)process->scheduler()) {
                changeCPUSchedulerPids << pid;
                renicePids << pid;
            } else if (reniceDlg->newCPUPriority != process->niceLevel())
                renicePids << pid;
            break;

        case KSysGuard::Process::RoundRobin:
            if (reniceDlg->newCPUSched != (int)process->scheduler() || reniceDlg->newCPUPriority != process->niceLevel()) {
                changeCPUSchedulerPids << pid;
            }
            break;
        }
        switch (reniceDlg->newIOSched) {
        case -2:
        case -1: // Invalid, not changed etc.
            break; // So do nothing
        case KSysGuard::Process::None:
            if (reniceDlg->newIOSched != (int)process->ioPriorityClass()) {
                // Unfortunately linux doesn't actually let us set the ioniceness back to none after being set to something else
                if (process->ioPriorityClass() != KSysGuard::Process::BestEffort || reniceDlg->newIOPriority != process->ioniceLevel())
                    changeIOSchedulerPids << pid;
            }
            break;
        case KSysGuard::Process::Idle:
            if (reniceDlg->newIOSched != (int)process->ioPriorityClass()) {
                changeIOSchedulerPids << pid;
            }
            break;
        case KSysGuard::Process::BestEffort:
            if (process->ioPriorityClass() == KSysGuard::Process::None && reniceDlg->newIOPriority == (process->niceLevel() + 20) / 5)
                break; // Don't set to BestEffort if it's on None and the nicelevel wouldn't change
        case KSysGuard::Process::RealTime:
            if (reniceDlg->newIOSched != (int)process->ioPriorityClass() || reniceDlg->newIOPriority != process->ioniceLevel()) {
                changeIOSchedulerPids << pid;
            }
            break;
        }
    }
    if (!changeCPUSchedulerPids.isEmpty()) {
        Q_ASSERT(reniceDlg->newCPUSched >= 0);
        if (!changeCpuScheduler(changeCPUSchedulerPids, (KSysGuard::Process::Scheduler)reniceDlg->newCPUSched, reniceDlg->newCPUPriority)) {
            delete reniceDlg;
            return;
        }
    }
    if (!renicePids.isEmpty()) {
        Q_ASSERT(reniceDlg->newCPUPriority <= 20 && reniceDlg->newCPUPriority >= -20);
        if (!reniceProcesses(renicePids, reniceDlg->newCPUPriority)) {
            delete reniceDlg;
            return;
        }
    }
    if (!changeIOSchedulerPids.isEmpty()) {
        if (!changeIoScheduler(changeIOSchedulerPids, (KSysGuard::Process::IoPriorityClass)reniceDlg->newIOSched, reniceDlg->newIOPriority)) {
            delete reniceDlg;
            return;
        }
    }
    delete reniceDlg;
    updateList();
}

bool KSysGuardProcessList::changeIoScheduler(const QList<long long> &pids, KSysGuard::Process::IoPriorityClass newIoSched, int newIoSchedPriority)
{
    auto result = d->mProcessController->setIOScheduler(pids, newIoSched, newIoSchedPriority);
    if (result == KSysGuard::ProcessController::Result::Success) {
        updateList();
        return true;
    } else if (result == KSysGuard::ProcessController::Result::Error) {
        KMessageBox::error(this,
                           i18n("You do not have the permission to change the I/O priority of the process and there "
                                "was a problem trying to run as root."));
    }

    return false;
}

bool KSysGuardProcessList::changeCpuScheduler(const QList<long long> &pids, KSysGuard::Process::Scheduler newCpuSched, int newCpuSchedPriority)
{
    auto result = d->mProcessController->setCPUScheduler(pids, newCpuSched, newCpuSchedPriority);

    if (result == KSysGuard::ProcessController::Result::Success) {
        updateList();
        return true;
    } else if (result == KSysGuard::ProcessController::Result::Error) {
        KMessageBox::error(this,
                           i18n("You do not have the permission to change the CPU Scheduler for the process and there "
                                "was a problem trying to run as root."));
    }
    return false;
}

bool KSysGuardProcessList::killProcesses(const QList<long long> &pids, int sig)
{
    auto result = d->mProcessController->sendSignal(pids, sig);

    if (result == KSysGuard::ProcessController::Result::Success) {
        updateList();
        return true;
    } else if (result == KSysGuard::ProcessController::Result::Error) {
        KMessageBox::error(this,
                           i18n("You do not have the permission to kill the process and there "
                                "was a problem trying to run as root."));
    }
    return false;
}

void KSysGuardProcessList::killSelectedProcesses()
{
    sendSignalToSelectedProcesses(SIGTERM, true);
}

void KSysGuardProcessList::sendSignalToSelectedProcesses(int sig, bool confirm)
{
    QModelIndexList selectedIndexes = d->mUi->treeView->selectionModel()->selectedRows();
    QStringList selectedAsStrings;
    QList<long long> selectedPids;

    QList<KSysGuard::Process *> processes = selectedProcesses();
    for (KSysGuard::Process *process : processes) {
        selectedPids << process->pid();
        if (!confirm)
            continue;
        QString name = d->mModel.getStringForProcess(process);
        selectedAsStrings << name;
    }

    if (selectedPids.isEmpty()) {
        if (confirm)
            KMessageBox::error(this, i18n("You must select a process first."));
        return;
    } else if (confirm && (sig == SIGTERM || sig == SIGKILL)) {
        int count = selectedAsStrings.count();
        QString msg;
        QString title;
        QString dontAskAgainKey;
        QString closeButton;
        if (sig == SIGTERM) {
            msg = i18np("Are you sure you want to end this process?  Any unsaved work may be lost.",
                        "Are you sure you want to end these %1 processes?  Any unsaved work may be lost",
                        count);
            title = i18ncp("Dialog title", "End Process", "End %1 Processes", count);
            dontAskAgainKey = QStringLiteral("endconfirmation");
            closeButton = i18n("End");
        } else if (sig == SIGKILL) {
            msg = i18np("<qt>Are you sure you want to <b>immediately and forcibly kill</b> this process?  Any unsaved work may be lost.",
                        "<qt>Are you sure you want to <b>immediately and forcibly kill</b> these %1 processes?  Any unsaved work may be lost",
                        count);
            title = i18ncp("Dialog title", "Forcibly Kill Process", "Forcibly Kill %1 Processes", count);
            dontAskAgainKey = QStringLiteral("killconfirmation");
            closeButton = i18n("Kill");
        }

        int res = KMessageBox::warningContinueCancelList(this,
                                                         msg,
                                                         selectedAsStrings,
                                                         title,
                                                         KGuiItem(closeButton, QStringLiteral("process-stop")),
                                                         KStandardGuiItem::cancel(),
                                                         dontAskAgainKey);
        if (res != KMessageBox::Continue)
            return;
    }

    // We have shown a GUI dialog box, which processes events etc.
    // So processes is NO LONGER VALID

    if (!killProcesses(selectedPids, sig))
        return;
    if (sig == SIGTERM || sig == SIGKILL) {
        for (long long pid : selectedPids) {
            KSysGuard::Process *process = d->mModel.getProcess(pid);
            if (process)
                process->timeKillWasSent().start();
            d->mUi->treeView->selectionModel()->clearSelection();
        }
    }
    updateList();
}

bool KSysGuardProcessList::showTotals() const
{
    return d->mModel.showTotals();
}

void KSysGuardProcessList::setShowTotals(bool showTotals) // slot
{
    d->mModel.setShowTotals(showTotals);
}

ProcessModel::Units KSysGuardProcessList::units() const
{
    return d->mModel.units();
}

void KSysGuardProcessList::setUnits(ProcessModel::Units unit)
{
    d->mModel.setUnits(unit);
}

void KSysGuardProcessList::saveSettings(KConfigGroup &cg)
{
    /* Note that the ksysguard program does not use these functions.  It saves the settings itself to an xml file instead */
    cg.writeEntry("units", (int)(units()));
    cg.writeEntry("ioUnits", (int)(d->mModel.ioUnits()));
    cg.writeEntry("ioInformation", (int)(d->mModel.ioInformation()));
    cg.writeEntry("showCommandLineOptions", d->mModel.isShowCommandLineOptions());
    cg.writeEntry("normalizeCPUUsage", d->mModel.isNormalizedCPUUsage());
    cg.writeEntry("showTooltips", d->mModel.isShowingTooltips());
    cg.writeEntry("showTotals", showTotals());
    cg.writeEntry("filterState", (int)(state()));
    cg.writeEntry("updateIntervalMSecs", updateIntervalMSecs());
    cg.writeEntry("headerState", d->mUi->treeView->header()->saveState());
    // If we change, say, the header between versions of ksysguard, then the old headerState settings will not be valid.
    // The version property lets us keep track of which version we are
    cg.writeEntry("version", PROCESSHEADERVERSION);
}

void KSysGuardProcessList::loadSettings(const KConfigGroup &cg)
{
    /* Note that the ksysguard program does not use these functions.  It saves the settings itself to an xml file instead */
    setUnits((ProcessModel::Units)cg.readEntry("units", (int)ProcessModel::UnitsKB));
    d->mModel.setIoUnits((ProcessModel::Units)cg.readEntry("ioUnits", (int)ProcessModel::UnitsKB));
    d->mModel.setIoInformation((ProcessModel::IoInformation)cg.readEntry("ioInformation", (int)ProcessModel::ActualBytesRate));
    d->mModel.setShowCommandLineOptions(cg.readEntry("showCommandLineOptions", false));
    d->mModel.setNormalizedCPUUsage(cg.readEntry("normalizeCPUUsage", true));
    d->mModel.setShowingTooltips(cg.readEntry("showTooltips", true));
    setShowTotals(cg.readEntry("showTotals", true));
    setStateInt(cg.readEntry("filterState", (int)ProcessFilter::AllProcesses));
    setUpdateIntervalMSecs(cg.readEntry("updateIntervalMSecs", 2000));
    int version = cg.readEntry("version", 0);
    if (version == PROCESSHEADERVERSION) { // If the header has changed, the old settings are no longer valid.  Only restore if version is the same
        restoreHeaderState(cg.readEntry("headerState", QByteArray()));
    }
}

void KSysGuardProcessList::restoreHeaderState(const QByteArray &state)
{
    d->mUi->treeView->header()->restoreState(state);
}

bool KSysGuardProcessList::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (obj == d->mUi->treeView) {
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
                d->mUi->treeView->selectionModel()->select(d->mUi->treeView->currentIndex(), QItemSelectionModel::Select | QItemSelectionModel::Rows);
                showProcessContextMenu(d->mUi->treeView->currentIndex());
                return true;

            } else if (keyEvent->matches(QKeySequence::MoveToPreviousLine) || keyEvent->matches(QKeySequence::SelectPreviousLine)
                       || keyEvent->matches(QKeySequence::MoveToPreviousPage) || keyEvent->matches(QKeySequence::SelectPreviousPage)) {
                if (d->mUi->treeView->selectionModel()->selectedRows().size() == 1 && d->mUi->treeView->selectionModel()->selectedRows().first().row() == 0) {
                    // when first row is selected, pressing up or pgup moves to the textfield
                    d->mUi->txtFilter->setFocus();
                    return true;
                }
            } else if (!keyEvent->text().isEmpty() && keyEvent->key() != Qt::Key_Tab
                       && (!keyEvent->modifiers() || keyEvent->modifiers() == Qt::ShiftModifier)) {
                // move to textfield and forward keyevent if user starts typing from treeview
                d->mUi->txtFilter->setFocus();
                QApplication::sendEvent(d->mUi->txtFilter, event);
                return true;
            }
        } else {
            Q_ASSERT(obj == d->mUi->txtFilter);
            if (d->mUi->treeView->model()->rowCount() == 0) {
                // treeview is empty, do nothing
                return false;
            }

            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
                // pressing enter will send enter to the first row in the list
                // the focusin eventfilter will make sure the first row is selected if there was
                // no previous selection
                d->mUi->treeView->setFocus();
                QApplication::sendEvent(d->mUi->treeView, event);
                return true;

            } else if (keyEvent->matches(QKeySequence::MoveToNextLine) || keyEvent->matches(QKeySequence::SelectNextLine)
                       || keyEvent->matches(QKeySequence::MoveToNextPage) || keyEvent->matches(QKeySequence::SelectNextPage)) {
                // attempting to move down by down-key or pgdown, or pressing enter will move focus
                // to the treeview
                d->mUi->treeView->setFocus();
                return true;
            }
        }
    }
    return false;
}

ProcessModel *KSysGuardProcessList::processModel()
{
    return &d->mModel;
}

void KSysGuardProcessList::setKillButtonVisible(bool visible)
{
    d->mUi->btnKillProcess->setVisible(visible);
}

bool KSysGuardProcessList::isKillButtonVisible() const
{
    return d->mUi->btnKillProcess->isVisible();
}
bool KSysGuardProcessList::scriptingEnabled() const
{
    return !!d->mScripting;
}
void KSysGuardProcessList::setScriptingEnabled(bool enabled)
{
    if (!!d->mScripting == enabled)
        return; // Nothing changed
    if (!enabled) {
        delete d->mScripting;
        d->mScripting = nullptr;
    } else {
        d->mScripting = new Scripting(this);
        d->mScripting->hide();
    }
}
