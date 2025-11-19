#ifndef UART_TRANSPORT_H
#define UART_TRANSPORT_H


#include "jldtp/jldtp.h"



int transport_uart_open(u32 baud_rate, u32 tx_pin, u32 rx_pin);


extern const struct jldtp_transport jldtp_uart_transport;







#endif

