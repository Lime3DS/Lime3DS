// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include "common/common_types.h"
#include "core/hle/service/service.h"
#include "network/artic_base/artic_base_client.h"

namespace FileSys {
class ArchiveBackend;
}

namespace Core {
class System;
}

namespace Service::CFG {

enum ConfigBlockID {
    ConfigSavegameVersionBlockID = 0x00000000, // Maybe?
    RtcCompensationBlockID = 0x00010000,
    AudioCalibrationBlockID = 0x00020000,
    LeapYearCounterBlockID = 0x00030000,
    UserTimeOffsetBlockID = 0x00030001,
    SettingsTimeOffsetBlockID = 0x00030002,
    TouchCalibrationBlockID = 0x00040000,
    AnalogStickCalibrationBlockID = 0x00040001, // Maybe?
    GyroscopeCalibrationBlockID = 0x00040002,
    AccelerometerCalibrationBlockID = 0x00040003,
    CStickCalibrationBlockID = 0x00040004,
    ScreenFlickerCalibrationBlockID = 0x00050000,
    BacklightControlsBlockID = 0x00050001,
    BacklightPwmCalibrationBlockID = 0x00050002,
    PowerSavingModeCalibrationBlockID = 0x00050003,
    PowerSavingModeCalibrationLegacyBlockID = 0x00050004,
    StereoCameraSettingsBlockID = 0x00050005,
    _3dSwitchingDelayBlockID = 0x00050006,
    Unknown_0x00050007 = 0x00050007,
    PowerSavingModeExtraConfigBlockID = 0x00050008,
    BacklightControlNew3dsBlockID = 0x00050009,
    Unknown_0x00060000 = 0x00060000,
    _3dFiltersBlockID = 0x00070000,
    SoundOutputModeBlockID = 0x00070001,
    MicrophoneEchoCancellationBlockID = 0x00070002,
    WifiConfigurationSlot0BlockID = 0x00080000,
    WifiConfigurationSlot1BlockID = 0x00080001,
    WifiConfigurationSlot2BlockID = 0x00080002,
    ConsoleUniqueID1BlockID = 0x00090000,
    ConsoleUniqueID2BlockID = 0x00090001,
    ConsoleUniqueID3BlockID = 0x00090002,
    UsernameBlockID = 0x000A0000,
    BirthdayBlockID = 0x000A0001,
    LanguageBlockID = 0x000A0002,
    CountryInfoBlockID = 0x000B0000,
    CountryNameBlockID = 0x000B0001,
    StateNameBlockID = 0x000B0002,
    LatitudeLongitudeBlockID = 0x000B0003,
    RestrictedPhotoExchangeBlockID = 0x000C0000,
    CoppacsRestrictionBlockID = 0x000C0001,
    ParentalRestrictionEmailBlockID = 0x000C0002,
    EULAVersionBlockID = 0x000D0000,
    Unknown_0x000E0000 = 0x000E0000,
    DebugConfigurationBlockID = 0x000F0000,
    Unknown_0x000F0001 = 0x000F0001,
    Unknown_0x000F0003 = 0x000F0003,
    ConsoleModelBlockID = 0x000F0004,
    NetworkUpdatesEnabledBlockID = 0x000F0005,
    XDeviceTokenBlockID = 0x000F0006,
    TwlEulaInfoBlockID = 0x00100000,
    TwlParentalRestrictionsBlockID = 0x00100001,
    TwlCountryCodeBlockID = 0x00100002,
    TwlMovableUniqueBlockIDBlockID = 0x00100003,
    SystemSetupRequiredBlockID = 0x00110000,
    LaunchMenuBlockID = 0x00110001,
    VolumeSliderBoundsBlockID = 0x00120000,
    DebugModeBlockID = 0x00130000,
    ClockSequenceBlockID = 0x00150000,
    Unknown_0x00150001 = 0x00150001,
    ServerType = 0x00150002,
    Unknown_0x00160000 = 0x00160000,
    MiiverseAccessKeyBlockID = 0x00170000,
    QtmInfraredLedRelatedBlockID = 0x00180000,
    QtmCalibrationDataBlockID = 0x00180001,
    Unknown_0x00190000 = 0x00190000,
};

enum SystemModel {
    NINTENDO_3DS = 0,
    NINTENDO_3DS_XL = 1,
    NEW_NINTENDO_3DS = 2,
    NINTENDO_2DS = 3,
    NEW_NINTENDO_3DS_XL = 4,
    NEW_NINTENDO_2DS_XL = 5
};

enum SystemLanguage {
    LANGUAGE_JP = 0,
    LANGUAGE_EN = 1,
    LANGUAGE_FR = 2,
    LANGUAGE_DE = 3,
    LANGUAGE_IT = 4,
    LANGUAGE_ES = 5,
    LANGUAGE_ZH = 6,
    LANGUAGE_KO = 7,
    LANGUAGE_NL = 8,
    LANGUAGE_PT = 9,
    LANGUAGE_RU = 10,
    LANGUAGE_TW = 11
};

enum SoundOutputMode { SOUND_MONO = 0, SOUND_STEREO = 1, SOUND_SURROUND = 2 };

struct EULAVersion {
    u8 minor;
    u8 major;
    INSERT_PADDING_BYTES(2);
};
static_assert(sizeof(EULAVersion) == 4, "EULAVersion must be exactly 0x4 bytes");

struct UsernameBlock {
    /// Exactly 20 bytes long, padded with zeros at the end if necessary
    std::array<char16_t, 10> username;
    u32 zero;
    u32 ng_word;
};
static_assert(sizeof(UsernameBlock) == 0x1C, "UsernameBlock must be exactly 0x1C bytes");

struct BirthdayBlock {
    u8 month; ///< The month of the birthday
    u8 day;   ///< The day of the birthday
};
static_assert(sizeof(BirthdayBlock) == 2, "BirthdayBlock must be exactly 2 bytes");

struct ConsoleModelInfo {
    u8 model;                  ///< The console model (3DS, 2DS, etc)
    std::array<u8, 3> unknown; ///< Unknown data
};
static_assert(sizeof(ConsoleModelInfo) == 4, "ConsoleModelInfo must be exactly 4 bytes");

struct ConsoleCountryInfo {
    std::array<u8, 2> unknown; ///< Unknown data
    u8 state_code;             ///< The state or province code.
    u8 country_code;           ///< The country code of the console
};
static_assert(sizeof(ConsoleCountryInfo) == 4, "ConsoleCountryInfo must be exactly 4 bytes");

struct BacklightControls {
    u8 power_saving_enabled; ///< Whether power saving mode is enabled.
    u8 brightness_level;     ///< The configured brightness level.
};
static_assert(sizeof(BacklightControls) == 2, "BacklightControls must be exactly 2 bytes");

struct New3dsBacklightControls {
    std::array<u8, 4> unknown_1; ///< Unknown data
    u8 auto_brightness_enabled;  ///< Whether auto brightness is enabled.
    std::array<u8, 3> unknown_2; ///< Unknown data
};
static_assert(sizeof(New3dsBacklightControls) == 8,
              "New3dsBacklightControls must be exactly 8 bytes");

/// Access control flags for config blocks
enum class AccessFlag : u16 {
    UserRead = 1 << 1,    ///< Readable by user applications (cfg:u)
    SystemWrite = 1 << 2, ///< Writable by system applications and services (cfg:s / cfg:i)
    SystemRead = 1 << 3,  ///< Readable by system applications and services (cfg:s / cfg:i)

