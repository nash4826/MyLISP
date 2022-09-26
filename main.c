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

/*         초기 선언          */

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// lval value
enum
{
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_FUN,
	LVAL_SEXPR,
	LVAL_QEXPR
}; // 0,1,2,3,4

typedef lval *(*lbuiltin)(lenv *, lval *);

// Lisp Value
struct lval
{
	int type;
	long num;
	/* Error 와 Symbol 타입은 문자열 타입*/
	char *err;
	char *sym;
	/* Count 와 pointer는 'lval*' 의 리스트*/
	lbuiltin fun;

	int count;
	struct lval **cell;
};

/* number 형 lval pointer */

lval *lval_num(long x)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* Error 형 lval pointer */

lval *lval_err(char *fmt, ...)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	/* va list를 만들고 초기화함 */
	va_list va;
	va_start(va, fmt);

	/*512바이트 할당*/
	v->err = malloc(512);

	/*최대 511개의 문자를 도달하면 Error 출력*/
	vsnprintf(v->err, 511, fmt, va);
	/* 실제 사용되는 byte의 수를 재할당한다.*/
	v->err = realloc(v->err, strlen(v->err) + 1);

	/*va list 해제*/
	va_end(va);

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

//함수포인터

lval *lval_fun(lbuiltin func)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun = func;
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

lval *lval_qexpr(void)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
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
		/*함수포인터는 del함수가 동작하지 않는다.*/
	case LVAL_FUN:
		break;
	/* Sexpr or Qexpr 가 존재하면 내부의 모든 요소 삭제 */
	case LVAL_QEXPR:
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

lval *lval_copy(lval *v)
{
	lval *x = malloc(sizeof(lval));
	x->type = v->type;

	switch (v->type)
	{
	/*함수와 정수는 바로 복사*/
	case LVAL_FUN:
		x->fun = v->fun;
		break;
	case LVAL_NUM:
		x->num = v->num;
		break;
	/* malloc과 strcpy를 사용하여 문자열을 복사*/
	case LVAL_ERR:
		x->err = malloc(strlen(v->err) + 1);
		strcpy(x->err, v->err);
		break;
	case LVAL_SYM:
		x->sym = malloc(strlen(v->sym) + 1);
		strcpy(x->sym, v->sym);
		break;
	/*각각 sub 표현식를 복사한 list들을 저장*/
	case LVAL_SEXPR:
	case LVAL_QEXPR:
		x->count = v->count;
		x->cell = malloc(sizeof(lval *) * x->count); // check
		for (int i = 0; i < x->count; i++)
		{
			x->cell[i] = lval_copy(v->cell[i]);
		}
		break;
	}

	return x;
}

lval *lval_add(lval *v, lval *x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval *) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

lval *lval_join(lval *x, lval *y)
{
	/*y에 있는 모든 cell을 순회하면서 x 에 할당*/
	for (int i = 0; i < y->count; i++)
	{
		x = lval_add(x, y->cell[i]);
	}

	/*비어있는 y를 삭제 후 x를 리턴*/
	free(y->cell);
	free(y);
	return x;
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

void lval_print_expr(lval *v, char open, char close)
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
	case LVAL_FUN:
		printf("<Function>");
		break;
	case LVAL_SEXPR:
		lval_print_expr(v, '(', ')');
		break;
	case LVAL_QEXPR:
		lval_print_expr(v, '{', '}');
		break;
	}
}

void lval_println(lval *v)
{
	lval_print(v);
	putchar('\n');
}

char *ltype_name(int t)
{
	switch (t)
	{
	case LVAL_FUN:
		return "Function";
	case LVAL_NUM:
		return "Number";
	case LVAL_ERR:
		return "Error";
	case LVAL_SYM:
		return "Symbol";
	case LVAL_SEXPR:
		return "S-Expression";
	case LVAL_QEXPR:
		return "Q-Expression";
	default:
		return "Unknown";
	}
}

// Environment

struct lenv
{
	int count;
	char **syms;
	lval **vals;
};

lenv *lenv_new(void)
{
	lenv *e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;

	return e;
}

