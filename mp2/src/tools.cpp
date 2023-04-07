#include "tools.h"

void diep(std::string s, int error_code) {
  perror(s.c_str());
  exit(error_code);
}