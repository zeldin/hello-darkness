#define LED_ID_LIGHT_SCR_LOCK  (0x68u)
#define LED_ID_LIGHT_CAPS_LOCK (0x6au)
#define LED_ID_LIGHT_NUM_LOCK  (0x6cu)
#define LED_ID_LIGHT_GAME_MODE (0x6du)
#define LED_ID_MAX             (0x8fu)

extern void LED_IRQHandler(void);
extern void LED_Start(void);
extern void LED_Set_LED(uint8_t id, uint16_t c0, uint16_t c1, uint16_t c2);
extern void LED_Set_LED_RGB(uint8_t id, uint16_t r, uint16_t g, uint16_t b);
extern void LED_Set_Key_RGB(uint8_t kc, uint16_t r, uint16_t g, uint16_t b);