void lenv_del(lenv *e)
{
	for (int i = 0; i < e->count; i++)
	{
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval *lenv_get(lenv *e, lval *k)
{
	/*Environment의 모든 item들을 반복 순회*/
	for (int i = 0; i < e->count; i++)
	{
		/*저장된 문자열이 Symbol 문자열과 일치하는지 확인*/
		/*만약 일치한다면, value을 복사하여 리턴*/
		if (strcmp(e->syms[i], k->sym) == 0)
		{
			return lval_copy(e->vals[i]);
		}
	}

	/*Symbol을 찾지 못하면 Error 를 리턴*/
	return lval_err("unbound symbol! '%s'", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v)
{
	/*Environment의 모든 item들을 반복 순회*/
	/*for문은 변수들이 이미 존재하는지를 보기 위해 쓰임*/
	for (int i = 0; i < e->count; i++)
	{
		/*변수들이 발견된다면, 현재 위치에서 삭제 */
		/*그리고 user가 제공한 변수로 대체함*/
		if (strcmp(e->syms[i], k->sym) == 0)
		{
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}

	/*존재하는 entry를 찾지 못한다면, 새로운 entry를 위해 공간을 할당한다.*/
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval *) * e->count);
	e->syms = realloc(e->syms, sizeof(char *) * e->count);

	/*lval과 symbol 문자열을 새로 할당한 곳에 복사한다.*/
	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count - 1], k->sym);
}

/*Builtins*/

#define LASSERT(args, cond, fmt, ...)       \
	if (!(cond))                              \
	{                                         \
		lval *err = lval_err(fmt, __VA_ARGS__); \
		lval_del(args);                         \
		return err;                             \
	}

