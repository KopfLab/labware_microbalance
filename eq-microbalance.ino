#include "Display.h"

const char WEBHOOK[] = "cubes_eq";
Display lcd (0x27, 20, 4, 0); // 20x4 display, don't reserve anything for messages

// information format
#define LINE_DATE   1
#define LINE_USER   2
#define LINE_G      3
#define LINE_T      4
#define LINE_N      5
#define LAST_LINE   6

// message format defintions
const char MSG_START = '2'; // beginning of date is the message start - is there a better way to do this??
#define DATE_START          0
#define DATE_END            12
#define TIME_START          15
#define USER_START          10
#define WEIGHT_INDICATOR    5 // T, G, etc.
#define WEIGHT_NUMBER_START 6
#define WEIGHT_NUMBER_END   19
#define WEIGHT_UNIT_START   20

// message content
#define MAX_VAR_SIZE 20
char date [MAX_VAR_SIZE];
char the_time [MAX_VAR_SIZE]; // time is a keyword
char user_name [MAX_VAR_SIZE];
char units [MAX_VAR_SIZE];
char G_value [MAX_VAR_SIZE];
char T_value [MAX_VAR_SIZE];
char N_value [MAX_VAR_SIZE];

// message processing
#define ERROR_RESET_TIME 2000
const char READ_ERROR_MSG[] = "unknown data format";
bool read_error = false;
long last_error_time = millis();
bool in_message = false;
bool message_received = false;
char msg [50]; 
int linecounter = 0;
int charcounter = 0;

// lcd info
char read_lcd[21] = "";
char read_memory1[21] = "";
char read_memory2[21] = "";
char read_memory3[21] = "";
const char READY_MSG[] = "ready to rock!";

// setup
void setup() {
    Serial.begin(19200); // serial for debugging
    Serial1.begin(9800); // balance via TX/RX pins
    
    Serial.println("STARTUP"); // debug
    lcd.init(); // lcd
    lcd.print_line(1, "starting up...");
    Particle.publish(WEBHOOK, "{\"datetime\":\"\",\"user\":\"startup\",\"units\":\"\",\"G\":\"\",\"T\":\"\",\"N\":\"\"}"); // google spreadsheet
    delay(2000);
    lcd.print_line(1, READY_MSG);
}

// loop
void loop() {
  
    // reset error message
    if (read_error && millis() - last_error_time > ERROR_RESET_TIME) {
        lcd.print_line(1, READY_MSG);
        read_error = false;
    }
  
    // check for data from scale
    if(Serial1.available() && message_received == false) {
        while(Serial1.available()) {
        byte c = Serial1.read();
            
            if (c == 0) {
              // do nothing when 0 character encountered
            } else if (c >= 32 && c <= 126) {
              // regular ASCII range
              msg [charcounter] = (char) c;
              charcounter++;
            } else  if (c == 13) {
              // return character received --> end of line
              msg [charcounter+1] = 0;
              charcounter = 0;
              
              // try to figure out if this is the starting 
              if ( msg[0] == MSG_START ) {
                  Serial.println("START OF MESSAGE");
                  in_message = true;
                  lcd.print_line(1, "reading...");
              }
              
              // process depending on whether we're in message or not
              if (!in_message) {
                Serial.print("NOT part of message - line: "); //DEBUG
                Serial.println(msg); //DEBUG  
                lcd.print_line(1, READ_ERROR_MSG);
                last_error_time = millis();
                read_error = true;
              } else {
                // message
                linecounter++;
                Serial.print("PART of message - line #"); //DEBUG
                Serial.print(linecounter); //DEBUG
                Serial.print(": "); //DEBUG
                Serial.println(msg); //DEBUG
                read_error = false;
                  
                // parse each line with expected values
                switch (linecounter) {
                  case LINE_DATE:
                    lcd.print_line(1, "reading date...");
                    store_value(date, DATE_START, DATE_END);
                    store_value(the_time, TIME_START);
                    Serial.print("VALUE date time: "); //DEBUG
                    Serial.print(date); //DEBUG
                    Serial.print(" "); //DEBUG
                    Serial.println(the_time); //DEBUG
                    break;
                  case LINE_USER:
                    lcd.print_line(1, "reading user...");
                    store_value(user_name, USER_START);
                    Serial.print("VALUE user: "); //DEBUG
                    Serial.println(user_name); //DEBUG
                    break;
                  case LINE_G:
                    lcd.print_line(1, "reading gross...");
                    store_value(G_value, WEIGHT_NUMBER_START, WEIGHT_NUMBER_END);
                    store_value(units, WEIGHT_UNIT_START);
                    Serial.print("VALUE G ["); //DEBUG
                    Serial.print(units); //DEBUG
                    Serial.print("]: "); //DEBUG
                    Serial.println(G_value); //DEBUG
                    break;
                  case LINE_T:
                    lcd.print_line(1, "reading tare...");
                    store_value(T_value, WEIGHT_NUMBER_START, WEIGHT_NUMBER_END);
                    Serial.print("VALUE T ["); //DEBUG
                    Serial.print(units); //DEBUG
                    Serial.print("]: "); //DEBUG
                    Serial.println(T_value); //DEBUG
                    break;
                  case LINE_N:
                    lcd.print_line(1, "reading net...");
                    store_value(N_value, WEIGHT_NUMBER_START, WEIGHT_NUMBER_END);
                    Serial.print("VALUE N ["); //DEBUG
                    Serial.print(units); //DEBUG
                    Serial.print("]: "); //DEBUG
                    Serial.println(N_value); //DEBUG
                    break; 
                }
                  
                // end of message
                if (linecounter == LAST_LINE) {
                    linecounter = 0; 
                    message_received = true;
                }
              }
            
            } else {
              // unknown character (127=backspace, 27=esc, etc.)
              Serial.println();//DEBUG
              Serial.print("Unknown int: ");//DEBUG
              Serial.println(c);//DEBUG
            }
            delay(10);
        }
    }
    
    // save message if received
    if (message_received) {
        
        // send to GS
        if (send_to_gs()) {
            Serial.println("SUCCESSFULLY SENT");
            lcd.print_line(1, "saved to spreadsheet");
            update_screen("ok"); 
        } else {
            Serial.println("SEND FAILED"); 
            update_screen("err");
            lcd.print_line(1, "send to GS failed !!");
        }
        // wait 2 seconds until next ready
        delay(2000);
        
        // reset buffers and message trackers
        for (int i=0; i < sizeof(msg); i++) msg[i] = ' ';
        date[0] = 0;
        the_time[0] = 0;
        units[0] = 0;
        G_value[0] = 0;
        T_value[0] = 0;
        N_value[0] = 0;
        message_received = false;
        in_message = false;
    
        // try to empty serial buffer 
        // wait time from balance tends to be too long for this to work
        while(Serial1.available()) {
            byte b = Serial1.read();
            delay(10);
        }
        
        // ready again
        lcd.print_line(1, READY_MSG);
    }

}

