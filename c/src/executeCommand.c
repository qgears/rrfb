#include <stdio.h>
#include <stdlib.h>

#define MAX_INPUT (1024*1024*8)

char * executeCommandAndReceiveInput(const char * command)
{
	FILE * f=popen(command, "r");
	size_t n=1;
	if(f!=NULL)
	{
		char * ret=malloc(MAX_INPUT+1);
		int ptr=0;
		while(ptr<MAX_INPUT && n>0)
		{
			n=fread(ret+ptr, 1, 1, f);
			if(n>0)
			{
				//printf("Char rec: %c ptr: %d n: %d\n", *(ret+ptr), ptr, (int)n);
				ptr+=n;
				//printf("Char rec: %c ptr: %d n: %d\n", *(ret+ptr), ptr, (int)n);
			}
		}
		*(ret+ptr)=0;
		fclose(f);
		return ret;
	}
	return NULL;
}
void executeCommandWithInput(const char * command, const char * content)
{
	FILE * f=popen(command, "w");
	if(f!=NULL)
	{
		while(*content!=0)
		{
			fwrite(content, 1, 1, f);
			content++;
		}
	}
	fclose(f);
}
char * readClipboard(const char* display_name)
{
	char cmd[128];
	snprintf(cmd, 128, "xclip -d %s -selection clipboard -o", display_name);
	char * data=executeCommandAndReceiveInput(cmd);
	return data;
}
void writeClipboard(const char* display_name, const char * data)
{
	if(data!=NULL)
	{
		char cmd[128];
		snprintf(cmd, 128, "xclip -d %s -selection clipboard -i", display_name);
		executeCommandWithInput(cmd, data);
	}
}
/*
int main(int argc, char ** argv)
{
	char * data=readClipboard();
	if(data!=NULL)
	{
		printf("xclip input: %s\n", data);
		free(data);
	}
	writeClipboard("ezt tedd a clipboardra!");
}
*/

// ezt tedd a clipboardra!
