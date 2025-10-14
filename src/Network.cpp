/*********************************************************************************
 *  MIT License
 *  
 *  Copyright (c) 2020-2025 Gregg E. Berman
 *  
 *  https://github.com/HomeSpan/HomeSpan
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, in      responseBody+="<div class=\"status success\">"
                    "<p>🎉 <b>WiFi 配网成功！</b></p>"
                    "<p>📶 已连接到网络：<b>" + String(wifiData.ssid) + "</b></p>"
                    "<p>🏠 HomeKit 设备已准备就绪</p>"
                    "<p>📱 您现在可以在 iPhone \"家庭\" App 中添加此设备</p>"
                    "</div>"
                    "<div class=\"status info\">"
                    "<p>🔄 设备将在 3 秒后自动重启...</p>"
                    "<p>📱 重启后即可进行 HomeKit 配对</p>"
                    "</div>"limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *  
 ********************************************************************************/

#include "version.h" 
 
#include <DNSServer.h>

#include "Network_HS.h"
#include "HomeSpan.h"
#include "Utils.h"

using namespace Utils;

///////////////////////////////

void Network_HS::scan(){

  WiFi.scanDelete();
  STATUS_UPDATE(start(LED_WIFI_SCANNING),HS_WIFI_SCANNING)
  int n=WiFi.scanNetworks();

  free(ssidList);
  ssidList=(char **)HS_CALLOC(n,sizeof(char *));
  numSSID=0;

  for(int i=0;i<n;i++){
    boolean found=false;
    for(int j=0;j<numSSID;j++){
      if(!strcmp(WiFi.SSID(i).c_str(),ssidList[j]))
        found=true;
    }
    if(!found){
      ssidList[numSSID]=(char *)HS_CALLOC(WiFi.SSID(i).length()+1,sizeof(char));
      sprintf(ssidList[numSSID],"%s",WiFi.SSID(i).c_str());
      numSSID++;
    }
  }

}

///////////////////////////////

void Network_HS::serialConfigure(){

  wifiData.ssid[0]='\0';
  wifiData.pwd[0]='\0';

  LOG0("*** WiFi Setup - Scanning for Networks...\n\n");
  
  scan();         // scan for networks    

  for(int i=0;i<numSSID;i++)
    LOG0("  %d) %s\n",i+1,ssidList[i]);
         
  while(!strlen(wifiData.ssid)){
    LOG0("\n>>> WiFi SSID: ");
    readSerial(wifiData.ssid,MAX_SSID);
    if(atoi(wifiData.ssid)>0 && atoi(wifiData.ssid)<=numSSID){
      strcpy(wifiData.ssid,ssidList[atoi(wifiData.ssid)-1]);
    }
    LOG0("%s\n",wifiData.ssid);
  }
  
  while(!strlen(wifiData.pwd)){
    LOG0(">>> WiFi PASS: ");
    readSerial(wifiData.pwd,MAX_PWD);    
    LOG0("%s\n",mask(wifiData.pwd,2).c_str());
  }

  return;
}

///////////////////////////////

boolean Network_HS::allowedCode(char *s){
  return(
    strcmp(s,"00000000") && strcmp(s,"11111111") && strcmp(s,"22222222") && strcmp(s,"33333333") && 
    strcmp(s,"44444444") && strcmp(s,"55555555") && strcmp(s,"66666666") && strcmp(s,"77777777") &&
    strcmp(s,"88888888") && strcmp(s,"99999999") && strcmp(s,"12345678") && strcmp(s,"87654321"));
}

///////////////////////////////

