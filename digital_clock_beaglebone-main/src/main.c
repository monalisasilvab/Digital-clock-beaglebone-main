#include "gpio.h"
#include "timers.h"
#include "lcd.h"
#include "interrupt.h"

lcd_handler_t lcd;

gpio_handle_t btn_fmt = {
  .port = GPIO1,
  .pin_number = 15
};
gpio_handle_t btn_sel = {
  .port = GPIO1,
  .pin_number = 14  
};
gpio_handle_t btn_set = {
  .port = GPIO0,
  .pin_number = 27
};
gpio_handle_t btn_alarm = {
  .port = GPIO2,
  .pin_number = 1
};
gpio_handle_t led_on = {
  .port = GPIO1,
  .pin_number = 16
};
gpio_handle_t led_alarm = {
  .port = GPIO1,
  .pin_number = 17
};
gpio_handle_t buzzer = {
  .port = GPIO1,
  .pin_number = 28
};

// Possible FSM States
typedef enum {
  NORMAL,
  ALARM_TRIGGERED,
  SELECT_FORMAT,
  ADJUST_HOUR,
  ADJUST_MINUTE,
  ADJUST_SECOND,
  ALARM_HOUR,
  ALARM_MINUTE,
  ALARM_SECOND
} states;

// Clock Variables
int clock_seconds;
int clock_minutes;
int clock_hours;
bool mode_24h;
int alarm_seconds;
int alarm_minutes;
int alarm_hours;
bool alarm_enabled;

// Control Variables
states current_state;
int led_level;
int ms_elapsed;

int int_to_str(int32_t value, char *buffer, uint8_t size) {
     char string[size];
     int i;
     for(i = 0; i < size - 1; i++) {
          string[i] = '0' + value % 10;
          value /= 10;
          if(value == 0) {
               break;
          }
     }
     int j;
     int a = i;
     for(j = 0; j <= i; j++) {
          buffer[j] = string[a--];
     }
     buffer[j++] = '\0';
     return j;
}

void write_clock(int hour, int min, int sec) {
  lcdSetCursor(&lcd, 0, 0);

  /* Show Hour */
  lcdWriteChar(&lcd, (hour / 10) + '0');
  lcdWriteChar(&lcd, (hour % 10) + '0');

  lcdWriteChar(&lcd, ':');

  /* Show Minute */
  lcdWriteChar(&lcd, (min / 10) + '0');
  lcdWriteChar(&lcd, (min % 10) + '0');

  lcdWriteChar(&lcd, ':');

  /* Show Second */
  lcdWriteChar(&lcd, (sec / 10) + '0');
  lcdWriteChar(&lcd, (sec % 10) + '0');
  
  if (!mode_24h) {
    if (clock_hours >= 12) {
      lcdWriteString(&lcd, "pm");
    }
    else {
      lcdWriteString(&lcd, "am");
    }
  }
  else {
    lcdWriteString(&lcd, "  ");
  }
}

// Increment current hours/minutes/seconds
void increment_clock() {
  if (ms_elapsed >= 1000) {
    clock_seconds++;
    ms_elapsed = 0;
  }
  
  if (clock_seconds >= 60) {
    clock_seconds = 0;
    clock_minutes++;
      
    if (clock_minutes >= 60) {
      clock_minutes = 0;
      clock_hours++;  

      if (clock_hours >= 24) {
        clock_hours = 0;
      }
    }
  }
}

// Display current hours/minutes/seconds
void display_clock() {
  int converted_hour = clock_hours;
  
  // Convert to 12h format
  if (!mode_24h) {
    if (clock_hours == 0) {
      converted_hour = 12;
    }
    else if (clock_hours > 12) {
      converted_hour = clock_hours - 12;
    }
  }

  write_clock(converted_hour, clock_minutes, clock_seconds);
}

// Display alarm hours/minutes/seconds
void display_alarm() {
  int converted_hour = alarm_hours;
  
  // Convert to 12h format
  if (!mode_24h) {
    if (alarm_hours == 0) {
      converted_hour = 12;
    }
    else if (alarm_hours > 12) {
      converted_hour = alarm_hours - 12;
    }
  }
  
  write_clock(converted_hour, alarm_minutes, alarm_seconds);
}

void display_mode() {
  lcdSetCursor(&lcd, 1, 0);

  switch (current_state) {
    case NORMAL:
        lcdWriteString(&lcd, "Alarm: ");
        if (alarm_enabled) {
            lcdWriteString(&lcd, "ON       ");
        }
        else {
            lcdWriteString(&lcd, "OFF      ");
        }
        break;
    
    case ALARM_TRIGGERED:
        lcdWriteString(&lcd, "Alarm triggered!");
        break;
    
    case SELECT_FORMAT:
        break;
    
    case ADJUST_HOUR:
        lcdWriteString(&lcd, "Adjust: Hour    ");
        break;
    
    case ADJUST_MINUTE:
        lcdWriteString(&lcd, "Adjust: Min     ");
        break;
    
    case ADJUST_SECOND:
        lcdWriteString(&lcd, "Adjust: Sec     ");
        break;
    
    case ALARM_HOUR:
        lcdWriteString(&lcd, "Alarm: Hour    ");
        break;
    
    case ALARM_MINUTE:
        lcdWriteString(&lcd, "Alarm: Min     ");
        break;
    
    case ALARM_SECOND:
        lcdWriteString(&lcd, "Alarm: Sec     ");
        break;
  }
}

