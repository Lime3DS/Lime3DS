// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/assert.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/kernel/wait_object.h"
#include "lime_qt/debugger/wait_tree.h"
#include "lime_qt/uisettings.h"

namespace {

constexpr std::array<std::array<Qt::GlobalColor, 2>, 8> WaitTreeColors{{
    {Qt::GlobalColor::darkGreen, Qt::GlobalColor::green},
    {Qt::GlobalColor::darkBlue, Qt::GlobalColor::cyan},
    {Qt::GlobalColor::darkRed, Qt::GlobalColor::red},
    {Qt::GlobalColor::darkYellow, Qt::GlobalColor::yellow},
    {Qt::GlobalColor::darkCyan, Qt::GlobalColor::cyan},
    {Qt::GlobalColor::red, Qt::GlobalColor::red},
    {Qt::GlobalColor::darkCyan, Qt::GlobalColor::cyan},
    {Qt::GlobalColor::gray, Qt::GlobalColor::gray},
}};

bool IsDarkTheme() {
    const auto& theme = UISettings::values.theme;
    return theme == QStringLiteral("qdarkstyle") || theme == QStringLiteral("colorful_dark");
}

} // namespace

WaitTreeItem::~WaitTreeItem() = default;

QColor WaitTreeItem::GetColor() const {
    if (IsDarkTheme()) {
        return QColor(Qt::GlobalColor::white);
    } else {
        return QColor(Qt::GlobalColor::black);
    }
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeItem::GetChildren() const {
    return {};
}

void WaitTreeItem::Expand() {
    if (IsExpandable() && !expanded) {
        children = GetChildren();
        for (std::size_t i = 0; i < children.size(); ++i) {
            children[i]->parent = this;
            children[i]->row = i;
        }
        expanded = true;
    }
}

WaitTreeItem* WaitTreeItem::Parent() const {
    return parent;
}

std::span<const std::unique_ptr<WaitTreeItem>> WaitTreeItem::Children() const {
    return children;
}

bool WaitTreeItem::IsExpandable() const {
    return false;
}

std::size_t WaitTreeItem::Row() const {
    return row;
}

std::vector<std::unique_ptr<WaitTreeThread>> WaitTreeItem::MakeThreadItemList(
    Core::System& system) {
    const u32 num_cores = system.GetNumCores();
    std::vector<std::unique_ptr<WaitTreeThread>> item_list;
    item_list.reserve(num_cores);
    for (u32 i = 0; i < num_cores; ++i) {
        const auto threads = system.Kernel().GetThreadManager(i).GetThreadList();
        item_list.reserve(item_list.size() + threads.size());
        for (std::size_t j = 0; j < threads.size(); ++j) {
            item_list.push_back(std::make_unique<WaitTreeThread>(*threads[j]));
            item_list.back()->row = j;
        }
    }
    return item_list;
}

WaitTreeText::WaitTreeText(QString t) : text(std::move(t)) {}
WaitTreeText::~WaitTreeText() = default;

QString WaitTreeText::GetText() const {
    return text;
}

WaitTreeWaitObject::WaitTreeWaitObject(const Kernel::WaitObject& o) : object(o) {}

bool WaitTreeExpandableItem::IsExpandable() const {
    return true;
}

QString WaitTreeWaitObject::GetText() const {
    return tr("[%1]%2 %3")
        .arg(object.GetObjectId())
        .arg(QString::fromStdString(object.GetTypeName()),
             QString::fromStdString(object.GetName()));
}

std::unique_ptr<WaitTreeWaitObject> WaitTreeWaitObject::make(const Kernel::WaitObject& object) {
    switch (object.GetHandleType()) {
    case Kernel::HandleType::Event:
        return std::make_unique<WaitTreeEvent>(static_cast<const Kernel::Event&>(object));
    case Kernel::HandleType::Mutex:
        return std::make_unique<WaitTreeMutex>(static_cast<const Kernel::Mutex&>(object));
    case Kernel::HandleType::Semaphore:
        return std::make_unique<WaitTreeSemaphore>(static_cast<const Kernel::Semaphore&>(object));
    case Kernel::HandleType::Timer:
        return std::make_unique<WaitTreeTimer>(static_cast<const Kernel::Timer&>(object));
    case Kernel::HandleType::Thread:
        return std::make_unique<WaitTreeThread>(static_cast<const Kernel::Thread&>(object));
    default:
        return std::make_unique<WaitTreeWaitObject>(object);
    }
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeWaitObject::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list;

    const auto& threads = object.GetWaitingThreads();
    if (threads.empty()) {
        list.push_back(std::make_unique<WaitTreeText>(tr("waited by no thread")));
    } else {
        list.push_back(std::make_unique<WaitTreeThreadList>(threads));
    }
    return list;
}

QString WaitTreeWaitObject::GetResetTypeQString(Kernel::ResetType reset_type) {
    switch (reset_type) {
    case Kernel::ResetType::OneShot:
        return tr("one shot");
    case Kernel::ResetType::Sticky:
        return tr("sticky");
    case Kernel::ResetType::Pulse:
        return tr("pulse");
    }
    UNREACHABLE();
    return {};
}

WaitTreeObjectList::WaitTreeObjectList(const std::vector<std::shared_ptr<Kernel::WaitObject>>& list,
                                       bool w_all)
    : object_list(list), wait_all(w_all) {}

QString WaitTreeObjectList::GetText() const {
    if (wait_all)
        return tr("waiting for all objects");
    return tr("waiting for one of the following objects");
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeObjectList::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(object_list.size());
    std::transform(object_list.begin(), object_list.end(), list.begin(),
                   [](const auto& t) { return WaitTreeWaitObject::make(*t); });
    return list;
}

WaitTreeThread::WaitTreeThread(const Kernel::Thread& thread) : WaitTreeWaitObject(thread) {}

QString WaitTreeThread::GetText() const {
    const auto& thread = static_cast<const Kernel::Thread&>(object);
    QString status;
    switch (thread.status) {
    case Kernel::ThreadStatus::Running:
        status = tr("running");
        break;
    case Kernel::ThreadStatus::Ready:
        status = tr("ready");
        break;
    case Kernel::ThreadStatus::WaitArb:
        status = tr("waiting for address 0x%1").arg(thread.wait_address, 8, 16, QLatin1Char('0'));
        break;
    case Kernel::ThreadStatus::WaitSleep:
        status = tr("sleeping");
        break;
    case Kernel::ThreadStatus::WaitIPC:
        status = tr("waiting for IPC response");
        break;
    case Kernel::ThreadStatus::WaitSynchAll:
    case Kernel::ThreadStatus::WaitSynchAny:
        status = tr("waiting for objects");
        break;
    case Kernel::ThreadStatus::WaitHleEvent:
        status = tr("waiting for HLE return");
        break;
    case Kernel::ThreadStatus::Dormant:
        status = tr("dormant");
        break;
    case Kernel::ThreadStatus::Dead:
        status = tr("dead");
        break;
    }
    QString pc_info = tr(" PC = 0x%1 LR = 0x%2")
                          .arg(thread.context.GetProgramCounter(), 8, 16, QLatin1Char('0'))
                          .arg(thread.context.GetLinkRegister(), 8, 16, QLatin1Char('0'));
    return QStringLiteral("%1%2 (%3) ").arg(WaitTreeWaitObject::GetText(), pc_info, status);
}

QColor WaitTreeThread::GetColor() const {
    const std::size_t color_index = IsDarkTheme() ? 1 : 0;

    const auto& thread = static_cast<const Kernel::Thread&>(object);
    switch (thread.status) {
    case Kernel::ThreadStatus::Running:
        return QColor(WaitTreeColors[0][color_index]);
    case Kernel::ThreadStatus::Ready:
        return QColor(WaitTreeColors[1][color_index]);
    case Kernel::ThreadStatus::WaitArb:
        return QColor(WaitTreeColors[2][color_index]);
    case Kernel::ThreadStatus::WaitSleep:
        return QColor(WaitTreeColors[3][color_index]);
    case Kernel::ThreadStatus::WaitIPC:
        return QColor(WaitTreeColors[4][color_index]);
    case Kernel::ThreadStatus::WaitSynchAll:
    case Kernel::ThreadStatus::WaitSynchAny:
    case Kernel::ThreadStatus::WaitHleEvent:
        return QColor(WaitTreeColors[5][color_index]);
    case Kernel::ThreadStatus::Dormant:
        return QColor(WaitTreeColors[6][color_index]);
    case Kernel::ThreadStatus::Dead:
        return QColor(WaitTreeColors[7][color_index]);
    default:
        return WaitTreeItem::GetColor();
    }
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeThread::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(WaitTreeWaitObject::GetChildren());

    const auto& thread = static_cast<const Kernel::Thread&>(object);

    QString processor;
    switch (thread.processor_id) {
    case Kernel::ThreadProcessorId::ThreadProcessorIdDefault:
        processor = tr("default");
        break;
    case Kernel::ThreadProcessorId::ThreadProcessorIdAll:
        processor = tr("all");
        break;
    case Kernel::ThreadProcessorId::ThreadProcessorId0:
        processor = tr("AppCore");
        break;
    case Kernel::ThreadProcessorId::ThreadProcessorId1:
        processor = tr("SysCore");
        break;
    default:
        processor = tr("Unknown processor %1").arg(thread.processor_id);
        break;
    }

    list.push_back(std::make_unique<WaitTreeText>(tr("object id = %1").arg(thread.GetObjectId())));
    list.push_back(std::make_unique<WaitTreeText>(tr("processor = %1").arg(processor)));
    list.push_back(std::make_unique<WaitTreeText>(tr("thread id = %1").arg(thread.GetThreadId())));
    if (auto process = thread.owner_process.lock()) {
        list.push_back(
            std::make_unique<WaitTreeText>(tr("process = %1 (%2)")
                                               .arg(QString::fromStdString(process->GetName()))
                                               .arg(process->process_id)));
    }
    list.push_back(std::make_unique<WaitTreeText>(tr("priority = %1(current) / %2(normal)")
                                                      .arg(thread.current_priority)
                                                      .arg(thread.nominal_priority)));
    list.push_back(std::make_unique<WaitTreeText>(
        tr("last running ticks = %1").arg(thread.last_running_ticks)));

    if (thread.held_mutexes.empty()) {
        list.push_back(std::make_unique<WaitTreeText>(tr("not holding mutex")));
    } else {
        list.push_back(std::make_unique<WaitTreeMutexList>(thread.held_mutexes));
    }
    if (thread.status == Kernel::ThreadStatus::WaitSynchAny ||
        thread.status == Kernel::ThreadStatus::WaitSynchAll ||
        thread.status == Kernel::ThreadStatus::WaitHleEvent) {
        list.push_back(std::make_unique<WaitTreeObjectList>(thread.wait_objects,
                                                            thread.IsSleepingOnWaitAll()));
    }

    return list;
}

WaitTreeEvent::WaitTreeEvent(const Kernel::Event& object) : WaitTreeWaitObject(object) {}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeEvent::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(WaitTreeWaitObject::GetChildren());

    list.push_back(std::make_unique<WaitTreeText>(
        tr("reset type = %1")
            .arg(GetResetTypeQString(static_cast<const Kernel::Event&>(object).GetResetType()))));
    return list;
}

