
#define CALGARY "./calgary"
#include <assert.h>
#include <mmintrin.h>



int corpus_to_input_buffer(char *** testBufs,int sizePerBuf, const char *filename) {
   FILE *file = fopen(filename, "r");
   if (file == NULL){
    printf("Failed to open file:%s\n", filename);
    exit(-1);
   }
   fseek(file, 0, SEEK_END);
   int size = ftell(file);
   fseek(file, 0, SEEK_SET);
   if (size < sizePerBuf){
       printf("Warning: corpus file is smaller than payload size -- results may be skewed\n");
   }
   int num_bufs = size / sizePerBuf ;

   *testBufs = (char **)malloc(sizeof(char *) * num_bufs);

   for ( int i=0; i < num_bufs; i++ ){
       uint64_t offset=0;
       *testBufs[i] = (char *)malloc(sizePerBuf);
       char *testBuf = *testBufs[i];
fill_file:
       uint64_t readLen = fread((void *) (testBuf + offset), 1, sizePerBuf - offset, file);
       if ( readLen < sizePerBuf ){
           rewind(file);
           offset += readLen;
           goto fill_file;
       }
   }
   return num_bufs;
}



int compare_buffers(const char *a, const char *b, int size) {
   for (int i = 0; i< size; i++) {
      if (a[i] != b[i]) {
         return i;
      }
   }
   return -1;
}

