#include <unistd.h>
#include <syscall.h>

long hello_world();

int main()
{
  hello_world();
  return 0;
}

long hello_world()
{
  return syscall(__NR_hello);
}
