#include <stdio.h>

int main() {
  fputs( "hello, ", stdout );
  fputs( "world", stdout );
  fprintf( stdout, "\n" );
}
