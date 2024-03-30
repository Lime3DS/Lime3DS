// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QRegularExpression>
#include <QString>
#include <QValidator>

class Validation {
public:
    Validation()
        : room_name(room_name_regex), nickname(nickname_regex), ip(ip_regex), port(0, 65535) {}

    ~Validation() = default;

    const QValidator* GetRoomName() const {
        return &room_name;
    }
    const QValidator* GetNickname() const {
        return &nickname;
    }
    const QValidator* GetIP() const {
        return &ip;
    }
    const QValidator* GetPort() const {
        return &port;
    }

private:
    /// room name can be alphanumeric and " " "_" "." and "-" and must have a size of 4-20
    QRegularExpression room_name_regex =
        QRegularExpression(QStringLiteral("^[a-zA-Z0-9._\\- ]{4,20}$"));
    QRegularExpressionValidator room_name;

    /// nickname can be alphanumeric and " " "_" "." and "-" and must have a size of 4-20
    QRegularExpression nickname_regex =
        QRegularExpression(QStringLiteral("^[a-zA-Z0-9._\\- ]{4,20}$"));
    QRegularExpressionValidator nickname;

    /// ipv4 / ipv6 / hostnames
    QRegularExpression ip_regex = QRegularExpression(QStringLiteral(
        // IPv4 regex
        "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$|"
        // IPv6 regex
        "^((([0-9A-Fa-f]{1,4}:){7}([0-9A-Fa-f]{1,4}|:))|"
        "(([0-9A-Fa-f]{1,4}:){6}(:[0-9A-Fa-f]{1,4}|((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-"
        "5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|"
        "(([0-9A-Fa-f]{1,4}:){5}(((:[0-9A-Fa-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)"
        "(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|"
        "(([0-9A-Fa-f]{1,4}:){4}(((:[0-9A-Fa-f]{1,4}){1,3})|((:[0-9A-Fa-f]{1,4})?:((25[0-5]|2[0-4]"
        "\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|"
        "(([0-9A-Fa-f]{1,4}:){3}(((:[0-9A-Fa-f]{1,4}){1,4})|((:[0-9A-Fa-f]{1,4}){0,2}:((25[0-5]|2["
        "0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|"
        "(([0-9A-Fa-f]{1,4}:){2}(((:[0-9A-Fa-f]{1,4}){1,5})|((:[0-9A-Fa-f]{1,4}){0,3}:((25[0-5]|2["
        "0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|"
        "(([0-9A-Fa-f]{1,4}:){1}(((:[0-9A-Fa-f]{1,4}){1,6})|((:[0-9A-Fa-f]{1,4}){0,4}:((25[0-5]|2["
        "0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|"
        "(:(((:[0-9A-Fa-f]{1,4}){1,7})|((:[0-9A-Fa-f]{1,4}){0,5}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?"
        "\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:)))(%.+)?$|"
        // Hostname regex
        "^([a-zA-Z0-9]+(-[a-zA-Z0-9]+)*\\.)+[a-zA-Z]{2,}$"));
    QRegularExpressionValidator ip;

    /// port must be between 0 and 65535
    QIntValidator port;
};
