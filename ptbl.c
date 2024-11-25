// arm-linux-gnueabi-gcc -O2 -marm ptbl.c -ldl -o ptbl -Wl,--export-dynamic
// arm-linux-gnueabihf-gcc -O2 -marm ptbl.c -ldl -o ptbl_hf -Wl,--export-dynamic
// mips-unknown-linux-uclibc-gcc -O2 ptbl.c -ldl -o ptbl_mips -Wl,--export-dynamic
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>

#define FOO_FUNC_SECTION __attribute__((aligned(8), section(".foo")))

typedef int (*fn_DBShmCliInit)();
typedef int (*fn_dbDmNeedHidden)(const char* tbl_name, const char* col_name);
typedef int (*fn_dbPrintTbl)(const char* tbl_name);
typedef void (*fn_dbPrintAllTbl)();

int ProcUserLog(const char* file, int line, const char* func, 
	int n1, int n2, const char* fmt, ...)
{
	/*va_list ap;

	printf("[%s(%d)%s] ", file, line, func);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');*/

	return 0;
}

int OssDebugPrintf(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	return 0;
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if(size)
	{
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}

	return ret;
}

FOO_FUNC_SECTION static int ret0()
{
	return 0;
}

FOO_FUNC_SECTION static int ret0_0()
{
	return 0;
}

static int change_code(unsigned char* addr)
{
	int ret = 1;
	unsigned char* new_code;
	size_t code_size = 0, i;
	ssize_t tmp;
	long page_size = sysconf(_SC_PAGESIZE);
	void* page_start = (void*)((long)addr & ~(page_size - 1));

	new_code = (unsigned char*)&ret0;
	tmp = (char*)&ret0 - (char*)&ret0_0;
	code_size = tmp >= 0 ? tmp : -tmp;

	if(code_size > 64)
	{
		fprintf(stderr, "Code size error: %zu\n", code_size);
		return ret;
	}

	ret = mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
	if(0 == ret)
	{
		static const char* tab = "0123456789ABCDEF";
		char buf[256];

		for(i=0; i<code_size; i++)
		{
			buf[3 * i    ] = tab[addr[i] >> 4];
			buf[3 * i + 1] = tab[addr[i] & 0xF];
			buf[3 * i + 2] = ' ';
		}
		buf[3 * i] = '\0';
		printf("%p original code: %s\n", addr, buf);

		memcpy(addr, new_code, code_size);
		for(i=0; i<code_size; i++)
		{
			buf[3 * i    ] = tab[addr[i] >> 4];
			buf[3 * i + 1] = tab[addr[i] & 0xF];
			buf[3 * i + 2] = ' ';
		}
		buf[3 * i] = '\0';
		printf("%p      new code: %s\n", addr, buf);
	}
	else
		printf("Failed to make memory writable\n");

	return ret;
}

static void help(void)
{
	fprintf(stderr, "p <table name> -- print table data.\n");
	fprintf(stderr, "all            -- print all table name.\n");
	fprintf(stderr, "exit           -- exit.\n");
}

int main(int argc, char* argv[])
{
	void *handle = NULL;
	fn_DBShmCliInit DBShmCliInit = NULL;
	fn_dbDmNeedHidden dbDmNeedHidden = NULL;
	fn_dbPrintTbl dbPrintTbl = NULL;
	fn_dbPrintAllTbl dbPrintAllTbl = NULL;
	char cmd[256];
	int len, ret = 1;

	handle = dlopen("libdb.so", RTLD_LAZY);
	if(NULL == handle)
	{
		fprintf(stderr, "dlopen error: %s\n", dlerror());
		goto lbl_exit;
	}

	DBShmCliInit = (fn_DBShmCliInit)dlsym(handle, "DBShmCliInit");
	dbDmNeedHidden = (fn_dbDmNeedHidden)dlsym(handle, "dbDmNeedHidden");
	dbPrintTbl = (fn_dbPrintTbl)dlsym(handle, "dbPrintTbl");
	dbPrintAllTbl = (fn_dbPrintAllTbl)dlsym(handle, "dbPrintAllTbl");
	if(NULL == DBShmCliInit || NULL == dbDmNeedHidden || 
		NULL == dbPrintTbl || NULL == dbPrintAllTbl)
	{
		fprintf(stderr, "DBShmCliInit: %p\n", DBShmCliInit);
		fprintf(stderr, "dbDmNeedHidden: %p\n", dbDmNeedHidden);
		fprintf(stderr, "dbPrintTbl: %p\n", dbPrintTbl);
		fprintf(stderr, "dbPrintAllTbl: %p\n", dbPrintAllTbl);
		fprintf(stderr, "Error!!! Symbol not found!\n");
		goto lbl_exit;
	}

	if(change_code((unsigned char*)dbDmNeedHidden) != 0)
		goto lbl_exit;

	ret = 0;
	DBShmCliInit();
	if(argc > 1)
	{
		dbPrintTbl(argv[1]);
		goto lbl_exit;
	}
	
	while(1)
	{
		fprintf(stderr, "@ ");
		fflush(stderr);
		fgets(cmd, sizeof(cmd), stdin);
		len = (int)strlen(cmd);
		if(len > 0 && '\n' == cmd[len - 1])
			cmd[len - 1] = '\0';

		if(strcmp(cmd, "exit") == 0)
			break;

		if(strncmp(cmd, "p ", 2) == 0)
		{
			printf("table name: [%s]\n", cmd+2);
			dbPrintTbl(cmd + 2);
		}
		else if(strcmp(cmd, "all") == 0)
			dbPrintAllTbl();
		else
			help();
	}

	ret = 0;
lbl_exit:
	if(handle != NULL)
		dlclose(handle);

	return ret;
}
