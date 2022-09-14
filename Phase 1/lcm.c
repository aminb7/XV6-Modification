#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

#define MIN_ARGS 3
#define MAX_ARGS 8
#define MAX_LENGTH 32

int
to_string(char* res, int num)
{
  int i = 1, j = MAX_LENGTH - 1;
  res[MAX_LENGTH] = '\n';
  while(num / i != 0) {
    res[j] = (num % (i * 10)) / i + '0';
    i = i * 10;
    j = j - 1;
  }
  return j;
}

int
gcd(int x, int y) 
{
  int smaller = (x > y) ? y : x;
  int bigger = (x > y) ? x : y;
  
  if(smaller == 0) 
    return bigger; 
  return gcd(smaller, bigger % smaller); 
} 

int
lcm(int* numbers, int size)
{
  int result = numbers[0];
  for(int i = 0; i < size; ++i)
  {
    result = (numbers[i] * result) / gcd(numbers[i], result);
  }
  return result;
}

int
main(int argc, char *argv[])
{
  if(argc < MIN_ARGS){
    printf(2, "lcm: too few args\n");
    exit();
  }

  int numbers[MAX_ARGS];
  for(int i = 0; i < argc - 1; ++i) {
    numbers[i] = atoi(argv[i + 1]);
  }

  int result = lcm(numbers, argc - 1);
  char result_in_string[MAX_LENGTH + 1];
  int offset = to_string(result_in_string, result);
   
  int fd = open("lcm_result.txt", O_CREATE | O_WRONLY);
  write(fd, result_in_string + offset + 1, MAX_LENGTH - offset);
  close(fd);
  exit();
}