void Network_HS::apConfigure(){

  LOG0("*** Starting Access Point: %s / %s\n",apSSID,apPassword);
          
  LOG0("\nScanning for Networks...\n\n");
  
  scan();                   // scan for networks    

  for(int i=0;i<numSSID;i++)
    LOG0("  %d) %s\n",i+1,ssidList[i]);

  STATUS_UPDATE(start(LED_AP_STARTED),HS_AP_STARTED)    

  NetworkServer apServer(80);
  client=0;
    
  const byte DNS_PORT = 53;
  DNSServer dnsServer;
  IPAddress apIP(192, 168, 4, 1);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID,apPassword);             // start access point
  dnsServer.start(DNS_PORT, "*", apIP);       // start DNS server that resolves every request to the address of this device
  apServer.begin();

  alarmTimeOut=millis()+lifetime;            // Access Point will shut down when alarmTimeOut is reached
  apStatus=0;                                // status will be "timed out" unless changed

  LOG0("\nReady.\n");

  while(1){                                  // loop until we get timed out (which will be accelerated if save/cancel selected)

    if(homeSpan.controlButton && homeSpan.controlButton->triggered(9999,3000)){
      LOG0("\n*** Access Point Terminated.  Restarting...\n\n");
      STATUS_UPDATE(start(LED_ALERT),HS_AP_TERMINATED)
      homeSpan.controlButton->wait();
      homeSpan.reboot();
    }

    if(millis()>alarmTimeOut){
      WiFi.softAPdisconnect(true);           // terminate connections and shut down captive access point
      delay(100);
      if(apStatus==1){
        LOG0("\n*** Access Point: Exiting and Saving Settings\n\n");
        return;
      } else {
        if(apStatus==0)
          LOG0("\n*** Access Point: Timed Out (%ld seconds).",lifetime/1000);
        else 
          LOG0("\n*** Access Point: Configuration Cancelled.");
        LOG0("  Restarting...\n\n");
        STATUS_UPDATE(start(LED_ALERT),HS_AP_TERMINATED)
        homeSpan.reboot();
      }
    }

    dnsServer.processNextRequest();

    if((client=apServer.accept())){                       // found a new HTTP client
      LOG2("=======================================\n");
      LOG1("** Access Point Client Connected: (");
      LOG1(millis()/1000);
      LOG1(" sec) ");
      LOG1(client.remoteIP());
      LOG1("\n");
      LOG2("\n");
      delay(50);                                        // pause to allow data buffer to begin to populate
    }
    
    if(client && client.available()){                   // if connection exists and data is available

      LOG2("<<<<<<<<< ");
      LOG2(client.remoteIP());
      LOG2(" <<<<<<<<<\n");

      int messageSize=client.available();        

      if(messageSize>MAX_HTTP){            // exceeded maximum number of bytes allowed
        badRequestError();
        LOG0("\n*** ERROR:  HTTP message of %d bytes exceeds maximum allowed (%d)\n\n",messageSize,MAX_HTTP);
        continue;
      } 

      TempBuffer<uint8_t> httpBuf(messageSize+1);      // leave room for null character added below
    
      int nBytes=client.read(httpBuf,messageSize);       // read all available bytes up to maximum allowed+1
      
      if(nBytes!=messageSize || client.available()!=0){
        badRequestError();
        LOG0("\n*** ERROR:  HTTP message not read correctly.  Expected %d bytes, read %d bytes, %d bytes remaining\n\n",messageSize,nBytes,client.available());
        continue;
      }
    
      httpBuf[nBytes]='\0';                       // add null character to enable string functions    
      char *body=(char *)httpBuf.get();                 // char pointer to start of HTTP Body
      char *p;                                          // char pointer used for searches
      
      if(!(p=strstr((char *)httpBuf.get(),"\r\n\r\n"))){
        badRequestError();
        LOG0("\n*** ERROR:  Malformed HTTP request (can't find blank line indicating end of BODY)\n\n");
        continue;      
      }

      *p='\0';                                          // null-terminate end of HTTP Body to faciliate additional string processing
      uint8_t *content=(uint8_t *)p+4;                  // byte pointer to start of optional HTTP Content
      int cLen=0;                                       // length of optional HTTP Content

      if((p=strstr(body,"Content-Length: ")))           // Content-Length is specified
        cLen=atoi(p+16);
      if(nBytes!=strlen(body)+4+cLen){
        badRequestError();
        LOG0("\n*** ERROR:  Malformed HTTP request (Content-Length plus Body Length does not equal total number of bytes read)\n\n");
        continue;        
      }

      LOG2(body);
      LOG2("\n------------ END BODY! ------------\n");

      content[cLen]='\0';                               // add a trailing null on end of any contents, which should always be text-based

      processRequest(body, (char *)content);            // process request
      
      LOG2("\n");

    } // process Client

    homeSpan.resetWatchdog();
  } // while 1

}

