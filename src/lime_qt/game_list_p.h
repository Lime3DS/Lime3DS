// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <map>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QRunnable>
#include <QStandardItem>
#include <QString>
#include <QWidget>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/loader/smdh.h"
#include "lime_qt/play_time_manager.h"
#include "lime_qt/uisettings.h"
#include "lime_qt/util/util.h"

namespace Service::FS {
enum class MediaType : u32;
}

enum class GameListItemType {
    Game = QStandardItem::UserType + 1,
    CustomDir = QStandardItem::UserType + 2,
    InstalledDir = QStandardItem::UserType + 3,
    SystemDir = QStandardItem::UserType + 4,
    AddDir = QStandardItem::UserType + 5,
    Favorites = QStandardItem::UserType + 6,
};

Q_DECLARE_METATYPE(GameListItemType);

/**
 * Gets the game icon from SMDH data.
 * @param smdh SMDH data
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap game icon
 */
static QPixmap GetQPixmapFromSMDH(const Loader::SMDH& smdh, bool large) {
    std::vector<u16> icon_data = smdh.GetIcon(large);
    const uchar* data = reinterpret_cast<const uchar*>(icon_data.data());
    int size = large ? 48 : 24;
    QImage icon(data, size, size, QImage::Format::Format_RGB16);
    return QPixmap::fromImage(icon);
}

/**
 * Gets the default icon (for games without valid SMDH)
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap default icon
 */
static QPixmap GetDefaultIcon(bool large) {
    int size = large ? 48 : 24;
    QPixmap icon(size, size);
    icon.fill(Qt::transparent);
    return icon;
}

/**
 * Gets the short game title from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString short title
 */
static QString GetQStringShortTitleFromSMDH(const Loader::SMDH& smdh,
                                            Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetShortTitle(language).data());
}

/**
 * Gets the long game title from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString long title
 */
static QString GetQStringLongTitleFromSMDH(const Loader::SMDH& smdh,
                                           Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetLongTitle(language).data());
}

/**
 * Gets the game region from SMDH data.
 * @param smdh SMDH data
 * @return QString region
 */
static QString GetRegionFromSMDH(const Loader::SMDH& smdh) {
    using GameRegion = Loader::SMDH::GameRegion;
    static const std::map<GameRegion, const char*> regions_map = {
        {GameRegion::Japan, QT_TRANSLATE_NOOP("GameRegion", "Japan")},
        {GameRegion::NorthAmerica, QT_TRANSLATE_NOOP("GameRegion", "North America")},
        {GameRegion::Europe, QT_TRANSLATE_NOOP("GameRegion", "Europe")},
        {GameRegion::Australia, QT_TRANSLATE_NOOP("GameRegion", "Australia")},
        {GameRegion::China, QT_TRANSLATE_NOOP("GameRegion", "China")},
        {GameRegion::Korea, QT_TRANSLATE_NOOP("GameRegion", "Korea")},
        {GameRegion::Taiwan, QT_TRANSLATE_NOOP("GameRegion", "Taiwan")}};

    std::vector<GameRegion> regions = smdh.GetRegions();

    if (regions.empty()) {
        return QCoreApplication::translate("GameRegion", "Invalid region");
    }

    const bool region_free =
        std::all_of(regions_map.begin(), regions_map.end(), [&regions](const auto& it) {
            return std::find(regions.begin(), regions.end(), it.first) != regions.end();
        });
    if (region_free) {
        return QCoreApplication::translate("GameRegion", "Region free");
    }

    const QString separator =
        UISettings::values.game_list_single_line_mode ? QStringLiteral(", ") : QStringLiteral("\n");
    QString result = QCoreApplication::translate("GameRegion", regions_map.at(regions.front()));
    for (auto region = ++regions.begin(); region != regions.end(); ++region) {
        result += separator + QCoreApplication::translate("GameRegion", regions_map.at(*region));
    }
    return result;
}

class GameListItem : public QStandardItem {
public:
    // used to access type from item index
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SortRole = Qt::UserRole + 2;
    GameListItem() = default;
    explicit GameListItem(const QString& string) : QStandardItem(string) {
        setData(string, SortRole);
    }
};

