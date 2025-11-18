

#ifndef INC_NEXTION_COM_H_
#define INC_NEXTION_COM_H_

extern int temperature[4];

static int build_nextion_msg(const char *comp, int temp, uint8_t *outbuf);
void SendTemperatureNextion(void);


#endif /* INC_NEXTION_COM_H_ */
