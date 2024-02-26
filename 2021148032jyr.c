#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <softTone.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

/**
 * ==================================== 
 * Init value 
 * ====================================
 */
// LCD
#define I2C_ADDR 0x27 //0x27 또는 0x3F일 가능성 
#define LCD_CHR 1 //데이터 전송 
#define LCD_CMD 0 //명령 전송 
#define LINE1 0x80 //첫째줄
#define LINE2 0xC0 //둘째줄
#define LCD_BACKLIGHT 0x08 //On
#define ENABLE  0b00000100 // Enable bit 

// Temperature
#define DHTPIN 7
#define MAXTIMINGS 85

// FND
#define TM1637_I2C_COMM1 0x40
#define TM1637_I2C_COMM2 0xC0
#define TM1637_I2C_COMM3 0x80
#define BITDELAY 100
#define PIN_CLK 27 //FND
#define PIN_DIO 28 //FND

// Buzzer
#define SPKR 26 //수동 부저 
#define TOTAL 6 // 

// LED
#define LED 31

static const uint8_t blank[] = {0,0,0,0};
const uint8_t digitToSegment[] = {
 // XGFEDCBA
  0b00111111,    // 0
  0b00000110,    // 1
  0b01011011,    // 2
  0b01001111,    // 3
  0b01100110,    // 4
  0b01101101,    // 5
  0b01111101,    // 6
  0b00000111,    // 7
  0b01111111,    // 8
  0b01101111,    // 9
  0b01000000     // dot
};

/*
mode = 0; 일반
mode = 1; PASS 3초
mode = 2; LED 출력 3초
*/
int mode = 0;

// DHT
int dht11_dat[5] = { 0, 0, 0, 0, 0 };

// FND
uint8_t m_pinClk, m_pinDIO, m_brightness; 

// Buzzer
int notes[] = { //경고 음계 배열
   261.63,0,  //Do
   261.63,0,  //Do
   261.63,0  //Do
};

int fd; //
char array_pass[] ="PASS!!"; //LCD 출력 멘트 
char array_warning[] = "Warning COVID19";
char array_nothing[] = "Nothing!";


/**
 * ====================================
 * Function 
 * ====================================
 */
//LCD 관련 함수들 
void lcd_toggle_enable(int bits){
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits | ENABLE));
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
    delayMicroseconds(500);
}

void lcd_byte(int bits, int mode){
    int bits_high;
    int bits_low;

    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    wiringPiI2CReadReg8(fd, bits_high);
    lcd_toggle_enable(bits_high);

    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low);
}

void typeChar(char val){
    lcd_byte(val, LCD_CHR);
}

void typeln(const char *s){
    while (*s) lcd_byte(*(s++), LCD_CHR);
}

void typeFloat(float myFloat){
    char buffer[20];
    sprintf(buffer, "%4.2f", myFloat);
    typeln(buffer);
}

void typeInt(int i){
    char buffer[20];
    sprintf(buffer, "%d", i);
    typeln(buffer);
}

void ClrLcd(void){
    lcd_byte(0x01, LCD_CMD);
    lcd_byte(0x02, LCD_CMD);
}

void lcdLoc(int line){
    lcd_byte(line, LCD_CMD);
}

void lcd_init(){
    lcd_byte(0x33, LCD_CMD); // Initialise
    lcd_byte(0x32, LCD_CMD); // Initialise
    lcd_byte(0x06, LCD_CMD); // Cursor move direction
    lcd_byte(0x0C, LCD_CMD); // 0x0f On, Blink off // Blink off, ON=0x0f 
    lcd_byte(0x28, LCD_CMD); // Data Length, number of lines, ofnt size
    lcd_byte(0x01, LCD_CMD); // Clear display
    delayMicroseconds(500);
}

void print(char array[]) {
    ClrLcd();
    lcdLoc(LINE1);
    typeln(array);
}

void printFloat(float data) {
    ClrLcd();
    lcdLoc(LINE1);
    typeFloat(data);
}

