#include "emul_ax.h"

std::atomic<int> submit_flag;
std::atomic<int> submit_status;
std::atomic<uint64_t> compl_addr;
std::atomic<uint64_t> p_dst_buf;