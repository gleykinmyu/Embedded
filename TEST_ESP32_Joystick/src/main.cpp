#include <algorithm>
#include <cstdio>
#include <cstring>
#include "joystick_axis.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_eth.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

namespace {
constexpr adc_channel_t channels[]={ADC_CHANNEL_0,ADC_CHANNEL_3,ADC_CHANNEL_7}; // GPIO36,39,35
constexpr gpio_num_t button=GPIO_NUM_32;
struct Settings { uint16_t zero[3]={2048,2048,2048}; uint16_t dead=80; uint8_t curve=0; };
Settings settings;
joystick::Axis axes[3];
int raw[3]{}, values[3]{};
bool pressed=false;
SemaphoreHandle_t lock;
adc_oneshot_unit_handle_t adc;
httpd_handle_t server;
bool valid(const Settings& s){
    if(s.curve>2||s.dead>400)return false;
    for(auto z:s.zero)if(z<201||z>3894)return false;
    return true;
}
void apply(){for(int i=0;i<3;i++){axes[i].calibrate(settings.zero[i]);axes[i].deadzone(settings.dead);axes[i].exponent(static_cast<joystick::Curve>(settings.curve));values[i]=axes[i].compute(raw[i]);}}
bool save(const Settings& s){
    nvs_handle_t h;
    if(nvs_open("joystick",NVS_READWRITE,&h)!=ESP_OK)return false;
    auto e=nvs_set_blob(h,"settings",&s,sizeof(s));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);return e==ESP_OK;
}
void load(){nvs_handle_t h;if(nvs_open("joystick",NVS_READONLY,&h)==ESP_OK){Settings s;size_t n=sizeof(s);if(nvs_get_blob(h,"settings",&s,&n)==ESP_OK&&n==sizeof(s)&&valid(s))settings=s;nvs_close(h);}}
void sample_task(void*){
    bool candidate=false;unsigned stable=0;
    TickType_t last=xTaskGetTickCount();
    for(;;){
        xSemaphoreTake(lock,portMAX_DELAY);
        for(int i=0;i<3;i++){int v;if(adc_oneshot_read(adc,channels[i],&v)==ESP_OK){raw[i]=v;values[i]=axes[i].compute(v);}}
        bool b=!gpio_get_level(button);
        if(b!=candidate){candidate=b;stable=0;}else if(stable<2)++stable;
        if(stable>=2)pressed=candidate;
        xSemaphoreGive(lock);
        vTaskDelayUntil(&last,pdMS_TO_TICKS(10));
    }
}
esp_err_t state(httpd_req_t* r){
    char buf[256];xSemaphoreTake(lock,portMAX_DELAY);
    snprintf(buf,sizeof(buf),"{\"raw\":[%d,%d,%d],\"value\":[%d,%d,%d],\"center\":[%u,%u,%u],\"button\":%s,\"curve\":%u,\"dead\":%u}",raw[0],raw[1],raw[2],values[0],values[1],values[2],settings.zero[0],settings.zero[1],settings.zero[2],pressed?"true":"false",settings.curve,settings.dead);
    xSemaphoreGive(lock);httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,buf);
}
esp_err_t change(httpd_req_t* r){
    char query[80],c[8],d[8];unsigned cv,dv;char extra;
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK||httpd_query_key_value(query,"curve",c,sizeof(c))!=ESP_OK||httpd_query_key_value(query,"dead",d,sizeof(d))!=ESP_OK||sscanf(c,"%u%c",&cv,&extra)!=1||sscanf(d,"%u%c",&dv,&extra)!=1||cv>2||dv>400)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid settings");
    xSemaphoreTake(lock,portMAX_DELAY);Settings s=settings;s.curve=cv;s.dead=dv;bool ok=save(s);if(ok){settings=s;apply();}xSemaphoreGive(lock);
    return ok?state(r):httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"NVS write failed");
}
esp_err_t zero(httpd_req_t* r){
    xSemaphoreTake(lock,portMAX_DELAY);Settings s=settings;bool ok=true;
    for(int i=0;i<3;i++){int sum=0;for(int j=0;j<32;j++){int v;if(adc_oneshot_read(adc,channels[i],&v)!=ESP_OK){ok=false;break;}sum+=v;}s.zero[i]=sum/32;}
    if(!ok||!valid(s)){xSemaphoreGive(lock);return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid center");}
    ok=save(s);if(ok){settings=s;apply();}xSemaphoreGive(lock);
    return ok?state(r):httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"NVS write failed");
}
esp_err_t ota(httpd_req_t* r){
    const auto* p=esp_ota_get_next_update_partition(nullptr);
    if(!p||r->content_len==0||r->content_len>p->size)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid image size");
    esp_ota_handle_t h;
    if(esp_ota_begin(p,r->content_len,&h)!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"OTA begin failed");
    char buf[2048];size_t left=r->content_len;
    while(left){int n=httpd_req_recv(r,buf,std::min(left,sizeof(buf)));if(n<=0||esp_ota_write(h,buf,n)!=ESP_OK){esp_ota_abort(h);return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"OTA transfer failed");}left-=n;}
    if(esp_ota_end(h)!=ESP_OK||esp_ota_set_boot_partition(p)!=ESP_OK)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Image validation failed");
    httpd_resp_sendstr(r,"OK, rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));esp_restart();return ESP_OK;
}
const char page[]=R"HTML(<!doctype html><html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Joystick</title>
<style>body{font:16px system-ui;max-width:620px;margin:24px auto;padding:0 16px;background:#17202b;color:#eee}meter{width:100%;height:28px}canvas{width:100%;background:#263342}button,input,select{font:inherit;margin:8px 0}</style>
<h1>Joystick</h1><p id="status">Connecting</p><div id="axes"></div><p>Button: <b id="btn">—</b></p>
<select id="curve"><option value="0">Linear</option><option value="1">Square</option><option value="2">Cubic</option></select>
<label>Dead zone <input id="dead" type="number" min="0" max="400"></label><button id="save">Save</button><canvas id="graph" width="500" height="200"></canvas>
<button id="zero">Set current position as zero</button><hr><h2>OTA</h2><input id="file" type="file" accept=".bin"><button id="upload">Upload firmware</button><p id="ota"></p>
<script>
const $=x=>document.getElementById(x);$('axes').innerHTML=['X','Y','Z'].map((a,i)=>'<p>'+a+' <b id="v'+i+'"></b> <small id="r'+i+'"></small><meter id="m'+i+'" min="-32767" max="32767" value="0"></meter></p>').join('');
function graph(){let c=$('graph'),g=c.getContext('2d'),w=c.width,h=c.height;g.clearRect(0,0,w,h);g.strokeStyle='#8795a5';g.beginPath();g.moveTo(0,h/2);g.lineTo(w,h/2);g.moveTo(w/2,0);g.lineTo(w/2,h);g.stroke();g.strokeStyle='#69baff';g.beginPath();for(let i=0;i<=w;i++){let x=i/w*2-1,y=Math.sign(x)*Math.pow(Math.abs(x),1+Number($('curve').value));g.lineTo(i,(1-y)*h/2)}g.stroke()}
let init=false,busy=false;async function poll(){try{if(!busy){let r=await fetch('/state',{cache:'no-store'});if(!r.ok)throw Error();let s=await r.json();$('status').textContent='Connected';if(!init){$('curve').value=s.curve;$('dead').value=s.dead;init=true;graph()}s.value.forEach((v,i)=>{$('v'+i).textContent=v;$('m'+i).value=v;$('r'+i).textContent='ADC '+s.raw[i]+' / zero '+s.center[i]});$('btn').textContent=s.button?'Pressed':'Released'}}catch(e){$('status').textContent='Disconnected'}finally{setTimeout(poll,50)}}
async function post(url){try{let r=await fetch(url,{method:'POST'});if(!r.ok)throw Error(await r.text());init=false}catch(e){alert(e.message)}}
$('curve').onchange=graph;$('save').onclick=()=>post('/settings?curve='+$('curve').value+'&dead='+$('dead').value);$('zero').onclick=()=>post('/zero');
$('upload').onclick=async()=>{let f=$('file').files[0];if(!f||busy||!confirm('Update firmware?'))return;busy=true;$('ota').textContent='Uploading...';try{let r=await fetch('/ota',{method:'POST',body:f});$('ota').textContent=await r.text()}catch(e){$('ota').textContent='Connection lost; check device after reboot'}finally{busy=false;init=false}};poll();graph();
</script></html>)HTML";
esp_err_t index(httpd_req_t* r){httpd_resp_set_type(r,"text/html; charset=utf-8");return httpd_resp_sendstr(r,page);}
void http_start(){httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=8192;ESP_ERROR_CHECK(httpd_start(&server,&cfg));
    const httpd_uri_t routes[]={{"/",HTTP_GET,index,nullptr},{"/state",HTTP_GET,state,nullptr},{"/settings",HTTP_POST,change,nullptr},{"/zero",HTTP_POST,zero,nullptr},{"/ota",HTTP_POST,ota,nullptr}};
    for(auto& route:routes)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&route));
}
void network(){
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_16,GPIO_MODE_OUTPUT));gpio_set_level(GPIO_NUM_16,0);vTaskDelay(pdMS_TO_TICKS(100));gpio_set_level(GPIO_NUM_16,1);vTaskDelay(pdMS_TO_TICKS(200));
    esp_netif_config_t nc=ESP_NETIF_DEFAULT_ETH();auto* netif=esp_netif_new(&nc);
    eth_mac_config_t mc=ETH_MAC_DEFAULT_CONFIG();eth_phy_config_t pc=ETH_PHY_DEFAULT_CONFIG();pc.phy_addr=1;pc.reset_gpio_num=-1;
    eth_esp32_emac_config_t ec=ETH_ESP32_EMAC_DEFAULT_CONFIG();ec.smi_gpio.mdc_num=23;ec.smi_gpio.mdio_num=18;ec.clock_config.rmii.clock_mode=EMAC_CLK_EXT_IN;ec.clock_config.rmii.clock_gpio=EMAC_CLK_IN_GPIO;
    auto* mac=esp_eth_mac_new_esp32(&ec,&mc);auto* phy=esp_eth_phy_new_lan87xx(&pc);assert(mac&&phy&&netif);
    esp_eth_config_t cfg=ETH_DEFAULT_CONFIG(mac,phy);esp_eth_handle_t eth=nullptr;ESP_ERROR_CHECK(esp_eth_driver_install(&cfg,&eth));uint8_t addr[6];ESP_ERROR_CHECK(esp_read_mac(addr,ESP_MAC_ETH));ESP_ERROR_CHECK(esp_eth_ioctl(eth,ETH_CMD_S_MAC_ADDR,addr));ESP_ERROR_CHECK(esp_netif_attach(netif,esp_eth_new_netif_glue(eth)));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_ETH_GOT_IP,[](void*,esp_event_base_t,int32_t,void* data){auto* e=static_cast<ip_event_got_ip_t*>(data);ESP_LOGI("joystick","IP: " IPSTR,IP2STR(&e->ip_info.ip));},nullptr));
    ESP_ERROR_CHECK(esp_eth_start(eth));
}
}
extern "C" void app_main(){
    ESP_ERROR_CHECK(nvs_flash_init());load();lock=xSemaphoreCreateMutex();assert(lock);
    adc_oneshot_unit_init_cfg_t uc{};uc.unit_id=ADC_UNIT_1;ESP_ERROR_CHECK(adc_oneshot_new_unit(&uc,&adc));
    adc_oneshot_chan_cfg_t cc{};cc.atten=ADC_ATTEN_DB_12;cc.bitwidth=ADC_BITWIDTH_12;for(auto ch:channels)ESP_ERROR_CHECK(adc_oneshot_config_channel(adc,ch,&cc));
    gpio_config_t bc{};bc.pin_bit_mask=1ULL<<button;bc.mode=GPIO_MODE_INPUT;bc.pull_up_en=GPIO_PULLUP_ENABLE;ESP_ERROR_CHECK(gpio_config(&bc));
    apply();assert(xTaskCreate(sample_task,"sample",4096,nullptr,5,nullptr)==pdPASS);network();http_start();
}