///////////////////////////////

void Network_HS::processRequest(char *body, char *formData){
  
  String responseHead="HTTP/1.1 200 OK\r\nContent-type: text/html\r\n";
  
  String responseBody="<html><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\"><head><title>HomeSpan 配网设置</title><style>"
                        "* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }"
                        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); margin: 0; padding: 10px; min-height: 100vh; overflow-x: hidden; }"
                        ".container { max-width: 500px; width: 100%; margin: 0 auto; background: rgba(255, 255, 255, 0.95); border-radius: 15px; padding: 20px; box-shadow: 0 10px 30px rgba(0,0,0,0.3); backdrop-filter: blur(10px); }"
                        ".header { text-align: center; margin-bottom: 25px; }"
                        ".header h1 { color: #4a5568; font-size: clamp(1.8rem, 5vw, 2.5rem); margin: 0; text-shadow: 2px 2px 4px rgba(0,0,0,0.1); line-height: 1.2; }"
                        ".header .subtitle { color: #718096; font-size: clamp(0.9rem, 3vw, 1.1rem); margin-top: 8px; }"
                        "p { font-size: clamp(1rem, 3.5vw, 1.2rem); margin: 12px 0; color: #2d3748; line-height: 1.5; }"
                        "label { display: block; font-size: clamp(1.1rem, 4vw, 1.3rem); margin: 18px 0 8px 0; color: #2d3748; font-weight: 600; position: relative; }"
                        ".input-wrapper { position: relative; margin-bottom: 15px; }"
                        ".input-icon { position: absolute; left: 12px; top: 50%; transform: translateY(-50%); font-size: clamp(1rem, 3.5vw, 1.2rem); color: #a0aec0; z-index: 1; pointer-events: none; }"
                        "input[type='text'], input[type='password'], input[type='tel'] { width: 100%; padding: 16px 12px 16px 40px; font-size: clamp(1rem, 4vw, 1.2rem); border: 2px solid #e2e8f0; border-radius: 12px; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); background: #ffffff; position: relative; z-index: 0; -webkit-appearance: none; appearance: none; }"
                        "input[type='text']:focus, input[type='password']:focus, input[type='tel']:focus { border-color: #667eea; outline: none; box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1), 0 4px 12px rgba(102, 126, 234, 0.15); transform: translateY(-1px); }"
                        "input[type='text']:focus + .input-icon, input[type='password']:focus + .input-icon, input[type='tel']:focus + .input-icon { color: #667eea; }"
                        "input[type='text']::placeholder, input[type='password']::placeholder, input[type='tel']::placeholder { color: #a0aec0; font-style: italic; }"
                        "input[type='text']:valid, input[type='password']:valid, input[type='tel']:valid { border-color: #48bb78; }"
                        "input[type='text']:invalid:not(:placeholder-shown), input[type='password']:invalid:not(:placeholder-shown), input[type='tel']:invalid:not(:placeholder-shown) { border-color: #f56565; }"
                        ".input-hint { font-size: clamp(0.8rem, 2.5vw, 0.9rem); color: #718096; margin-top: 5px; display: flex; align-items: center; line-height: 1.4; }"
                        ".input-hint.error { color: #e53e3e; }"
                        ".input-hint.success { color: #38a169; }"
                        "datalist { background: white; border: 1px solid #e2e8f0; border-radius: 8px; }"
                        "input[type='submit'], button { width: 100%; background: linear-gradient(45deg, #667eea, #764ba2); color: white; border: none; padding: 16px 20px; font-size: clamp(1.1rem, 4vw, 1.3rem); border-radius: 12px; cursor: pointer; margin: 12px 0; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); font-weight: 600; position: relative; overflow: hidden; touch-action: manipulation; -webkit-appearance: none; }"
                        "input[type='submit']:hover, button:hover { transform: translateY(-2px); box-shadow: 0 8px 25px rgba(102, 126, 234, 0.4); }"
                        "input[type='submit']:active, button:active { transform: translateY(0px); transition: transform 0.1s; }"
                        "input[type='submit']::before, button::before { content: ''; position: absolute; top: 0; left: -100%; width: 100%; height: 100%; background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent); transition: left 0.5s; }"
                        "input[type='submit']:hover::before, button:hover::before { left: 100%; }"
                        ".cancel-btn { background: linear-gradient(45deg, #f56565, #e53e3e) !important; margin-top: 8px; }"
                        ".status { text-align: center; padding: 18px; border-radius: 12px; margin: 18px 0; }"
                        ".success { background: linear-gradient(135deg, #c6f6d5, #9ae6b4); color: #22543d; border: 2px solid #68d391; }"
                        ".warning { background: linear-gradient(135deg, #fed7d7, #fc8181); color: #742a2a; border: 2px solid #f56565; }"
                        ".info { background: linear-gradient(135deg, #bee3f8, #90cdf4); color: #2a4365; border: 2px solid #63b3ed; }"
                        ".loading { background: linear-gradient(135deg, #e2e8f0, #cbd5e0); color: #4a5568; border: 2px solid #a0aec0; }"
                        ".form-group { margin-bottom: 20px; }"
                        ".led-indicator { display: inline-block; width: 10px; height: 10px; border-radius: 50%; background: #48bb78; margin-right: 6px; animation: blink 1.5s infinite; box-shadow: 0 0 8px rgba(72, 187, 120, 0.5); }"
                        "@keyframes blink { 0%, 50% { opacity: 1; box-shadow: 0 0 8px rgba(72, 187, 120, 0.5); } 51%, 100% { opacity: 0.3; box-shadow: 0 0 4px rgba(72, 187, 120, 0.2); } }"
                        ".progress { width: 100%; height: 6px; background: #e2e8f0; border-radius: 3px; overflow: hidden; margin: 15px 0; box-shadow: inset 0 1px 2px rgba(0,0,0,0.1); }"
                        ".progress-bar { height: 100%; background: linear-gradient(45deg, #667eea, #764ba2); animation: progress 2s infinite; border-radius: 3px; }"
                        "@keyframes progress { 0% { width: 0%; } 100% { width: 100%; } }"
                        ".wifi-strength { display: inline-block; margin-left: 6px; }"
                        ".strength-bar { display: inline-block; width: 2px; height: 8px; background: #a0aec0; margin: 0 1px; border-radius: 1px; }"
                        ".strength-bar.active { background: #48bb78; }"
                        ".wifi-list { max-height: 200px; overflow-y: auto; border: 1px solid #e2e8f0; border-radius: 8px; background: white; margin-top: 5px; display: none; position: relative; z-index: 10; }"
                        ".wifi-item { padding: 12px 15px; border-bottom: 1px solid #f7fafc; cursor: pointer; display: flex; justify-content: space-between; align-items: center; transition: background 0.2s; }"
                        ".wifi-item:last-child { border-bottom: none; }"
                        ".wifi-item:hover { background: #f7fafc; }"
                        ".wifi-item.selected { background: #ebf8ff; color: #2b6cb0; }"
                        ".wifi-name { font-weight: 500; }"
                        ".wifi-security { font-size: 0.8em; color: #718096; margin-left: 8px; }"
                        ".manual-input-toggle { color: #667eea; cursor: pointer; text-decoration: underline; font-size: 0.9em; margin-top: 8px; }"
                        ".network-search { position: relative; }"
                        ".search-icon { position: absolute; right: 12px; top: 50%; transform: translateY(-50%); color: #a0aec0; cursor: pointer; }"
                        "input[type='text'].manual-mode { border-color: #f56565; }"
                        ".input-mode-indicator { font-size: 0.8em; color: #667eea; margin-left: 5px; }"
                        "@media (max-width: 480px) {"
                        "  body { padding: 8px; }"
                        "  .container { padding: 16px; border-radius: 12px; }"
                        "  .header { margin-bottom: 20px; }"
                        "  .header h1 { font-size: 1.8rem; }"
                        "  .header .subtitle { font-size: 0.95rem; }"
                        "  p { font-size: 1rem; margin: 10px 0; }"
                        "  label { font-size: 1.1rem; margin: 15px 0 6px 0; }"
                        "  input[type='text'], input[type='password'], input[type='tel'] { padding: 14px 10px 14px 36px; font-size: 1rem; }"
                        "  .input-icon { left: 10px; font-size: 1rem; }"
                        "  input[type='submit'], button { padding: 14px 16px; font-size: 1.1rem; margin: 10px 0; }"
                        "  .status { padding: 15px; margin: 15px 0; }"
                        "  .form-group { margin-bottom: 18px; }"
                        "  .input-hint { font-size: 0.85rem; }"
                        "}"
                        "@media (max-width: 320px) {"
                        "  body { padding: 5px; }"
                        "  .container { padding: 12px; }"
                        "  .header h1 { font-size: 1.6rem; }"
                        "  input[type='text'], input[type='password'], input[type='tel'] { padding: 12px 8px 12px 32px; }"
                        "  .input-icon { left: 8px; }"
                        "  input[type='submit'], button { padding: 12px 14px; }"
                        "}"
                        "@media (orientation: landscape) and (max-height: 500px) {"
                        "  body { padding: 5px; }"
                        "  .container { padding: 15px; }"
                        "  .header { margin-bottom: 15px; }"
                        "  .header h1 { font-size: 1.5rem; }"
                        "  .form-group { margin-bottom: 15px; }"
                        "  .status { padding: 12px; margin: 12px 0; }"
                        "}"
                      "</style></head>"
                      "<body><div class=\"container\"><div class=\"header\"><h1>🏠 HomeSpan 配网</h1><div class=\"subtitle\">智能家居设备配置</div></div>";

  if(!strncmp(body,"POST /configure ",16) &&                              // POST CONFIGURE
     strstr(body,"Content-Type: application/x-www-form-urlencoded")){     // check that content is from a form

    LOG2(formData);                                                       // print form data
    LOG2("\n------------ END DATA! ------------\n");
               
    LOG1("In Post Configure...\n");

    getFormValue(formData,"network",wifiData.ssid,MAX_SSID);
    getFormValue(formData,"pwd",wifiData.pwd,MAX_PWD);

    STATUS_UPDATE(start(LED_WIFI_CONNECTING),HS_WIFI_CONNECTING)
        
    responseBody+="<meta http-equiv = \"refresh\" content = \"" + String(homeSpan.wifiTimeCounter/1000) + "; url = /wifi-status\" />"
                  "<div class=\"status info\"><div class=\"progress\"><div class=\"progress-bar\"></div></div>"
                  "<p>🔗 正在连接 WiFi 网络：</p><p><b>" + String(wifiData.ssid) + "</b></p>"
                  "<p>⏱️ 等待连接响应中... (" + String((homeSpan.wifiTimeCounter++)/1000) + " 秒)</p></div>";
                  
    WiFi.begin(wifiData.ssid,wifiData.pwd);              
  
  } else {

  if(!strncmp(body,"GET /auto-complete ",19)){                           // GET AUTO-COMPLETE
    responseBody+="<div class=\"status success\">"
                  "<p>✅ <b>配网已完成！</b></p>"
                  "<p>🔄 正在重启 HomeSpan 设备...</p>"
                  "<p>📱 请关闭此窗口</p>"
                  "</div>";
    alarmTimeOut=millis()+2000;
    apStatus=1;
    
  } else {

  if(!strncmp(body,"POST /save ",11)){                                    // GET SAVE
    getFormValue(formData,"code",setupCode,8);

    if(allowedCode(setupCode)){
      responseBody+="<div class=\"status success\"><p>✅ <b>设置已保存！</b></p><p>🔄 正在重启 HomeSpan 设备...</p><p>📱 请关闭此窗口</p></div>";
      alarmTimeOut=millis()+2000;
      apStatus=1;
      
    } else {
    responseBody+="<meta http-equiv = \"refresh\" content = \"4; url = /wifi-status\" />"
                  "<div class=\"status warning\"><p>❌ <b>设置代码不符合要求 - 过于简单！</b></p><p>🔄 正在返回配置页面...</p></div>";      
    }
    
  } else {

  if(!strncmp(body,"GET /cancel ",12)){                                   // GET CANCEL
    responseBody+="<div class=\"status warning\"><p>❌ <b>配置已取消！</b></p><p>🔄 正在重启 HomeSpan 设备...</p><p>📱 请关闭此窗口</p></div>";
    alarmTimeOut=millis()+2000;
    apStatus=-1;
    
  } else {

  if(!strncmp(body,"GET /wifi-status ",17)){                              // GET WIFI-STATUS

    LOG1("In Get WiFi Status...\n");

    if(WiFi.status()!=WL_CONNECTED){
      responseHead+="Refresh: " + String(homeSpan.wifiTimeCounter/1000) + "\r\n";     
      responseBody+="<div class=\"status loading\"><div class=\"progress\"><div class=\"progress-bar\"></div></div>"
                    "<p>🔄 重新尝试连接到：</p><p><b>" + String(wifiData.ssid) + "</b></p>"
                    "<p>⏱️ 等待连接响应中... (" + String((homeSpan.wifiTimeCounter++)/1000) + " 秒)</p>"
                    "<p>⏰ 配网超时倒计时：" + String((alarmTimeOut-millis())/1000) + " 秒</p>"
                    "<button class=\"cancel-btn\" onclick=\"document.location='/hotspot-detect.html'\">取消配网</button></div>";
      WiFi.begin(wifiData.ssid,wifiData.pwd);
      
    } else {

      STATUS_UPDATE(start(LED_AP_CONNECTED),HS_AP_CONNECTED)
          
      responseBody+="<div class=\"status success\">"
                    "<p>🎉 <b>WiFi 配网成功！</b></p>"
                    "<p>📶 已连接到网络：<b>" + String(wifiData.ssid) + "</b></p>"
                    "<p>🏠 HomeKit 设备已准备就绪</p>"
                    "<p>📱 您现在可以在 iPhone \"家庭\" App 中添加此设备</p>"
                    "</div>"
                    "<div class=\"status info\">"
                    "<p> 设备将在 3 秒后自动重启...</p>"
                    "<p>📱 重启后即可进行 HomeKit 配对</p>"
                    "</div>"
                    "<script>"
                    "setTimeout(function() {"
                    "  window.location.href = '/auto-complete';"
                    "}, 3000);"
                    "</script>";
    }
  
  } else {

  if(!strncmp(body,"GET /homespan-landing ",22)){
    LOG1("In Landing Page...\n");

    STATUS_UPDATE(start(LED_AP_CONNECTED),HS_AP_CONNECTED)
    homeSpan.wifiTimeCounter.reset();

    responseBody+="<div class=\"status info\">"
                  "<p>🎯 <b>欢迎使用 HomeSpan！</b></p>"
                  "<p>📡 此页面用于配置您的 HomeSpan 设备连接到 WiFi 网络。</p>"
                  "<p><span class=\"led-indicator\"></span>配置过程中，设备指示灯应该呈现<em>双闪烁</em>状态。</p>"
                  "</div>";
                  
    responseBody+="<form action=\"/configure\" method=\"post\">"
                  "<div class=\"form-group\">"
                  "<label for=\"network\">📶 选择 WiFi 网络：</label>"
                  "<div class=\"network-search\">"
                  "<div class=\"input-wrapper\">"
                  "<span class=\"input-icon\">📡</span>"
                  "<input type=\"text\" name=\"network\" id=\"network\" placeholder=\"点击选择网络或手动输入\" required maxlength=" + String(MAX_SSID) + " autocomplete=\"off\" onclick=\"toggleWifiList()\" oninput=\"filterWifiList()\">"
                  "<span class=\"search-icon\" onclick=\"toggleManualMode()\">✏️</span>"
                  "</div>"
                  "<div class=\"wifi-list\" id=\"wifiList\">";

    for(int i=0;i<numSSID;i++) {
        responseBody+="<div class=\"wifi-item\" onclick=\"selectWifi('" + String(ssidList[i]) + "')\">";
        responseBody+="<div><span class=\"wifi-name\">" + String(ssidList[i]) + "</span>";
        // 这里可以添加安全类型显示，暂时先显示基本信息
        responseBody+="<span class=\"wifi-security\">🔒 加密</span></div>";
        responseBody+="<div class=\"wifi-strength\">";
        // 简单的信号强度显示（可以根据实际RSSI值调整）
        for(int j=0; j<4; j++) {
            responseBody+="<span class=\"strength-bar active\"></span>";
        }
        responseBody+="</div></div>";
    }
    
    responseBody+="</div>"
                  "<div class=\"input-hint\">"
                  "💡 点击上方输入框显示可用网络列表，或 "
                  "<span class=\"manual-input-toggle\" onclick=\"toggleManualMode()\">手动输入网络名称</span>"
                  "<span class=\"input-mode-indicator\" id=\"modeIndicator\">（列表模式）</span>"
                  "</div>"
                  "</div>"
                  "<div class=\"form-group\">"
                  "<label for=\"pwd\">🔐 WiFi 密码：</label>"
                  "<div class=\"input-wrapper\">"
                  "<span class=\"input-icon\">🔑</span>"
                  "<input type=\"password\" id=\"pwd\" name=\"pwd\" placeholder=\"请输入 WiFi 密码\" required maxlength=" + String(MAX_PWD) + ">"
                  "</div>"
                  "<div class=\"input-hint\">🔒 密码将使用加密方式安全存储在设备中</div>"
                  "</div>"
                  
                  "<script>"
                  "let isManualMode = false;"
                  "let wifiListVisible = false;"
                  
                  "function toggleWifiList() {"
                  "  if (!isManualMode) {"
                  "    const list = document.getElementById('wifiList');"
                  "    wifiListVisible = !wifiListVisible;"
                  "    list.style.display = wifiListVisible ? 'block' : 'none';"
                  "  }"
                  "}"
                  
                  "function selectWifi(ssid) {"
                  "  document.getElementById('network').value = ssid;"
                  "  document.getElementById('wifiList').style.display = 'none';"
                  "  wifiListVisible = false;"
                  "  document.getElementById('pwd').focus();"
                  "}"
                  
                  "function toggleManualMode() {"
                  "  isManualMode = !isManualMode;"
                  "  const input = document.getElementById('network');"
                  "  const indicator = document.getElementById('modeIndicator');"
                  "  const list = document.getElementById('wifiList');"
                  "  "
                  "  if (isManualMode) {"
                  "    input.placeholder = '手动输入网络名称（SSID）';"
                  "    input.className = 'manual-mode';"
                  "    indicator.textContent = '（手动模式）';"
                  "    list.style.display = 'none';"
                  "    wifiListVisible = false;"
                  "  } else {"
                  "    input.placeholder = '点击选择网络或手动输入';"
                  "    input.className = '';"
                  "    indicator.textContent = '（列表模式）';"
                  "  }"
                  "}"
                  
                  "function filterWifiList() {"
                  "  if (!isManualMode) {"
                  "    const input = document.getElementById('network');"
                  "    const filter = input.value.toLowerCase();"
                  "    const items = document.querySelectorAll('.wifi-item');"
                  "    "
                  "    items.forEach(item => {"
                  "      const name = item.querySelector('.wifi-name').textContent.toLowerCase();"
                  "      item.style.display = name.includes(filter) ? 'flex' : 'none';"
                  "    });"
                  "    "
                  "    if (filter.length > 0) {"
                  "      document.getElementById('wifiList').style.display = 'block';"
                  "      wifiListVisible = true;"
                  "    }"
                  "  }"
                  "}"
                  
                  "// 点击页面其他地方时隐藏WiFi列表"
                  "document.addEventListener('click', function(e) {"
                  "  if (!e.target.closest('.network-search')) {"
                  "    document.getElementById('wifiList').style.display = 'none';"
                  "    wifiListVisible = false;"
                  "  }"
                  "});"
                  "</script>";
                  
    responseBody+="<input type=\"submit\" value=\"🚀 开始配网\">"
                  "</form>";

    responseBody+="<button class=\"cancel-btn\" onclick=\"document.location='/cancel'\">❌ 取消配置</button>";
                  
  } else {
  
  if(!strstr(body,"wispr")){
    responseHead="HTTP/1.1 302 Found\r\nLocation: /homespan-landing\r\n";    
  }

  } // end else - wispr

  } // end else - homespan-landing

  } // end else - wifi-status

  } // end else - cancel

  } // end else - save

  } // end else - auto-complete

  responseHead+="\r\n";               // add blank line between reponse header and body
  responseBody+="</div></body></html>";     // close out body and html tags

  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(responseHead);
  LOG2(responseBody);
  LOG2("\n");
  client.print(responseHead);
  client.print(responseBody);
  LOG2("------------ SENT! --------------\n");
    
} // processRequest

