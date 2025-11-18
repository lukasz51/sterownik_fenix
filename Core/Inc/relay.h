
#ifndef INC_RELAY_H_
#define INC_RELAY_H_

#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

void relay1(uint8_t state);
void relay2(uint8_t state);
void relay3(uint8_t state);
void relay4(uint8_t state);
void relay5(uint8_t state);
void relay6(uint8_t state);


#endif /* INC_RELAY_H_ */
