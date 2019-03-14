#include <msp430.h>
#include <PCD8544.h>
#include <stdbool.h>
#include <inttypes.h>
#include <rand.h>

#define WRITE_ENABLE       0x06
#define WRITE_DISABLE      0x04
#define CHIP_ERASE         0xc7
#define READ_STATUS_REG_1  0x05
#define READ_DATA          0x03
#define PAGE_PROGRAM       0x02
#define JEDEC_ID           0x9F
#define BLOCK_ERASE        0xD8

#define abs(x) ((x>0)?(x):(-x))

#define NOP         0b00000000
#define FunctionSet 0b00100000 //0  0  1  0  0 PD  V  H
#define DispControl 0b00001100 //0  0  0  0  1  D  0  E
#define SetYAddress 0b01000000 //0  1  0  0  0 Y2 Y1 Y0
#define SetXAddress 0b10000000 //1 X6 X5 X4 X3 X2 X1 X0
#define SS_LCD BIT0
#define SS_MEM BIT5
#define DC     BIT3

#define LCD5110_RES_PIN     BIT2
#define LCD5110_SCLK_PIN    BIT4
#define LCD5110_DN_PIN      BIT2
#define LCD5110_SCE_PIN     BIT0
#define LCD5110_DC_PIN      BIT3
#define LCD5110_SELECT      P1OUT &= ~LCD5110_SCE_PIN
#define LCD5110_DESELECT    P1OUT |= LCD5110_SCE_PIN
#define LCD5110_SET_COMMAND P1OUT &= ~LCD5110_DC_PIN
#define LCD5110_SET_DATA    P1OUT |= LCD5110_DC_PIN
#define LCD5110_COMMAND     0
#define LCD5110_DATA        1

#define SPI_MSB_FIRST UCA0CTL0 |= UCMSB // or UCA0CTL0 |= UCMSB (USCIA) or USICTL0 &= ~USILSB (USI)
#define SPI_LSB_FIRST UCA0CTL0 &= ~UCMSB // or UCA0CTL0 &= ~UCMSB or USICTL0 |= USILSB (USI)

#define UserLED BIT5    //P2.5 is connected to User LED

uint8_t ManID = 0,memoryType = 0,capacity = 0;
uint32_t addr = 0x00000000,in=0;
unsigned int base_cnt, meas_cnt;
int delta_cnt, k;
uint8_t i,val;
int pt;

//                        one       two         three       aeroplane       batak       bus         chidiya    maina       naav        patang       Table      tota        train       GAMEOVER    PlaceFinger Game
uint32_t startAddr[16] = {0         ,11904      ,23808      ,36864          ,55680      ,70656      ,82560     ,116352     ,130560     ,142848     ,199680     ,241920     ,256128     ,98304      ,156672     ,212736}       ; //Stores starting addresses of all audio files
uint32_t endinAddr[16] = {11904     ,23808      ,36864      ,55680          ,70656      ,82560      ,98304     ,130560     ,142848     ,156672     ,212736     ,256128     ,269184     ,116352     ,199680     ,241920}       ; //Stores ending addresses of all audio files
int fly[10]            = {                                   1              ,1          ,0          ,1         ,1          ,0          ,1          ,0          ,1          ,0     }       ; //Stores whether the audio at that index fly or not
int index;                //= 1;

unsigned char transmit(unsigned char data)
{
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
    while (!(IFG2 & UCA0RXIFG)); // As soon as the transmit is complete the SPIF bit is set in the SPSR register of the spi module. The while condition is turned to 0 as soon as this happens
    return (UCA0RXBUF);
}

void unbusy(void)
{
    P1OUT |= BIT5;
    P1OUT &= ~BIT5;
    transmit(READ_STATUS_REG_1);
    while (transmit(0x00) != 0x00)
    {
    };
    while (!(IFG2 & UCA0TXIFG));
    P1OUT |= BIT5;
}