    // Commonly used combinations for convenience
    System = SystemRead | SystemWrite,
    Global = UserRead | SystemRead | SystemWrite,
};
DECLARE_ENUM_FLAG_OPERATORS(AccessFlag);

struct SecureInfoA {
    std::array<u8, 0x100> signature;
    u8 region;
    u8 unknown;
    std::array<u8, 0xF> serial_number;
};
static_assert(sizeof(SecureInfoA) == 0x111);

struct LocalFriendCodeSeedB {
    std::array<u8, 0x100> signature;
    u64 unknown;
    u64 friend_code_seed;
};
static_assert(sizeof(LocalFriendCodeSeedB) == 0x110);

enum class SecureDataLoadStatus {
    Loaded,
    NotFound,
    Invalid,
    IOError,
};

class Module final {
public:
    Module(Core::System& system_);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cfg, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        void UseArticClient(std::shared_ptr<Network::ArticBase::Client>& client) {
            GetModule()->artic_client = client;
        }

        /**
         * CFG::GetCountryCodeString service function
         *  Inputs:
         *      1 : Country Code ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Country's 2-char string
         */
        void GetCountryCodeString(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetCountryCodeID service function
         *  Inputs:
         *      1 : Country Code 2-char string
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Country Code ID
         */
        void GetCountryCodeID(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetRegion service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Region value loaded from SecureInfo offset 0x100
         */
        void GetRegion(Kernel::HLERequestContext& ctx);

        /**
         * CFG::SecureInfoGetByte101 service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Value loaded from SecureInfo offset 0x101
         */
        void SecureInfoGetByte101(Kernel::HLERequestContext& ctx);

        /**
         * CFG::SecureInfoGetSerialNo service function
         *  Inputs:
         *      1 : Buffer Size
         *      2-3: Output mapped buffer
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2-3 : Output mapped buffer
         */
        void SecureInfoGetSerialNo(Kernel::HLERequestContext& ctx);

        /**
         * CFG::SetUUIDClockSequence service function
         *  Inputs:
         *      1 : UUID Clock Sequence
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetUUIDClockSequence(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetUUIDClockSequence service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : UUID Clock Sequence
         */
        void GetUUIDClockSequence(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetTransferableId service function
         *  Inputs:
         *      1 : 20 bit application ID salt
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Hash/"ID" lower word
         *      3 : Hash/"ID" upper word
         */
        void GetTransferableId(Kernel::HLERequestContext& ctx);

        /**
         * CFG::IsCoppacsSupported service function
         *  Inputs:
         *      1 : None
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 1 if the system is a Canada or USA model, 0 otherwise
         */
        void IsCoppacsSupported(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetSystemModel service function
         *  Inputs:
         *      0 : 0x00050000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Model of the console
         */
        void GetSystemModel(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetModelNintendo2DS service function
         *  Inputs:
         *      0 : 0x00060000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : 0 if the system is a Nintendo 2DS, 1 otherwise
         */
        void GetModelNintendo2DS(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetConfig service function
         *  Inputs:
         *      0 : 0x00010082
         *      1 : Size
         *      2 : Block ID
         *      3 : Descriptor for the output buffer
         *      4 : Output buffer pointer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetConfig(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetSystemConfig service function
         *  Inputs:
         *      0 : 0x04010082 / 0x08010082
         *      1 : Size
         *      2 : Block ID
         *      3 : Descriptor for the output buffer
         *      4 : Output buffer pointer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetSystemConfig(Kernel::HLERequestContext& ctx);

        /**
         * CFG::SetSystemConfig service function
         *  Inputs:
         *      0 : 0x04020082 / 0x08020082
         *      1 : Block ID
         *      2 : Size
         *      3 : Descriptor for the output buffer
         *      4 : Output buffer pointer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *  Note:
         *      The parameters order is different from GetConfig/GetSystemConfig's,
         *      where Block ID and Size are switched.
         */
        void SetSystemConfig(Kernel::HLERequestContext& ctx);

        /**
         * CFG::UpdateConfigNANDSavegame service function
         *  Inputs:
         *      0 : 0x04030000 / 0x08030000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UpdateConfigNANDSavegame(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetLocalFriendCodeSeedData service function
         *  Inputs:
         *      1 : Buffer Size
         *      2-3: Output mapped buffer
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetLocalFriendCodeSeedData(Kernel::HLERequestContext& ctx);

        /**
         * CFG::GetLocalFriendCodeSeed service function
         *  Inputs:
         *  Outputs:
         *      0 : Result Header code
         *      1 : Result of function, 0 on success, otherwise error code
         *      2-3 : Friend code seed
         */
        void GetLocalFriendCodeSeed(Kernel::HLERequestContext& ctx);

        /**
         * CFG::FormatConfig service function
         *  Inputs:
         *      0 : 0x08060000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void FormatConfig(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> cfg;
    };

private:
    // Represents save data that would normally be stored in the MCU
    // on real hardware. Try to keep this struct backwards compatible
    // if a new version is needed to prevent data loss.
    struct MCUData {
        struct Header {
            static constexpr u32 MAGIC_VALUE = 0x4455434D;
            static constexpr u32 VERSION_VALUE = 1;
            u32 magic = MAGIC_VALUE;
            u32 version = VERSION_VALUE;
            u64 reserved = 0;
        };
        Header header;
        u32 clock_sequence = 0;

        [[nodiscard]] bool IsValid() const {
            return header.magic == Header::MAGIC_VALUE && header.version == Header::VERSION_VALUE;
        }
    };

    ResultVal<void*> GetConfigBlockPointer(u32 block_id, u32 size, AccessFlag accesss_flag);

    /**
     * Reads a block with the specified id and flag from the Config savegame buffer
     * and writes the output to output. The input size must match exactly the size of the requested
     * block.
     *
     * @param block_id The id of the block we want to read
     * @param size The size of the block we want to read
     * @param accesss_flag The requested block must have this access flag set
     * @param output A pointer where we will write the read data
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result GetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag, void* output);

    /**
     * Reads data from input and writes to a block with the specified id and flag
     * in the Config savegame buffer. The input size must match exactly the size of the target
     * block.
     *
     * @param block_id The id of the block we want to write
     * @param size The size of the block we want to write
     * @param accesss_flag The target block must have this access flag set
     * @param input A pointer where we will read data and write to Config savegame buffer
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetConfigBlock(u32 block_id, u32 size, AccessFlag accesss_flag, const void* input);

    /**
     * Creates a block with the specified id and writes the input data to the cfg savegame buffer in
     * memory. The config savegame file in the filesystem is not updated.
     *
     * @param block_id The id of the block we want to create
     * @param size The size of the block we want to create
     * @param accesss_flags The access flags of the new block
     * @param data A pointer containing the data we will write to the new block
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result CreateConfigBlock(u32 block_id, u16 size, AccessFlag accesss_flags, const void* data);

    /**
     * Deletes the config savegame file from the filesystem, the buffer in memory is not affected
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result DeleteConfigNANDSaveFile();

    /**
     * Re-creates the config savegame file in memory and the filesystem with the default blocks
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result FormatConfig();

    /**
     * Open the config savegame file and load it to the memory buffer
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result LoadConfigNANDSaveFile();

    /**
     * Loads MCU specific data
     */
    void LoadMCUConfig();

public:
    u32 GetRegionValue();

    // Utilities for frontend to set config data.
    // Note: UpdateConfigNANDSavegame should be called after making changes to config data.

    /**
     * Sets the username in config savegame.
     * @param name the username to set. The maximum size is 10 in char16_t.
     */
    void SetUsername(const std::u16string& name);

    /**
     * Gets the username from config savegame.
     * @returns the username
     */
    std::u16string GetUsername();

    /**
     * Sets the profile birthday in config savegame.
     * @param month the month of birthday.
     * @param day the day of the birthday.
     */
    void SetBirthday(u8 month, u8 day);

    /**
     * Gets the profile birthday from the config savegame.
     * @returns a tuple of (month, day) of birthday
     */
    std::tuple<u8, u8> GetBirthday();

    /**
     * Sets the system language in config savegame.
     * @param language the system language to set.
     */
    void SetSystemLanguage(SystemLanguage language);

    /**
     * Gets the system language from config savegame.
     * @returns the system language
     */
    SystemLanguage GetSystemLanguage();

    /**
     * Sets the sound output mode in config savegame.
     * @param mode the sound output mode to set
     */
    void SetSoundOutputMode(SoundOutputMode mode);

    /**
     * Gets the sound output mode from config savegame.
     * @returns the sound output mode
     */
    SoundOutputMode GetSoundOutputMode();

    /**
     * Sets the country code in config savegame.
     * The state code is set to a default value.
     * @param country_code the country code to set
     */
    void SetCountryCode(u8 country_code);

    /**
     * Gets the country code from config savegame.
     * @returns the country code
     */
    u8 GetCountryCode();

    /**
     * Sets the country and state codes in config savegame.
     * @param country_code the country code to set
     * @param state_code the state code to set
     */
    void SetCountryInfo(u8 country_code, u8 state_code);

    /**
     * Gets the state code from config savegame.
     * @returns the state code
     */
    u8 GetStateCode();

    /**
     * Generates a new random console unique id.
     *
     * @returns A pair containing a random number and a random console ID.
     *
     * @note The random number is a random generated 16bit number stored at 0x90002, used for
     *       generating the console ID.
     */
    std::pair<u32, u64> GenerateConsoleUniqueId() const;

    /**
     * Sets the random_number and the  console unique id in the config savegame.
     * @param random_number the random_number to set
     * @param console_id the console id to set
     */
    Result SetConsoleUniqueId(u32 random_number, u64 console_id);

    /**
     * Gets the console unique id from config savegame.
     * @returns the console unique id
     */
    u64 GetConsoleUniqueId();

    /**
     * Sets the accepted EULA version in the config savegame.
     * @param version the version to set
     */
    void SetEULAVersion(const EULAVersion& version);

    /**
     * Gets the accepted EULA version from config savegame.
     * @returns the EULA version
     */
    EULAVersion GetEULAVersion();

    /**
     * Sets whether system initial setup is needed in config savegame.
     * @param setup_needed Whether system initial setup is needed.
     */
    void SetSystemSetupNeeded(bool setup_needed);

    /**
     * Gets whether system initial setup is needed from config savegame.
     * @returns Whether system initial setup is needed.
     */
    bool IsSystemSetupNeeded();

    /**
     * Writes the config savegame memory buffer to the config savegame file in the filesystem
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result UpdateConfigNANDSavegame();

    /**
     * Invalidates the loaded secure data so that it is loaded again.
     */
    void InvalidateSecureData();
    /**
     * Loads the LocalFriendCodeSeed_B file from NAND.
     * @returns LocalFriendCodeSeedBLoadStatus indicating the file load status.
     */
    SecureDataLoadStatus LoadSecureInfoAFile();

    /**
     * Loads the LocalFriendCodeSeed_B file from NAND.
     * @returns LocalFriendCodeSeedBLoadStatus indicating the file load status.
     */
    SecureDataLoadStatus LoadLocalFriendCodeSeedBFile();

    /**
     * Gets the SecureInfo_A path in the host filesystem
     * @returns std::string SecureInfo_A path in the host filesystem
     */
    std::string GetSecureInfoAPath();

    /**
     * Gets the LocalFriendCodeSeed_B path in the host filesystem
     * @returns std::string LocalFriendCodeSeed_B path in the host filesystem
     */
    std::string GetLocalFriendCodeSeedBPath();

    /**
     * Saves MCU specific data
     */
    void SaveMCUConfig();

private:
    void UpdatePreferredRegionCode();
    SystemLanguage GetRawSystemLanguage();

    Core::System& system;

    static constexpr u32 CONFIG_SAVEFILE_SIZE = 0x8000;
    std::array<u8, CONFIG_SAVEFILE_SIZE> cfg_config_file_buffer;
    std::unique_ptr<FileSys::ArchiveBackend> cfg_system_save_data_archive;
    u32 preferred_region_code = 0;
    bool secure_info_a_loaded = false;
    SecureInfoA secure_info_a;
    bool local_friend_code_seed_b_loaded = false;
    LocalFriendCodeSeedB local_friend_code_seed_b;
    bool preferred_region_chosen = false;
    MCUData mcu_data{};

    std::shared_ptr<Network::ArticBase::Client> artic_client = nullptr;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

/// Convenience function for getting a SHA256 hash of the Console ID
std::string GetConsoleIdHash(Core::System& system);

} // namespace Service::CFG

SERVICE_CONSTRUCT(Service::CFG::Module)
BOOST_CLASS_EXPORT_KEY(Service::CFG::Module)
