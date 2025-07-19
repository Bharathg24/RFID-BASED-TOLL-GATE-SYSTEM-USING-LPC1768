#include "LPC17xx.h"
#include <stdio.h>
#include <string.h>

#define BUZZER (1<<27)
#define PCLK 25000000
#define BAUDRATE 9600

#define SCK  (1 << 7)
#define MISO (1 << 8)
#define MOSI (1 << 9)
#define SSEL (1 << 6)

#define CommandReg         0x01
#define ModeReg            0x11
#define TxControlReg       0x14
#define TxAutoReg          0x15
#define TModeReg           0x2A
#define TPrescalerReg      0x2B
#define TReloadRegH        0x2C
#define TReloadRegL        0x2D
#define CommIEnReg         0x02

#define BitFramingReg      0x0D
#define FIFODataReg        0x09
#define FIFOLevelReg       0x0A
#define CommIrqReg         0x04
#define ErrorReg           0x06
#define ControlReg         0x0C

#define PCD_IDLE           0x00
#define PCD_RESETPHASE     0x0F
#define PCD_TRANSCEIVE     0x0C
#define PCD_AUTHENT        0x0E

#define PICC_REQIDL        0x26
#define PICC_ANTICOLL      0x93

#define MI_OK              1
#define MI_ERR             0
#define MI_NOTAGERR        2
#define MAX_LEN            16


void delay_ms(uint32_t ms) {
    LPC_SC->PCONP |= (1 << 1);
    LPC_TIM0->CTCR = 0x0;
    LPC_TIM0->PR = 25000 - 1;
    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TCR = 0x01;
    while (LPC_TIM0->TC < ms);
    LPC_TIM0->TCR = 0x00;
    LPC_TIM0->TC = 0;
}


void UART0_Init(void) {
    LPC_PINCON->PINSEL0 |= 0x50;
    LPC_UART0->LCR = 0x83;
    LPC_UART0->DLM = 0;
    LPC_UART0->DLL = PCLK / (16 * BAUDRATE);
    LPC_UART0->LCR = 0x03;
}

void UART0_SendChar(char c) {
    while (!(LPC_UART0->LSR & (1 << 5)));
    LPC_UART0->THR = c;
}

void UART0_SendString(const char *str) {
    while (*str) UART0_SendChar(*str++);
}
void SPI_INIT(void) {
    LPC_SC->PCONP |= (1 << 21);
LPC_PINCON->PINSEL0 |=(0x02 << 14) | (0x02 << 16) | (0x02 << 18);
    LPC_GPIO0->FIODIR |= SSEL;
    LPC_GPIO0->FIOSET = SSEL;
    LPC_SSP1->CR0 = 0x0707;
    LPC_SSP1->CPSR = 8;
    LPC_SSP1->CR1 = 0x02;
}

uint8_t SPI_Transfer(uint8_t data) {
    LPC_SSP1->DR = data;
    while (LPC_SSP1->SR & (1 << 4));
    return LPC_SSP1->DR;
}

void CS_LOW(void)  { LPC_GPIO0->FIOCLR = SSEL; }
void CS_HIGH(void) { LPC_GPIO0->FIOSET = SSEL; }


void RFID_WriteReg(uint8_t reg, uint8_t val) {
    CS_LOW();
    SPI_Transfer((reg << 1) & 0x7E);
    SPI_Transfer(val);
    CS_HIGH();
}

uint8_t RFID_ReadReg(uint8_t reg) {
    uint8_t val;
    CS_LOW();
    SPI_Transfer(((reg << 1) & 0x7E) | 0x80);
    val = SPI_Transfer(0x00);
    CS_HIGH();
    return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = RFID_ReadReg(reg);
    RFID_WriteReg(reg, tmp & (~mask));
}

void RFID_AntennaOn(void) {
    uint8_t val = RFID_ReadReg(TxControlReg);
    if (!(val & 0x03)) {
        RFID_WriteReg(TxControlReg, val | 0x03);
    }
}

void RFID_Init(void) {
    RFID_WriteReg(CommandReg, PCD_RESETPHASE);
    delay_ms(50);
    RFID_WriteReg(TModeReg, 0x8D);
    RFID_WriteReg(TPrescalerReg, 0x3E);
    RFID_WriteReg(TReloadRegL, 0x1E);
    RFID_WriteReg(TReloadRegH, 0);
    RFID_WriteReg(TxAutoReg, 0x40);
    RFID_WriteReg(ModeReg, 0x3D);
    RFID_AntennaOn();
}


uint8_t RFID_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint32_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    if (command == PCD_AUTHENT) {
        irqEn = 0x12;
        waitIRq = 0x10;
    }
    if (command == PCD_TRANSCEIVE) {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    RFID_WriteReg(CommIEnReg, irqEn | 0x80);
    RFID_WriteReg(CommIrqReg, 0x7F);
    RFID_WriteReg(FIFOLevelReg, 0x80);
    RFID_WriteReg(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++) {
        RFID_WriteReg(FIFODataReg, sendData[i]);
    }

    RFID_WriteReg(CommandReg, command);
    if (command == PCD_TRANSCEIVE) {
        MFRC522_SetBitMask(BitFramingReg, 0x80);
    }

    i = 2000;
    do {
        n = RFID_ReadReg(CommIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(RFID_ReadReg(ErrorReg) & 0x1B)) {
            status = MI_OK;
            if (n & irqEn & 0x01) status = MI_NOTAGERR;

            if (command == PCD_TRANSCEIVE) {
                n = RFID_ReadReg(FIFOLevelReg);
                lastBits = RFID_ReadReg(ControlReg) & 0x07;
                *backLen = lastBits ? (n - 1) * 8 + lastBits : n * 8;

                if (n == 0) n = 1;
                if (n > MAX_LEN) n = MAX_LEN;

                for (i = 0; i < n; i++) {
                    backData[i] = RFID_ReadReg(FIFODataReg);
                }
            }
        } else {
            status = MI_ERR;
        }
    }

    return status;
}


uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint32_t backBits;

    RFID_WriteReg(BitFramingReg, 0x07);
    tagType[0] = reqMode;
    status = RFID_ToCard(PCD_TRANSCEIVE, tagType, 1, tagType, &backBits);
    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint32_t unLen;

    RFID_WriteReg(BitFramingReg, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = RFID_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

    if (status == MI_OK) {
        for (i = 0; i < 4; i++) serNumCheck ^= serNum[i];
        if (serNumCheck != serNum[4]) status = MI_ERR;
    }
    return status;
}
void SERVO_Init(void) {
    LPC_SC->PCONP |= (1 << 6);  // Power to PWM1
    LPC_PINCON->PINSEL4 &= ~(3 << 0);
    LPC_PINCON->PINSEL4 |= (1 << 0);  // P2.0 = PWM1.1
    // PCLK_PWM1 = CCLK/4 = 25MHz
    LPC_SC->PCLKSEL0 &= ~(3 << 12); // bits 13:12 = 00
    LPC_PWM1->PR = 0;         // No prescaler
    LPC_PWM1->MR0 = 500000;   // 20ms period (500000 ticks at 25MHz)
    LPC_PWM1->MR1 = 37500;    // 1.5ms default pulse width
    LPC_PWM1->LER |= (1 << 0) | (1 << 1); // Latch MR0 and MR1
    LPC_PWM1->PCR = (1 << 9);  // Enable PWM1.1 output
    LPC_PWM1->TCR = (1 << 0) | (1 << 3);  // Enable Timer and PWM
}


// Rotate servo to specific angle
void servo_rotate(float angle) {
    float min_pulse = 0.5;   // ms for 0°
    float max_pulse = 2.5;  // ms for 180°
    float pulse_ms = min_pulse + ((angle / 180.0) * (max_pulse - min_pulse));
    uint32_t pulse_ticks = pulse_ms * 25000;  // For 25MHz PWM clock
    LPC_PWM1->MR1 = pulse_ticks;
		LPC_PWM1->LER |= (1 << 1);  // Latch MR1 update
}

int main(void) {
    LPC_GPIO1->FIODIR |= BUZZER;      // Set buzzer pin as output
    uint8_t status;
    uint8_t tagType[2];
    uint8_t serNum[5];
    char buf[64];

    // Known UIDs
    uint8_t knownUIDs[6][5] = {
        {0x5E, 0x04, 0x93, 0x5F, 0x96},
        {0xB3, 0x2F, 0x83, 0x2D, 0x32},
        {0xF3, 0x74, 0x7C, 0x14, 0xEF},
        {0xCB, 0x20, 0x93, 0x5F, 0x27},
        {0xD4, 0x95, 0xFC, 0x03, 0xBE},
        {0x43, 0x23, 0x9A, 0x14, 0xEE}
    };
    int balances[6] = {101, 80, 60, 150, 50, 120};

    // Init hardware
    SystemInit();
    UART0_Init();
    SPI_INIT();
    RFID_Init();
    SERVO_Init();

    UART0_SendString("RFID Initialized\r\n");

    while (1) {
        status = MFRC522_Request(PICC_REQIDL, tagType);
        if (status == MI_OK) {
            status = MFRC522_Anticoll(serNum);
            if (status == MI_OK) {
                UART0_SendString("CARD DETECTED! UID: ");
                for (int i = 0; i < 5; i++) {
                    sprintf(buf, "%02X ", serNum[i]);
                    UART0_SendString(buf);
                }
                UART0_SendString("\r\n");

                int matchIndex = -1;
                for (int i = 0; i < 6; i++) {
                    int matched = 1;
                    for (int j = 0; j < 5; j++) {
                        if (serNum[j] != knownUIDs[i][j]) {
                            matched = 0;
                            break;
                        }
                    }
                    if (matched) {
                        matchIndex = i;
                        break;
                    }
                }

                if (matchIndex != -1) {
                    if (balances[matchIndex] >= 50) {
                        balances[matchIndex] -= 50;
                        UART0_SendString("TOLL AMOUNT ~50RS DEDUCTED\r\n");
                        sprintf(buf, "REMAINING BALANCE: %d\r\n", balances[matchIndex]);
                        UART0_SendString(buf);

                        // ? Move servo up
                        servo_rotate(0);
                        delay_ms(1000);
                        servo_rotate(90); // ? Move servo back

                        LPC_GPIO1->FIOCLR = BUZZER; // Ensure buzzer off
                    } else {
                        UART0_SendString("BALANCE INSUFFICIENT\r\n");
                        LPC_GPIO1->FIOSET = BUZZER;   // ?? Buzz
                        delay_ms(1000);             // hold buzzer 1s
                        LPC_GPIO1->FIOCLR = BUZZER; // stop buzzer
                    }
                } else {
                    UART0_SendString("UNKNOWN CARD\r\n");
                }
            }
        } else {
            UART0_SendString("NO CARD DETECTED\r\n");
        }

        delay_ms(1000); // Poll every 1s
    }
}