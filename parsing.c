#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char *readline(char* prompt){
	fputs(prompt,stdout);
	fgets(buffer,2048,stdin);
	char *cpy = malloc(sizeof(buffer)+1);
	strcpy(cpy,buffer);
	cpy[strlen(cpy)-1] ='\0';
	return cpy;
}

void add_history(char *unused){}

#else
//Linux or Mac
#include <editline/readline.h>
#include <editline/history.h>
#endif


//Lisp Value Struct
typedef struct{
	int type;
	long num;
	int err;
} lval;


/********************************************************/

/*         Error handle          */

//lval type 열거형
enum { LVAL_NUM,LVAL_ERR }; // 0,1

//err type 열거형
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM }; 


lval lval_num(long x){
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

lval lval_err(int x){
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

// lval 출력
void lval_print(lval v){
	switch(v.type){
		case LVAL_NUM: 
			printf("%li",v.num);
			break;
		
		case LVAL_ERR:
			if(v.err == LERR_DIV_ZERO){
			  printf("Error: Division By Zero!");
			}
			if(v.err == LERR_BAD_OP){
			  printf("Error: Invalid Operator!");
			}
			if (v.err == LERR_BAD_NUM)  {
		      printf("Error: Invalid Number!");
			}
			break;
	}
}

void lval_println(lval v){
	lval_print(v);
	putchar('\n');
}


/********************************************************/

/*             Eval             */


lval eval_op(lval x,char *op,lval y){
	
	if(x.type == LVAL_ERR) return x;
	if(y.type == LVAL_ERR) return y;
	
	
	if(strcmp(op,"+") == 0) return lval_num(x.num + y.num);
	if(strcmp(op,"-") == 0) return lval_num(x.num - y.num);
	if(strcmp(op,"*") == 0) return lval_num(x.num * y.num);
	if(strcmp(op,"/") == 0){
		return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
	}
	
	return lval_err(LERR_BAD_OP);	
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

lval eval(mpc_ast_t* t){
	//재귀
	
	// tag 가 number라면 integer 로 변환하여 리턴
	if(strstr(t->tag, "number")){
		errno = 0;
		long x = strtol(t->contents,NULL,10);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}
	
	// 연산자는 항상 두번째 자식이다.
	char *op = t->children[1]->contents;
	
	// 'x'라는 long 형 변수에 3번째 자식을 저장한다.
	lval x = eval(t->children[2]);
	
	//남아있는 자식과 결합되어있는 것을 반복한다.
	int i = 3;
	while(strstr(t->children[i]->tag,"expr")){
		x = eval_op(x,op,eval(t->children[i]));
		i++;
	}
	
	return x;
}




int main(){
	/*전위표현식 parser 프로그램*/
	
	
	/*몇몇 parser 만들기*/
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lispy = mpc_new("lispy");
	
	// parser들을 정의한다.(정규 표현식)
	mpca_lang(MPCA_LANG_DEFAULT,
    "                                                     \
      number   : /-?[0-9]+/ ;                             \
      operator : '+' | '-' | '*' | '/' ;                  \
      expr     : <number> | '(' <operator> <expr>+ ')' ;  \
      lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);
	/*Information print*/
	puts("Lispy Version 1.0.0");
	puts("Press Ctrl + C to Exit\n");
	
	/*never ending loop*/
	while(1){
		char *input = readline("lispy> ");
		add_history(input);
		
		// input 값을 parse 시도
		mpc_result_t r;
		if(mpc_parse("<stdin>",input,Lispy,&r)){
			lval result = eval(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		}else{
			/*parse 실패시 error 출력과 제거*/
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	/*parser들 해제*/
	mpc_cleanup(4,Number,Operator,Expr,Lispy);
	return 0;
}