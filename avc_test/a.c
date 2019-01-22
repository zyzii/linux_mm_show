#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

int main()
{
	pid_t pid;
	char *addr;
	int i;

	addr = mmap(NULL, 4096 * 10, PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0); 
	if (addr == MAP_FAILED)
		return -1;

	for (i = 0; i < 4096 * 10; i += 4096) {
		addr[i] = 2;
		printf("[%s] now, i : %d\n", __func__, i);
	}

	pid = fork();
	if (pid < 0) {
		printf("error\n");
	} else if (pid == 0) { /* child */
		addr[0] = 3;
	} else {               /* parent */
		addr[0] = 5;
	}

	printf("the value is :%d, pid: %x\n", addr[0], getpid());
	

	munmap(addr, 4096);

	return 0;
}
