#ifndef WEB_BINARY_H
#define WEB_BINARY_H

// index.html
extern const uint8_t _binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t _binary_index_html_end[] asm("_binary_index_html_end");

// style.css
extern const uint8_t _binary_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t _binary_style_css_end[] asm("_binary_style_css_end");

// app.js
extern const uint8_t _binary_app_js_start[] asm("_binary_app_js_start");
extern const uint8_t _binary_app_js_end[] asm("_binary_app_js_end");

// favicon.ico
extern const uint8_t _binary_favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t _binary_favicon_ico_end[] asm("_binary_favicon_ico_end");

#endif /* WEB_BINARY_H */
