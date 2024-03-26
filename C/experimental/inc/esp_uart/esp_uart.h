#define UART_ID     uart1
#define BAUD_RATE   115200
#define DATA_BITS   8
#define STOP_BITS   1
#define PARITY      UART_PARITY_NONE




bool marker_found(char c);
void on_uart_rx();
uint32_t get_esp_ip_as_u32();
void get_esp_ip();

void init_esp_uart(uint8_t uart_tx,uint8_t uart_rx);