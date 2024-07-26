// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "core/hle/service/am/am.h"
#include "core/hle/service/fs/archive.h"
#include "core/system_titles.h"

namespace Core {

struct SystemTitle {
    std::string_view name;
    u32 sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds;
    std::array<u32, NUM_SYSTEM_TITLE_REGIONS> title_id_lows;
};

struct SystemTitleCategory {
    std::string_view name;
    u32 title_id_high;
    std::vector<SystemTitle> titles;
};

static constexpr u32 NUM_SYSTEM_TITLE_CATEGORIES = 9;

static constexpr u64 home_menu_title_id_high = 0x00040030;
static constexpr SystemTitle home_menu_title = {
    .name = "HOME Menu",
    .title_id_lows = {0x00008202, 0x00008F02, 0x00009802, 0x00009802, 0x0000A102, 0x0000A902,
                      0x0000B102},
};

static const std::array<SystemTitleCategory, NUM_SYSTEM_TITLE_CATEGORIES>
    system_titles =
        {
            {
                {.name = "System Applications",
                 .title_id_high = 0x00040010,
                 .titles =
                     {
                         {
                             .name = "System Settings",
                             .title_id_lows = {0x00020000, 0x00021000, 0x00022000, 0x00022000,
                                               0x00026000, 0x00027000, 0x00028000},
                         },
                         {
                             .name = "Download Play",
                             .title_id_lows = {0x00020100, 0x00021100, 0x00022100, 0x00022100,
                                               0x00026100, 0x00027100, 0x00028100},
                         },
                         {
                             .name = "Activity Log",
                             .title_id_lows = {0x00020200, 0x00021200, 0x00022200, 0x00022200,
                                               0x00026200, 0x00027200, 0x00028200},
                         },
                         {
                             .name = "Health and Safety Information (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00020300, 0x00021300, 0x00022300, 0x00022300,
                                               0x00026300, 0x00027300, 0x00028300},
                         },
                         {
                             .name = "Health and Safety Information (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20020300, 0x20021300, 0x20022300, 0x20022300, 0,
                                               0x20027300, 0},
                         },
                         {
                             .name = "Nintendo 3DS Camera",
                             .title_id_lows = {0x00020400, 0x00021400, 0x00022400, 0x00022400,
                                               0x00026400, 0x00027400, 0x00028400},
                         },
                         {
                             .name = "Nintendo 3DS Sound",
                             .title_id_lows =
                                 {0x00020500,
                                  0x00021500, 0x00022500, 0x00022500, 0x00026500, 0x00027500, 0x00028500},
                         },
                         {
                             .name = "Mii Maker",
                             .title_id_lows =
                                 {0x00020700,
                                  0x00021700, 0x00022700, 0x00022700, 0x00026700, 0x00027700, 0x00028700},
                         },
                         {
                             .name = "StreetPass Mii Plaza",
                             .title_id_lows =
                                 {0x00020800,
                                  0x00021800, 0x00022800, 0x00022800, 0x00026800, 0x00027800, 0x00028800},
                         },
                         {
                             .name = "Nintendo eShop",
                             .title_id_lows = {0x00020900, 0x00021900, 0x00022900, 0x00022900, 0,
                                               0x00027900, 0x00028900},
                         },
                         {
                             .name = "System Transfer",
                             .title_id_lows = {0x00020A00, 0x00021A00, 0x00022A00, 0x00022A00, 0,
                                               0x00027A00, 0x00028A00},
                         },
                         {
                             .name = "Nintendo Zone",
                             .title_id_lows = {0x00020B00, 0x00021B00, 0x00022B00, 0x00022B00, 0,
                                               0, 0},
                         },
                         {
                             .name = "Face Raiders (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows =
                                 {0x00020D00,
                                  0x00021D00, 0x00022D00, 0x00022D00, 0x00026D00, 0x00027D00, 0x00028D00},
                         },
                         {
                             .name = "Face Raiders (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20020D00, 0x20021D00, 0x20022D00, 0x20022D00, 0,
                                               0x20027D00, 0},
                         },
                         {
                             .name = "AR Games",
                             .title_id_lows =
                                 {0x00020E00,
                                  0x00021E00, 0x00022E00, 0x00022E00, 0x00026E00, 0x00027E00, 0x00028E00},
                         },
                         {
                             .name = "System Updater (Safe Mode)",
                             .title_id_lows =
                                 {0x00020F00,
                                  0x00021F00, 0x00022F00, 0x00022F00, 0x00026F00, 0x00027F00, 0x00028F00},
                         },
                         {
                             .name = "Nintendo Network ID Settings",
                             .title_id_lows = {0x0002BF00, 0x0002C000, 0x0002C100, 0x0002C100, 0,
                                               0, 0},
                         },
                         {
                             .name = "microSD Management (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20023100, 0x20024100, 0x20025100, 0x20025100, 0,
                                               0, 0},
                         },
                         {
                             .name = "HOME Menu Digital Manual (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2002C800, 0x2002CF00, 0x2002D000, 0x2002D000, 0,
                                               0x2002D700, 0},
                         },
                         {
                             .name = "Friends List Digital Manual (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2002C900, 0x2002D100, 0x2002D200, 0x2002D200, 0,
                                               0x2002D800, 0},
                         },
                         {
                             .name = "Notifications Digital Manual (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2002CA00, 0x2002D300, 0x2002D400, 0x2002D400, 0,
                                               0x2002D900, 0},
                         },
                         {
                             .name = "Game Notes Digital Manual (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2002CB00, 0x2002D500, 0x2002D600, 0x2002D600, 0,
                                               0x2002DA00, 0},
                         },
                     }},
                {.name = "System Data Archives",
                 .title_id_high = 0x0004001B,
                 .titles =
                     {
                         {
                             .name = "ClCertA",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010002, 0x00010002, 0x00010002, 0x00010002,
                                               0x00010002, 0x00010002, 0x00010002},
                         },
                         {
                             .name = "NS CFA",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010702, 0x00010702, 0x00010702, 0x00010702,
                                               0x00010702, 0x00010702, 0x00010702},
                         },
                         {
                             .name = "Dummy CFA",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010802, 0x00010802, 0x00010802, 0x00010802,
                                               0x00010802, 0x00010802, 0x00010802},
                         },
                         {
                             .name = "NNID Web Browser Data",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00018002, 0x00018002, 0x00018002, 0x00018002,
                                               0x00018002, 0x00018002, 0x00018002},
                         },
                         {
                             .name = "Miiverse Offline Mode Web Browser Data",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00018102, 0x00018102, 0x00018102, 0x00018102,
                                               0x00018102, 0x00018102, 0x00018102},
                         },
                         {
                             .name = "NNID/Miiverse OSS CROs",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00018202, 0x00018202, 0x00018202, 0x00018202,
                                               0x00018202, 0x00018202, 0x00018202},
                         },
                         {
                             .name = "NFC Peripheral Firmware",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00019002, 0x00019002, 0x00019002, 0x00019002,
                                               0x00019002, 0x00019002, 0x00019002},
                         },
                     }},
                {.name = "System Applets",
                 .title_id_high = 0x00040030,
                 .titles =
                     {
                         home_menu_title,
                         {
                             .name = "Camera Applet",
                             .title_id_lows = {0x00008402, 0x00009002, 0x00009902, 0x00009902,
                                               0x0000A202, 0x0000AA02, 0x0000B202},
                         },
                         {
                             .name = "Instruction Manual",
                             .title_id_lows = {0x00008602, 0x00009202, 0x00009B02, 0x00009B02,
                                               0x0000A402, 0x0000AC02, 0x0000B402},
                         },
                         {
                             .name = "Game Notes",
                             .title_id_lows = {0x00008702, 0x00009302, 0x00009C02, 0x00009C02,
                                               0x0000A502, 0x0000AD02, 0x0000B502},
                         },
                         {
                             .name = "Internet Browser (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00008802, 0x00009402, 0x00009D02, 0x00009D02,
                                               0x0000A602, 0x0000AE02, 0x0000B602},
                         },
                         {
                             .name = "Internet Browser (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20008802, 0x20009402, 0x20009D02, 0x20009D02, 0,
                                               0x2000AE02, 0},
                         },
                         {
                             .name = "Error Display",
                             .title_id_lows = {0x00008A02, 0x00008A02, 0x00008A02, 0x00008A02,
                                               0x00008A02, 0x00008A02, 0x00008A02},
                         },
                         {
                             .name = "Error Display (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00008A03, 0x00008A03, 0x00008A03, 0x00008A03,
                                               0x00008A03, 0x00008A03, 0x00008A03},
                         },
                         {
                             .name = "Error Display (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20008A03, 0x20008A03, 0x20008A03, 0x20008A03,
                                               0x20008A03, 0x20008A03, 0x20008A03},
                         },
                         {
                             .name = "Friends List",
                             .title_id_lows = {0x00008D02, 0x00009602, 0x00009F02, 0x00009F02,
                                               0x0000A702, 0x0000AF02, 0x0000B702},
                         },
                         {
                             .name = "Notifications",
                             .title_id_lows = {0x00008E02, 0x00009702, 0x0000A002, 0x0000A002,
                                               0x0000A802, 0x0000B002, 0x0000B802},
                         },
                         {
                             .name = "Software Keyboard",
                             .title_id_lows = {0x0000C002, 0x0000C802, 0x0000D002, 0x0000D002,
                                               0x0000D802, 0x0000DE02, 0x0000E402},
                         },
                         {
                             .name = "Software Keyboard (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x0000C003, 0x0000C803, 0x0000D003, 0x0000D003,
                                               0x0000D803, 0x0000DE03, 0x0000E403},
                         },
                         {
                             .name = "Software Keyboard (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2000C003, 0x2000C803, 0x2000D003, 0x2000D003, 0,
                                               0x2000DE03, 0},
                         },
                         {
                             .name = "Mii Picker",
                             .title_id_lows = {0x0000C102, 0x0000C902, 0x0000D102, 0x0000D102,
                                               0x0000D902, 0x0000DF02, 0x0000E502},
                         },
                         {
                             .name = "Photo Picker",
                             .title_id_lows = {0x0000C302, 0x0000CB02, 0x0000D302, 0x0000D302,
                                               0x0000DB02, 0x0000E102, 0x0000E702},
                         },
                         {
                             .name = "Sound Picker",
                             .title_id_lows = {0x0000C402, 0x0000CC02, 0x0000D402, 0x0000D402,
                                               0x0000DC02, 0x0000E202, 0x0000E802},
                         },
                         {
                             .name = "Error/EULA Display",
                             .title_id_lows = {0x0000C502, 0x0000C502, 0x0000C502, 0x0000C502,
                                               0x0000CF02, 0x0000CF02, 0x0000CF02},
                         },
                         {
                             .name = "Error/EULA Display (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x0000C503, 0x0000C503, 0x0000C503, 0x0000C503,
                                               0x0000CF03, 0x0000CF03, 0x0000CF03},
                         },
                         {
                             .name = "Error/EULA Display (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x2000C503, 0x2000C503, 0x2000C503, 0x2000C503,
                                               0x2000CF03, 0x2000CF03, 0x2000CF03},
                         },
                         {
                             .name = "Circle Pad Pro Calibration",
                             .title_id_lows = {0x0000CD02, 0x0000CD02, 0x0000CD02, 0x0000CD02,
                                               0x0000D502, 0x0000D502, 0x0000D502},
                         },
                         {
                             .name = "Nintendo eShop Applet",
                             .title_id_lows = {0x0000C602, 0x0000CE02, 0x0000D602, 0x0000D602, 0,
                                               0x0000E302, 0x0000E902},
                         },
                         {
                             .name = "Miiverse",
                             .title_id_lows = {0x0000BC02, 0x0000BD02, 0x0000BE02, 0x0000BE02, 0,
                                               0, 0},
                         },
                         {
                             .name = "Miiverse System Library",
                             .title_id_lows = {0x0000F602, 0x0000F602, 0x0000F602, 0x0000F602, 0,
                                               0, 0},
                         },
                         {
                             .name = "Miiverse Post Applet",
                             .title_id_lows = {0x00008302, 0x00008B02, 0x0000BA02, 0x0000BA02, 0,
                                               0, 0},
                         },
                         {
                             .name = "Amiibo Settings",
                             .title_id_lows = {0x00009502, 0x00009E02, 0x0000B902, 0x0000B902, 0,
                                               0x00008C02, 0x0000BF02},
                         },
                     }},
                {.name = "Shared Data Archives",
                 .title_id_high = 0x0004009B,
                 .titles =
                     {
                         {
                             .name = "CFL_Res.dat",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010202, 0x00010202, 0x00010202, 0x00010202,
                                               0x00010202, 0x00010202, 0x00010202},
                         },
                         {
                             .name = "Region Manifest",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010402, 0x00010402, 0x00010402, 0x00010402,
                                               0x00010402, 0x00010402, 0x00010402},
                         },
                         {
                             .name = "Public Root-CA Certificates",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010602, 0x00010602, 0x00010602, 0x00010602,
                                               0x00010602, 0x00010602, 0x00010602},
                         },
                         {
                             .name = "CHN/CN Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0, 0, 0x00011002, 0, 0},
                         },
                         {
                             .name = "TWN/TN Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0, 0, 0, 0, 0x00011102},
                         },
                         {
                             .name = "NL/NL Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011202, 0x00011202, 0, 0, 0},
                         },
                         {
                             .name = "EN/GB Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011302, 0x00011302, 0, 0, 0},
                         },
                         {
                             .name = "EN/US Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0x00011402, 0, 0, 0, 0, 0},
                         },
                         {
                             .name = "FR/FR/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011502, 0x00011502, 0, 0, 0},
                         },
                         {
                             .name = "FR/CA/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0x00011602, 0, 0, 0, 0, 0},
                         },
                         {
                             .name = "DE/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011702, 0x00011702, 0, 0, 0},
                         },
                         {
                             .name = "IT/IT Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011802, 0x00011802, 0, 0, 0},
                         },
                         {
                             .name = "JA_small/32 Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00011902, 0, 0, 0, 0, 0, 0},
                         },
                         {
                             .name = "KO/KO Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0, 0, 0, 0x00011A02, 0},
                         },
                         {
                             .name = "PT/PT/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011B02, 0x00011B02, 0, 0, 0},
                         },
                         {
                             .name = "RU/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0, 0x00011C02, 0x00011C02, 0, 0, 0},
                         },
                         {
                             .name = "ES/ES Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0x00011D02, 0x00011D02, 0x00011D02, 0, 0, 0},
                         },
                         {
                             .name = "PT/BR/regular Dictionary",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0, 0x00011E02, 0, 0, 0, 0, 0},
                         },
                         {
                             .name = "Error Strings",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00012202, 0x00012302, 0x00012102, 0x00012102,
                                               0x00012402, 0x00012502, 0x00012602},
                         },
                         {
                             .name = "EULA",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00013202, 0x00013302, 0x00013102, 0x00013102,
                                               0x00013502, 0, 0},
                         },
                         {
                             .name = "JPN/EUR/USA System Font",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00014002, 0x00014002, 0x00014002, 0x00014002,
                                               0x00014002, 0x00014002, 0x00014002},
                         },
                         {
                             .name = "CHN System Font",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00014102, 0x00014102, 0x00014102, 0x00014102,
                                               0x00014102, 0x00014102, 0x00014102},
                         },
                         {
                             .name = "KOR System Font",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00014202, 0x00014202, 0x00014202, 0x00014202,
                                               0x00014202, 0x00014202, 0x00014202},
                         },
                         {
                             .name = "TWN System Font",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00014302, 0x00014302, 0x00014302, 0x00014302,
                                               0x00014302, 0x00014302, 0x00014302},
                         },
                         {
                             .name = "Rate",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00015202, 0x00015302, 0x00015102, 0x00015102, 0,
                                               0x00015502, 0x0015602},
                         },
                     }},
                {.name = "System Data Archives",
                 .title_id_high = 0x000400DB,
                 .titles =
                     {
                         {
                             .name = "NGWord List",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010302, 0x00010302, 0x00010302, 0x00010302,
                                               0x00010302, 0x00010302, 0x00010302},
                         },
                         {
                             .name = "Nintendo Zone Hotspot List",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00010502, 0x00010502, 0x00010502, 0x00010502,
                                               0x00010502, 0x00010502, 0x00010502},
                         },
                         {
                             .name = "NVer (O3DS)",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::Minimal,
                             .title_id_lows = {0x00016202, 0x00016302, 0x00016102, 0x00016102,
                                               0x00016402, 0x00016502, 0x00016602},
                         },
                         {
                             .name = "NVer (N3DS)",
                             .sets = SystemTitleSet::New3ds | SystemTitleSet::Minimal,
                             .title_id_lows = {0x20016202, 0x20016302, 0x20016102, 0x20016102, 0,
                                               0x20016502, 0},
                         },
                         {
                             .name = "CVer",
                             .sets = SystemTitleSet::Old3ds | SystemTitleSet::New3ds |
                                     SystemTitleSet::Minimal,
                             .title_id_lows = {0x00017202, 0x00017302, 0x00017102, 0x00017102,
                                               0x00017402, 0x00017502, 0x00017602},
                         },
                     }},
                {.name = "System Modules",
                 .title_id_high = 0x00040130,
                 .titles =
                     {
                         {
                             .name = "AM Module",
                             .title_id_lows = {0x00001502, 0x00001502, 0x00001502, 0x00001502,
                                               0x00001502, 0x00001502, 0x00001502},
                         },
                         {
                             .name = "AM Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001503, 0x00001503, 0x00001503, 0x00001503,
                                               0x00001503, 0x00001503, 0x00001503},
                         },
                         {
                             .name = "AM Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001503, 0x20001503, 0x20001503, 0x20001503,
                                               0x20001503, 0x20001503, 0x20001503},
                         },
                         {
                             .name = "Camera Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001602, 0x00001602, 0x00001602, 0x00001602,
                                               0x00001602, 0x00001602, 0x00001602},
                         },
                         {
                             .name = "Camera Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001602, 0x20001602, 0x20001602, 0x20001602,
                                               0x20001602, 0x20001602, 0x20001602},
                         },
                         {
                             .name = "Config Module",
                             .title_id_lows = {0x00001702, 0x00001702, 0x00001702, 0x00001702,
                                               0x00001702, 0x00001702, 0x00001702},
                         },
                         {
                             .name = "Config Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001703, 0x00001703, 0x00001703, 0x00001703,
                                               0x00001703, 0x00001703, 0x00001503},
                         },
                         {
                             .name = "Config Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001703, 0x20001703, 0x20001703, 0x20001703,
                                               0x20001703, 0x20001703, 0x20001703},
                         },
                         {
                             .name = "Codec Module",
                             .title_id_lows = {0x00001802, 0x00001802, 0x00001802, 0x00001802,
                                               0x00001802, 0x00001802, 0x00001802},
                         },
                         {
                             .name = "Codec Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001803, 0x00001803, 0x00001803, 0x00001803,
                                               0x00001803, 0x00001803, 0x00001803},
                         },
                         {
                             .name = "Codec Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001803, 0x20001803, 0x20001803, 0x20001803,
                                               0x20001803, 0x20001803, 0x20001803},
                         },
                         {
                             .name = "DSP Module",
                             .title_id_lows = {0x00001A02, 0x00001A02, 0x00001A02, 0x00001A02,
                                               0x00001A02, 0x00001A02, 0x00001A02},
                         },
                         {
                             .name = "DSP Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001A03, 0x00001A03, 0x00001A03, 0x00001A03,
                                               0x00001A03, 0x00001A03, 0x00001A03},
                         },
                         {
                             .name = "DSP Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001A03, 0x20001A03, 0x20001A03, 0x20001A03,
                                               0x20001A03, 0x20001A03, 0x20001A03},
                         },
                         {
                             .name = "GPIO Module",
                             .title_id_lows = {0x00001B02, 0x00001B02, 0x00001B02, 0x00001B02,
                                               0x00001B02, 0x00001B02, 0x00001B02},
                         },
                         {
                             .name = "GPIO Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001B03, 0x00001B03, 0x00001B03, 0x00001B03,
                                               0x00001B03, 0x00001B03, 0x00001B03},
                         },
                         {
                             .name = "GPIO Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001B03, 0x20001B03, 0x20001B03, 0x20001B03,
                                               0x20001B03, 0x20001B03, 0x20001B03},
                         },
                         {
                             .name = "GSP Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001C02, 0x00001C02, 0x00001C02, 0x00001C02,
                                               0x00001C02, 0x00001C02, 0x00001C02},
                         },
                         {
                             .name = "GSP Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001C02, 0x20001C02, 0x20001C02, 0x20001C02,
                                               0x20001C02, 0x20001C02, 0x20001C02},
                         },
                         {
                             .name = "GSP Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001C03, 0x00001C03, 0x00001C03, 0x00001C03,
                                               0x00001C03, 0x00001C03, 0x00001C03},
                         },
                         {
                             .name = "GSP Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001C03, 0x20001C03, 0x20001C03, 0x20001C03,
                                               0x20001C03, 0x20001C03, 0x20001C03},
                         },
                         {
                             .name = "HID Module",
                             .title_id_lows = {0x00001D02, 0x00001D02, 0x00001D02, 0x00001D02,
                                               0x00001D02, 0x00001D02, 0x00001D02},
                         },
                         {
                             .name = "HID Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001D03, 0x00001D03, 0x00001D03, 0x00001D03,
                                               0x00001D03, 0x00001D03, 0x00001D03},
                         },
                         {
                             .name = "HID Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001D03, 0x20001D03, 0x20001D03, 0x20001D03,
                                               0x20001D03, 0x20001D03, 0x20001D03},
                         },
                         {
                             .name = "I2C Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001E02, 0x00001E02, 0x00001E02, 0x00001E02,
                                               0x00001E02, 0x00001E02, 0x00001E02},
                         },
                         {
                             .name = "I2C Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001E02, 0x20001E02, 0x20001E02, 0x20001E02,
                                               0x20001E02, 0x20001E02, 0x20001E02},
                         },
                         {
                             .name = "I2C Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001E03, 0x00001E03, 0x00001E03, 0x00001E03,
                                               0x00001E03, 0x00001E03, 0x00001E03},
                         },
                         {
                             .name = "I2C Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001E03, 0x20001E03, 0x20001E03, 0x20001E03,
                                               0x20001E03, 0x20001E03, 0x20001E03},
                         },
                         {
                             .name = "MCU Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001F02, 0x00001F02, 0x00001F02, 0x00001F02,
                                               0x00001F02, 0x00001F02, 0x00001F02},
                         },
                         {
                             .name = "MCU Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001F02, 0x20001F02, 0x20001F02, 0x20001F02,
                                               0x20001F02, 0x20001F02, 0x20001F02},
                         },
                         {
                             .name = "MCU Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00001F03, 0x00001F03, 0x00001F03, 0x00001F03,
                                               0x00001F03, 0x00001F03, 0x00001F03},
                         },
                         {
                             .name = "MCU Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20001F03, 0x20001F03, 0x20001F03, 0x20001F03,
                                               0x20001F03, 0x20001F03, 0x20001F03},
                         },
                         {
                             .name = "MIC Module",
                             .title_id_lows = {0x00002002, 0x00002002, 0x00002002, 0x00002002,
                                               0x00002002, 0x00002002, 0x00002002},
                         },
                         {
                             .name = "PDN Module",
                             .title_id_lows = {0x00002102, 0x00002102, 0x00002102, 0x00002102,
                                               0x00002102, 0x00002102, 0x00002102},
                         },
                         {
                             .name = "PDN Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002103, 0x00002103, 0x00002103, 0x00002103,
                                               0x00002103, 0x00002103, 0x00002103},
                         },
                         {
                             .name = "PDN Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002103, 0x20002103, 0x20002103, 0x20002103,
                                               0x20002103, 0x20002103, 0x20002103},
                         },
                         {
                             .name = "PTM Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002202, 0x00002202, 0x00002202, 0x00002202,
                                               0x00002202, 0x00002202, 0x00002202},
                         },
                         {
                             .name = "PTM Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002202, 0x20002202, 0x20002202, 0x20002202,
                                               0x20002202, 0x20002202, 0x20002202},
                         },
                         {
                             .name = "PTM Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002203, 0x00002203, 0x00002203, 0x00002203,
                                               0x00002203, 0x00002203, 0x00002203},
                         },
                         {
                             .name = "PTM Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002203, 0x20002203, 0x20002203, 0x20002203,
                                               0x20002203, 0x20002203, 0x20002203},
                         },
                         {
                             .name = "SPI Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002302, 0x00002302, 0x00002302, 0x00002302,
                                               0x00002302, 0x00002302, 0x00002302},
                         },
                         {
                             .name = "SPI Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002302, 0x20002302, 0x20002302, 0x20002302,
                                               0x20002302, 0x20002302, 0x20002302},
                         },
                         {
                             .name = "SPI Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002303, 0x00002303, 0x00002303, 0x00002303,
                                               0x00002303, 0x00002303, 0x00002303},
                         },
                         {
                             .name = "SPI Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002303, 0x20002303, 0x20002303, 0x20002303,
                                               0x20002303, 0x20002303, 0x20002303},
                         },
                         {
                             .name = "AC Module",
                             .title_id_lows = {0x00002402, 0x00002402, 0x00002402, 0x00002402,
                                               0x00002402, 0x00002402, 0x00002402},
                         },
                         {
                             .name = "AC Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002403, 0x00002403, 0x00002403, 0x00002403,
                                               0x00002403, 0x00002403, 0x00002403},
                         },
                         {
                             .name = "AC Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002403, 0x20002403, 0x20002403, 0x20002403,
                                               0x20002403, 0x20002403, 0x20002403},
                         },
                         {
                             .name = "CECD Module",
                             .title_id_lows = {0x00002602, 0x00002602, 0x00002602, 0x00002602,
                                               0x00002602, 0x00002602, 0x00002602},
                         },
                         {
                             .name = "CSND Module",
                             .title_id_lows = {0x00002702, 0x00002702, 0x00002702, 0x00002702,
                                               0x00002702, 0x00002702, 0x00002702},
                         },
                         {
                             .name = "CSND Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002703, 0x00002703, 0x00002703, 0x00002703,
                                               0x00002703, 0x00002703, 0x00002703},
                         },
                         {
                             .name = "CSND Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002703, 0x20002703, 0x20002703, 0x20002703,
                                               0x20002703, 0x20002703, 0x20002703},
                         },
                         {
                             .name = "DLP Module",
                             .title_id_lows = {0x00002802, 0x00002802, 0x00002802, 0x00002802,
                                               0x00002802, 0x00002802, 0x00002802},
                         },
                         {
                             .name = "HTTP Module",
                             .title_id_lows = {0x00002902, 0x00002902, 0x00002902, 0x00002902,
                                               0x00002902, 0x00002902, 0x00002902},
                         },
                         {
                             .name = "HTTP Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002903, 0x00002903, 0x00002903, 0x00002903,
                                               0x00002903, 0x00002903, 0x00002903},
                         },
                         {
                             .name = "HTTP Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002903, 0x20002903, 0x20002903, 0x20002903,
                                               0x20002903, 0x20002903, 0x20002903},
                         },
                         {
                             .name = "MP Module",
                             .title_id_lows = {0x00002A02, 0x00002A02, 0x00002A02, 0x00002A02,
                                               0x00002A02, 0x00002A02, 0x00002A02},
                         },
                         {
                             .name = "MP Module (Safe Mode)",
                             .title_id_lows = {0x00002A03, 0x00002A03, 0x00002A03, 0x00002A03,
                                               0x00002A03, 0x00002A03, 0x00002A03},
                         },
                         {
                             .name = "NDM Module",
                             .title_id_lows = {0x00002B02, 0x00002B02, 0x00002B02, 0x00002B02,
                                               0x00002B02, 0x00002B02, 0x00002B02},
                         },
                         {
                             .name = "NIM Module",
                             .title_id_lows = {0x00002C02, 0x00002C02, 0x00002C02, 0x00002C02,
                                               0x00002C02, 0x00002C02, 0x00002C02},
                         },
                         {
                             .name = "NIM Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002C03, 0x00002C03, 0x00002C03, 0x00002C03,
                                               0x00002C03, 0x00002C03, 0x00002C03},
                         },
                         {
                             .name = "NIM Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002C03, 0x20002C03, 0x20002C03, 0x20002C03,
                                               0x20002C03, 0x20002C03, 0x20002C03},
                         },
                         {
                             .name = "NWM Module",
                             .title_id_lows = {0x00002D02, 0x00002D02, 0x00002D02, 0x00002D02,
                                               0x00002D02, 0x00002D02, 0x00002D02},
                         },
                         {
                             .name = "NWM Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002D03, 0x00002D03, 0x00002D03, 0x00002D03,
                                               0x00002D03, 0x00002D03, 0x00002D03},
                         },
                         {
                             .name = "NWM Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002D03, 0x20002D03, 0x20002D03, 0x20002D03,
                                               0x20002D03, 0x20002D03, 0x20002D03},
                         },
                         {
                             .name = "Sockets Module",
                             .title_id_lows = {0x00002E02, 0x00002E02, 0x00002E02, 0x00002E02,
                                               0x00002E02, 0x00002E02, 0x00002E02},
                         },
                         {
                             .name = "Sockets Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002E03, 0x00002E03, 0x00002E03, 0x00002E03,
                                               0x00002E03, 0x00002E03, 0x00002E03},
                         },
                         {
                             .name = "Sockets Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002E03, 0x20002E03, 0x20002E03, 0x20002E03,
                                               0x20002E03, 0x20002E03, 0x20002E03},
                         },
                         {
                             .name = "SSL Module",
                             .title_id_lows = {0x00002F02, 0x00002F02, 0x00002F02, 0x00002F02,
                                               0x00002F02, 0x00002F02, 0x00002F02},
                         },
                         {
                             .name = "SSL Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00002F03, 0x00002F03, 0x00002F03, 0x00002F03,
                                               0x00002F03, 0x00002F03, 0x00002F03},
                         },
                         {
                             .name = "SSL Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20002F03, 0x20002F03, 0x20002F03, 0x20002F03,
                                               0x20002F03, 0x20002F03, 0x20002F03},
                         },
                         {
                             .name = "PS Module",
                             .title_id_lows = {0x00003102, 0x00003102, 0x00003102, 0x00003102,
                                               0x00003102, 0x00003102, 0x00003102},
                         },
                         {
                             .name = "PS Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00003103, 0x00003103, 0x00003103, 0x00003103,
                                               0x00003103, 0x00003103, 0x00003103},
                         },
                         {
                             .name = "PS Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20003103, 0x20003103, 0x20003103, 0x20003103,
                                               0x20003103, 0x20003103, 0x20003103},
                         },
                         {
                             .name = "Friends Module",
                             .title_id_lows = {0x00003202, 0x00003202, 0x00003202, 0x00003202,
                                               0x00003202, 0x00003202, 0x00003202},
                         },
                         {
                             .name = "Friends Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00003203, 0x00003203, 0x00003203, 0x00003203,
                                               0x00003203, 0x00003203, 0x00003203},
                         },
                         {
                             .name = "Friends Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20003203, 0x20003203, 0x20003203, 0x20003203,
                                               0x20003203, 0x20003203, 0x20003203},
                         },
                         {
                             .name = "IR Module",
                             .title_id_lows = {0x00003302, 0x00003302, 0x00003302, 0x00003302,
                                               0x00003302, 0x00003302, 0x00003302},
                         },
                         {
                             .name = "IR Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00003303, 0x00003303, 0x00003303, 0x00003303,
                                               0x00003303, 0x00003303, 0x00003303},
                         },
                         {
                             .name = "IR Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20003303, 0x20003303, 0x20003303, 0x20003303,
                                               0x20003303, 0x20003303, 0x20003303},
                         },
                         {
                             .name = "BOSS Module",
                             .title_id_lows = {0x00003402, 0x00003402, 0x00003402, 0x00003402,
                                               0x00003402, 0x00003402, 0x00003402},
                         },
                         {
                             .name = "News Module",
                             .title_id_lows = {0x00003502, 0x00003502, 0x00003502, 0x00003502,
                                               0x00003502, 0x00003502, 0x00003502},
                         },
                         {
                             .name = "RO Module",
                             .title_id_lows = {0x00003702, 0x00003702, 0x00003702, 0x00003702,
                                               0x00003702, 0x00003702, 0x00003702},
                         },
                         {
                             .name = "ACT Module",
                             .title_id_lows = {0x00003802, 0x00003802, 0x00003802, 0x00003802,
                                               0x00003802, 0x00003802, 0x00003802},
                         },
                         {
                             .name = "NFC Module (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00004002, 0x00004002, 0x00004002, 0x00004002,
                                               0x00004002, 0x00004002, 0x00004002},
                         },
                         {
                             .name = "NFC Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20004002, 0x20004002, 0x20004002, 0x20004002,
                                               0x20004002, 0x20004002, 0x20004002},
                         },
                         {
                             .name = "MVD Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20004102, 0x20004102, 0x20004102, 0x20004102,
                                               0x20004102, 0x20004102, 0x20004102},
                         },
                         {
                             .name = "QTM Module (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20004202, 0x20004202, 0x20004202, 0x20004202,
                                               0x20004202, 0x20004202, 0x20004202},
                         },
                         {
                             .name = "NS Module",
                             .title_id_lows = {0x00008002, 0x00008002, 0x00008002, 0x00008002,
                                               0x00008002, 0x00008002, 0x00008002},
                         },
                         {
                             .name = "NS Module (Safe Mode, O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00008003, 0x00008003, 0x00008003, 0x00008003,
                                               0x00008003, 0x00008003, 0x00008003},
                         },
                         {
                             .name = "NS Module (Safe Mode, N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20008003, 0x20008003, 0x20008003, 0x20008003,
                                               0x20008003, 0x20008003, 0x20008003},
                         },
                     }},
                {.name = "System Firmware",
                 .title_id_high = 0x00040138,
                 .titles =
                     {
                         {
                             .name = "NATIVE_FIRM (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00000002, 0x00000002, 0x00000002, 0x00000002,
                                               0x00000002, 0x00000002, 0x00000002},
                         },
                         {
                             .name = "NATIVE_FIRM (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20000002, 0x20000002, 0x20000002, 0x20000002,
                                               0x20000002, 0x20000002, 0x20000002},
                         },
                         {
                             .name = "SAFE_MODE_FIRM (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00000003, 0x00000003, 0x00000003, 0x00000003,
                                               0x00000003, 0x00000003, 0x00000003},
                         },
                         {
                             .name = "SAFE_MODE_FIRM (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20000003, 0x20000003, 0x20000003, 0x20000003,
                                               0x20000003, 0x20000003, 0x20000003},
                         },
                         {
                             .name = "TWL_FIRM (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00000102, 0x00000102, 0x00000102, 0x00000102,
                                               0x00000102, 0x00000102, 0x00000102},
                         },
                         {
                             .name = "TWL_FIRM (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20000102, 0x20000102, 0x20000102, 0x20000102,
                                               0x20000102, 0x20000102, 0x20000102},
                         },
                         {
                             .name = "AGB_FIRM (O3DS)",
                             .sets = SystemTitleSet::Old3ds,
                             .title_id_lows = {0x00000202, 0x00000202, 0x00000202, 0x00000202,
                                               0x00000202, 0x00000202, 0x00000202},
                         },
                         {
                             .name = "AGB_FIRM (N3DS)",
                             .sets = SystemTitleSet::New3ds,
                             .title_id_lows = {0x20000202, 0x20000202, 0x20000202, 0x20000202,
                                               0x20000202, 0x20000202, 0x20000202},
                         },
                     }},
                {.name = "TWL System Applications",
                 .title_id_high = 0x00048005,
                 .titles =
                     {
                         {
                             .name = "DS Internet",
                             .title_id_lows = {0x42383841, 0x42383841, 0x42383841, 0x42383841,
                                               0x42383841, 0x42383841, 0x42383841},
                         },
                         {
                             .name = "DS Download Play",
                             .title_id_lows = {0x484E4441, 0x484E4441, 0x484E4441, 0x484E4441,
                                               0x484E4443, 0x484E444B, 0x484E4441},
                         },
                     }},
                {.name = "TWL System Data Archives",
                 .title_id_high = 0x0004800F,
                 .titles =
                     {
                         {
                             .name = "DS Card Whitelist",
                             .title_id_lows = {0x484E4841, 0x484E4841, 0x484E4841, 0x484E4841,
                                               0x484E4841, 0x484E4841, 0x484E4841},
                         },
                         {
                             .name = "DS Version Data",
                             .title_id_lows = {0x484E4C41, 0x484E4C41, 0x484E4C41, 0x484E4C41,
                                               0x484E4C41, 0x484E4C41, 0x484E4C41},
                         },
                     }},
            }};

