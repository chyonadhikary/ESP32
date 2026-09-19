#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "bangla_font.h"
#include "web_page.h"

#define TAG "BN_OLED"
#define I2C_PORT I2C_NUM_0
#define SDA_GPIO GPIO_NUM_21
#define SCL_GPIO GPIO_NUM_22
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_PAGES 8
#define MAX_LYRICS 64
#define FRAME_BYTES 1024
#define LYRIC_MAGIC 0x4c595243U

static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t oled_dev;

typedef struct { uint32_t start_ms, end_ms; uint8_t frame[FRAME_BYTES]; } runtime_lyric_t;
static runtime_lyric_t lyrics[MAX_LYRICS];
static uint16_t lyric_count;
static uint8_t fb[OLED_WIDTH * OLED_PAGES];
static char current_ip[16] = "192.168.4.1";
static httpd_handle_t server;
static volatile uint32_t playback_start_ms;
static volatile bool wifi_connected;

static esp_err_t oled_cmd(uint8_t cmd){uint8_t d[2]={0,cmd};return i2c_master_transmit(oled_dev,d,sizeof(d),100);}
static void oled_flush(void){for(int pg=0;pg<8;pg++){oled_cmd(0xB0|pg);oled_cmd(0);oled_cmd(0x10);uint8_t p[129];p[0]=0x40;memcpy(p+1,fb+pg*128,128);ESP_ERROR_CHECK(i2c_master_transmit(oled_dev,p,sizeof(p),100));}}
static void pixel(int x,int y,bool on){if(x<0||x>=128||y<0||y>=64)return;uint8_t *c=&fb[(y/8)*128+x],m=1<<(y&7);if(on)*c|=m;else*c&=~m;}
static esp_err_t oled_init(void){i2c_master_bus_config_t bus_cfg={.i2c_port=I2C_PORT,.sda_io_num=SDA_GPIO,.scl_io_num=SCL_GPIO,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg,&i2c_bus));i2c_device_config_t dev_cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=OLED_ADDR,.scl_speed_hz=400000};ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus,&dev_cfg,&oled_dev));uint8_t a[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0,0x40,0x8D,0x14,0x20,0,0xA1,0xC8,0xDA,0x12,0x81,0x8F,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};for(size_t i=0;i<sizeof(a);i++)ESP_ERROR_CHECK(oled_cmd(a[i]));return ESP_OK;}
static const uint8_t digits[11][5]={{0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x62,0x51,0x49,0x49,0x46},{0x22,0x49,0x49,0x49,0x36},{0x18,0x14,0x12,0x7f,0x10},{0x2f,0x49,0x49,0x49,0x31},{0x3e,0x49,0x49,0x49,0x32},{1,1,0x71,9,7},{0x36,0x49,0x49,0x49,0x36},{0x26,0x49,0x49,0x49,0x3e},{0,0x60,0x60,0,0}};
static void ascii_ip(void){memset(fb,0,sizeof(fb));int x=4;for(const char *s=current_ip;*s;s++){int n=(*s=='.')?10:*s-'0';for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(digits[n][col]&(1<<row))pixel(x+col,28+row,true);x+=6;}oled_flush();}
static void draw_frame(const uint8_t *f){memcpy(fb,f,FRAME_BYTES);oled_flush();}
static int hex_digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static void url_decode(char *s){char *r=s;for(char *p=s;*p;p++){if(*p=='%'&&hex_digit(p[1])>=0&&hex_digit(p[2])>=0){*r++=(char)((hex_digit(p[1])<<4)|hex_digit(p[2]));p+=2;}else{*r++=(*p=='+')?' ':*p;}}*r=0;}
static void wifi_save(const char *body){char ssid[33]={0},pass[65]={0};sscanf(body,"ssid=%32[^&]&password=%64s",ssid,pass);url_decode(ssid);url_decode(pass);nvs_handle_t h;if(nvs_open("wifi",NVS_READWRITE,&h)==ESP_OK){nvs_set_str(h,"ssid",ssid);nvs_set_str(h,"pass",pass);nvs_commit(h);nvs_close(h);}esp_wifi_set_config(WIFI_IF_STA,&(wifi_config_t){.sta={.ssid="",.password=""}});wifi_config_t c={0};strncpy((char*)c.sta.ssid,ssid,sizeof(c.sta.ssid)-1);strncpy((char*)c.sta.password,pass,sizeof(c.sta.password)-1);esp_wifi_set_config(WIFI_IF_STA,&c);esp_wifi_connect();}
static esp_err_t root(httpd_req_t *r){httpd_resp_set_type(r,"text/html; charset=utf-8");return httpd_resp_send(r,INDEX_HTML,HTTPD_RESP_USE_STRLEN);}
static esp_err_t status_get(httpd_req_t *r){char json[128];snprintf(json,sizeof(json),"{\"connected\":%s,\"ip\":\"%s\",\"ap\":\"192.168.4.1\"}",wifi_connected?"true":"false",current_ip);httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,json);}
static esp_err_t wifi_post(httpd_req_t *r){char b[128]={0};int n=httpd_req_recv(r,b,sizeof(b)-1);if(n<=0)return ESP_FAIL;b[n]=0;wifi_save(b);return httpd_resp_sendstr(r,"Wi‑Fi connection started. OLED-এ IP দেখুন; page আবার খুলুন।");}
static esp_err_t forget_post(httpd_req_t *r){nvs_handle_t h;if(nvs_open("wifi",NVS_READWRITE,&h)==ESP_OK){nvs_erase_all(h);nvs_commit(h);nvs_close(h);}esp_wifi_disconnect();return httpd_resp_sendstr(r,"Saved Wi‑Fi forgotten. ESP32 এখন setup hotspot-এ আছে।");}
static uint32_t u32(const uint8_t *p){return p[0]|p[1]<<8|p[2]<<16|p[3]<<24;}static uint16_t u16(const uint8_t *p){return p[0]|p[1]<<8;}
static esp_err_t lyrics_post(httpd_req_t *r){int total=r->content_len;if(total<8||total>8+MAX_LYRICS*1034)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"SRT payload too large or empty");uint8_t *b=malloc(total);if(!b)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"RAM unavailable");int got=0;while(got<total){int n=httpd_req_recv(r,(char*)b+got,total-got);if(n<=0){free(b);return ESP_FAIL;}got+=n;}if(u32(b)!=LYRIC_MAGIC){free(b);return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid lyrics data");}uint16_t count=u16(b+4);if(count>MAX_LYRICS||total<8+(int)count*1034){free(b);return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid lyric count");}int p=8;for(int i=0;i<count;i++){lyrics[i].start_ms=u32(b+p);lyrics[i].end_ms=u32(b+p+4);p+=10;memcpy(lyrics[i].frame,b+p,FRAME_BYTES);p+=FRAME_BYTES;}lyric_count=count;playback_start_ms=esp_log_timestamp();free(b);return httpd_resp_sendstr(r,"SRT converted. এখন গানটি 0:00 থেকে চালান; OLED timing শুরু হয়েছে।");}
static void web_start(void){httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.max_uri_handlers=10;c.max_open_sockets=4;httpd_start(&server,&c);httpd_uri_t a={"/",HTTP_GET,root,NULL};httpd_uri_t s={"/api/status",HTTP_GET,status_get,NULL};httpd_uri_t w={"/api/wifi",HTTP_POST,wifi_post,NULL};httpd_uri_t f={"/api/forget",HTTP_POST,forget_post,NULL};httpd_uri_t l={"/api/lyrics",HTTP_POST,lyrics_post,NULL};httpd_register_uri_handler(server,&a);httpd_register_uri_handler(server,&s);httpd_register_uri_handler(server,&w);httpd_register_uri_handler(server,&f);httpd_register_uri_handler(server,&l);}
static void event(void *a,esp_event_base_t b,int32_t id,void *d){if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_START)esp_wifi_connect();if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){wifi_connected=false;strncpy(current_ip,"192.168.4.1",sizeof(current_ip));}if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){ip_event_got_ip_t *e=d;wifi_connected=true;snprintf(current_ip,sizeof(current_ip),IPSTR,IP2STR(&e->ip_info.ip));ESP_LOGI(TAG,"Wi-Fi IP: %s",current_ip);ascii_ip();}}
static void wifi_start(void){ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_ap();esp_netif_create_default_wifi_sta();wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&c));esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,NULL);esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,NULL);wifi_config_t ap={.ap={.ssid="ESP32-Bangla-OLED",.ssid_len=17,.channel=1,.password="12345678",.max_connection=2,.authmode=WIFI_AUTH_WPA2_PSK}};wifi_config_t sta={0};nvs_handle_t h;if(nvs_open("wifi",NVS_READONLY,&h)==ESP_OK){size_t a1=sizeof(sta.sta.ssid),a2=sizeof(sta.sta.password);nvs_get_str(h,"ssid",(char*)sta.sta.ssid,&a1);nvs_get_str(h,"pass",(char*)sta.sta.password,&a2);nvs_close(h);}ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&sta));ESP_ERROR_CHECK(esp_wifi_start());}
void app_main(void){esp_err_t ne=nvs_flash_init();if(ne==ESP_ERR_NVS_NO_FREE_PAGES||ne==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());ESP_ERROR_CHECK(nvs_flash_init());}else ESP_ERROR_CHECK(ne);ESP_ERROR_CHECK(oled_init());wifi_start();web_start();ascii_ip();while(1){uint32_t elapsed=esp_log_timestamp()-playback_start_ms;int found=-1;for(int i=0;i<lyric_count;i++)if(elapsed>=lyrics[i].start_ms&&elapsed<lyrics[i].end_ms){found=i;break;}static int last=-2;if(found!=last){if(found>=0)draw_frame(lyrics[found].frame);else{memset(fb,0,sizeof(fb));oled_flush();}last=found;}vTaskDelay(pdMS_TO_TICKS(50));}}
