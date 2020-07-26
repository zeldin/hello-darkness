extern uint8_t EFFECT_GetLedX(uint8_t id);
extern uint8_t EFFECT_GetLedY(uint8_t id);
extern const uint8_t *EFFECT_GetLedColumnXs(unsigned column);
extern const uint8_t *EFFECT_GetLedColumnYs(unsigned column);
extern void EFFECT_Solid(void *buf, uint8_t r, uint8_t g, uint8_t b);

extern void EFFECT_Rainbow(void *buf, uint32_t travel);
