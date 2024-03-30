// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <QDialog>
#include "common/common_types.h"

namespace IPCDebugger {
struct ObjectInfo;
struct RequestRecord;
} // namespace IPCDebugger

namespace Ui {
class RecordDialog;
}

class RecordDialog : public QDialog {
    Q_OBJECT

public:
    explicit RecordDialog(QWidget* parent, const IPCDebugger::RequestRecord& record,
                          const QString& service, const QString& function);
    ~RecordDialog() override;

private:
    QString FormatObject(const IPCDebugger::ObjectInfo& object) const;
    QString FormatCmdbuf(std::span<const u32> cmdbuf) const;
    void UpdateCmdbufDisplay();

    std::unique_ptr<Ui::RecordDialog> ui;
    std::array<std::vector<u32>, 4> cmdbufs;
};
