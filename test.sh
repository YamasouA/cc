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

assert 0 'main() {0;}'
assert 42 'main() {42;}'
assert 21 'main() {5+20-4;}'
assert 41 'main() { 12 + 34 -  5 ;}'
assert 47 'main() {5+6*7;}'
assert 15 'main() {5*(9-6);}'
assert 4 'main() {(3+5)/2;}'
assert 10 'main() {-10+20;}'
assert 10 'main() {- -10;}'
assert 10 'main() {- - +10;}'

assert 0 'main() {0==1;}'
assert 1 'main() {42==42;}'
assert 1 'main() {0!=1;}'
assert 0 'main() {42!=42;}'

assert 1 'main() {0<1;}'
assert 0 'main() {1<1;}'
assert 0 'main() {2<1;}'
assert 1 'main() {0<=1;}'
assert 1 'main() {1<=1;}'
assert 0 'main() {2<=1;}'

assert 1 'main() {1>0;}'
assert 0 'main() {1>1;}'
assert 0 'main() {1>2;}'
assert 1 'main() {1>=0;}'
assert 1 'main() {1>=1;}'
assert 0 'main() {1>=2;}'

assert 5 'main() {a=5;}'
assert 4 'main() {a = 2; b = 4;}'
assert 6 'main() {a = 2; b = a + 4;}'

assert 47 'main() {foo=42; bar = foo + 5;}'
assert 0 'main() {return 0;}'
assert 11 'main() {return 10 + 1;}'
assert 1 'main() {return 1 == 1;}'
assert 42 'main() {a = 2; b=40; return a + b;}'

assert 3 'main() {if (0) return 2; return 3;}'
assert 3 'main() {if (1-1) return 2; return 3;}'
assert 2 'main() {if (1) return 2; return 3;}'
assert 2 'main() {if (2-1) return 2; return 3;}'
assert 2 'main() {if (2-1) a = 2; else a = 3; return a;}'
assert 3 'main() {if (0) a = 2; else a = 3; return a;}'

assert 10 'main() {i = 0; while(i<10) i = i + 1; return i;}'

assert 55 'main() {i=0; j=0; for (i=0; i<=10; i=i+1) j=i+j; return j;}'
assert 3 'main() {for (;;) return 3; return 5;}'

assert 3 'main() {{1; {2;} return 3;}}'
assert 55 'main() {i=0; j=0; while(i<=10) {j=i+j; i=i+1;} return j;}'

assert 3 'main() {return ret3();}'
assert 5 'main() {return ret5();}'
assert 8 'main() {return ret3() + ret5();}'

assert 8 'main() {return add(3, 5);}'
assert 2 'main() {return sub(5, 3);}'
assert 21 'main() {return add6(1,2,3,4,5,6);}'

assert 3 'main() {x=3; return *&x;}'
assert 3 'main() {x=3; y=&x; z=&y; return **z;}'
assert 5 'main() {x=3; y=&x; *y=5; return x;}'

# chibiccとは変数のoffsetの順序が異なる
assert 5 'main() {x=3; y=5; return *(&x-8);}'
assert 3 'main() {x=3; y=5; return *(&y+8);}'
assert 7 'main() {x=3; y=5; *(&x-8)=7; return y;}'
assert 7 'main() {x=3; y=5; *(&y+8)=7; return x;}'

echo OK