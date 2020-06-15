#ifndef _DOORS_WWW_H_
#define _DOORS_WWW_H_

#ifdef PUBLIC
  #error "PUBLIC already defined"
#endif

#if DOORS_WWW
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

// http headers
PUBLIC char * http_html_hdr
#if DOORS_WWW
 =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/html\r\n"
  "Content-Length: %d\r\n\r\n"
#endif
;

PUBLIC char * http_xml_hdr
#if DOORS_WWW
 =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: application/xml\r\n"
  "Content-Length: %d\r\n\r\n"
#endif
;

PUBLIC char * http_html_hdr_not_found
#if DOORS_WWW
 =
  "HTTP/1.1 404 Not Found\r\n"
  "Content-type: text/html\r\n\r\n"
  "<html><body><p>Error 404: Command not found.</p></body></html>"
#endif
;

PUBLIC char * http_css_hdr
#if DOORS_WWW
 =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: text/css\r\n"
  "Content-Length: %d\r\n\r\n"
#endif
;

PUBLIC char * http_png_hdr
#if DOORS_WWW
 =
  "HTTP/1.1 200 OK\r\n"
  "Content-type: image/png\r\n"
  "Content-Length: %d\r\n\r\n"
#endif
;

PUBLIC char message_0[MESSAGE_SIZE];
PUBLIC char message_1[MESSAGE_SIZE];
PUBLIC char severity_0[10];
PUBLIC char severity_1[10];

PUBLIC void start_http_server();
PUBLIC void init_http_server();

#undef PUBLIC
#endif