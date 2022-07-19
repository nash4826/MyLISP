#define _CRT_SECURE_NO_WARNINGS
#include "mpc.h"
#include <stdlib.h>

#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt)
{
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char *cpy = malloc(sizeof(buffer) + 1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy) - 1] = '\0';
	return cpy;
}

void add_history(char *unused) {}

#else
// Linux or Mac
#include <editline/readline.h>
#include <editline/history.h>
#endif

/********************************************************/

/*         Error handle          */

// lval type
enum
{
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_SEXPR
}; // 0,1,2,3

// Lisp Value Struct
typedef struct lval
{
	int type;
	long num;
	/* Error 와 Symbol 타입은 문자열 타입*/
	char *err;
	char *sym;
	/* Count 와 pointer는 'lval*' 의 리스트*/
	int count;
	struct lval **cell;
} lval;

/* number 형 lval pointer */

lval *lval_num(long x)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* Error 형 lval pointer */

lval *lval_err(char *m)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
	return v;
}

/* Symbol lval pointer */

lval *lval_sym(char *s)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

/* Sexpr lval pointer */

lval *lval_sexpr(void)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void lval_del(lval *v)
{
	switch (v->type)
	{
		/* Number 타입은 del함수가 동작하지 않는다.*/
	case LVAL_NUM:
		break;
		/* Err 와 Sym 은 문자열 Data(malloc 사용)이므로 free 함수 호출 */
	case LVAL_ERR:
		free(v->err);
		break;
	case LVAL_SYM:
		free(v->sym);
		break;
		/* Sexpr가 존재하면 내부의 모든 요소 삭제 */
	case LVAL_SEXPR:
		for (int i = 0; i < v->count; i++)
		{
			lval_del(v->cell[i]);
		}
		/* cell 변수는 2중 포인터이므로 다시 free함수 호출하여 메모리 해제 */
		free(v->cell);
		break;
	}

	/* lval 구조체 포인터 해제(매개변수) */
	free(v);
}

lval *lval_add(lval *v, lval *x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval *) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

lval *lval_pop(lval *v, int i)
{
	/* i(cell index)의 해당하는 item을 find 후 할당 */
	lval *x = v->cell[i];

	/* i의 해당하눈 item을 메모리 최상단(top)으로 이동  */
	memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

	/* list 안의 item 수 감소 */
	v->count--;

	/* 사용된 메모리 재할당 */
	v->cell = realloc(v->cell, sizeof(lval *) * v->count);
	return x;
}

lval *lval_take(lval *v, int i)
{
	lval *x = lval_pop(v, i);
	lval_del(v);
	return x;
}

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close)
{
	putchar(open);
	for (int i = 0; i < v->count; i++)
	{
		/* 값 출력 */
		lval_print(v->cell[i]);

		/* 마지막 요소가 공백이라면 출력하지 않기  */
		if (i != (v->count - 1))
		{
			putchar(' ');
		}
	}
	putchar(close);
}

// lval 출력
void lval_print(lval *v)
{
	switch (v->type)
	{
	case LVAL_NUM:
		printf("%li", v->num);
		break;

	case LVAL_ERR:
		printf("Error: %s", v->err);
		break;
	case LVAL_SYM:
		printf("%s", v->sym);
		break;
	case LVAL_SEXPR:
		lval_expr_print(v, '(', ')');
		break;
	}
}

void lval_println(lval *v)
{
	lval_print(v);
	putchar('\n');
}

/********************************************************/

/*             Eval             */

