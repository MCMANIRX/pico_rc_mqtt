#define UART_ID     uart1
#define BAUD_RATE   115200
#define DATA_BITS   8
#define STOP_BITS   1
#define PARITY      UART_PARITY_NONE

#define UART_TX    8
#define UART_RX    9


bool marker_found(char c);
void on_uart_rx();
uint32_t get_esp_ip_as_u32();
void get_esp_ip();

void init_uart1();