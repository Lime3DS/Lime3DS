// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

object SystemSaveGame {
    external fun save()

    external fun load()

    external fun getIsSystemSetupNeeded(): Boolean

    external fun setSystemSetupNeeded(needed: Boolean)

    external fun getUsername(): String

    external fun setUsername(username: String)

    /**
     * Returns birthday as an array with the month as the first element and the
     * day as the second element
     */
    external fun getBirthday(): ShortArray

    external fun setBirthday(month: Short, day: Short)

    external fun getSystemLanguage(): Int

    external fun setSystemLanguage(language: Int)

    external fun getSoundOutputMode(): Int

    external fun setSoundOutputMode(mode: Int)

    external fun getCountryCode(): Short

    external fun setCountryCode(countryCode: Short)

    external fun getPlayCoins(): Int

    external fun setPlayCoins(coins: Int)

    external fun getConsoleId(): Long

    external fun regenerateConsoleId()
}

enum class BirthdayMonth(val code: Int, val days: Int) {
    JANUARY(1, 31),
    FEBRUARY(2, 29),
    MARCH(3, 31),
    APRIL(4, 30),
    MAY(5, 31),
    JUNE(6, 30),
    JULY(7, 31),
    AUGUST(8, 31),
    SEPTEMBER(9, 30),
    OCTOBER(10, 31),
    NOVEMBER(11, 30),
    DECEMBER(12, 31);

    companion object {
        fun getMonthFromCode(code: Short): BirthdayMonth? =
            entries.firstOrNull { it.code == code.toInt() }
    }
}
