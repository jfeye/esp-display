#include <stdlib.h>
#include <ESP8266WebServer.h>

template<size_t N>
size_t sendProgmem(ESP8266WebServer* server, const prog_char (&str)[N]){
  return server->client().write_P(str, N);
}
