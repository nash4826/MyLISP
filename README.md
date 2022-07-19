## 나의 LISP 만들기

전위표현식으로 구성된 식을 계산하는 계산기 설계

> 오픈소스 : mpc

> 참고 : https://buildyourownlisp.com/

> 실행

`make main`

`./main.exe `

> Sample

lispy> + 1 (\* 7 5) 3
39
lispy> (- 100)
-100
lispy>
()
lispy> /
/
lispy> (/ ())
Error: Cannot operate on non-number!
lispy>

# 진행상황

<li>Symbolic Expressions</li>
<li>makefile 만들기</li>

Error Message 는 한글로 입력하면 글자 깨짐 현상 발생