// Temperature
int read_dht11_dat()
{
   uint8_t laststate = HIGH;
   uint8_t counter   = 0;
   uint8_t j = 0, i = 0;

   dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;

   /* pull pin down for 18 milliseconds */
   pinMode( DHTPIN, OUTPUT );
   digitalWrite( DHTPIN, LOW );
   delay( 18 );

   /* then pull it up for 40 microseconds */ 
   digitalWrite( DHTPIN, HIGH );
   delayMicroseconds( 40 );

   /* prepare to read the pin */
   pinMode( DHTPIN, INPUT );
   //printf("before : %d %d %d %d %d\n", dht11_dat[0], dht11_dat[1], dht11_dat[2], dht11_dat[3], dht11_dat[4]);
   /* detect change and read data */
   for ( i = 0; i < MAXTIMINGS; i++ )
   {
      counter = 0;
      while ( digitalRead( DHTPIN ) == laststate )
      {
         counter++; 
         //printf("digital : %d\n", digitalRead(DHTPIN));
         delayMicroseconds( 1 );
         if ( counter == 255 )
         {
            break;
         }
      }
      laststate = digitalRead( DHTPIN );
      if ( counter == 255 )
         break;
      /* ignore first 3 transitions */
      if ( (i >= 4) && (i % 2 == 0) )
      {
         /* shove each bit into the storage bytes */
         dht11_dat[j / 8] <<= 1;
         if ( counter > 50 )
            dht11_dat[j / 8] |= 1;
         j++;
      }
   }
   //printf("result : %d %d %d %d %d\n", dht11_dat[0], dht11_dat[1], dht11_dat[2], dht11_dat[3], dht11_dat[4]);
   //printf("%d %d", (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]), ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF));
   /*
    * check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
    * print it out if data is good
     * C = dht 2 & 3
    */
   if ( (j >= 40) &&
        (dht11_dat[4] == ( (dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF) ) )
   {
      return 1;
   }

    return 0;
}

// fnd관련 함수들 
void TMstartWrite(){
    pinMode(m_pinDIO, OUTPUT);
    delayMicroseconds(BITDELAY);
}

void TMstopWrite(){
    pinMode(m_pinDIO, OUTPUT);
    delayMicroseconds(BITDELAY);
    pinMode(m_pinClk, INPUT);
    delayMicroseconds(BITDELAY);
    pinMode(m_pinDIO, INPUT);
    delayMicroseconds(BITDELAY);
}

void TMWriteByte(uint8_t b){ 
    uint8_t data = b;
    // uint8_t i = 0;

    // 8 Data Bits
    for(uint8_t i = 0; i < 8; i++) {
        // CLK low
        pinMode(m_pinClk, OUTPUT);
        delayMicroseconds(BITDELAY);

        // Set data bit
        if (data & 0x01)
            pinMode(m_pinDIO, INPUT);
        else
            pinMode(m_pinDIO, OUTPUT);

        delayMicroseconds(BITDELAY);

        // CLK high
        pinMode(m_pinClk, INPUT);
        delayMicroseconds(BITDELAY);
        data = data >> 1;
    }

    // Wait for acknowledge
    // CLK to zero
    pinMode(m_pinClk, OUTPUT);
    pinMode(m_pinDIO, INPUT);
    delayMicroseconds(BITDELAY);

    // CLK to high
    pinMode(m_pinClk, INPUT);
    delayMicroseconds(BITDELAY);
    if (digitalRead(m_pinDIO) == 0) pinMode(m_pinDIO, OUTPUT);
    delayMicroseconds(BITDELAY);
    pinMode(m_pinClk, OUTPUT);
    delayMicroseconds(BITDELAY);
}

void TMsetBrightness(uint8_t brightness){
    m_brightness = ((brightness * 0x7) | 0x08) & 0x0f;
}

void TMsetSegments(const uint8_t segments[], uint8_t length, uint8_t pos){
     // Write COMM1
   TMstartWrite();
   TMWriteByte(TM1637_I2C_COMM1);
   TMstopWrite();

   // Write COMM2 + first digit address
   TMstartWrite();
   TMWriteByte(TM1637_I2C_COMM2 + (pos & 0x03));

   // Write the data bytes c99 모든 선언은 최상위에서 해야한다. 
   for (uint8_t k=0; k < length; k++)
     TMWriteByte(segments[k]);

   TMstopWrite();
    
   // Write COMM3 + brightness
   TMstartWrite();
   // TMwriteByte(TM1637_I2C_COMM3 + (m_brightness & 0x0f));
   TMWriteByte(TM1637_I2C_COMM3 + m_brightness);
   TMstopWrite();
}

void TMclear(){
    TMsetSegments(blank, 4, 0);
} 

void TMshowNumber(int num, uint8_t dots, bool leading_zero, uint8_t length, uint8_t pos){

    uint8_t digits[4];
    const static int divisors[] = {1,10,100,1000};
    bool leading = true;

    for(int8_t k = 0; k < 4; k++){
        int divisor = divisors[4 - 1 - k];
        int d = num/divisor;
        uint8_t digit = 0;

        if(d==0){
            if(leading_zero || !leading || (k==3)) digit = digitToSegment[d];
            else digit = 0;
        }else{
            digit = digitToSegment[d];
            num -= d * divisor;
            leading = false;
        }

        digit |= (dots & 0x80);
        dots <<= 1;
        digits[k] = digit;
    }
    TMsetSegments(digits + (4 - length), length, pos);
}

void TMshowDouble(double x){
    const uint8_t
    minus[] = {64,64,64,64},
    zeropoint[] = {0B10111111};
    int x100;
    if (x>99) x = 99.99;
    x100=x*(x<0 ? -1000 : 1000);
    x100=x100/10+(x100%10 >4);
    if(x100<100){
        TMsetSegments(zeropoint, 1, 1);
        TMshowNumber(x100, 0b01000000, true, 2,2);
        TMsetSegments(x<0 ? minus : blank, 1, 0);
    }else if (x<0){
        TMsetSegments(minus, 1,0);
        TMshowNumber(x100, 0b01000000, false, 3, 1);
    }else{
        TMshowNumber(x100, 0b01000000, false, 4, 0);
    }
}

void TMsetup(){  // FND 셋업
    m_pinClk = PIN_CLK;
    m_pinDIO = PIN_DIO; 

    pinMode(m_pinClk, INPUT);
    pinMode(m_pinDIO, INPUT);
    digitalWrite(m_pinClk, LOW);
    digitalWrite(m_pinDIO, LOW);
}

void showTemperature(double temperature) {
    TMclear();
    TMshowDouble(temperature);
}

// 부저 재생 함수
int musicPlay() 
{
   int i;
   softToneCreate(SPKR); // 톤 생성을 위한 gpio핀 초기화 함수
        
   for (i=0; i<TOTAL; ++i){
        if (i % 2 == 0) {
            digitalWrite(LED, HIGH);
        } else {
            digitalWrite(LED, LOW);
        }
        
      softToneWrite(SPKR, notes[i]); // 지정된 gpio핀에서 소프트톤 출력 함수
        // gpio연결 한거에서 경고 음계 재생해라.
      delay(280); //각 음마다 delay 존재 
   }

   digitalWrite(LED, LOW);
   return 0;
}

double parseTemperature(int integer, int decimal) {
    double temp = 0.0;

    if (decimal >= 10) {
        temp = (double)decimal / 100;
    }
    else {
        temp = (double)decimal / 10;
    }

    return integer + temp;
}

// 초기 함수 
void init() {
    if (wiringPiSetup() == -1) exit (1);

    fd = wiringPiI2CSetup(I2C_ADDR); // LCD 
    lcd_init(); // LCD

    TMsetup(); // FND
    TMsetBrightness(1); // FND

    // LED
    pinMode(LED, OUTPUT);
    digitalWrite(LED,LOW);
}


void main() {    
    int executetemperaturecount = 0; 
    int count = 0;
    int change = 0;
    float temperature = 0.0f;
    int temperatureResult = 0;
    int temperature_int = 0;
    int temperature_decimal = 0;

    init();
    
    while(1) {
        if(executetemperaturecount >= 1){
            temperatureResult = read_dht11_dat();
            executetemperaturecount = 0;
            printf("success %d\n" , temperatureResult);
        }
    
        if (temperatureResult) {
            temperature_int = dht11_dat[2];
            temperature_decimal = dht11_dat[3];
            temperature = parseTemperature(temperature_int, temperature_decimal);
            
        }

        if (mode == 0) {
            if (temperature > 33.0 ) { //33도 넘으면 알람
                mode = 2;
                count = 0;
            }
            else if (temperature <= 33.0 && temperature >= 30.0) { //30 33 사이면 pass
                mode = 1;
                count = 0;
            }
            else {
                mode = 0;
            } //아님 nothing출력 
        }

        showTemperature(temperature);
        switch(mode) {
            case 0:
                print(array_nothing);
            break;

            case 1:
                print(array_pass);
                //temperature = 0.0;
            break;

            case 2:
                print(array_warning);
                musicPlay();
                //temperature = 0.0;
                mode = 0;
            break;
        
            default: break;
        }

        count++;

        if (count > 30) {
            mode = 0;
            count = 0;
        }

        executetemperaturecount++;
        delay(200); 
    }
}