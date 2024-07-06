#pragma once

#include "initialisation.h"
#include <vector>
#include <array>

// Struct added to classes that need settings saved
struct ConfigSaver {
	void* settingsAddress;
	uint32_t settingsSize;
	void (*validateSettings)(void);		// function pointer to method that will validate config settings when restored
};


class Config {
	friend class CDCHandler;					// Allow the serial handler access to private data for printing
public:
	static constexpr uint8_t configVersion = 1;
	
	// STM32G473 category 3 device 256k Flash in 128 pages of 2048k (though memory browser indicates part actually has 512k??)
	static constexpr uint32_t flashConfigPage = 100;	// Config start page
	static constexpr uint32_t flashPageSize = 2048;
	static constexpr uint32_t configPageCount = 20;		// Allow 20 pages for config giving a config size of 41k before erase needed
	uint32_t* flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashPageSize * (flashConfigPage - 1));;

	// Constructor taking multiple config savers: Get total config block size from each saver
	Config(std::initializer_list<ConfigSaver*> initList) : configSavers(initList) {
		for (auto saver : configSavers) {
			settingsSize += saver->settingsSize;
		}
		// Ensure config size (+ 4 byte header + 1 byte index) is aligned to 8 byte boundary
		settingsSize = AlignTo16Bytes(settingsSize + headerSize);
	}

	void ScheduleSave();				// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	bool SaveConfig(const bool forceSave = false);
	void EraseConfig();					// Erase flash page containing config
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

private:
	static constexpr uint32_t flashAllErrors = FLASH_SR_OPERR  | FLASH_SR_PROGERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_PGSERR | FLASH_SR_MISERR | FLASH_SR_FASTERR | FLASH_SR_RDERR  | FLASH_SR_OPTVERR;

	bool scheduleSave = false;
	uint32_t saveBooked = false;

	const std::vector<ConfigSaver*> configSavers;
	uint32_t settingsSize = 0;			// Size of all settings from each config saver module + size of config header

	const char ConfigHeader[4] = {'C', 'F', 'G', configVersion};
	static constexpr uint32_t headerSize = sizeof(ConfigHeader) + 1;
	int32_t currentSettingsOffset = -1;	// Offset within flash page to block containing active/latest settings

	uint32_t currentIndex = 0;			// Each config gets a new index to track across multiple pages
	uint32_t currentPage = flashConfigPage;			// Page containing current config
	struct CfgPage {
		uint32_t page;
		uint8_t index;
		bool dirty;
	};
	std::array<CfgPage, configPageCount> pages;

	void SetCurrentConfigAddr() {
		flashConfigAddr = reinterpret_cast<uint32_t* const>(FLASH_BASE + flashPageSize * (currentPage - 1));
	}
	void FlashUnlock();
	void FlashLock();
	void FlashErasePage(uint8_t page);
	bool FlashWaitForLastOperation();
	bool FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);

	static const inline uint32_t AlignTo16Bytes(uint32_t val) {
		val += 15;
		val >>= 4;
		val <<= 4;
		return val;
	}
};

extern Config config;
