#include<stdio.h>

#define __WEAK_INLINE __attribute__((__weak__,__always_inline__)) 

void cyc_test_func_insert();
__WEAK_INLINE void cyc_test_func_insert(){
    printf("insert func success \n");
}
