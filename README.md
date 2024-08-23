# minishell

  Minishell is a project of my first year of Master in Computer Science, from the subject Operating Systems. The objective was to code our own shell, manipulating processes using basic system calls such as fork(), wait(), signal(), kill(). The management of commands containing pipes is made by communicating between processes using a pipe of file descriptors. The handling of background processes was also quite interresting, we had to code our own version of ```jobs``` and ```fg```.

To compile the project, the command is : ```gcc -Wall -Wextra minishell.c libparser.a -o minishell -static -g```  
Then run ```chmod u+x ./minishell```  
Then ```./minishell```  
