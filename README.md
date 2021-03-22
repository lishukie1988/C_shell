# C_shell
 

- Summary:
	- This is a fully functional bash shell simulator written in C.
	- The following basic commands are directly handled by the shell:
		- "cd", "status", "exit"
	- All the other bash commands are handled by dynamically forked child processes.
	- Foreground mode can be toggled on & off by pressing Ctrl+Z. Once foreground mode is entered, all commands are handled as foreground commands.
	- All background processes are cleared manually from the process table by the shell once completed.
	- The program can be terminated only by entering "exit". All background processes will be terminated and cleared instantly from the process table upon termination.


- Please compile the "smallsh.c" program into an executable file using the following command:
	- gcc -std=c99 -o smallsh smallsh.c


- Please run the executable file using the following command:
	- ./smallsh