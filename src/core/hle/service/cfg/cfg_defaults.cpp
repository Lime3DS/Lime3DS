// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/cfg/cfg_defaults.h"

namespace Service::CFG {

constexpr std::size_t NUM_LOCATION_NAMES = 0x10;
constexpr std::size_t LOCATION_NAME_SIZE = 0x40;
constexpr auto MakeLocationNames(std::u16string_view input) {
    std::array<std::array<u16_le, LOCATION_NAME_SIZE>, NUM_LOCATION_NAMES> out{};
    for (std::size_t i = 0; i < NUM_LOCATION_NAMES; ++i) {
        std::copy(input.cbegin(), input.cend(), out[i].data());
    }
    return out;
}

constexpr u8 UNITED_STATES_COUNTRY_ID = 49;
constexpr u8 WASHINGTON_DC_STATE_ID = 2;

constexpr u64_le DEFAULT_USER_TIME_OFFSET = 0;
constexpr BacklightControls DEFAULT_BACKLIGHT_CONTROLS{0, 2};
/**
 * TODO(Subv): Find out what this actually is, these values fix some NaN uniforms in some games,
 * for example Nintendo Zone
 * Thanks Normmatt for providing this information
 */
constexpr std::array<float, 8> DEFAULT_STEREO_CAMERA_SETTINGS = {
    62.0f, 289.0f, 76.80000305175781f, 46.08000183105469f,
    10.0f, 5.0f,   55.58000183105469f, 21.56999969482422f,
};
constexpr New3dsBacklightControls DEFAULT_NEW_3DS_BACKLIGHT_CONTROLS{{0, 0, 0, 0}, 0, {0, 0, 0}};
constexpr u8 DEFAULT_SOUND_OUTPUT_MODE = SOUND_STEREO;
// NOTE: These two are placeholders. They are randomly generated elsewhere, rather than using fixed
// constants.
constexpr u64_le DEFAULT_CONSOLE_ID = 0;
constexpr u32_le DEFAULT_CONSOLE_RANDOM_NUMBER = 0;
constexpr UsernameBlock DEFAULT_USERNAME{{u"LIME3DS"}, 0, 0};
constexpr BirthdayBlock DEFAULT_BIRTHDAY{3, 4}; // March 4th, 2024
constexpr u8 DEFAULT_LANGUAGE = LANGUAGE_EN;
constexpr ConsoleCountryInfo DEFAULT_COUNTRY_INFO{
    {0, 0}, WASHINGTON_DC_STATE_ID, UNITED_STATES_COUNTRY_ID};
constexpr std::array<std::array<u16_le, LOCATION_NAME_SIZE>, NUM_LOCATION_NAMES>
    DEFAULT_COUNTRY_NAMES = MakeLocationNames(u"United States");
constexpr std::array<std::array<u16_le, LOCATION_NAME_SIZE>, NUM_LOCATION_NAMES>
    DEFAULT_STATE_NAMES = MakeLocationNames(u"Washington DC");
constexpr u32_le DEFAULT_LATITUDE_LONGITUDE = 0;
constexpr std::array<u8, 0xC0> DEFAULT_RESTRICTED_PHOTO_EXCHANGE_SETTINGS = {};
constexpr std::array<u8, 0x14> DEFAULT_COPPACS_RESTRICTION_SETTINGS = {};
constexpr std::array<u8, 0x200> DEFAULT_PARENTAL_RESTRICTION_EMAIL = {};
constexpr EULAVersion DEFAULT_EULA_VERSION{0x7F, 0x7F};
constexpr u8 DEFAULT_0x000E0000_DATA = 0;
constexpr ConsoleModelInfo DEFAULT_CONSOLE_MODEL{NEW_NINTENDO_3DS_XL, {0, 0, 0}};
constexpr std::array<u8, 0x28> DEFAULT_X_DEVICE_TOKEN = {};
constexpr u32_le DEFAULT_SYSTEM_SETUP_REQUIRED_FLAG = 1;
constexpr u32_le DEFAULT_DEBUG_MODE_FLAG = 0;
constexpr u32_le DEFAULT_CLOCK_SEQUENCE = 0;
constexpr const char DEFAULT_SERVER_TYPE[4] = {'L', '1', '\0', '\0'};
constexpr u32_le DEFAULT_0x00160000_DATA = 0;
constexpr u32_le DEFAULT_MIIVERSE_ACCESS_KEY = 0;

static const std::unordered_map<ConfigBlockID, ConfigBlockDefaults> DEFAULT_CONFIG_BLOCKS = {
    {UserTimeOffsetBlockID,
     {AccessFlag::Global, &DEFAULT_USER_TIME_OFFSET, sizeof(DEFAULT_USER_TIME_OFFSET)}},
    {BacklightControlsBlockID,
     {AccessFlag::System, &DEFAULT_BACKLIGHT_CONTROLS, sizeof(DEFAULT_BACKLIGHT_CONTROLS)}},
    {StereoCameraSettingsBlockID,
     {AccessFlag::Global, &DEFAULT_STEREO_CAMERA_SETTINGS, sizeof(DEFAULT_STEREO_CAMERA_SETTINGS)}},
    {BacklightControlNew3dsBlockID,
     {AccessFlag::System, &DEFAULT_NEW_3DS_BACKLIGHT_CONTROLS,
      sizeof(DEFAULT_NEW_3DS_BACKLIGHT_CONTROLS)}},
    {SoundOutputModeBlockID,
     {AccessFlag::Global, &DEFAULT_SOUND_OUTPUT_MODE, sizeof(DEFAULT_SOUND_OUTPUT_MODE)}},
    {ConsoleUniqueID1BlockID,
     {AccessFlag::Global, &DEFAULT_CONSOLE_ID, sizeof(DEFAULT_CONSOLE_ID)}},
    {ConsoleUniqueID2BlockID,
     {AccessFlag::Global, &DEFAULT_CONSOLE_ID, sizeof(DEFAULT_CONSOLE_ID)}},
    {ConsoleUniqueID3BlockID,
     {AccessFlag::Global, &DEFAULT_CONSOLE_RANDOM_NUMBER, sizeof(DEFAULT_CONSOLE_RANDOM_NUMBER)}},
    {UsernameBlockID, {AccessFlag::Global, &DEFAULT_USERNAME, sizeof(DEFAULT_USERNAME)}},
    {BirthdayBlockID, {AccessFlag::Global, &DEFAULT_BIRTHDAY, sizeof(DEFAULT_BIRTHDAY)}},
    {LanguageBlockID, {AccessFlag::Global, &DEFAULT_LANGUAGE, sizeof(DEFAULT_LANGUAGE)}},
    {CountryInfoBlockID, {AccessFlag::Global, &DEFAULT_COUNTRY_INFO, sizeof(DEFAULT_COUNTRY_INFO)}},
    {CountryNameBlockID,
     {AccessFlag::Global, &DEFAULT_COUNTRY_NAMES, sizeof(DEFAULT_COUNTRY_NAMES)}},
    {StateNameBlockID, {AccessFlag::Global, &DEFAULT_STATE_NAMES, sizeof(DEFAULT_STATE_NAMES)}},
    {LatitudeLongitudeBlockID,
     {AccessFlag::Global, &DEFAULT_LATITUDE_LONGITUDE, sizeof(DEFAULT_LATITUDE_LONGITUDE)}},
    {RestrictedPhotoExchangeBlockID,
     {AccessFlag::Global, &DEFAULT_RESTRICTED_PHOTO_EXCHANGE_SETTINGS,
      sizeof(DEFAULT_RESTRICTED_PHOTO_EXCHANGE_SETTINGS)}},
    {CoppacsRestrictionBlockID,
     {AccessFlag::Global, &DEFAULT_COPPACS_RESTRICTION_SETTINGS,
      sizeof(DEFAULT_COPPACS_RESTRICTION_SETTINGS)}},
    {ParentalRestrictionEmailBlockID,
     {AccessFlag::Global, &DEFAULT_PARENTAL_RESTRICTION_EMAIL,
      sizeof(DEFAULT_PARENTAL_RESTRICTION_EMAIL)}},
    {EULAVersionBlockID, {AccessFlag::Global, &DEFAULT_EULA_VERSION, sizeof(DEFAULT_EULA_VERSION)}},
    {Unknown_0x000E0000,
     {AccessFlag::Global, &DEFAULT_0x000E0000_DATA, sizeof(DEFAULT_0x000E0000_DATA)}},
    {ConsoleModelBlockID,
     {AccessFlag::System, &DEFAULT_CONSOLE_MODEL, sizeof(DEFAULT_CONSOLE_MODEL)}},
    {XDeviceTokenBlockID,
     {AccessFlag::System, &DEFAULT_X_DEVICE_TOKEN, sizeof(DEFAULT_X_DEVICE_TOKEN)}},
    {SystemSetupRequiredBlockID,
     {AccessFlag::System, &DEFAULT_SYSTEM_SETUP_REQUIRED_FLAG,
      sizeof(DEFAULT_SYSTEM_SETUP_REQUIRED_FLAG)}},
    {DebugModeBlockID,
     {AccessFlag::Global, &DEFAULT_DEBUG_MODE_FLAG, sizeof(DEFAULT_DEBUG_MODE_FLAG)}},
    {ClockSequenceBlockID,
     {AccessFlag::System, &DEFAULT_CLOCK_SEQUENCE, sizeof(DEFAULT_CLOCK_SEQUENCE)}},
    {ServerType, {AccessFlag::Global, DEFAULT_SERVER_TYPE, sizeof(DEFAULT_SERVER_TYPE)}},
    {Unknown_0x00160000,
     {AccessFlag::Global, &DEFAULT_0x00160000_DATA, sizeof(DEFAULT_0x00160000_DATA)}},
    {MiiverseAccessKeyBlockID,
     {AccessFlag::Global, &DEFAULT_MIIVERSE_ACCESS_KEY, sizeof(DEFAULT_MIIVERSE_ACCESS_KEY)}},
};

const EULAVersion& GetDefaultEULAVersion() {
    return DEFAULT_EULA_VERSION;
}

const std::unordered_map<ConfigBlockID, ConfigBlockDefaults>& GetDefaultConfigBlocks() {
    return DEFAULT_CONFIG_BLOCKS;
}

bool HasDefaultConfigBlock(ConfigBlockID block_id) {
    return DEFAULT_CONFIG_BLOCKS.contains(block_id);
}

const ConfigBlockDefaults& GetDefaultConfigBlock(ConfigBlockID block_id) {
    return DEFAULT_CONFIG_BLOCKS.at(block_id);
}

} // namespace Service::CFG
