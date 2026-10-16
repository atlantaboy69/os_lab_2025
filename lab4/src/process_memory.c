/* Program to display address information about the process */
/* Adapted from Gray, J., program 1.4 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* Изменено %8X на %p для поддержки 64-битных адресов */
#define SHW_ADR(ID, I) (printf("ID %s \t is at virtual address: %p\n", ID, (void*)&I))

extern int etext, edata, end; /* Global variables for process
                                 memory */

char *cptr = "This message is output by the function showit()\n"; /* Static */
char buffer1[25];
int showit(); /* Function prototype */

main() {
  int i = 0; /* Automatic variable */

  /* Изменено %8X на %p */
  printf("\nAddress etext: %p \n", (void*)&etext);
  printf("Address edata: %p \n", (void*)&edata);
  printf("Address end  : %p \n", (void*)&end);

  SHW_ADR("main", main);
  
  /* Для функции showit берем адрес напрямую, чтобы макрос не ругался на типы */
  printf("ID showit \t is at virtual address: %p\n", (void*)showit);
  
  SHW_ADR("cptr", cptr);
  SHW_ADR("buffer1", buffer1);
  SHW_ADR("i", i);
  strcpy(buffer1, "A demonstration\n");   /* Library function */
  write(1, buffer1, strlen(buffer1) + 1); /* System call */
  showit(cptr);

} /* end of main function */

/* A function follows */
int showit(p) char *p;
{
  char *buffer2;
  SHW_ADR("buffer2", buffer2);
  if ((buffer2 = (char *)malloc((unsigned)(strlen(p) + 1))) != NULL) {
    /* Изменено %X на %p */
    printf("Alocated memory at %p\n", (void*)buffer2);
    strcpy(buffer2, p);    /* copy the string */
    printf("%s", buffer2); /* Didplay the string */
    free(buffer2);         /* Release location */
  } else {
    printf("Allocation error\n");
    exit(1);
  }
}