void timer_init()
{
    //set WDT for time keeping purpose
    //need an interrupt every 1/8000 of a second,i.e. when count reaches 2000
    WDTCTL = WDT_MDLY_0_5;                      // WDT 1/32000s, SMCLK, interval timer
    IE1 |= WDTIE;                               // Enable WDT interrupt


    //Timer A0 sourced by pin oscillator
    TA0CTL   = TASSEL_3 + MC_2 + TACLR;       // TACLK, cont mode
    TA0CCTL1 = CM_3 + CCIS_2 +CAP;               // Pos&Neg,GND,Cap


    /*Configure Ports for relaxation oscillator*/
    /*The P1SEL2 register allows Timer_A to receive it's clock from a GPIO*/
    /*See the Application Information section of the device datasheet for info*/
    P1DIR  &= ~BIT6;                        // P1.6 is the input used here
    P1SEL  &= ~BIT6;
    P1SEL2 |=  BIT6;


    //set timer 1 to drive pwm

    TA1CCR0 = 256;          //8 bit PWM mode
    TA1CCR2 = 127;          //initial width

    TA1CTL |= TASSEL_2;     //select SMCLK as source
    TA1CTL |= TACLR;        //clear
    TA1CTL |= MC_1;         //up mode

    TA1CCTL2 |= OUTMOD_7;   //reset set mode

}

void clockInit()
{
    BCSCTL1 |= CALBC1_16MHZ;                              // Set DCO to 1MHz
    DCOCTL  |= CALDCO_16MHZ;
}

void spi_init()
{
    P1DIR |= BIT5;
    P1OUT |= BIT5;

    P1DIR |= SS_LCD;                                      //SS of GLCD
    P1OUT |= SS_LCD;

    P1DIR |= DC;                                         //DC of GLCD
    P1OUT &=~DC;

    P1SEL |= BIT1 + BIT2 + BIT4;                        //P1.4 -> CLK , P1.2 -> SIMO, P1.1 -> SOMI
    P1SEL2|= BIT1 + BIT2 + BIT4;                        //P1.4 -> CLK , P1.2 -> SIMO, P1.1 -> SOMI

    UCA0CTL1 |= UCSWRST;                                // Hold USCI in reset state
    UCA0CTL0 |= UCCKPH + UCMST + UCSYNC + UCMSB;        // 3-pin, 8-bit, SPI Master
    UCA0CTL0 &= ~UCCKPL;
    UCA0CTL1 |= UCSSEL_2;                               // Clock -> SMCLK
    UCA0BR0 = 2;                                     // SPI CLK -> SMCLK/2
    UCA0BR1 = 0;
    UCA0MCTL = 0;
    UCA0CTL1 &= ~UCSWRST;
}

void measure_count()
{
    TA0CTL |= TACLR;                        // Clear Timer_A TAR

    __delay_cycles(500);

    TA0CCTL1 ^= CCIS0;                      // Create SW capture of CCR1
    TA0CCTL1 ^= CCIS0;                      // Create SW capture of CCR1
    meas_cnt = TACCR1;                      // Save result
}

void writeToLCD(unsigned char dataCommand, unsigned char data)
{
//    __disable_interrupt();
    __delay_cycles(10000);
    BCSCTL1 = CALBC1_1MHZ;                  // Set DCO to 1MHz
    DCOCTL  = CALDCO_1MHZ;
    UCA0CTL1 |= UCSWRST;                    // Hold USCI in reset state
    UCA0BR0 = 1;                            // SPI CLK -> SMCLK
    UCA0BR1 = 0;                            // SPI CLK -> SMCLK
    UCA0CTL1 &= ~UCSWRST;

    __delay_cycles(10000);

    LCD5110_SELECT;

    if(dataCommand)
    {
        LCD5110_SET_DATA;
    }
    else
    {
        LCD5110_SET_COMMAND;
    }

    UCA0TXBUF = data;
    while(!(IFG2 & UCA0TXIFG));

    LCD5110_DESELECT;

    __delay_cycles(10000);

    BCSCTL1 = CALBC1_16MHZ;                 // Set DCO to 16MHz
    DCOCTL  = CALDCO_16MHZ;
    UCA0CTL1 |= UCSWRST;                    // Hold USCI in reset state
    UCA0BR0 = 2;                            // SPI CLK -> SMCLK/2
    UCA0BR1 = 0;                            // SPI CLK -> SMCLK
    UCA0CTL1 &= ~UCSWRST;
    __delay_cycles(10000);
//    __enable_interrupt();
}

