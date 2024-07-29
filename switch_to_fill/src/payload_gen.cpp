#include "payload_gen.h"
std::string gen_compressible_string(std::string payload, int input_size){
  std::string append_string = payload;
  while(payload.size() < input_size){
    payload += append_string;
  }
  return payload;
}