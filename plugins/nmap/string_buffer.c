#include "string_buffer.h"

#include <string.h>
#include <stdio.h>
#include<stdarg.h>
#include <stdlib.h>

void string_buffer_init(struct string_buffer *s_b, char *b_buff, size_t b_len)
{
    s_b->b_buff = b_buff;
    s_b->b_len = b_len;
    s_b->b_alloc = 0;
    memset(s_b->b_buff,0,b_len);
}
bool string_buffer_add_str(struct string_buffer *s_b, char *str)
{
    int len= strlen(str);

    //if str doesn't fit, return false;
    if(len+s_b->b_alloc + 1 > s_b->b_len)
    {
	return false;
    }

    //defensive
    memmove(&s_b->b_buff[s_b->b_alloc],str,len);
    s_b->b_alloc+=len;
    s_b->b_buff[s_b->b_alloc]='\0';
    return true;
}
bool string_buffer_add_fmt(struct string_buffer *s_b, char *fmt, ...)
{
   va_list v_a;

   /* Determine required size. */
   va_start(v_a, fmt);
   int len = vsnprintf(NULL, 0, fmt, v_a);
   va_end(v_a);
   
   if (len < 0 || len+s_b->b_alloc + 1 > s_b->b_len)
       return false;

   /*
     work around to not call vsnprintf directly on s_b->b_buff[s_b->b_alloc]
     not sure if strictly necessary but it ensures the buffers are distinct
   */
   char str[len+1];
   va_start(v_a, fmt);
   len = vsnprintf(str, len+1, fmt, v_a);
   va_end(v_a);
   
   if (len < 0) {
       return false;
   }
   //defensive
    memmove(&s_b->b_buff[s_b->b_alloc],str,len);
    s_b->b_alloc+=len;
    
    return true;
   
}

void string_buffer_clear(struct string_buffer *s_b)
{
    s_b->b_alloc = 0;
    memset(s_b->b_buff,0,s_b->b_len);
}

void string_buffer_resize(struct string_buffer *s_b, char *n_buff, size_t n_len)
{
    
    memset(n_buff,0,n_len);
    if(n_len >= s_b->b_alloc)
    {
	strcpy(n_buff,s_b->b_buff);
	s_b->b_buff = n_buff;
	s_b->b_len = n_len;
    }
}

int run_sb(void)
{

    char big_buf[1024];
    struct string_buffer s_b;
    string_buffer_init(&s_b,big_buf,1024);
    string_buffer_add_str(&s_b,"Hello World!\n");

    
    for(int i=0;i<100;i++)
    {
	printf("%d:%d ",i,string_buffer_add_fmt(&s_b,"Num: %d\n",i));
		
    }
    printf("\n");
    

    printf("%s",s_b.b_buff);
    printf("b_alloc:%zu\n",s_b.b_alloc);
    

    return 0;
}