//////////////////////////////////////

int Network_HS::getFormValue(const char *formData, const char *tag, char *value, int maxSize){

  char *s=strcasestr(formData,tag); // find start of tag
  
  if(!s)                            // if not found, return -1
    return(-1);

  char *v=index(s,'=');             // find '='

  if(!v)                            // if not found, return -1 (this should not happen)
    return(-1);

  v++;                              // point to beginning of value 
  int len=0;                        // track length of value
  
  while(*v!='\0' && *v!='&' && len<maxSize){      // copy the value until null, '&', or maxSize is reached
    if(*v=='%'){                                  // this is an escaped character of form %XX
      v++;
      sscanf(v,"%2x",(unsigned int *)value++);
      v+=2;
    } else {
      *value++=(*v=='+'?' ':*v);                  // HTML Forms use '+' for spaces (and '+' signs are escaped)
      v++;
    }
    len++;
  }

  *value='\0';                      // add terminating null
  return(len);
  
}

//////////////////////////////////////

int Network_HS::badRequestError(){

  char s[]="HTTP/1.1 400 Bad Request\r\n\r\n";
  LOG2("\n>>>>>>>>>> ");
  LOG2(client.remoteIP());
  LOG2(" >>>>>>>>>>\n");
  LOG2(s);
  client.print(s);
  LOG2("------------ SENT! --------------\n");
  
  delay(1);
  client.stop();

  return(-1);
}