WaitTreeMutex::WaitTreeMutex(const Kernel::Mutex& object) : WaitTreeWaitObject(object) {}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeMutex::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(WaitTreeWaitObject::GetChildren());

    const auto& mutex = static_cast<const Kernel::Mutex&>(object);
    if (mutex.lock_count) {
        list.push_back(
            std::make_unique<WaitTreeText>(tr("locked %1 times by thread:").arg(mutex.lock_count)));
        list.push_back(std::make_unique<WaitTreeThread>(*mutex.holding_thread));
    } else {
        list.push_back(std::make_unique<WaitTreeText>(tr("free")));
    }
    return list;
}

WaitTreeSemaphore::WaitTreeSemaphore(const Kernel::Semaphore& object)
    : WaitTreeWaitObject(object) {}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeSemaphore::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(WaitTreeWaitObject::GetChildren());

    const auto& semaphore = static_cast<const Kernel::Semaphore&>(object);
    list.push_back(
        std::make_unique<WaitTreeText>(tr("available count = %1").arg(semaphore.available_count)));
    list.push_back(std::make_unique<WaitTreeText>(tr("max count = %1").arg(semaphore.max_count)));
    return list;
}

WaitTreeTimer::WaitTreeTimer(const Kernel::Timer& object) : WaitTreeWaitObject(object) {}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeTimer::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(WaitTreeWaitObject::GetChildren());

    const auto& timer = static_cast<const Kernel::Timer&>(object);

    list.push_back(std::make_unique<WaitTreeText>(
        tr("reset type = %1").arg(GetResetTypeQString(timer.GetResetType()))));
    list.push_back(
        std::make_unique<WaitTreeText>(tr("initial delay = %1").arg(timer.GetInitialDelay())));
    list.push_back(
        std::make_unique<WaitTreeText>(tr("interval delay = %1").arg(timer.GetIntervalDelay())));
    return list;
}

