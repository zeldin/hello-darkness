#include <stdint.h>
#include <stdbool.h>
#include <stm32f4xx.h>

#include "error.h"
#include "led.h"
#include "key.h"
#include "usb.h"

static const uint8_t KeyCodes[KEY_CODE_MAX+1] = {
	0x3a, 0x29, 0x1f, 0x1e, 0x35, 0x14, 0x2b, 0x04, 0x39, 0x64, 0x1d, 0xe1, 0xe3, 0xe0, 0, 0,
	0x3c, 0x3b, 0x20, 0x21, 0x08, 0x1a, 0x15, 0x16, 0x09, 0x07, 0x1b, 0x06, 0x8b, 0xe2, 0, 0,
	0x3e, 0x3d, 0x24, 0x22, 0x23, 0x1c, 0x17, 0x0b, 0x0a, 0x05, 0x11, 0x19, 0x2c, 0x88, 0, 0,
	0x3f, 0x40, 0x25, 0x26, 0x0c, 0x18, 0x12, 0x0d, 0x0f, 0x0e, 0x10, 0x37, 0x36, 0x8a, 0, 0,
	0x42, 0x41, 0x2e, 0x27, 0x2d, 0x2f, 0x13, 0x30, 0x33, 0x34, 0x32, 0x38, 0xe7, 0xe6, 0, 0,
	0x43, 0x45, 0x89, 0x44, 0x2a, 0x49, 0x4c, 0x4d, 0x31, 0x28, 0xe5, 0x87, 0x65, 0xe4, 0, 0,
	0x48, 0x46, 0x53, 0x47, 0x4b, 0x4a, 0x5f, 0x4e, 0x5c, 0x59, 0x52, 0x51, 0x4f, 0x50, 0, 0,
	0x00, 0x00, 0xf1, 0x00, 0x54, 0x61, 0x60, 0x5e, 0x5d, 0x5a, 0x5b, 0x62, 0x85, 0x63, 0, 0,
	0xf3, 0x00, 0xf2, 0x00, 0x55, 0x56, 0x57, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58,
};

static uint8_t HIDReport0[8];
static uint8_t HIDReport1[8];
static bool HIDReportOverflow;
static uint8_t ToggleA, ToggleB;
static uint16_t LastKeyMask[14];

static bool AddKeyToReport(uint8_t kc)
{
	unsigned i;
	for (i=2; i<8; i++) {
		if (HIDReport0[i] == kc)
			return true;
		else if(HIDReport0[i] == 0) {
			HIDReport0[i] = kc;
			return true;
		}
	}
	return false;
}

static bool RemoveKeyFromReport(uint8_t kc)
{
	unsigned i;
	for (i=2; i<8; i++) {
		if (HIDReport0[i] == kc) {
			while (i < 7) {
				if (!(HIDReport0[i] = HIDReport0[i+1]))
					break;
				i++;
			}
			HIDReport0[i] = 0;
			return true;
		}
		else if(HIDReport0[i] == 0)
			break;
	}
	return false;
}

static void KeyDown(uint8_t kc)
{
	if (kc <= KEY_CODE_MAX) {
		LED_Set_Key_RGB(kc, 0xff, 0xff, 0xff);
		kc = KeyCodes[kc];
		if (!kc)
			;
		else if (kc < 0xe0) {
			if (!AddKeyToReport(kc))
				HIDReportOverflow = true;
		} else if (kc < 0xe8) {
			HIDReport0[0] |= 1 << (kc & 7);
		} else if (kc >= 0xf0 && kc < 0xf8) {
			HIDReport1[0] |= 1 << (kc & 7);
		}
	}
}

static void KeyUp(uint8_t kc)
{
	if (kc <= KEY_CODE_MAX) {
		LED_Set_Key_RGB(kc, 0, 0, 0);
		kc = KeyCodes[kc];
		if (!kc)
			;
		else if (kc < 0xe0) {
			if (RemoveKeyFromReport(kc))
				HIDReportOverflow = false;
		} else if (kc < 0xe8) {
			HIDReport0[0] &= ~(1 << (kc & 7));
		} else if (kc >= 0xf0 && kc < 0xf8) {
			HIDReport1[0] &= ~(1 << (kc & 7));
		}
	}
}

void ADC_MaskCallback(uint8_t column, uint16_t mask)
{
	if (mask)
			ToggleB = ~ToggleA;

	mask ^= LastKeyMask[column];

	if (mask) {
		unsigned kc = column;
		uint16_t new_mask = LastKeyMask[column] ^ mask;
		LastKeyMask[column] = new_mask;
		do {
			if ((mask & 1)) {
				if ((new_mask & 1))
					KeyDown(kc);
				else
					KeyUp(kc);
			}
			mask >>= 1;
			new_mask >>= 1;
			kc += 0x10u;
		} while (mask);
	}
	if (column == 13) {
		if (HIDReportOverflow) {
			static uint8_t overflow_report[8] = "\0\0\1\1\1\1\1\1";
			overflow_report[0] = HIDReport0[0];
			USB_HIDInReportSubmit(0, overflow_report);
		} else
			USB_HIDInReportSubmit(0, HIDReport0);
		USB_HIDInReportSubmit(1, HIDReport1);
	}
}

void TIM_EncoderCallback(uint8_t value)
{
	static uint8_t old_value;
	int8_t delta = value - old_value;

	if (!delta)
		return;

	old_value = value;
	ToggleB = ~ToggleA;

	if (KEY_CheckKeyState(KEY_CODE_LIGHT))
		LED_AdjustBrightness(delta * 2);
	else
		HIDReport1[2] += delta;
}

void USB_HIDOutReportCallback(const uint8_t *report)
{
	static const uint8_t LED_id[4] = {
		LED_ID_LIGHT_NUM_LOCK,
		LED_ID_LIGHT_CAPS_LOCK,
		LED_ID_LIGHT_SCR_LOCK,
		LED_ID_LIGHT_GAME_MODE
	};
	unsigned i, mask = *report;
	for (i=0; i<4; i++) {
		if (mask & 1)
			LED_Set_LED_RGB(LED_id[i], 0xff, 0xff, 0xff);
		else
			LED_Set_LED_RGB(LED_id[i], 0, 0, 0);
		mask >>= 1;
	}
}

bool KEY_CheckRecentKeypress(void)
{
	if (ToggleA == ToggleB)
		return false;
	ToggleA = ToggleB;
	return true;
}

bool KEY_CheckKeyState(uint8_t kc)
{
	unsigned column = kc & 0xf;
	if (column < 14)
		return (LastKeyMask[column] >> (kc >> 4)) & 1;
	else
		return false;
}
