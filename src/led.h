#define LED_ID_LIGHT_SCR_LOCK  (0x68u)
#define LED_ID_LIGHT_CAPS_LOCK (0x6au)
#define LED_ID_LIGHT_NUM_LOCK  (0x6cu)
#define LED_ID_LIGHT_GAME_MODE (0x6du)
#define LED_ID_MAX             (0x8fu)

#define LED_COLUMN_MAX         8

extern void LED_IRQHandler(void);
extern void LED_Start(void);
extern void LED_Set_LED(uint8_t id, uint8_t c0, uint8_t c1, uint8_t c2);
extern void LED_Set_LED_RGB(uint8_t id, uint8_t r, uint8_t g, uint8_t b);
extern void LED_Set_Key_RGB(uint8_t kc, uint8_t r, uint8_t g, uint8_t b);
extern void LED_Do_Key_LEDs(uint8_t kc, void (*func)(uint8_t id, void *context), void *context);
extern void LED_Set_ColumnEffect(void *buffer, unsigned column, const uint8_t *rgb);
extern void *LED_GetEffectBuffer(void);
extern void LED_CommitEffectBuffer(void *buf);
extern void LED_ClearEffect(void);
extern void LED_AdjustBrightness(int delta);