/// Game list icon sizes (in px)
static const std::unordered_map<UISettings::GameListIconSize, int> IconSizes{
    {UISettings::GameListIconSize::NoIcon, 0},
    {UISettings::GameListIconSize::SmallIcon, 24},
    {UISettings::GameListIconSize::LargeIcon, 48},
};

/**
 * A specialization of GameListItem for path values.
 * This class ensures that for every full path value it holds, a correct string representation
 * of just the filename (with no extension) will be displayed to the user.
 * If this class receives valid SMDH data, it will also display game icons and titles.
 */
class GameListItemPath : public GameListItem {
public:
    static constexpr int TitleRole = SortRole + 1;
    static constexpr int FullPathRole = SortRole + 2;
    static constexpr int ProgramIdRole = SortRole + 3;
    static constexpr int ExtdataIdRole = SortRole + 4;
    static constexpr int LongTitleRole = SortRole + 5;
    static constexpr int MediaTypeRole = SortRole + 6;

    GameListItemPath() = default;
    GameListItemPath(const QString& game_path, std::span<const u8> smdh_data, u64 program_id,
                     u64 extdata_id, Service::FS::MediaType media_type) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(qulonglong(extdata_id), ExtdataIdRole);
        setData(quint32(media_type), MediaTypeRole);

        if (UISettings::values.game_list_icon_size.GetValue() ==
            UISettings::GameListIconSize::NoIcon) {
            // Do not display icons
            setData(QPixmap(), Qt::DecorationRole);
        }

        bool large = UISettings::values.game_list_icon_size.GetValue() ==
                     UISettings::GameListIconSize::LargeIcon;

        if (!Loader::IsValidSMDH(smdh_data)) {
            // SMDH is not valid, set a default icon
            if (UISettings::values.game_list_icon_size.GetValue() !=
                UISettings::GameListIconSize::NoIcon)
                setData(GetDefaultIcon(large), Qt::DecorationRole);
            return;
        }

        Loader::SMDH smdh;
        std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

        // Get icon from SMDH
        if (UISettings::values.game_list_icon_size.GetValue() !=
            UISettings::GameListIconSize::NoIcon) {
            setData(GetQPixmapFromSMDH(smdh, large), Qt::DecorationRole);
        }

        // Get title from SMDH
        setData(GetQStringShortTitleFromSMDH(smdh, Loader::SMDH::TitleLanguage::English),
                TitleRole);

        // Get long title from SMDH
        setData(GetQStringLongTitleFromSMDH(smdh, Loader::SMDH::TitleLanguage::English),
                LongTitleRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole || role == SortRole) {
            std::string path, filename, extension;
            Common::SplitPath(data(FullPathRole).toString().toStdString(), &path, &filename,
                              &extension);

            const std::unordered_map<UISettings::GameListText, QString> display_texts{
                {UISettings::GameListText::FileName, QString::fromStdString(filename + extension)},
                {UISettings::GameListText::FullPath, data(FullPathRole).toString()},
                {UISettings::GameListText::TitleName, data(TitleRole).toString()},
                {UISettings::GameListText::LongTitleName, data(LongTitleRole).toString()},
                {UISettings::GameListText::TitleID,
                 QString::fromStdString(fmt::format("{:016X}", data(ProgramIdRole).toULongLong()))},
            };

            const QString& row1 =
                display_texts.at(UISettings::values.game_list_row_1.GetValue()).simplified();

            if (role == SortRole)
                return row1.toLower();

            QString row2;
            const auto row_2_id = UISettings::values.game_list_row_2.GetValue();
            if (row_2_id != UISettings::GameListText::NoText) {
                if (!row1.isEmpty()) {
                    row2 = UISettings::values.game_list_single_line_mode.GetValue()
                               ? QStringLiteral("     ")
                               : QStringLiteral("\n     ");
                }
                row2 += display_texts.at(row_2_id).simplified();
            }
            return QString(row1 + row2);
        } else {
            return GameListItem::data(role);
        }
    }
};

