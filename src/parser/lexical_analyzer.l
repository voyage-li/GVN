%option noyywrap
%{
/*****************声明和选项设置  begin*****************/
#include <stdio.h>
#include <stdlib.h>

#include "syntax_tree.h"
#include "syntax_analyzer.h"

int lines;
int pos_start;
int pos_end;

void pass_node(char *text){
     yylval.node = new_syntax_tree_node(text);
}

/*****************声明和选项设置  end*****************/

%}


%%
 /* to do for students */
 /* two cases for you, pass_node will send flex's token to bison */
\/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+\/ {
    pos_start = pos_end;
    char * temp = yytext;
    int i = 0;
    int num = 0;
    int last = 0;
    while(temp[i] != '\0')
    {
        if(temp[i] == '\n')
        {
            num++;
            last = i;
        }
        i++;
    }
    lines += num;
    if(num == 0)
        pos_end += strlen(yytext);
    else
        pos_end = strlen(yytext)- last - 1;
}
\+ 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return ADD;}
\- 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return MINUS;}
\* 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return MULTIPLY;}
\/ 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return DIVISION;}
\< 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return SMALL;}
\> 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return BIG;}
\; 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return SEMI;}
\, 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return COMMA;}
\( 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return LSMALL;}
\) 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return RSMALL;}
\[ 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return LMIDDLE;}
\] 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return RMIDDLE;}
\{ 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return LBIG;}
\} 	{pos_start = pos_end; pos_end += 1;pass_node(yytext); return RBIG;}
\= {pos_start = pos_end; pos_end += 1;pass_node(yytext); return EQUALE;}
\<\= {pos_start = pos_end; pos_end += 2;pass_node(yytext); return SMALLEQUALE;}
\>\= {pos_start = pos_end; pos_end += 2;pass_node(yytext); return BIGEQUALE;}
\=\= {pos_start = pos_end; pos_end += 2;pass_node(yytext); return DOUBLEEQUALE;}
\!\= {pos_start = pos_end; pos_end += 2;pass_node(yytext); return NOTEQUALE;}

else {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return ELSE;}
if {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return IF;}
int {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return INT;}
return {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return RETURN;}
void {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return VOID;}
while {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return WHILE;}
float {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return FLOAT;}
[0-9]+ {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext); return INTEGER;}
[a-zA-Z]+ {pos_start = pos_end;pos_end+=strlen(yytext); pass_node(yytext); return ID;}
[0-9]+\.|[0-9]*\.[0-9]+ {pos_start = pos_end;pos_end+=strlen(yytext);pass_node(yytext);return FLOATPOINT;}
[ ] {pos_end++;}
[\t] {pos_end+=4;}
[\n] {lines++; pos_start = 0; pos_end = 0;}

. {pos_start = pos_end;pos_end++;return ERROR;}


%%