WaitTreeMutexList::WaitTreeMutexList(
    const boost::container::flat_set<std::shared_ptr<Kernel::Mutex>>& list)
    : mutex_list(list) {}

QString WaitTreeMutexList::GetText() const {
    return tr("holding mutexes");
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeMutexList::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(mutex_list.size());
    std::transform(mutex_list.begin(), mutex_list.end(), list.begin(),
                   [](const auto& t) { return std::make_unique<WaitTreeMutex>(*t); });
    return list;
}

WaitTreeThreadList::WaitTreeThreadList(const std::vector<std::shared_ptr<Kernel::Thread>>& list)
    : thread_list(list) {}

QString WaitTreeThreadList::GetText() const {
    return tr("waited by thread");
}

std::vector<std::unique_ptr<WaitTreeItem>> WaitTreeThreadList::GetChildren() const {
    std::vector<std::unique_ptr<WaitTreeItem>> list(thread_list.size());
    std::transform(thread_list.begin(), thread_list.end(), list.begin(),
                   [](const auto& t) { return std::make_unique<WaitTreeThread>(*t); });
    return list;
}

WaitTreeModel::WaitTreeModel(QObject* parent) : QAbstractItemModel(parent) {}

QModelIndex WaitTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!hasIndex(row, column, parent))
        return {};

    if (parent.isValid()) {
        WaitTreeItem* parent_item = static_cast<WaitTreeItem*>(parent.internalPointer());
        parent_item->Expand();
        return createIndex(row, column, parent_item->Children()[row].get());
    }

    return createIndex(row, column, thread_items[row].get());
}

