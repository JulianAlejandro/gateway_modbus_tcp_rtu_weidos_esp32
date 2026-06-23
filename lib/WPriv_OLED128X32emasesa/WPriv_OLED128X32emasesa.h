#ifndef WPRIV_OLED128X32emasesa_H
#define WPRIV_OLED128X32emasesa_H

#include <Arduino.h>
#include <U8g2lib.h>

//Icons:
//Weidmüller: weidmuller_logo_icon
//Battery: battery_charging_icon, battery_empty_icon, battery_low_icon, battery_medium_icon, battery_high_icon, battery_very_high_icon, battery_full_icon
//Connections: wifi_icon;ethernet_icon;nb_iot_icon;radio_signal_no;radio_signal_very_low;radio_signal_low;radio_signal_normal;radio_signal_high;radio_signal_very_high;
//Alarms: warning_icon;error_icon;info_icon;error_icon;
//Tank: tank_empty_icon; tank_very_low_icon;tank_low_icon;tank_medium_icon;tank_high_icon;tank_very_high_icon;tank_full_icon;
//PVSystem: pv_icon;load_thunder_icon;load_schucko_icon;
//Utils: check_icon, cancel_icon
//Monitor: temperature_icon, gauge_arrow_up_icon, gauge_arrow_down_icon; gauge_arrow_right_icon, gauge_arrow_left_icon, flow_left_icon, flow_right_icon 

#define VISU_SCREEN_SHOW_TIME 5000
#define VISU_SCREEN_TRANSITION_TIME 1000
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define ICON_WIDTH  24
#define ICON_HEIGHT 24
#define ICON_SPACING 4
#define OLED_TRANSITION_TIME 1500
#define OLED_VISU_TIME 5000

// weidmuller 24x24 icons gallery for OLED
extern const unsigned char water_icon[];
extern const unsigned char cloud_icon[];
extern const unsigned char messaging_icon[];
extern const unsigned char battery_charging_icon[];
extern const unsigned char battery_empty_icon[];
extern const unsigned char battery_low_icon[];
extern const unsigned char battery_medium_icon[];
extern const unsigned char battery_high_icon[];
extern const unsigned char battery_very_high_icon[];
extern const unsigned char battery_full_icon[];
extern const unsigned char wifi_icon[];
extern const unsigned char ethernet_icon[];
extern const unsigned char nb_iot_icon[];
extern const unsigned char radio_signal_no[];
extern const unsigned char radio_signal_very_low[];
extern const unsigned char radio_signal_low[];
extern const unsigned char radio_signal_normal[];
extern const unsigned char radio_signal_high[];
extern const unsigned char radio_signal_very_high[];
extern const unsigned char warning_icon[];
extern const unsigned char error_icon[];
extern const unsigned char info_icon[];
extern const unsigned char critical_icon[];
extern const unsigned char tank_empty_icon[];
extern const unsigned char tank_very_low_icon[];
extern const unsigned char tank_low_icon[] ;
extern const unsigned char tank_medium_icon[];
extern const unsigned char tank_high_icon[];
extern const unsigned char tank_very_high_icon[];
extern const unsigned char tank_full_icon[];
extern const unsigned char pv_icon[];
extern const unsigned char weidmuller_logo_icon[];
extern const unsigned char check_icon[];
extern const unsigned char cancel_icon[];
extern const unsigned char temperature_icon[];
extern const unsigned char gauge_arrow_up_icon[];
extern const unsigned char gauge_arrow_down_icon[];
extern const unsigned char gauge_arrow_right_icon[];
extern const unsigned char gauge_arrow_left_icon[];
extern const unsigned char flow_left_icon[];
extern const unsigned char flow_right_icon[];
extern const unsigned char load_thunder_icon[];
extern const unsigned char load_schucko_icon[];
extern const unsigned char settings_icon[];
extern const unsigned char emasesa_icon[];

void showWeidmullerApplication(U8G2 &u8g2Ref, const unsigned char* logo, const char* mainTitle, const char* subText); 
void hideWeidmullerApplication(U8G2 &u8g2Ref, const unsigned char* logo, const char* mainTitle, const char* subText);
void showLogo3LinesRight(U8G2 &u8g2Ref, const unsigned char* logo, const char* line1, const char* line2, const char* line3);
void showLogo3LinesLeft(U8G2 &u8g2Ref, const unsigned char* logo, const char* line1, const char* line2, const char* line3);
void showLogo3LinesRightNoTitle(U8G2 &u8g2Ref, const unsigned char* logo, const char* line1, const char* line2, const char* line3);
void showLogo3LinesLeftNoTitle(U8G2 &u8g2Ref, const unsigned char* logo, const char* line1, const char* line2, const char* line3); 
void show3LinesFull(U8G2 &u8g2Ref,const char* line1, const char* line2, const char* line3);
void show3LinesFullNoTitle(U8G2 &u8g2Ref,const char* line1, const char* line2, const char* line3);
void show2LinesFull(U8G2 &u8g2Ref,const char* line1, const char* line2);
void showLogo2LinesLeft(U8G2 &u8g2Ref, const unsigned char* logo,const char* line1, const char* line2);
void showLogo1LinesLeft(U8G2 &u8g2Ref, const unsigned char* logo,const char* line1);


#endif