class GameListItemCompat : public GameListItem {
    Q_DECLARE_TR_FUNCTIONS(GameListItemCompat)
public:
    static constexpr int CompatNumberRole = SortRole;
    GameListItemCompat() = default;
    explicit GameListItemCompat(const QString& compatibility) {
        setData(type(), TypeRole);

        struct CompatStatus {
            QString color;
            const char* text;
            const char* tooltip;
        };
        // clang-format off
        static const std::map<QString, CompatStatus> status_data = {
        {QStringLiteral("0"),  {QStringLiteral("#5c93ed"), QT_TR_NOOP("Perfect"),    QT_TR_NOOP("Game functions flawless with no audio or graphical glitches, all tested functionality works as intended without\nany workarounds needed.")}},
        {QStringLiteral("1"),  {QStringLiteral("#47d35c"), QT_TR_NOOP("Great"),      QT_TR_NOOP("Game functions with minor graphical or audio glitches and is playable from start to finish. May require some\nworkarounds.")}},
        {QStringLiteral("2"),  {QStringLiteral("#94b242"), QT_TR_NOOP("Okay"),       QT_TR_NOOP("Game functions with major graphical or audio glitches, but game is playable from start to finish with\nworkarounds.")}},
        {QStringLiteral("3"),  {QStringLiteral("#f2d624"), QT_TR_NOOP("Bad"),        QT_TR_NOOP("Game functions, but with major graphical or audio glitches. Unable to progress in specific areas due to glitches\neven with workarounds.")}},
        {QStringLiteral("4"),  {QStringLiteral("#ff0000"), QT_TR_NOOP("Intro/Menu"), QT_TR_NOOP("Game is completely unplayable due to major graphical or audio glitches. Unable to progress past the Start\nScreen.")}},
        {QStringLiteral("5"),  {QStringLiteral("#828282"), QT_TR_NOOP("Won't Boot"), QT_TR_NOOP("The game crashes when attempting to startup.")}},
        {QStringLiteral("99"), {QStringLiteral("#000000"), QT_TR_NOOP("Not Tested"), QT_TR_NOOP("The game has not yet been tested.")}}};
        // clang-format on

        auto iterator = status_data.find(compatibility);
        if (iterator == status_data.end()) {
            LOG_WARNING(Frontend, "Invalid compatibility number {}", compatibility.toStdString());
            return;
        }
        const CompatStatus& status = iterator->second;
        setData(compatibility, CompatNumberRole);
        setText(tr(status.text));
        setToolTip(tr(status.tooltip));
        setData(CreateCirclePixmapFromColor(status.color), Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CompatNumberRole).value<QString>() <
               other.data(CompatNumberRole).value<QString>();
    }
};

class GameListItemRegion : public GameListItem {
public:
    GameListItemRegion() = default;
    explicit GameListItemRegion(std::span<const u8> smdh_data) {
        setData(type(), TypeRole);

        if (!Loader::IsValidSMDH(smdh_data)) {
            setText(QObject::tr("Invalid region"));
            return;
        }

        Loader::SMDH smdh;
        std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

        setText(GetRegionFromSMDH(smdh));
        setData(GetRegionFromSMDH(smdh), SortRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }
};

/**
 * A specialization of GameListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class GameListItemSize : public GameListItem {
public:
    static constexpr int SizeRole = SortRole;

    GameListItemSize() = default;
    explicit GameListItemSize(const qulonglong size_bytes) {
        setData(type(), TypeRole);
        setData(size_bytes, SizeRole);
    }
    explicit GameListItemSize(const QString& string) {
        // This is required to avoid incorrect virtual function call in
        // GameListItem's constructor
        setText(string);
        setData(string, SortRole);
    }

    void setData(const QVariant& value, int role) override {
        // By specializing setData for SizeRole, we can ensure that the numerical and string
        // representations of the data are always accurate and in the correct format.
        if (role == SizeRole) {
            qulonglong size_bytes = value.toULongLong();
            GameListItem::setData(ReadableByteSize(size_bytes), Qt::DisplayRole);
            GameListItem::setData(value, SizeRole);
        } else {
            GameListItem::setData(value, role);
        }
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string
     * representation.
     */
    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};

