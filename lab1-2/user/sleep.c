#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc ==1){
        printf("Please enter the parameters\n");
    }
    else{
      printf("Nothing happened\n");
      int duration = atoi(argv[1]);
      sleep(duration);
    }
  exit(0);
}