#define LASSERT_TYPE(func, args, index, expect)                                        \
	LASSERT(args, args->cell[index]->type == expect,                                     \
					"Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
					func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NUM(func, args, num)                                                  \
	LASSERT(args, args->count == num,                                                   \
					"Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
					func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index)   \
	LASSERT(args, args->cell[index]->count != 0, \
					"Function '%s' passed {} for argument %i.", func, index);

lval *lval_eval(lenv *e, lval *v);

lval *builtin_list(lenv *e, lval *a)
{
	a->type = LVAL_QEXPR;
	return a;
}

lval *builtin_head(lenv *e, lval *a)
{
	/* 에러 확인 */
	LASSERT_NUM("head", a, 1);
	LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
	LASSERT_NOT_EMPTY("head", a, 0);

	/*에러 없으면 첫번째 인자 가져오기*/
	lval *v = lval_take(a, 0);

	/*모든 요소(head가 없는) 삭제 그리고 리턴*/
	while (v->count > 1)
	{
		lval_del(lval_pop(v, 1));
	}
	return v;
}

lval *builtin_tail(lenv *e, lval *a)
{
	/* 에러 확인 */
	LASSERT_NUM("tail", a, 1);
	LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
	LASSERT_NOT_EMPTY("tail", a, 0);

	/*에러 없으면 첫번째 인자 가져오기*/
	lval *v = lval_take(a, 0);

	/*첫번째 요소 삭제 그리고 리턴*/
	lval_del(lval_pop(v, 0));
	return v;
}

lval *builtin_eval(lenv *e, lval *a)
{
	LASSERT_NUM("eval", a, 1);
	LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

	lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval *builtin_join(lenv *e, lval *a)
{
	for (int i = 0; i < a->count; i++)
	{
		LASSERT_TYPE("join", a, i, LVAL_QEXPR);
	}

	lval *x = lval_pop(a, 0);

	while (a->count)
	{
		lval *y = lval_pop(a, 0);
		x = lval_join(x, y);
	}
	lval_del(a);
	return x;
}

lval *builtin_op(lenv *e, lval *a, char *op)
{
	/* Numbers 아닌 데이터 찾기 */
	for (int i = 0; i < a->count; i++)
	{
		LASSERT_TYPE(op, a, i, LVAL_NUM);
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

lval *builtin_add(lenv *e, lval *a)
{
	return builtin_op(e, a, "+");
}
lval *builtin_sub(lenv *e, lval *a)
{
	return builtin_op(e, a, "-");
}
lval *builtin_mul(lenv *e, lval *a)
{
	return builtin_op(e, a, "*");
}
lval *builtin_div(lenv *e, lval *a)
{
	return builtin_op(e, a, "/");
}

lval *builtin_def(lenv *e, lval *a)
{
	LASSERT_TYPE("def", a, 0, LVAL_QEXPR);

	/*첫번째 인자는 심볼리스트이다.*/
	lval *syms = a->cell[0];

	/*첫번째 리스트의 모든 요소는 심볼이라는 것을 보장한다.*/
	for (int i = 0; i < syms->count; i++)
	{
		LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
						"Function 'def' cannot define non-symbol. "
						"Got %s, Expected %s.",
						ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
	}

	/*정확한 심볼과 value값들을 확인한다.*/
	LASSERT(a, (syms->count == a->count - 1),
					"Function 'def' passed too many arguments for symbols. "
					"Got %i, Expected %i.",
					syms->count, a->count - 1);

	/*심볼에 있는 value값들의 복사본을 할당한다.*/
	for (int i = 0; i < syms->count; i++)
	{
		lenv_put(e, syms->cell[i], a->cell[i + 1]);
	}
	lval_del(a);
	return lval_sexpr();
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
	lval *k = lval_sym(name);
	lval *v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k);
	lval_del(v);
}

void lenv_add_builtins(lenv *e)
{
	/* Variable Functions */
	lenv_add_builtin(e, "def", builtin_def);

	/*list 함수*/
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);

	/*수학적 함수*/
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
}

/*평가*/

lval *lval_eval_sexpr(lenv *e, lval *v)
{
	/* 자식 요소 평가 */
	for (int i = 0; i < v->count; i++)
	{
		v->cell[i] = lval_eval(e, v->cell[i]);
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
	if (f->type != LVAL_FUN)
	{
		lval *err = lval_err(
				"S-Expression starts with incorrect type. "
				"Got %s, Expected %s.",
				ltype_name(f->type), ltype_name(LVAL_FUN));

		lval_del(f);
		lval_del(v);
		return err;
	}

	/* builtin 함수 호출 */

	lval *result = f->fun(e, v);
	lval_del(f);
	return result;
}

lval *lval_eval(lenv *e, lval *v)
{
	if (v->type == LVAL_SYM)
	{
		lval *x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	/* SexPression 평가 */
	if (v->type == LVAL_SEXPR)
		return lval_eval_sexpr(e, v);

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

/* Reading */

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
	if (strstr(t->tag, "qexpr"))
	{
		x = lval_qexpr();
	}

	/* 유효한 표현식이 포함되어 있는 리스트 채우기*/
	for (int i = 0; i < t->children_num; i++)
	{
		if (strcmp(t->children[i]->contents, "(") == 0)
			continue;
		if (strcmp(t->children[i]->contents, ")") == 0)
			continue;
		if (strcmp(t->children[i]->contents, "{") == 0)
			continue;
		if (strcmp(t->children[i]->contents, "}") == 0)
			continue;

		if (strcmp(t->children[i]->tag, "regex") == 0)
			continue;

		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;
}

/* main 함수 */
int main(int argc, char **argv)
{
	/*전위표현식 parser 프로그램*/

	/*몇몇 parser 만들기*/
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Qexpr = mpc_new("qexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lispy = mpc_new("lispy");

	// parser들을 정의한다.(정규 표현식)
	mpca_lang(MPCA_LANG_DEFAULT,
						" 															\
      number : /-?[0-9]+/ ;                             \
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;                  \
			sexpr	: '(' <expr>* ')' ; \
			qexpr : '{' <expr>* '}'; \
			expr : <number> | <symbol> | <sexpr> | <qexpr>; \
			lispy : /^/ <expr>* /$/ ; \
		",
						Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
	/*Information print*/
	puts("Lispy Version 1.0.1");
	puts("Press Ctrl + C to Exit\n");

	lenv *e = lenv_new();
	lenv_add_builtins(e);

	/*never ending loop*/
	while (1)
	{
		char *input = readline("lispy> ");
		add_history(input);

		// input 값을 parse 시도
		mpc_result_t r;
		if (mpc_parse("<stdin>", input, Lispy, &r))
		{
			lval *x = lval_eval(e, lval_read(r.output));
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

	lenv_del(e);
	/*parser들 해제*/
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
	return 0;
}