/***************************/
/******** FUNCTIONS ********/
/***************************/

// value store functions
void store_value (char* target, const int start, const int stop) {  
    bool found_value = false;
    int first = start;
    int last = stop;
    for(int i = first; i <= last; i++){
        if ( !found_value && (byte) msg[i] == 32) {
            // found space at beginning of 
            first++;
        } else if (found_value && ( (byte) msg[i] == 32 || (byte) msg[i] == 0 ) ) {
            // found space at end of string
            last = i;
            break;
        } else {
            found_value = true;
        }
    }   
    
    // safety checks
    if (last < first) last = first;
    if (last - first >= MAX_VAR_SIZE) last = first + MAX_VAR_SIZE - 1; 

    // copy data
    strncpy(target, msg + first, last - first);
    target[last-first] = 0; // string termination

    // debug msgs
    Serial.print("FOUND string [");
    Serial.print(first);
    Serial.print("-");
    Serial.print(last);
    Serial.print("] --> '");
    Serial.print(target);
    Serial.println("'");
}

// value store function with stop (searches to end of message)
void store_value (char* target, const int start) {
    store_value(target, start, sizeof(msg));
}

// update lcd screen
void update_screen(const char* status) {
    
    // assemble lcd message
    snprintf(read_lcd, sizeof(read_lcd), "%s %s %s %s", status, the_time, N_value, units);
    
    strncpy(read_memory3, read_memory2, sizeof(read_memory3)-1);
    strncpy(read_memory2, read_memory1, sizeof(read_memory2)-1);
    strncpy(read_memory1, read_lcd, sizeof(read_memory1)-1);
    
    Serial.print("LCD line 2:"); // DEBUG
    Serial.println(read_memory1); // DEBUG
    lcd.print_line(2, read_memory1);
    
    Serial.print("LCD line 3:"); // DEBUG
    Serial.println(read_memory2); // DEBUG
    lcd.print_line(3, read_memory2);
    
    Serial.print("LCD line 4:"); // DEBUG
    Serial.println(read_memory3); // DEBUG
    lcd.print_line(4, read_memory3);
}

// send to google
char json_buffer[255];
bool send_to_gs () {
  snprintf(json_buffer, sizeof(json_buffer),
    "{\"datetime\":\"%s %s\",\"user\":\"%s\",\"units\":\"%s\",\"G\":\"%s\",\"T\":\"%s\",\"N\":\"%s\"}",
    date, the_time, user_name, units, G_value, T_value, N_value);
  Serial.print("JSON to GS: "); // DEBUG
  Serial.println(json_buffer); // DEBUG
  return(Particle.publish(WEBHOOK, json_buffer));
}