//////////////////////////////////////
  
HS_ExpCounter::HS_ExpCounter(uint32_t _minCount, uint32_t _maxCount, uint8_t _totalSteps){
  config(_minCount,_maxCount,_totalSteps);
}

void HS_ExpCounter::config(uint32_t _minCount, uint32_t _maxCount, uint8_t _totalSteps){  
  if(_minCount==0 || _maxCount==0 || _totalSteps==0){
    ESP_LOGE("HS_Counter","call to config(%ld,%ld,%d) ignored: all parameters must be non-zero\n",_minCount,_maxCount,_totalSteps);
  } else {
    minCount=_minCount;
    maxCount=_maxCount;
    totalSteps=_totalSteps;
  }
  reset();
}

void HS_ExpCounter::reset(){
  nStep=0;
}

HS_ExpCounter::operator uint32_t(){
  return(minCount*pow((double)maxCount/(double)minCount,(double)nStep/(double)totalSteps));
}

HS_ExpCounter& HS_ExpCounter::operator++(){
  nStep++;
  if(nStep>totalSteps)
    nStep=0;
  return(*this);    
}

HS_ExpCounter HS_ExpCounter::operator++(int){
  HS_ExpCounter temp=*this;
  operator++();
  return(temp);
}

//////////////////////////////////////