std::vector<u64> GetSystemTitleIds(SystemTitleSet set, u32 region) {
    std::vector<u64> title_ids;
    for (const auto& category : system_titles) {
        for (const auto& title : category.titles) {
            if ((title.sets & set) != 0 && title.title_id_lows[region] != 0) {
                title_ids.push_back((static_cast<u64>(category.title_id_high) << 32) |
                                    static_cast<u64>(title.title_id_lows[region]));
            }
        }
    }

    return title_ids;
}

u64 GetHomeMenuTitleId(u32 region) {
    return (static_cast<u64>(home_menu_title_id_high) << 32) |
           static_cast<u64>(home_menu_title.title_id_lows[region]);
}

std::string GetHomeMenuNcchPath(u32 region) {
    return Service::AM::GetTitleContentPath(Service::FS::MediaType::NAND,
                                            GetHomeMenuTitleId(region));
}

std::optional<u32> GetSystemTitleRegion(u64 title_id) {
    const auto title_id_high = static_cast<u32>(title_id >> 32);
    const auto category = std::find_if(system_titles.begin(), system_titles.end(),
                                       [title_id_high](const SystemTitleCategory& category) {
                                           return category.title_id_high == title_id_high;
                                       });
    if (category == system_titles.end()) {
        return std::nullopt;
    }

    const auto title_id_low = static_cast<u32>(title_id & 0xFFFFFFFF);
    for (const auto& title : category->titles) {
        for (u32 region = 0; region < NUM_SYSTEM_TITLE_REGIONS; region++) {
            if (title.title_id_lows[region] == title_id_low) {
                return region;
            }
        }
    }

    return std::nullopt;
}

} // namespace Core
