/*-
 * main.c
 * Minishell C source
 * Shows how to use "obtain_order" input interface function.
 *
 * Copyright (c) 1993-2002-2019, Francisco Rosales <frosal@fi.upm.es>
 * Todos los derechos reservados.
 *
 * Publicado bajo Licencia de Proyecto Educativo Práctico
 * <http://laurel.datsi.fi.upm.es/~ssoo/LICENCIA/LPEP>
 *
 * Queda prohibida la difusión total o parcial por cualquier
 * medio del material entregado al alumno para la realización
 * de este proyecto o de cualquier material derivado de este,
 * incluyendo la solución particular que desarrolle el alumno.
 *
 * DO NOT MODIFY ANYTHING OVER THIS LINE
 * THIS FILE IS TO BE MODIFIED
 */

#include <stddef.h>			/* NULL */
#include <stdio.h>			/* setbuf, printf */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

extern int obtain_order();		/* See parser.y for description */

int main(void)
{
	char ***argvv = NULL;
	int argvc;
	char **argv = NULL;
	int argc;
	char *filev[3] = { NULL, NULL, NULL };
	int bg;
	int ret;
	pid_t pid;
	int fd[2];
	int prev_in;
	int st;

	setbuf(stdout, NULL);			/* Unbuffered */
	setbuf(stdin, NULL);

	// El msh ignora las señales de teclado (se hereda en los hijos)
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	while (1) {

		while(waitpid(-1, NULL, WNOHANG) > 0);

		fprintf(stderr, "%s", "msh> ");	/* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		if (ret == 0) break;		/* EOF */
		if (ret == -1) continue;	/* Syntax error */
		argvc = ret - 1;		/* Line */
		if (argvc == 0) continue;	/* Empty line */

		pid_t *pids = malloc(argvc * sizeof(pid_t));

		int redir[3] = { -1, -1, -1 };
		int redir_error = 0;

		if (filev[0]) {
			if ((redir[0] = open(filev[0], O_RDONLY)) == -1) {
				perror(filev[0]);
				redir_error = 1;
			}
		}

		if(!redir_error && filev[1]) {
			if ((redir[1] = creat(filev[1], 0666)) == -1) {
				perror(filev[1]);
				redir_error = 1;
			}
		}

		if(!redir_error && filev[2]) {
			if ((redir[2] = creat(filev[2], 0666)) == -1) {
				perror(filev[2]);
				redir_error = 1;
			}
		}

		if (redir_error) {
			for (int k = 0; k < 3; k++) {
				if (redir[k] != -1) close(redir[k]);
			}
			free(pids);
			continue;
		}

		prev_in = -1;

		for(int i = 0; i < argvc; i++) {
			argv = argvv[i];
			int is_last = (i == argvc - 1);
			
			if(!is_last) {
				if(pipe(fd) == -1) {
					perror("pipe");
					exit(1);
				}
			}

			pid = fork();

			switch(pid){
				// Error en el fork
				case -1:
					perror("fork");
					exit(1);

				// Proceso hijo
				case 0:

					// En primer plano se restaura la acción por defecto
					// En background se mantiene ignorada
					if(!bg) {
						sa.sa_handler = SIG_DFL;
						sigaction(SIGINT, &sa, NULL);
						sigaction(SIGQUIT, &sa, NULL);
					}

					if(i == 0 && redir[0] != -1) {
						dup2(redir[0], 0);
					}

					if(is_last && redir[1] != -1) {
						dup2(redir[1], 1);
					}

					if(redir[2] != -1) {
						dup2(redir[2], 2);
					}

					close(redir[0]);
					close(redir[1]);
					close(redir[2]);

					if(prev_in != -1) {
						dup2(prev_in, 0);
						close(prev_in);
					}

					if(!is_last) {
						dup2(fd[1], 1);
						close(fd[0]);
						close(fd[1]);
					}
					execvp(argv[0], argv);
					perror(argv[0]);
					exit(1);

				// Proceso padre
				default: 
					if(prev_in != -1) {
						close(prev_in);
					}

					if(!is_last) {
						close(fd[1]);
						prev_in = fd[0];
					}
					pids[i] = pid;
			}
		}
		
		close(redir[0]);
		close(redir[1]);
		close(redir[2]);


		// Si no está en el segundo plano
		if(!bg) {
			for(int i = 0; i < argvc; i++) {
				waitpid(pids[i], &st, 0);
			}
		} else {
			// Buffer estático, putenv guarda el puntero
			static char str[32];

			// Guarda el pid del último mandato en la variable bgpid
			snprintf(str, sizeof(str), "bgpid=%d", pids[argvc - 1]);
			putenv(str);

			// Muestra el pid por el estandar error
			fprintf(stderr, "[%d]\n", pids[argvc - 1]);
		}

		free(pids);

	}
	exit(0);
	return 0;
}