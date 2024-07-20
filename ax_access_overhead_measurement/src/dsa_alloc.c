#include "dsa_alloc.h"
#include <cstdlib>

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