void lcd_init(void) {
  gpio_handle_t rs;
  rs.port = GPIO1;
  rs.pin_number = 6;
  gpioPInitPin(&rs, OUTPUT);

  gpio_handle_t en;
  en.port = GPIO1;
  en.pin_number = 7;
  gpioPInitPin(&en, OUTPUT);

  gpio_handle_t d4;
  d4.port = GPIO1;
  d4.pin_number = 2;
  gpioPInitPin(&d4, OUTPUT);

  gpio_handle_t d5;
  d5.port = GPIO1;
  d5.pin_number = 3;
  gpioPInitPin(&d5, OUTPUT);

  gpio_handle_t d6;
  d6.port = GPIO1;
  d6.pin_number = 13;
  gpioPInitPin(&d6, OUTPUT);

  gpio_handle_t d7;
  d7.port = GPIO1;
  d7.pin_number = 12;
  gpioPInitPin(&d7, OUTPUT);

  lcd.rs = rs;
  lcd.en = en;
  lcd.data[0] = d4;
  lcd.data[1] = d5;
  lcd.data[2] = d6;
  lcd.data[3] = d7;

  lcdInitModule(&lcd);
}

bool button_set_pressed;
bool button_fmt_pressed;
bool button_sel_pressed;
bool button_alarm_pressed;

void ButtonHandler(void) {
	if(gpioCheckIntFlag(&btn_set, GPIO_INTC_LINE_1)) {
    putCh('A');
    button_set_pressed = true; 
    gpioClearIntFlag(&btn_set, GPIO_INTC_LINE_1);
	}
	else if(gpioCheckIntFlag(&btn_fmt, GPIO_INTC_LINE_1)) {
    putCh('B');
    button_fmt_pressed = true; 
    gpioClearIntFlag(&btn_fmt, GPIO_INTC_LINE_1);
	}
	else if(gpioCheckIntFlag(&btn_sel, GPIO_INTC_LINE_2)) {
    putCh('C');
    button_sel_pressed = true; 
    gpioClearIntFlag(&btn_sel, GPIO_INTC_LINE_2);
	}
	else if(gpioCheckIntFlag(&btn_alarm, GPIO_INTC_LINE_1)) {
    putCh('D');
    button_alarm_pressed = true; 
    gpioClearIntFlag(&btn_alarm, GPIO_INTC_LINE_1);
	}
  delay_ms(60);  
}

void components_init(void) {
  gpioAintcConfigure(SYS_INT_GPIOINT0A, 0, &ButtonHandler);
  gpioAintcConfigure(SYS_INT_GPIOINT1A, 0, &ButtonHandler);
  gpioAintcConfigure(SYS_INT_GPIOINT1B, 0, &ButtonHandler);
  gpioAintcConfigure(SYS_INT_GPIOINT2A, 0, &ButtonHandler);

  gpioPInitPin(&btn_fmt, INPUT);
  gpioConfigPull(&btn_fmt, PULLUP);
  gpioPinIntEnable(&btn_fmt, GPIO_INTC_LINE_1);
  gpioIntTypeSet(&btn_fmt, GPIO_INTC_TYPE_FALL_EDGE);

  gpioPInitPin(&btn_sel, INPUT);
  gpioConfigPull(&btn_sel, PULLUP);
  gpioPinIntEnable(&btn_sel, GPIO_INTC_LINE_2);
  gpioIntTypeSet(&btn_sel, GPIO_INTC_TYPE_FALL_EDGE);

  gpioPInitPin(&btn_set, INPUT);
  gpioConfigPull(&btn_set, PULLUP);
  gpioPinIntEnable(&btn_set, GPIO_INTC_LINE_1);
  gpioIntTypeSet(&btn_set, GPIO_INTC_TYPE_FALL_EDGE);

  gpioPInitPin(&btn_alarm, INPUT);
  gpioConfigPull(&btn_alarm, PULLUP);
  gpioPinIntEnable(&btn_alarm, GPIO_INTC_LINE_1);
  gpioIntTypeSet(&btn_alarm, GPIO_INTC_TYPE_FALL_EDGE);
  
  gpioPInitPin(&led_on, OUTPUT);
  gpioSetPinValue(&led_on, LOW);
  gpioPInitPin(&led_alarm, OUTPUT);
  gpioSetPinValue(&led_alarm, HIGH);

  gpioPInitPin(&buzzer, OUTPUT);

  lcd_init();
}

