#ifndef PAYLOAD_GEN_H
#define PAYLOAD_GEN_H

#include <string>
using namespace std;
std::string gen_compressible_string(std::string payload, int input_size);
void gen_compressible_string_in_place(std::string &payload, int input_size);
void **create_random_chain_starting_at(int size, void **st_addr);

/*
  @return a random chain for pointer chasing of size bytes
*/
void **create_random_chain(int size);

#endif