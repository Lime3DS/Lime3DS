// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.viewmodel

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import io.github.lime3ds.android.utils.FileUtil.asDocumentFile
import io.github.lime3ds.android.utils.GpuDriverMetadata
import io.github.lime3ds.android.utils.GpuDriverHelper

class DriverViewModel : ViewModel() {
    val areDriversLoading get() = _areDriversLoading.asStateFlow()
    private val _areDriversLoading = MutableStateFlow(false)

    val isDriverReady get() = _isDriverReady.asStateFlow()
    private val _isDriverReady = MutableStateFlow(true)

    val isDeletingDrivers get() = _isDeletingDrivers.asStateFlow()
    private val _isDeletingDrivers = MutableStateFlow(false)

    val driverList get() = _driverList.asStateFlow()
    private val _driverList = MutableStateFlow(mutableListOf<Pair<Uri, GpuDriverMetadata>>())

    var previouslySelectedDriver = 0
    var selectedDriver = -1

    private val _selectedDriverMetadata =
        MutableStateFlow(
            GpuDriverHelper.customDriverData.name
                ?: LimeApplication.appContext.getString(R.string.system_gpu_driver)
        )
    val selectedDriverMetadata: StateFlow<String> get() = _selectedDriverMetadata

    private val _newDriverInstalled = MutableStateFlow(false)
    val newDriverInstalled: StateFlow<Boolean> get() = _newDriverInstalled

    val driversToDelete = mutableListOf<Uri>()

    val isInteractionAllowed
        get() = !areDriversLoading.value && isDriverReady.value && !isDeletingDrivers.value

    init {
        _areDriversLoading.value = true
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val drivers = GpuDriverHelper.getDrivers()
                val currentDriverMetadata = GpuDriverHelper.customDriverData
                for (i in drivers.indices) {
                    if (drivers[i].second == currentDriverMetadata) {
                        setSelectedDriverIndex(i)
                        break
                    }
                }

                _driverList.value = drivers
                _areDriversLoading.value = false
            }
        }
    }

    fun setSelectedDriverIndex(value: Int) {
        if (selectedDriver != -1) {
            previouslySelectedDriver = selectedDriver
        }
        selectedDriver = value
    }

    fun setNewDriverInstalled(value: Boolean) {
        _newDriverInstalled.value = value
    }

    fun addDriver(driverData: Pair<Uri, GpuDriverMetadata>) {
        val driverIndex = _driverList.value.indexOfFirst { it == driverData }
        if (driverIndex == -1) {
            setSelectedDriverIndex(_driverList.value.size)
            _driverList.value.add(driverData)
            _selectedDriverMetadata.value = driverData.second.name
                ?: LimeApplication.appContext.getString(R.string.system_gpu_driver)
        } else {
            setSelectedDriverIndex(driverIndex)
        }
    }

    fun removeDriver(driverData: Pair<Uri, GpuDriverMetadata>) {
        _driverList.value.remove(driverData)
    }

    fun onCloseDriverManager() {
        _isDeletingDrivers.value = true
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                for (driverUri in driversToDelete) {
                    val driver = driverUri.asDocumentFile() ?: continue
                    if (driver.exists()) {
                        driver.delete()
                    }
                }
                driversToDelete.clear()
                _isDeletingDrivers.value = false
            }
        }

        if (GpuDriverHelper.customDriverData == driverList.value[selectedDriver].second) {
            return
        }

        _isDriverReady.value = false
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                if (selectedDriver == 0) {
                    GpuDriverHelper.installDefaultDriver()
                    setDriverReady()
                    return@withContext
                }

                val driverToInstall = driverList.value[selectedDriver].first.asDocumentFile()
                if (driverToInstall == null) {
                    GpuDriverHelper.installDefaultDriver()
                    return@withContext
                }

                if (driverToInstall.exists()) {
                    if (!GpuDriverHelper.installCustomDriverPartial(driverToInstall.uri)) {
                        return@withContext
                    }
                } else {
                    GpuDriverHelper.installDefaultDriver()
                }
                setDriverReady()
            }
        }
    }

    private fun setDriverReady() {
        _isDriverReady.value = true
        _selectedDriverMetadata.value = GpuDriverHelper.customDriverData.name
            ?: LimeApplication.appContext.getString(R.string.system_gpu_driver)
    }
}
