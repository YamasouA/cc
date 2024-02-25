#!/bin/bash

cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }
int add6(int a, int b, int c, int d, int e, int f) {
  return a+b+c+d+e+f;
}
EOF

assert() {
    expected="$1"
    input="$2"

    ./9cc "$input" > tmp.s
    cc -o tmp tmp.s tmp2.o
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

assert 0 'int main() {0;}'
assert 42 'int main() {42;}'
assert 21 'int main() {5+20-4;}'
assert 41 'int main() { 12 + 34 -  5 ;}'
assert 47 'int main() {5+6*7;}'
assert 15 'int main() {5*(9-6);}'
assert 4 'int main() {(3+5)/2;}'
assert 10 'int main() {-10+20;}'
assert 10 'int main() {- -10;}'
assert 10 'int main() {- - +10;}'

assert 0 'int main() {0==1;}'
assert 1 'int main() {42==42;}'
assert 1 'int main() {0!=1;}'
assert 0 'int main() {42!=42;}'

assert 1 'int main() {0<1;}'
assert 0 'int main() {1<1;}'
assert 0 'int main() {2<1;}'
assert 1 'int main() {0<=1;}'
assert 1 'int main() {1<=1;}'
assert 0 'int main() {2<=1;}'

assert 1 'int main() {1>0;}'
assert 0 'int main() {1>1;}'
assert 0 'int main() {1>2;}'
assert 1 'int main() {1>=0;}'
assert 1 'int main() {1>=1;}'
assert 0 'int main() {1>=2;}'

assert 5 'int main() {int a=5;}'
assert 4 'int main() {int a = 2; int b = 4;}'
assert 6 'int main() {int a = 2; int b = a + 4;}'

assert 47 'int main() {int foo=42; int bar = foo + 5;}'
assert 0 'int main() {return 0;}'
assert 11 'int main() {return 10 + 1;}'
assert 1 'int main() {return 1 == 1;}'
assert 42 'int main() {int a = 2; int b=40; return a + b;}'

assert 3 'int main() {if (0) return 2; return 3;}'
assert 3 'int main() {if (1-1) return 2; return 3;}'
assert 2 'int main() {if (1) return 2; return 3;}'
assert 2 'int main() {if (2-1) return 2; return 3;}'
assert 2 'int main() {int a; if (2-1) a = 2; else a = 3; return a;}'
assert 3 'int main() {int a; if (0) a = 2; else a = 3; return a;}'

assert 10 'int main() {int i = 0; while(i<10) i = i + 1; return i;}'

assert 55 'int main() {int i=0; int j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;}'
assert 3 'int main() {for (;;) return 3; return 5;}'

assert 3 'int main() {{1; {2;} return 3;}}'
assert 55 'int main() {int i=0; int j=0; while(i<=10) {j=i+j; i=i+1;} return j;}'

assert 3 'int main() {return ret3();}'
assert 5 'int main() {return ret5();}'
assert 8 'int main() {return ret3() + ret5();}'

assert 8 'int main() {return add(3, 5);}'
assert 2 'int main() {return sub(5, 3);}'
assert 21 'int main() {return add6(1,2,3,4,5,6);}'

assert 3 'int main() {int x=3; return *&x;}'
assert 3 'int main() {int x=3; int y=&x; int z=&y; return **z;}'
assert 5 'int main() {int x=3; int y=&x; *y=5; return x;}'

# chibiccとは変数のoffsetの順序が異なる
assert 5 'int main() {int x=3; int y=5; return *(&x-8);}'
assert 3 'int main() {int x=3; int y=5; return *(&y+8);}'
assert 7 'int main() {int x=3; int y=5; *(&x-8)=7; return y;}'
assert 7 'int main() {int x=3; int y=5; *(&y+8)=7; return x;}'

assert 7 'int main() { return add2(3,4); } int add2(int x, int y) { return x+y; }'
assert 1 'int main() { return sub2(4,3); } int sub2(int x, int y) { return x-y; }'
assert 55 'int main() { return fib(9); } int fib(int x) { if (x<=1) return 1; return fib(x-1) + fib(x-2); }'

echo OK