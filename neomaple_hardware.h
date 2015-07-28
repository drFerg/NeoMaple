#ifndef NEO_MAPLE_HARD_H
#define NEO_MAPLE_HARD_H

#ifdef __cplusplus
extern "C"
{
#endif

void neomaple_hard_init(uint8_t *buffer);
void neomaple_hard_send(uint8_t *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* NEO_MAPLE_HARD_H */