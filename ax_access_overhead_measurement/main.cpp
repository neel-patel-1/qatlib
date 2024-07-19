#include "router.pb.h"
#include "ch3_hash.h"
#include "print_utils.h"
#include "timer_utils.h"

#include <string>
#include <dml/dml.h>

using namespace std;
bool gDebugParam = 1;
/* move a buffer to LLC */
void dsa_llc_realloc(void *dst, void *src, int size){
  dml_path_t execution_path = DML_PATH_HW;
  dml_job_t *dml_job_ptr = NULL;

  uint32_t job_size = 0u;
  dml_status_t status = dml_get_job_size(execution_path, &job_size);
  dml_job_ptr = (dml_job_t *)malloc(job_size);
  status = dml_init_job(execution_path, dml_job_ptr);
  dml_job_ptr->operation              = DML_OP_MEM_MOVE;
  dml_job_ptr->source_first_ptr       = (uint8_t *)src;
  dml_job_ptr->source_length          = size;
  dml_job_ptr->destination_first_ptr  = (uint8_t *)dst;
  dml_job_ptr->flags = DML_FLAG_PREFETCH_CACHE;
  status = dml_execute_job(dml_job_ptr, DML_WAIT_MODE_BUSY_POLL);

}

void gen_serialized(string *serialized, router::RouterRequest *request){
  string query = "/region/cluster/foo:key|#|etc";
  string value = "bar";
  request->set_key(query);
  request->set_value(value);
  request->set_operation(0);

  request->SerializeToString(serialized);
}

static inline void deserialize(string *serialized, router::RouterRequest *request){
  request->ParseFromString(*serialized);
}

static inline void gen_and_deser_host(string *serialized, router::RouterRequest *request){
  gen_serialized(serialized, request);
  deserialize(serialized, request);
}

int main(){
  string serialized, desered;
  router::RouterRequest request;

  /* prepare serialized */
  gen_serialized(&serialized, &request);
  /* deserialize */
  request.ParseFromString(serialized);
  PRINT_DBG("Hashing key: %s\n", request.key().c_str());

  /* hash */

  {
    PRINT("GPCoreDeserialize: ");
    time_code_region(
      gen_serialized(&serialized, &request),
      deserialize(&serialized, &request),
      NULL,
      1000
    );
  }

  {
    PRINT("GPCoreHash: ");
    time_code_region(
      gen_and_deser_host(&serialized, &request),
      furc_hash(request.key().c_str(), request.key().size(), 16),
      NULL,
      1000
    );
  }

}