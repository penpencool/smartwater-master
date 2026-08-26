#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>
#include <WebServer.h>

#include "web_style_css.h"
#include "web_content_html.h"
#include "web_script_js.h"

// ฟังก์ชันสำหรับส่ง HTML หน้า Dashboard แบบประกอบชิ้นส่วน (HTML + CSS + JS)
inline void sendDashboardPage(WebServer &server) {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "<!DOCTYPE html><html lang=\"th\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"><title>Smart Water Master Controller</title><link href=\"https://fonts.googleapis.com/css2?family=Prompt:wght@300;400;500;600;700&display=swap\" rel=\"stylesheet\"><style>");
    
    server.sendContent_P(DASHBOARD_CSS);
    server.sendContent("</style></head><body>");
    
    server.sendContent_P(DASHBOARD_BODY);
    server.sendContent("<script>");
    
    server.sendContent_P(DASHBOARD_JS);
    server.sendContent("</script></body></html>");
}

#endif // WEB_DASHBOARD_H