void writeCharToLCD(char c)
{
    unsigned char i;
    for(i = 0; i < 5; i++)
    {
        writeToLCD(LCD5110_DATA, font[c - 0x20][i]);
    }
    writeToLCD(LCD5110_DATA, 0);
}

void writeStringToLCD(const char *string)
{
    while(*string)
    {
        writeCharToLCD(*string++);
    }
}

void writeBlockToLCD(char *byte, unsigned char length)
{
    unsigned char c = 0;
    while(c < length)
    {
        writeToLCD(LCD5110_DATA, *byte++);
        c++;
    }
}

void writeGraphicToLCD(char *byte, unsigned char transform)
{
    int c = 0;
    char block[8];

    if(transform & FLIP_V)
    {
        SPI_LSB_FIRST;
    }
    if(transform & ROTATE)
    {
        c = 1;
        while(c != 0)
        {
            (*byte & 0x01) ? (block[7] |= c) : (block[7] &= ~c);
            (*byte & 0x02) ? (block[6] |= c) : (block[6] &= ~c);
            (*byte & 0x04) ? (block[5] |= c) : (block[5] &= ~c);
            (*byte & 0x08) ? (block[4] |= c) : (block[4] &= ~c);
            (*byte & 0x10) ? (block[3] |= c) : (block[3] &= ~c);
            (*byte & 0x20) ? (block[2] |= c) : (block[2] &= ~c);
            (*byte & 0x40) ? (block[1] |= c) : (block[1] &= ~c);
            (*byte & 0x80) ? (block[0] |= c) : (block[0] &= ~c);
            *byte++;
            c <<= 1;
        }
    } else
    {
        while(c < 8) {
            block[c++] = *byte++;
        }
    }

    if(transform & FLIP_H)
    {
        c = 7;
        while(c > -1)
        {
            writeToLCD(LCD5110_DATA, block[c--]);
        }
    } else
    {
        c = 0;
        while(c < 8)
        {
            writeToLCD(LCD5110_DATA, block[c++]);
        }
    }
    SPI_MSB_FIRST;
}
void setAddr(unsigned char xAddr, unsigned char yAddr)
{
    writeToLCD(LCD5110_COMMAND, PCD8544_SETXADDR | xAddr);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETYADDR | yAddr);
}

void clearLCD()
{
    setAddr(0,0);
    int c = 0;
    while(c < PCD8544_MAXBYTES)
    {
        writeToLCD(LCD5110_DATA, 0);
        c++;
    }
    setAddr(0, 0);
}

void clearBank(unsigned char bank)
{
    setAddr(0, bank);
    int c = 0;
    while(c < PCD8544_HPIXELS)
    {
        writeToLCD(LCD5110_DATA, 0);
        c++;
    }
    setAddr(0, bank);
}

void initLCD()
{
    writeToLCD(LCD5110_COMMAND, PCD8544_FUNCTIONSET | PCD8544_EXTENDEDINSTRUCTION);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETVOP | 0x3F);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETTEMP | 0x02);
    writeToLCD(LCD5110_COMMAND, PCD8544_SETBIAS | 0x03);
    writeToLCD(LCD5110_COMMAND, PCD8544_FUNCTIONSET);
    writeToLCD(LCD5110_COMMAND, PCD8544_DISPLAYCONTROL | PCD8544_DISPLAYNORMAL);

}