lval *builtin_op(lval *a, char *op)
{
	/* Numbers 아닌 데이터 찾기 */
	for (int i = 0; i < a->count; i++)
	{
		if (a->cell[i]->type != LVAL_NUM)
		{
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}

	/* 첫번째 요소 POP */
	lval *x = lval_pop(a, 0);

	/* 다음 요소에 숫자가 아닌 빼기(-)만 존재한다면 음수 기호로 수행하라 */

	if ((strcmp(op, "-") == 0) && a->count == 0)
	{
		x->num = -x->num;
	}

	/* 남아있는 요소가 있다면 */

	while (a->count > 0)
	{
		/* 다음 요소 POP */
		lval *y = lval_pop(a, 0);

		/* 연산 수행 */
		if (strcmp(op, "+") == 0)
		{
			x->num += y->num;
		}
		if (strcmp(op, "-") == 0)
		{
			x->num -= y->num;
		}
		if (strcmp(op, "*") == 0)
		{
			x->num *= y->num;
		}
		if (strcmp(op, "/") == 0)
		{
			if (y->num == 0)
			{
				lval_del(x);
				lval_del(y);
				x = lval_err("Division By Zero.");
				break;
			}
			x->num /= y->num;
		}

		lval_del(y);
	}

	lval_del(a);
	return x;
}

lval *lval_eval(lval *v);

lval *lval_eval_sexpr(lval *v)
{
	/* 자식 요소 평가 */
	for (int i = 0; i < v->count; i++)
	{
		v->cell[i] = lval_eval(v->cell[i]);
	}

	/* Error 체크 */

	for (int i = 0; i < v->count; i++)
	{
		if (v->cell[i]->type == LVAL_ERR)
		{
			return lval_take(v, i);
		}
	}

	/* 표현식(Expression)이 비어있다면 */
	if (v->count == 0)
		return v;

	/* 단일 표현식이라면 */
	if (v->count == 1)
		return lval_take(v, 0);

	/* 첫번째 요소가 Symbol 보장 */

	lval *f = lval_pop(v, 0);
	if (f->type != LVAL_SYM)
	{
		lval_del(f);
		lval_del(v);
		return lval_err("S-expression Does not start with symbol.");
	}

	/* builtin 함수 호출 */

	lval *result = builtin_op(v, f->sym);
	lval_del(f);
	return result;
}

lval *lval_eval(lval *v)
{
	/* SexPression 평가 */
	if (v->type == LVAL_SEXPR)
		return lval_eval_sexpr(v);

	/* 나머지 lval type들이 같다면 */
	return v;
}

// mpc.h 에 있는 mpc_ast_t 구조체(Tree)
/*
typedef struct mpc_ast_t {
	char *tag; //expr,number,regex 등의 구문 분석 목록
	char *contents; // 실제 input값
	mpc_state_t state; //Node를 찾았을 때의 state의 대한 정보(사용하지 않음)
	int children_num; //Node에 자식의 수가 저장되어있는 변수
	struct mpc_ast_t** children; //자식의 목록(이중포인터)
} mpc_ast_t;
*/

lval *lval_read_num(mpc_ast_t *t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_read(mpc_ast_t *t)
{
	/* Symbol이나 Number 라면 형변환 */
	if (strstr(t->tag, "number"))
		return lval_read_num(t);
	if (strstr(t->tag, "symbol"))
		return lval_sym(t->contents);

	/* root(>) 또는 sexpr 이면 빈 list를 만든다 */
	lval *x = NULL;
	if (strcmp(t->tag, ">") == 0)
	{
		x = lval_sexpr();
	}
	if (strstr(t->tag, "sexpr"))
	{
		x = lval_sexpr();
	}

	/* 유효한 표현식이 포함되어 있는 리스트 채우기*/
	for (int i = 0; i < t->children_num; i++)
	{
		if (strcmp(t->children[i]->contents, "(") == 0)
			continue;
		if (strcmp(t->children[i]->contents, ")") == 0)
			continue;
		if (strcmp(t->children[i]->tag, "regex") == 0)
			continue;

		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;
}

int main(int argc, char **argv)
{
	/*전위표현식 parser 프로그램*/

	/*몇몇 parser 만들기*/
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lispy = mpc_new("lispy");

	// parser들을 정의한다.(정규 표현식)
	mpca_lang(MPCA_LANG_DEFAULT,
						" 															\
      number : /-?[0-9]+/ ;                             \
      symbol : '+' | '-' | '*' | '/' ;                  \
			sexpr	: '(' <expr>* ')' ; \
			expr : <number> | <symbol> | <sexpr> ; \
			lispy : /^/ <expr>* /$/ ; \
		",
						Number, Symbol, Sexpr, Expr, Lispy);
	/*Information print*/
	puts("Lispy Version 1.0.1");
	puts("Press Ctrl + C to Exit\n");

	/*never ending loop*/
	while (1)
	{
		char *input = readline("lispy> ");
		add_history(input);

		// input 값을 parse 시도
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lispy, &r))
		{
			lval *x = lval_eval(lval_read(r.output));
			lval_println(x);
			lval_del(x);
			mpc_ast_delete(r.output);
		}
		else
		{
			/*parse 실패시 error 출력과 제거*/
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	/*parser들 해제*/
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
	return 0;
}
