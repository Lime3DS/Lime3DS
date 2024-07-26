// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/news/news.h"

namespace Service::NEWS {

class NEWS_U final : public Module::Interface {
public:
    explicit NEWS_U(std::shared_ptr<Module> news);

private:
    SERVICE_SERIALIZATION(NEWS_U, news, Module)
};

} // namespace Service::NEWS

BOOST_CLASS_EXPORT_KEY(Service::NEWS::NEWS_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::NEWS::NEWS_U)
