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

assert 0 '0;'
assert 42 '42;'
assert 21 '5+20-4;'
assert 41 ' 12 + 34 -  5 ;'
assert 47 '5+6*7;'
assert 15 '5*(9-6);'
assert 4 '(3+5)/2;'
assert 10 '-10+20;'
assert 10 '- -10;'
assert 10 '- - +10;'

assert 0 '0==1;'
assert 1 '42==42;'
assert 1 '0!=1;'
assert 0 '42!=42;'

assert 1 '0<1;'
assert 0 '1<1;'
assert 0 '2<1;'
assert 1 '0<=1;'
assert 1 '1<=1;'
assert 0 '2<=1;'

assert 1 '1>0;'
assert 0 '1>1;'
assert 0 '1>2;'
assert 1 '1>=0;'
assert 1 '1>=1;'
assert 0 '1>=2;'

assert 5 'a=5;'
assert 4 'a = 2; b = 4;'
assert 6 'a = 2; b = a + 4;'

assert 47 'foo=42; bar = foo + 5;'
assert 0 'return 0;'
assert 11 'return 10 + 1;'
assert 1 'return 1 == 1;'
assert 42 'a = 2; b=40; return a + b;'

assert 3 'if (0) return 2; return 3;'
assert 3 'if (1-1) return 2; return 3;'
assert 2 'if (1) return 2; return 3;'
assert 2 'if (2-1) return 2; return 3;'
assert 2 'if (2-1) a = 2; else a = 3; return a;'
assert 3 'if (0) a = 2; else a = 3; return a;'

assert 10 'i = 0; while(i<10) i = i + 1; return i;'

assert 55 'i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;'
assert 3 'for (;;) return 3; return 5;'

assert 3 '{1; {2;} return 3;}'
assert 55 'i=0; j=0; while(i<=10) {j=i+j; i=i+1;} return j;'

assert 3 'return ret3();'
assert 5 'return ret5();'
assert 8 'return ret3() + ret5();'

assert 8 'return add(3, 5);'
assert 2 'return sub(5, 3);'
assert 21 'return add6(1,2,3,4,5,6);'

assert 3 'x=3; return *&x;'
assert 3 'x=3; y=&x; z=&y; return **z;'
assert 5 'x=3; y=&x; *y=5; return x;'

# chibiccとは変数のoffsetの順序が異なる
assert 5 'x=3; y=5; return *(&x-8);'
assert 3 'x=3; y=5; return *(&y+8);'
assert 7 'x=3; y=5; *(&x-8)=7; return y;'
assert 7 'x=3; y=5; *(&y+8)=7; return x;'

echo OK