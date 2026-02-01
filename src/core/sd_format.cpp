// SD card formatting implementation

#include "sd_format.h"
#include "config.h"
#include "sd_layout.h"
#include "sdlog.h"
#include "../web/fileserver.h"
#include <SD.h>

// FATFS (ESP-IDF) headers may not exist in all Arduino builds.
// Guard format APIs to avoid compile errors.
#if __has_include(<ff.h>)
#include <ff.h>
#include <diskio.h>
#include <sd_diskio.h>
#define SD_FORMAT_HAS_FF 1
#else
#define SD_FORMAT_HAS_FF 0
#endif

namespace {
SDFormat::Result makeResult(bool success, bool usedFallback, const char* msg) {
    SDFormat::Result res{};
    res.success = success;
    res.usedFallback = usedFallback;
    if (msg && msg[0]) {
        strncpy(res.message, msg, sizeof(res.message) - 1);
        res.message[sizeof(res.message) - 1] = '\0';
    } else {
        res.message[0] = '\0';
    }
    return res;
}

bool wipePorkchopLayout() {
    const char* root = SDLayout::newRoot();
    if (SD.exists(root)) {
        if (!FileServer::deletePathRecursive(String(root))) {
            return false;
        }
    }
    SDLayout::setUseNewLayout(true);
    SDLayout::ensureDirs();
    return true;
}

#if SD_FORMAT_HAS_FF
constexpr uint64_t kGiB = 1024ULL * 1024 * 1024;
constexpr uint64_t kMaxFormatBytes = 32ULL * kGiB; // Cardputer docs prefer FAT32 <= 32GB

DWORD pickAllocationUnitBytes(uint64_t cardBytes) {
    if (cardBytes == 0) return 0;

    uint64_t capped = cardBytes;
    if (capped > kMaxFormatBytes) {
        capped = kMaxFormatBytes;
    }

    if (capped <= 8 * kGiB) return 4 * 1024;
    if (capped <= 16 * kGiB) return 8 * 1024;
    if (capped <= 32 * kGiB) return 16 * 1024;
    return 32 * 1024;
}

struct DiskGeometry {
    DWORD sectorSize = 0;
    DWORD sectorCount = 0;
    uint64_t bytes = 0;
};

enum class RawInitStatus : uint8_t {
    OK,
    NO_CARD,
    WRITE_PROTECT
};

void reportProgress(SDFormat::ProgressCallback cb, const char* stage, uint8_t percent) {
    if (cb) {
        cb(stage, percent);
    }
}

RawInitStatus initRawDisk(uint8_t& pdrv) {
    const uint32_t speeds[] = {
        25000000,
        20000000,
        10000000,
        8000000,
        4000000,
        1000000
    };

    Config::prepareSDBus();
    for (uint32_t speed : speeds) {
        uint8_t drive = sdcard_init(Config::sdCsPin(), &Config::sdSpi(), speed);
        if (drive == 0xFF) {
            continue;
        }
        DSTATUS status = disk_initialize(drive);
        if (status & STA_PROTECT) {
            sdcard_uninit(drive);
            return RawInitStatus::WRITE_PROTECT;
        }
        if (status & STA_NOINIT) {
            sdcard_uninit(drive);
            continue;
        }
        pdrv = drive;
        return RawInitStatus::OK;
    }
    return RawInitStatus::NO_CARD;
}

bool getDiskGeometry(uint8_t pdrv, DiskGeometry& geo) {
    DWORD sectorSize = 0;
    DWORD sectorCount = 0;
    if (disk_ioctl(pdrv, GET_SECTOR_SIZE, &sectorSize) != RES_OK) return false;
    if (disk_ioctl(pdrv, GET_SECTOR_COUNT, &sectorCount) != RES_OK) return false;
    if (sectorSize == 0 || sectorCount == 0) return false;
    geo.sectorSize = sectorSize;
    geo.sectorCount = sectorCount;
    geo.bytes = static_cast<uint64_t>(sectorSize) * sectorCount;
    return true;
}

#if FF_MULTI_PARTITION
bool partitionToMax32GiB(uint8_t pdrv, DWORD sectorSize, uint64_t cardBytes, uint8_t* workbuf, size_t workbufSize) {
    if (cardBytes <= kMaxFormatBytes) return true;
    if (!workbuf || workbufSize < FF_MAX_SS) return false;

    if (sectorSize == 0) return false;

    const uint64_t targetBytes = kMaxFormatBytes;
    const DWORD targetSectors = static_cast<DWORD>(targetBytes / sectorSize);
    if (targetSectors == 0) return false;

    DWORD partSizes[4] = {targetSectors, 0, 0, 0};
    FRESULT fr = f_fdisk(pdrv, partSizes, workbuf);
    return fr == FR_OK;
}
#endif

bool fullErase(uint8_t pdrv, const DiskGeometry& geo, SDFormat::ProgressCallback cb) {
    if (geo.sectorSize == 0 || geo.sectorCount == 0) return false;

    uint64_t targetSectors = geo.sectorCount;
    const uint64_t maxSectors = kMaxFormatBytes / geo.sectorSize;
    if (maxSectors > 0 && targetSectors > maxSectors) {
        targetSectors = maxSectors;
    }
    if (targetSectors == 0) return false;

    static uint8_t zeroBuf[4096] = {};
    const uint32_t sectorsPerChunk = static_cast<uint32_t>(sizeof(zeroBuf) / geo.sectorSize);
    if (sectorsPerChunk == 0) return false;

    uint64_t written = 0;
    uint8_t lastPercent = 255;
    while (written < targetSectors) {
        uint32_t todo = sectorsPerChunk;
        if (written + todo > targetSectors) {
            todo = static_cast<uint32_t>(targetSectors - written);
        }
        if (disk_write(pdrv, zeroBuf, static_cast<DWORD>(written), static_cast<UINT>(todo)) != RES_OK) {
            return false;
        }
        written += todo;
        uint8_t percent = static_cast<uint8_t>((written * 100) / targetSectors);
        if (percent != lastPercent) {
            lastPercent = percent;
            reportProgress(cb, "ERASING", percent);
        }
        yield();
    }

    disk_ioctl(pdrv, CTRL_SYNC, nullptr);
    return true;
}

bool fatfsFormat(uint8_t pdrv, uint64_t cardBytes, DWORD sectorSize) {
    // FATFS format uses logical drive strings like "0:"
    char drive[3] = {static_cast<char>('0' + pdrv), ':', '\0'};
    DWORD auSize = pickAllocationUnitBytes(cardBytes);
#if defined(MKFS_PARM)
    MKFS_PARM opt{};
    opt.fmt = FM_FAT32;
    opt.n_fat = 1;
    opt.align = 0;
    opt.n_root = 0;
    opt.au_size = auSize;
#else
    // Older FatFs uses a BYTE for format flags (FDISK not supported here).
    BYTE opt = FM_FAT32;
#endif

    static uint8_t workbuf[4096];
#if FF_MULTI_PARTITION
    if (cardBytes > kMaxFormatBytes) {
        if (!partitionToMax32GiB(pdrv, sectorSize, cardBytes, workbuf, sizeof(workbuf))) {
            return false;
        }
    }
#endif
#if defined(MKFS_PARM)
    FRESULT fr = f_mkfs(drive, &opt, workbuf, sizeof(workbuf));
#else
    FRESULT fr = f_mkfs(drive, opt, auSize, workbuf, sizeof(workbuf));
#endif
    return fr == FR_OK;
}
#endif
} // namespace