/**
 * GameListItem for Play Time values.
 * This object stores the play time of a game in seconds, and its readable
 * representation in minutes/hours
 */
class GameListItemPlayTime : public GameListItem {
public:
    static constexpr int PlayTimeRole = SortRole;

    GameListItemPlayTime() = default;
    explicit GameListItemPlayTime(const qulonglong time_seconds) {
        setData(time_seconds, PlayTimeRole);
    }

    void setData(const QVariant& value, int role) override {
        qulonglong time_seconds = value.toULongLong();
        GameListItem::setData(PlayTime::ReadablePlayTime(time_seconds), Qt::DisplayRole);
        GameListItem::setData(value, PlayTimeRole);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(PlayTimeRole).toULongLong() < other.data(PlayTimeRole).toULongLong();
    }
};

class GameListDir : public GameListItem {
public:
    static constexpr int GameDirRole = Qt::UserRole + 2;

    explicit GameListDir(UISettings::GameDir& directory,
                         GameListItemType dir_type = GameListItemType::CustomDir)
        : dir_type{dir_type} {
        setData(type(), TypeRole);

        UISettings::GameDir* game_dir = &directory;
        setData(QVariant(UISettings::values.game_dirs.indexOf(directory)), GameDirRole);

        const int icon_size = IconSizes.at(UISettings::values.game_list_icon_size.GetValue());
        switch (dir_type) {
        case GameListItemType::InstalledDir:
            setData(QIcon::fromTheme(QStringLiteral("sd_card")).pixmap(icon_size),
                    Qt::DecorationRole);
            setData(QObject::tr("Installed Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::SystemDir:
            setData(QIcon::fromTheme(QStringLiteral("chip")).pixmap(icon_size), Qt::DecorationRole);
            setData(QObject::tr("System Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::CustomDir: {
            QString icon_name = QFileInfo::exists(game_dir->path) ? QStringLiteral("folder")
                                                                  : QStringLiteral("bad_folder");
            setData(QIcon::fromTheme(icon_name).pixmap(icon_size), Qt::DecorationRole);
            setData(game_dir->path, Qt::DisplayRole);
            break;
        }
        default:
            break;
        }
    }

    int type() const override {
        return static_cast<int>(dir_type);
    }

    /**
     * Override to prevent automatic sorting.
     */
    bool operator<(const QStandardItem& other) const override {
        return false;
    }

private:
    GameListItemType dir_type;
};

class GameListAddDir : public GameListItem {
public:
    explicit GameListAddDir() {
        setData(type(), TypeRole);

        int icon_size = IconSizes.at(UISettings::values.game_list_icon_size.GetValue());
        setData(QIcon::fromTheme(QStringLiteral("plus")).pixmap(icon_size), Qt::DecorationRole);
        setData(QObject::tr("Add New Game Directory"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::AddDir);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameListFavorites : public GameListItem {
public:
    explicit GameListFavorites() {
        setData(type(), TypeRole);

        const int icon_size = IconSizes.at(UISettings::values.game_list_icon_size.GetValue());
        setData(QIcon::fromTheme(QStringLiteral("star"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
        setData(QObject::tr("Favorites"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Favorites);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class GameListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit GameListSearchField(GameList* parent = nullptr);

    void setFilterResult(int visible, int total);
    bool IsEmpty() const;

    void clear();
    void setFocus();

private:
    void changeEvent(QEvent*) override;
    void RetranslateUI();

    class KeyReleaseEater : public QObject {
    public:
        explicit KeyReleaseEater(GameList* gamelist, QObject* parent = nullptr);

    private:
        GameList* gamelist = nullptr;
        QString edit_filter_text_old;

    protected:
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };
    int visible;
    int total;

    QHBoxLayout* layout_filter = nullptr;
    QTreeView* tree_view = nullptr;
    QLabel* label_filter = nullptr;
    QLineEdit* edit_filter = nullptr;
    QLabel* label_filter_result = nullptr;
    QToolButton* button_filter_close = nullptr;
};
