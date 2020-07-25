extern void USB_Setup_USB(void);
extern void USB_HIDInReportSubmit(unsigned channel, const uint8_t *report);
extern void USB_HIDOutReportCallback(const uint8_t *report);
