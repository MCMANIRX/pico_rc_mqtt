#include <stdio.h>
#include <pico/stdlib.h>
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "esp_uart.h"

bool found_marker;
bool ip_found;
char ip_c_arr[4*4] ={0};


bool marker_found(char c) {
    const char marker[] = "://";  // search string
    static int pos = 0;  // correct characters received so far
    if (c == -1) return false;  // shortcut for a common case
    if (c == marker[pos]) {
        pos++;
        //putchar(pos+'0');                      // count a correct character
        if (marker[pos] == '\0') {  // we got the complete string
            pos = 0;                // reset for next time
            return true;            // report success
            irq_set_enabled(UART1_IRQ, false); //turn off to avoid issues

        }
    } else if (c == marker[0]) {
        pos = 1;        // we have the initial character again
    } else {
        pos = 0;        // we received a wrong character
    }
    return false;
}


void on_uart_rx() {

    while (uart_is_readable(UART_ID)) {
        static int ip_idx  = 0;

        char c = uart_getc(UART_ID);

       // if(!found_marker)
       //     putchar(c);

        if(found_marker){
            if(c == '\''){
                found_marker = false;

                ip_idx = 0;

                ip_found = true;
            }
            else {    
                ip_c_arr[ip_idx++] = c;

            }

        
    }
    if(marker_found(c) && (!found_marker))
            found_marker = true;
}
}

uint32_t get_esp_ip_as_u32() {

    uint8_t octet[3] = {0};
    int buf_idx   = 0;
    int ip_idx    = 0;
    int octet_idx = 0;
    uint32_t buf  = 0;
    bool done = false;
    int len = strlen(ip_c_arr);

    while(!done) {
        octet_idx = 0;

        while(ip_c_arr[ip_idx+octet_idx]!='.')
            if(ip_c_arr[ip_idx+octet_idx++]=='\0'){
                done = true;
                break;
            }
       //     printf("\toct len %d\n",octet_idx);
        //sleep_ms(500);

        for(int i = 0 ; i < octet_idx+2; ++i){
            octet[2-i] = ip_c_arr[ip_idx+(2-i)];
           // printf("c %c",ip_c_arr[ip_idx+(2-i)]);
        }
        
       // printf("oct %s\n",octet);

        buf |= ((atoi(octet))<< (24 -(8*buf_idx++)));

       // printf("buf %x\n",buf);

        octet[0] = 0;
        octet[1] = 0;
        octet[2] = 0;
        
        ip_idx+=octet_idx+1;

    }
    return buf;

}

void get_esp_ip(uint8_t *buf) {

    uint8_t octet[3] = {0};
    int buf_idx   = 0;
    int ip_idx    = 0;
    int octet_idx = 0;
    bool done = false;
    int len = strlen(ip_c_arr);

    while(!done) {
        octet_idx = 0;

        while(ip_c_arr[ip_idx+octet_idx]!='.')
            if(ip_c_arr[ip_idx+octet_idx++]=='\0'){
                done = true;
                break;
            }
       //     printf("\toct len %d\n",octet_idx);
        //sleep_ms(500);

        for(int i = 0 ; i < octet_idx+2; ++i){
            octet[2-i] = ip_c_arr[ip_idx+(2-i)];
           // printf("c %c",ip_c_arr[ip_idx+(2-i)]);
        }
        
       // printf("oct %s\n",octet);

        buf[buf_idx++] = atoi(octet);

       // printf("buf %x\n",buf);

        octet[0] = 0;
        octet[1] = 0;
        octet[2] = 0;
        
        ip_idx+=octet_idx+1;

    }

}



void init_esp_uart() {

    found_marker = false;
    ip_found = false;

    uart_init(UART_ID, 2400);

    gpio_set_function(UART_TX, GPIO_FUNC_UART);
    gpio_set_function(UART_RX, GPIO_FUNC_UART);

    int __unused actual = uart_set_baudrate(UART_ID, BAUD_RATE);

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(UART_ID, false, false);

    // Set our data format
    uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    // Turn off FIFO's - we want to do this character by character
    uart_set_fifo_enabled(UART_ID, false);

    // Set up a RX interrupt
    // We need to set up the handler first
    // Select correct interrupt for the UART we are using
    //int UART_IRQ = UART_ID == uart0 ? UART0_IRQ : UART1_IRQ;

    // And set up and enable the interrupt handlers
    irq_set_exclusive_handler(UART1_IRQ, on_uart_rx);
    irq_set_enabled(UART1_IRQ, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_ID, true, false);
}


/*
int main() {
    stdio_init_all();
    init_esp_uart();

    while(1) {
        
        sleep_ms(1000);

        if(ip_found){
            sleep_ms(1000);
            printf("%x ",get_esp_ip());
            printf("\n");
        }
    }
}*/

