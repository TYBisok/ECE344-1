#include "common.h"

int factorial(int num);

int
main(int argc, char **argv)
{
	if(argc != 2 || atoi(argv[1]) <= 0) printf("Huh?\n");
	else if(atoi(argv[1]) > 12) printf("Overflow\n");
        else {
            int i=0;
            while(argv[i] != '\0') {
                if(argv[1][i] == '.') printf("Huh?\n");
                i++;
            }
        }
	printf("%d\n", factorial(atoi(argv[1])));
	return 0;
}

int factorial(int num) {
	if(num == 1) return num;
	return num * factorial(num-1);
}
