#include "yanel.h"

int main(void)
{
	char string[] = "HELLO\n";
	int len, a, b;

	len = _strlen(string);
	for (int i = 0; i < len; i++)
		_putchar(string[i]);
	a = _get_dec();
	b = _get_dec();
	_print_dec(a + b);
	_putchar('\n');
	_print_udec(a + b);
	_putchar('\n');
	_print_hex(a + b);
	_putchar('\n');
	_print_bin(a + b);
	_putchar('\n');
	return 0;
}
