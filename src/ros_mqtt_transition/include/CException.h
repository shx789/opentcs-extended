/*
** file name CException.h
*/
#ifndef _CEXCEPTION_H_
#define _CEXCEPTION_H_
#include <setjmp.h>
#include <stdlib.h>
#include <stdarg.h>
#include <execinfo.h>
#include <stdio.h>
#include <signal.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <iostream>

/*
01）SIGHUP：本信号在用户终端连接（正常或非正常）结束时发出，通常是在终端的控制进程结束时，通知同一session内的各个作业，这时它们与控制终端不再关联；
02）SIGINT：程序终止（interrupt）信号，在用户键入INTR字符（通常是Ctrl-C）时发出；
03）SIGQUIT：和SIGINT类似，但由QUIT字符（通常是Ctrl-）来控制。进程在因收到SIGQUIT退出时会产生core文件，在这个意义上类似于一个程序错误信号；
04）SIGILL：执行了非法指令。通常是因为可执行文件本身出现错误，或者试图执行数据段。堆栈溢出时也有可能产生这个信号；
05）SIGTRAP：由断点指令或其它trap指令产生，由debugger使用；
06）SIGABRT：程序自己发现错误并调用abort时产生；
06）SIGIOT：在PDP-11上由iot指令产生，在其它机器上和SIGABRT一样；
07）SIGBUS：非法地址，：包括内存地址对齐（alignment）出错。eg：访问一个四个字长的整数，但其地址不是4的倍数；
08）SIGFPE：在发生致命的算术运算错误时发出。不仅包括浮点运算错误，还包括溢出及除数为0等其它所有的算术的错误；
09）SIGKILL：用来立即结束程序的运行。本信号不能被阻塞，处理和忽略；
10）SIGUSR1：留给用户使用；
11）SIGSEGV：试图访问未分配给自己的内存，或试图往没有写权限的内存地址写数据；
12）SIGUSR2：留给用户使用；
13）SIGPIPE：Broken：pipe；
14）SIGALRM：时钟定时信号，计算的是实际的时间或时钟时间。alarm函数使用该信号；
15）SIGTERM：程序结束（terminate）信号，与SIGKILL不同的是该信号可以被阻塞和处理。通常用来要求程序自己正常退出。shell命令kill缺省产生这个信号；
17）SIGCHLD：子进程结束时，父进程会收到这个信号；
18）SIGCONT：让一个停止（stopped）的进程继续执行。本信号不能被阻塞。可以用一个handler来让程序在由stopped状态变为继续执行时完成特定的工作。例如，重新显示提示符；
19）SIGSTOP：停止（stopped）进程的执行。注意它和terminate以及interrupt的区别：该进程还未结束，只是暂停执行。本信号不能被阻塞，处理或忽略；
20）SIGTSTP：停止进程的运行，但该信号可以被处理和忽略。用户键入SUSP字符时（通常是Ctrl-Z）发出这个信号；
21）SIGTTIN：当后台作业要从用户终端读数据时，该作业中的所有进程会收到SIGTTIN信号。缺省时这些进程会停止执行；
22）SIGTTOU：类似于SIGTTIN，但在写终端（或修改终端模式）时收到；
23）SIGURG：有”紧急”数据或out-of-band数据到达socket时产生；
24）SIGXCPU：超过CPU时间资源限制。这个限制可以由getrlimit/setrlimit来读取/改变；
25）SIGXFSZ：超过文件大小资源限制；
26）SIGVTALRM：虚拟时钟信号。类似于SIGALRM，但是计算的是该进程占用的CPU时间；
27）SIGPROF：类似于SIGALRM/SIGVTALRM，但包括该进程用的CPU时间以及系统调用的时间；
28）SIGWINCH：窗口大小改变时发出；
29）SIGIO：文件描述符准备就绪，可以开始进行输入/输出操作；
30）SIGPWR：Power：failure；
*/

void WriteLog(int sig)
{
  static unsigned int time_cnt = 0;
    //获取系统时间戳
	time_t timeReal;
	time(&timeReal);
	timeReal = timeReal + 8*3600;
	tm* t = gmtime(&timeReal); 

  char data[30] ={0};
  char data1[20] = {0};
  sprintf(data,"%d-%02d-%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec); 
  sprintf(data1,"error num:%d\n",sig); 
  // 向txt文档中写入数据
  std::ofstream dataFile;
  dataFile.open("/home/robotcar/log/log.txt", std::ofstream::app);
    //std::fstream file("/home/robotcar/log/log.txt", std::ios::out);
  time_cnt++;
  if(dataFile.is_open())
  {
    dataFile <<"------------"<< time_cnt <<"------------"<< std::endl;  // 写入数据
    dataFile <<data<< std::endl;     // 写入数据
    dataFile <<data1<< std::endl;     // 写入数据
    dataFile.close();                           // 关闭文档
  }
	printf("%d-%02d-%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec); 
  printf("error num:%d\n",sig); 

}

typedef struct Except_frame
{
    jmp_buf env;
    int flag;
    void clear()
    {
       flag = 0;
       bzero(env,sizeof(env));
    }
    bool isDef()
    {
       return flag;
    }
    Except_frame()
    {
      clear();
    }
}Except_frame;
extern Except_frame* except_stack;
extern void errorDump();
extern void recvSignal(int sig);
Except_frame* except_stack = new Except_frame;

void errorDump()
{
    const int maxLevel = 200;
    void* buffer[maxLevel];
    int level = backtrace(buffer, maxLevel);
    const int SIZE_T = 1024;
    char cmd[SIZE_T] = "addr2line -C -f -e ";
    char* prog = cmd + strlen(cmd);
    readlink("/proc/self/exe", prog, sizeof(cmd) - (prog-cmd)-1);
    FILE* fp = popen(cmd, "w");
    if (!fp)
    {
        perror("popen");
        return;
    }
    for (int i = 0; i < level; ++i)
    {
        fprintf(fp, "%p\n", buffer[i]);
    }
    fclose(fp);
}
 
void recvSignal(int sig)
{
    //printf("received signal %d !!!\n",sig);
    WriteLog(sig);
    errorDump();
    siglongjmp(except_stack->env,1);
}
#define TRY \
    except_stack->flag = sigsetjmp(except_stack->env,1);\
    if(!except_stack->isDef()) \
    { \
      signal(SIGSEGV,recvSignal); \
      //printf("start use TRY\n");
#define END_TRY \
    }\
    else\
    {\
      except_stack->clear();\
    }\
    //printf("stop use TRY\n");
#define RETURN_NULL \
    } \
    else \
    { \
      except_stack->clear();\
    }\
    return NULL;
#define RETURN_PARAM  { \
      except_stack->clear();\
    }\
    return x;
#define EXIT_ZERO \
    }\
    else \
    { \
      except_stack->clear();\
    }\
    exit(0);
#endif