QModelIndex WaitTreeModel::parent(const QModelIndex& index) const {
    if (!index.isValid())
        return {};

    WaitTreeItem* parent_item = static_cast<WaitTreeItem*>(index.internalPointer())->Parent();
    if (!parent_item) {
        return QModelIndex();
    }
    return createIndex(static_cast<int>(parent_item->Row()), 0, parent_item);
}

int WaitTreeModel::rowCount(const QModelIndex& parent) const {
    if (!parent.isValid())
        return static_cast<int>(thread_items.size());

    WaitTreeItem* parent_item = static_cast<WaitTreeItem*>(parent.internalPointer());
    parent_item->Expand();
    return static_cast<int>(parent_item->Children().size());
}

int WaitTreeModel::columnCount(const QModelIndex&) const {
    return 1;
}

QVariant WaitTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};

    switch (role) {
    case Qt::DisplayRole:
        return static_cast<WaitTreeItem*>(index.internalPointer())->GetText();
    case Qt::ForegroundRole:
        return static_cast<WaitTreeItem*>(index.internalPointer())->GetColor();
    default:
        return {};
    }
}

void WaitTreeModel::ClearItems() {
    thread_items.clear();
}

void WaitTreeModel::InitItems(Core::System& system) {
    thread_items = WaitTreeItem::MakeThreadItemList(system);
}

WaitTreeWidget::WaitTreeWidget(Core::System& system_, QWidget* parent)
    : QDockWidget(tr("Wait Tree"), parent), system{system_} {
    setObjectName(QStringLiteral("WaitTreeWidget"));
    view = new QTreeView(this);
    view->setHeaderHidden(true);
    setWidget(view);
    setEnabled(false);
}

void WaitTreeWidget::OnDebugModeEntered() {
    if (!system.IsPoweredOn()) {
        return;
    }
    model->InitItems(system);
    view->setModel(model);
    setEnabled(true);
}

void WaitTreeWidget::OnDebugModeLeft() {
    setEnabled(false);
    view->setModel(nullptr);
    model->ClearItems();
}

void WaitTreeWidget::OnEmulationStarting(EmuThread* emu_thread) {
    model = new WaitTreeModel(this);
    view->setModel(model);
    setEnabled(false);
}

void WaitTreeWidget::OnEmulationStopping() {
    view->setModel(nullptr);
    delete model;
    setEnabled(false);
}
