#include <string.h>
#include <stdlib.h>
#include "SanUSB1X.h"
#include "lcd.h"

#define BUFFER_SIZE 64

//INFORMAÇÃO: Manipule o valor recebido do servidor PUSH na linha 124!!!!!!

void clear_local_buffer(void);

typedef struct {
    char items[BUFFER_SIZE];
    int first;
    int last;
    int count;
} ring_buffer;

//Configurations
char ip_addr[64] = "sanusb.tk";
int ip_port = 80;
char device_id[8] = "codigo";
//flags
char wifi_connected = 0, receive_enabled = 0;
//state
char c, pos = 0, blink = 0;
ring_buffer buffer;
char recv[BUFFER_SIZE];

void init_buffer(ring_buffer* buffer) {
    buffer->first = 0;
    buffer->last = 0;
    buffer->count = 0;
}

void push(ring_buffer* buffer, char c) {
    buffer->items[buffer->last] = c;
    buffer->last = (buffer->last + 1) % BUFFER_SIZE;
    buffer->count++;
}

char pop(ring_buffer* buffer) {
    char ret = buffer->items[buffer->first];
    buffer->first = (buffer->first + 1) % BUFFER_SIZE;
    buffer->count--;

    return ret;
}

void interrupt interrupcao() {
    if (serial_interrompeu) {
        serial_interrompeu = 0;
        c = RCREG;
        if (receive_enabled) {
            push(&buffer, c);
            //O primeiro caractere legível que o módulo envia é 'r'
        } else if (c == 'r') {
            inverte_saida(pin_b7);
            receive_enabled = 1;
            push(&buffer, c);
        }
    }
}

void main() {
    clock_int_4MHz();
    TRISB = 0;
    memset(recv, 0, BUFFER_SIZE);
    
    init_buffer(&buffer);
    habilita_interrupcao(recep_serial);
    taxa_serial(9600);
    nivel_alto(pin_b7);
    //    tempo_ms(500);
    nivel_baixo(pin_b7);
    nivel_baixo(pin_b6);
    lcd_ini();
    Lcd_Cmd(LCD_CURSOR_OFF);
    //printf("AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip_addr, ip_port);
    while (1) {
        if (!entrada_pin_e3) {
            Reset();
        }//pressionar o botão para gravação
        while (blink) {
            nivel_baixo(pin_b7);
            nivel_alto(pin_b6);
            timer0_ms(100);
            nivel_alto(pin_b7);
            nivel_baixo(pin_b6);
            timer0_ms(100);
            blink--;
        }

        if (buffer.count > 0) {
            c = pop(&buffer);
            recv[pos++] = c;
        }
        if (pos > 0) {
            if (strchr(recv, '>')) {
                tempo_ms(100);
                sendsw(device_id);
                clear_local_buffer();
            }

            if (strstr(recv, "+IPD,") && strchr(recv, ':')) {
                unsigned char size, lcd_pos;
                char* str = strchr(recv, ',') + 1;
                str = strtok(str, ":");
                size = atoi(str);
                //Apaga o buffer local, pois este será utilizado
                clear_local_buffer();
                while (pos < size) {
                    if (buffer.count > 0) {
                        recv[pos++] = pop(&buffer);
                    }
                }
                //Não é mais recebido do servidor: valor é fixo
                //str = strtok(recv, ":");
                //blink = atoi(str);
                blink = 5;
                
                str = strtok(NULL, ":");
                /*--------------'str' contem o texto recebido do servidor--------------*/
                /*--------------Manipule 'str' aqui                      --------------*/
                Lcd_Cmd(LCD_CLEAR);
                
                lcd_pos = size <= 18 ? (19 - size) / 2 : 1;
                lcd_escreve(1, lcd_pos, str);
                /*--------------Pare de manipular 'str' aqui             --------------*/
                
                
                clear_local_buffer();
            }

            if (strstr(recv, "\r\n")) {
                inverte_saida(pin_b7);
                if (strstr(recv, "WIFI GOT IP")) {
                    printf("AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip_addr, ip_port);
                    wifi_connected = 1;
                }
                if (strstr(recv, "CONNECT") && wifi_connected) {
                    printf("AT+CIPSEND=%d\r\n", strlen(device_id));
                }
                if (strstr(recv, "CLOSED") != NULL) {
                    nivel_baixo(pin_b7);
                }
                memset(recv, '\0', BUFFER_SIZE);
                pos = 0;
            }

        }

    }
}

void clear_local_buffer() {
    memset(recv, '\0', BUFFER_SIZE);
    pos = 0;
}

void putch(char data) {
    while (!TXIF);
    TXREG = data;
}