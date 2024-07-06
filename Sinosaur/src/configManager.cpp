#include "configManager.h"
#include <cstring>
#include <cstdio>
#include <cmath>

bool Config::SaveConfig(const bool forceSave)
{
	bool result = true;
	if (forceSave || (scheduleSave && SysTickVal > saveBooked + 60000)) {			// 60 seconds between saves
		GpioPin::SetHigh(GPIOD, 5);
		scheduleSave = false;

		if (currentSettingsOffset == -1) {					// Default = -1 if not first set in RestoreConfig
			currentSettingsOffset = 0;
		} else {
			currentSettingsOffset += settingsSize;
			if ((uint32_t)currentSettingsOffset > flashPageSize - settingsSize) {
				// Check if another page is available for use
				for (auto& page : pages) {
					if (!page.dirty && page.page != currentPage) {
						if (++currentIndex > configPageCount) { 	// set new index, wrapping at count of allowed config pages
							currentIndex = 0;
						}
						currentPage = page.page;
						SetCurrentConfigAddr();
						currentSettingsOffset = 0;
						page.index = currentIndex;
						page.dirty = true;
						break;
					}
				}
				if (currentSettingsOffset != 0) {			// No free configurations slots found - abort save
					printf("Error saving config - no space\r\n");
					return false;
				}
			}
		}
		uint32_t* flashPos = flashConfigAddr + currentSettingsOffset / 4;


		uint8_t configBuffer[settingsSize];					// Will hold all the data to be written by config savers
		memcpy(configBuffer, ConfigHeader, 4);
		configBuffer[4] = currentIndex;						// Store the index of the config to identify the active page
		uint32_t configPos = headerSize;
		for (auto& saver : configSavers) {					// Add individual config settings to buffer after header
			memcpy(&configBuffer[configPos], saver->settingsAddress, saver->settingsSize);
			configPos += saver->settingsSize;
		}

		FlashUnlock();										// Unlock Flash memory for writing
		FLASH->SR = flashAllErrors;							// Clear error flags in Status Register
		result = FlashProgram(flashPos, reinterpret_cast<uint32_t*>(&configBuffer), settingsSize);
		FlashLock();										// Lock Flash

		if (result) {
			printf("Config Saved (%lu bytes at %#010lx)\r\n", settingsSize, (uint32_t)flashPos);
		} else {
			printf("Error saving config\r\n");
		}
		GpioPin::SetLow(GPIOD, 5);
	}
	return result;
}


void Config::RestoreConfig()
{
	// Initialise page array - used to manage which page contains current config, and which pages are available for writing when current page full
	for (uint32_t i = 0; i < configPageCount; ++i) {
		pages[i].page = flashConfigPage + i;
		uint32_t* const addr = flashConfigAddr + i * (flashPageSize / 4);

		// Check if page is dirty
		for (uint32_t w = 0; w < (flashPageSize / 4); ++w) {
			if (addr[w] != 0xFFFFFFFF) {
				pages[i].dirty = true;
				break;
			}
		}

		// Check if there is a config block at the start of the page and read the index number if so
		pages[i].index = (addr[0] == *(uint32_t*)ConfigHeader) ? (uint8_t)addr[1] : 255;
	}

	// Work out which is the active config page: will be the highest index from the bottom before the sequence jumps
	std::sort(pages.begin(), pages.end(), [](const CfgPage& l, const CfgPage& r) { return l.index < r.index; });
	uint32_t index = pages[0].index;
	if (index == 255) {
		currentPage = flashConfigPage;
		currentIndex = 0;					// Each page is assigned an index to determine which contains the latest config
	} else {
		currentPage = pages[0].page;
		for (uint32_t i = 1; i < configPageCount; ++i) {
			if (pages[i].index == index + 1) {
				++index;
				currentPage = pages[i].page;
			} else {
				break;
			}
		}
		currentIndex = index;
	}
	SetCurrentConfigAddr();				// Set the base address of the page holding the current config

	// Erase any dirty pages that are not the current one, or do not contain config data
	for (auto& page : pages) {
		if (page.dirty && (page.page != currentPage || page.index == 255)) {
			FlashErasePage(page.page);
			page.index = 255;
			page.dirty = false;
		}
	}
	std::sort(pages.begin(), pages.end(), [](const CfgPage& l, const CfgPage& r) { return l.page < r.page; });

	// Locate latest (active) config block
	uint32_t pos = 0;
	while (pos <= flashPageSize - settingsSize) {
		if (*(flashConfigAddr + pos / 4) == *(uint32_t*)ConfigHeader) {
			currentSettingsOffset = pos;
			pos += settingsSize;
		} else {
			break;			// Either reached the end of the page or found the latest valid config block
		}
	}

	if (currentSettingsOffset >= 0) {
		const uint8_t* flashConfig = reinterpret_cast<uint8_t*>(flashConfigAddr) + currentSettingsOffset;
		uint32_t configPos = headerSize;		// Position in buffer to retrieve settings from

		// Restore settings
		for (auto saver : configSavers) {
			memcpy(saver->settingsAddress, &flashConfig[configPos], saver->settingsSize);
			if (saver->validateSettings != nullptr) {
				saver->validateSettings();
			}
			configPos += saver->settingsSize;
		}
	}
}