void placeFinger()
{
    addr  = startAddr[14];       //play audio of PlaceFinger
    __enable_interrupt();

    while(1)
    {
        measure_count();
        delta_cnt = meas_cnt - base_cnt;

        if(abs(delta_cnt) > 40)
        {
            __disable_interrupt();
            index = rand();
            index = abs(index%9) + 3;
            addr  = startAddr[index];
            __enable_interrupt();

            break;
        }
        else if(addr  == endinAddr[14])
        {
            addr  = startAddr[14];                      //keeps speaking "place finger" until the user does so
        }
    }
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                           // Stop watchdog timer

    clockInit();
    __delay_cycles(100);
    spi_init();
    __delay_cycles(100);
    timer_init();
    __delay_cycles(100);

    P2DIR |= UserLED;

    P2DIR |= BIT2;                           // Reset the LCD
    P2OUT &=~BIT2;
    __delay_cycles(4000);
    P2OUT |= BIT2;
    __delay_cycles(4000);

    P1OUT |= BIT5;
    P1OUT &= ~BIT5;

    transmit(JEDEC_ID);
    ManID  = transmit(0);
    memoryType = transmit(0);
    capacity = transmit(0);

    P1OUT |= BIT5;

    initLCD();
    __delay_cycles(100);

    for(i=0;i<25;i++)
    {
        measure_count();
        base_cnt = (meas_cnt+base_cnt)/2;
    }

    i = 0x39;                               //ascii for '9'

    clearBank(0);
    writeStringToLCD(" Chidiya Udd ");
    clearBank(1);
    writeStringToLCD(" with MSP430 ");
    clearBank(2);
    writeStringToLCD("Score : ");
    setAddr(45,2);
    writeCharToLCD(i);

    P2DIR |= BIT4;          //P2.4 is connected to input of PAM
    P2SEL |= BIT4;
    P2SEL2&=~BIT4;

    __enable_interrupt();

    placeFinger();

    addr  = startAddr[15];      //The Game will begin in
    while(addr!=endinAddr[15]);

    addr  = startAddr[2];       //Three
    while(addr!=endinAddr[2]);

    addr  = startAddr[1];       //Two
    while(addr!=endinAddr[1]);

    addr  = startAddr[0];       //One
    while(addr!=endinAddr[0]);

    index = rand();
    index = abs(index%9) + 3;
    addr  = startAddr[index];

    while(1)
    {
        if(addr==endinAddr[index])                  //if end of current audio is reached play next audio
        {
            __disable_interrupt();

            if(fly[index-3]==1)                       //current audio being played flies
            {                                       //audio has ended but user didn't lift finger
                measure_count();
                delta_cnt = meas_cnt - base_cnt;

                if(abs(delta_cnt) > 40)
                {
                    i--;
                    setAddr(45,2);
                    writeCharToLCD(i);
                }
            }
            else
            {
                measure_count();
                delta_cnt = meas_cnt - base_cnt;

                if(abs(delta_cnt) < 40)
                {
                    i--;
                    setAddr(45,2);
                    writeCharToLCD(i);
                }
            }
            __delay_cycles(64000000);       //stops for 5sec
            placeFinger();
        }
        if(i==0x30)
        {
            while(1)
            {
                addr  = startAddr[13];      //audio for Game Over
                while(addr!=endinAddr[13]);
                __disable_interrupt();
                __delay_cycles(32000000);       //stops for 2sec
                __enable_interrupt();
            }
        }
    }
}


// Watchdog Timer interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) watchdog_timer (void)
#else
#error Compiler not supported!
#endif
{
    pt++;
    if(pt==2)
    {
        //coming into this indicates 1/8000 of a second have passed
        //so read the next byte and set the new pulse width
        P1OUT &= ~BIT5;

        transmit(READ_DATA);
        transmit( (addr&0x00FF0000) >> 16);
        transmit( (addr&0x0000FF00) >>  8);
        transmit( (addr&0x000000FF));

        val = transmit(0);

        TA1CCR2 = val;

        P1OUT |= BIT5;

        addr++;
        pt=0;
    }
}
