// Copyright 2021-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include "citra_qt/configuration/configure_storage.h"

#include <QMessageBox>
#include <QProgressDialog>

#include "common/file_util.h"
#include "common/settings.h"
#include "ui_configure_storage.h"

ConfigureStorage::ConfigureStorage(bool is_powered_on_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureStorage>()), is_powered_on{is_powered_on_},
      parent{parent} {
    ui->setupUi(this);
    SetConfiguration();

    connect(ui->open_nand_dir, &QPushButton::clicked, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    connect(ui->change_nand_dir, &QPushButton::clicked, this, [this]() {
        ui->change_nand_dir->setEnabled(false);
        const QString og_path =
            QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir));
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select NAND Directory"),
            QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir, dir_path.toStdString());
            MigrateFolder(og_path, dir_path);
            SetConfiguration();
        }
        ui->change_nand_dir->setEnabled(true);
    });

    connect(ui->open_sdmc_dir, &QPushButton::clicked, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    connect(ui->change_sdmc_dir, &QPushButton::clicked, this, [this]() {
        ui->change_sdmc_dir->setEnabled(false);
        const QString og_path =
            QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir));
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select SDMC Directory"),
            QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir)),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir, dir_path.toStdString());
            MigrateFolder(og_path, dir_path);
            SetConfiguration();
        }
        ui->change_sdmc_dir->setEnabled(true);
    });

    connect(ui->toggle_virtual_sd, &QCheckBox::clicked, this, [this]() {
        ApplyConfiguration();
        SetConfiguration();
    });
    connect(ui->toggle_custom_storage, &QCheckBox::clicked, this, [this]() {
        ApplyConfiguration();
        SetConfiguration();
    });
}

ConfigureStorage::~ConfigureStorage() = default;

void ConfigureStorage::SetConfiguration() {
    ui->nand_group->setVisible(Settings::values.use_custom_storage.GetValue());
    QString nand_path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir));
    ui->nand_dir_path->setText(nand_path);
    ui->open_nand_dir->setEnabled(!nand_path.isEmpty());

    ui->sdmc_group->setVisible(Settings::values.use_virtual_sd &&
                               Settings::values.use_custom_storage);
    QString sdmc_path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir));
    ui->sdmc_dir_path->setText(sdmc_path);
    ui->open_sdmc_dir->setEnabled(!sdmc_path.isEmpty());

    ui->toggle_virtual_sd->setChecked(Settings::values.use_virtual_sd.GetValue());
    ui->toggle_custom_storage->setChecked(Settings::values.use_custom_storage.GetValue());
    ui->toggle_compress_cia->setChecked(Settings::values.compress_cia_installs.GetValue());
    ui->async_fs_operations->setChecked(Settings::values.async_fs_operations.GetValue());

    ui->storage_group->setEnabled(!is_powered_on);
}

void ConfigureStorage::ApplyConfiguration() {
    Settings::values.use_virtual_sd = ui->toggle_virtual_sd->isChecked();
    Settings::values.use_custom_storage = ui->toggle_custom_storage->isChecked();
    Settings::values.compress_cia_installs = ui->toggle_compress_cia->isChecked();
    Settings::values.async_fs_operations = ui->async_fs_operations->isChecked();

    if (!Settings::values.use_custom_storage) {
        FileUtil::UpdateUserPath(FileUtil::UserPath::NANDDir,
                                 GetDefaultUserPath(FileUtil::UserPath::NANDDir));
        FileUtil::UpdateUserPath(FileUtil::UserPath::SDMCDir,
                                 GetDefaultUserPath(FileUtil::UserPath::SDMCDir));
    }
}

void ConfigureStorage::MigrateFolder(const QString& from, const QString& dest) {
    bool source_exists = FileUtil::Exists(from.toStdString());
    bool dest_exists = FileUtil::Exists(dest.toStdString());

    bool dest_has_content = false;
    bool source_has_content = false;

    if (source_exists) {
        source_has_content = !FileUtil::IsEmptyDir(from.toStdString());
    }

    if (dest_exists) {
        dest_has_content = !FileUtil::IsEmptyDir(dest.toStdString());
    }

    if (!source_has_content) {
        return;
    }

    QString message;
    if (dest_has_content) {
        message = tr("Data exists in both the old and new locations.\n\n"
                     "Old: %1\n"
                     "New: %2\n\n"
                     "Would you like to migrate the data from the old location?\n"
                     "WARNING: This will overwrite any data in the new location!")
                      .arg(from)
                      .arg(dest);
    } else {
        message = tr("Would you like to migrate your data to the new location?\n\n"
                     "From: %1\n"
                     "To: %2")
                      .arg(from)
                      .arg(dest);
    }

    QMessageBox::StandardButton reply =
        QMessageBox::question(parent, tr("Migrate Save Data"), message,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QProgressDialog progress(tr("Migrating save data..."), tr("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    if (!dest_exists) {
        if (!FileUtil::CreateFullPath(dest.toStdString())) {
            progress.close();
            QMessageBox::warning(this, tr("Migration Failed"),
                                 tr("Failed to create destination directory."));
            return;
        }
    }

    FileUtil::CopyDir(from.toStdString() + "/", dest.toStdString() + "/");

    progress.close();

    QMessageBox::StandardButton deleteReply =
        QMessageBox::question(this, tr("Migration Complete"),
                              tr("Data has been migrated successfully.\n\n"
                                 "Would you like to delete the old data?"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (deleteReply == QMessageBox::Yes) {
        if (!FileUtil::DeleteDirRecursively(from.toStdString())) {
            QMessageBox::warning(this, tr("Deletion Failed"), tr("Failed to delete old data."));
        }
    }
}

void ConfigureStorage::RetranslateUI() {
    ui->retranslateUi(this);
}