void Config::EraseConfig()
{
	for (uint32_t i = 0; i < configPageCount; ++i) {
		FlashErasePage(flashConfigPage + i);
		pages[i].dirty = false;
		pages[i].index = 255;
	}

	printf("Config Erased\r\n");
}


void Config::ScheduleSave()
{
	// called whenever a config setting is changed to schedule a save after waiting to see if any more changes are being made
	scheduleSave = true;
	saveBooked = SysTickVal;
}


void Config::FlashUnlock()
{
	// Unlock the FLASH control register access
	if ((FLASH->CR & FLASH_CR_LOCK) != 0)  {
		FLASH->KEYR = 0x45670123U;						// These magic numbers unlock the flash for programming
		FLASH->KEYR = 0xCDEF89ABU;
	}
}


void Config::FlashLock()
{
	FLASH->CR |= FLASH_CR_LOCK;							// Lock the FLASH Registers access
}


void Config::FlashErasePage(uint8_t page)
{
	FlashUnlock();										// Unlock Flash memory for writing
	FLASH->SR = flashAllErrors;							// Clear error flags in Status Register

	FLASH->CR &= ~FLASH_CR_PNB_Msk;
	FLASH->CR |= (page - 1) << FLASH_CR_PNB_Pos;		// Page number selection
	FLASH->CR |= FLASH_CR_PER;							// Page erase
	FLASH->CR |= FLASH_CR_STRT;
	FlashWaitForLastOperation();
	FLASH->CR &= ~FLASH_CR_PER;

	FlashLock();										// Lock Flash
}


bool Config::FlashWaitForLastOperation()
{
	if (FLASH->SR & flashAllErrors) {					// If any error occurred abort
		FLASH->SR = flashAllErrors;						// Clear error flags in Status Register
		return false;
	}

	while ((FLASH->SR & FLASH_SR_BSY) == FLASH_SR_BSY) {}

	if ((FLASH->SR & FLASH_SR_EOP) == FLASH_SR_EOP) {	// Check End of Operation flag
		FLASH->SR = FLASH_SR_EOP;						// Clear FLASH End of Operation pending bit
	}

	return true;
}


bool Config::FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size)
{
	if (!FlashWaitForLastOperation()) {
		return false;
	}
	FLASH->CR |= FLASH_CR_PG;

	__ISB();
	__DSB();

	// Each write block is 64 bits
	for (uint16_t b = 0; b < std::ceil(static_cast<float>(size) / 8); ++b) {
		for (uint8_t i = 0; i < 2; ++i) {
			*dest_addr = *src_addr;
			++dest_addr;
			++src_addr;
		}

		if (!FlashWaitForLastOperation()) {
			FLASH->CR &= ~FLASH_CR_PG;					// Clear programming flag
			return false;
		}
	}

	__ISB();
	__DSB();

	FLASH->CR &= ~FLASH_CR_PG;							// Clear programming flag
	return true;
}