void finite_state_machine(void) {
  display_mode();
  
  switch (current_state) {
    case NORMAL:
        // Output Logic
        increment_clock();
        display_clock();
        
        // Next State Logic
        if (alarm_enabled &&
            clock_seconds == alarm_seconds &&
            clock_minutes == alarm_minutes &&
            clock_hours == alarm_hours) {
            current_state = ALARM_TRIGGERED;
        }
        else if (button_fmt_pressed) {
            button_fmt_pressed = false;
            current_state = SELECT_FORMAT;
        }
        else if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = ADJUST_HOUR;
        }
        else if (button_alarm_pressed) {
            button_alarm_pressed = false;
            current_state = ALARM_HOUR;
        }
        
        delay_ms(200);
        ms_elapsed += 200;
        break;
    
    case ALARM_TRIGGERED:
        delay_ms(200);
        ms_elapsed += 200;
        increment_clock();
        display_clock();
    
        led_level = !led_level;
        gpioSetPinValue(&led_alarm, led_level);
    
        if (button_alarm_pressed) {
            button_alarm_pressed = false;
            led_level = HIGH;
            gpioSetPinValue(&led_alarm, led_level);
            current_state = NORMAL;
        }
    
        gpioSetPinValue(&buzzer, HIGH);
        delay_ms(300);
        gpioSetPinValue(&buzzer, LOW);
    
        break;
    
    case SELECT_FORMAT:
        // Toggle the mode
        mode_24h = !mode_24h;
        display_clock();
    
        // Return to NORMAL
        current_state = NORMAL;
        break;
    
    case ADJUST_HOUR:
        display_clock();
    
        if (button_set_pressed) {
            button_set_pressed = false;
            clock_hours = (clock_hours + 1) % 24;
        }
    
        // Next State Logic
        else if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = ADJUST_MINUTE;
        }
        
        delay_ms(200);
        break;
    
    case ADJUST_MINUTE:
        display_clock();
    
        if (button_set_pressed) {
            button_set_pressed = false;
            clock_minutes = (clock_minutes + 1) % 60;
        }
    
        // Next State Logic
        if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = ADJUST_SECOND;
        }
    
        delay_ms(200);
        break;
    
    case ADJUST_SECOND:
        display_clock();
    
        if (button_set_pressed) {
            button_set_pressed = false;
            clock_seconds = (clock_seconds + 1) % 60;
        }
    
        // Next State Logic
        if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = NORMAL;
        }
    
        delay_ms(200);
        break;
    
    case ALARM_HOUR:
        display_alarm();
        
    
        if (button_set_pressed) {
            button_set_pressed = false;
            alarm_hours = (alarm_hours + 1) % 24;
        }
    
        // Next State Logic
        else if (button_alarm_pressed) {
            button_alarm_pressed = false;
            current_state = NORMAL;
            alarm_enabled = false;
        }
        else if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = ALARM_MINUTE;
        }
    
        delay_ms(200);
        ms_elapsed += 200;
        increment_clock();
        break;
    
    case ALARM_MINUTE:
        display_alarm();
    
        if (button_set_pressed) {
            button_set_pressed = false;
            alarm_minutes = (alarm_minutes + 1) % 60;
        }
    
        // Next State Logic
        else if (button_alarm_pressed) {
            button_alarm_pressed = false;
            current_state = NORMAL;
            alarm_enabled = false;
        }
        else if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = ALARM_SECOND;
        }
    
        delay_ms(200);
        ms_elapsed += 200;
        increment_clock();
        break;
    
    case ALARM_SECOND:
        display_alarm();
    
        if (button_set_pressed) {
            button_set_pressed = false;
            alarm_seconds = (alarm_seconds + 1) % 60;
        }
    
        // Next State Logic
        else if (button_alarm_pressed) {
            button_alarm_pressed = false;
            current_state = NORMAL;
            alarm_enabled = false;
        }
        else if (button_sel_pressed) {
            button_sel_pressed = false;
            current_state = NORMAL;
            alarm_enabled = true;
        }
    
        delay_ms(200);
        ms_elapsed += 200;
        increment_clock();
        break;
  }
}

int main() {
  /* Initialize variables */
  clock_seconds = 0;
  clock_minutes = 0;
  clock_hours = 20;
  mode_24h = true;
  alarm_seconds = 3;
  alarm_minutes = 0;
  alarm_hours = 20;
  alarm_enabled = true;
  led_level = LOW;
  ms_elapsed = 0;
  button_set_pressed = false;
  button_fmt_pressed = false;
  button_sel_pressed = false;
  button_alarm_pressed = false;

  IntDisableWatchdog();

  /* Init modules */
  IntAINTCInit();
  gpioInitModule(GPIO0);
  gpioInitModule(GPIO1);
  gpioInitModule(GPIO2);
  gpioInitModule(GPIO3);
  timerInitModule();

  components_init();

  while (1) {
    finite_state_machine();
  }
}