namespace SDFormat {

Result formatCard(FormatMode mode, bool allowFallback, ProgressCallback cb) {
    bool logWasEnabled = SDLog::isEnabled();
    SDLog::close();
    SDLog::setEnabled(false);

#if SD_FORMAT_HAS_FF
    SD.end();

    uint8_t pdrv = 0xFF;
    RawInitStatus rawInit = initRawDisk(pdrv);
    if (rawInit == RawInitStatus::WRITE_PROTECT) {
        SDLog::setEnabled(logWasEnabled);
        return makeResult(false, false, "WRITE PROTECT");
    }
    if (rawInit != RawInitStatus::OK) {
        SDLog::setEnabled(logWasEnabled);
        return makeResult(false, false, "NO SD CARD");
    }

    DiskGeometry geo{};
    if (!getDiskGeometry(pdrv, geo)) {
        sdcard_uninit(pdrv);
        SDLog::setEnabled(logWasEnabled);
        return makeResult(false, false, "GEOMETRY FAIL");
    }

    if (mode == FormatMode::FULL) {
        reportProgress(cb, "ERASING", 0);
        if (!fullErase(pdrv, geo, cb)) {
            sdcard_uninit(pdrv);
            SDLog::setEnabled(logWasEnabled);
            return makeResult(false, false, "ERASE FAIL");
        }
    }

    reportProgress(cb, "FORMAT", 0);
    if (!fatfsFormat(pdrv, geo.bytes, geo.sectorSize)) {
        sdcard_uninit(pdrv);
        if (allowFallback && Config::reinitSD() && wipePorkchopLayout()) {
            reportProgress(cb, "WIPE", 100);
            SDLog::setEnabled(logWasEnabled);
            return makeResult(true, true, "WIPE OK");
        }
        SDLog::setEnabled(logWasEnabled);
        return makeResult(false, allowFallback, "FORMAT FAIL");
    }

    sdcard_uninit(pdrv);
    delay(80);
    if (!Config::reinitSD()) {
        SDLog::setEnabled(logWasEnabled);
        return makeResult(false, false, "REMOUNT FAIL");
    }
    SDLayout::setUseNewLayout(true);
    SDLayout::ensureDirs();
    reportProgress(cb, "FORMAT", 100);
    SDLog::setEnabled(logWasEnabled);
    return makeResult(true, false, mode == FormatMode::FULL ? "FULL OK" : "FORMAT OK");
#endif

    if (allowFallback && Config::isSDAvailable() && wipePorkchopLayout()) {
        reportProgress(cb, "WIPE", 100);
        SDLog::setEnabled(logWasEnabled);
        return makeResult(true, true, "WIPE OK");
    }

    SDLog::setEnabled(logWasEnabled);
    return makeResult(false, allowFallback, "FORMAT FAIL");
}

} // namespace SDFormat
