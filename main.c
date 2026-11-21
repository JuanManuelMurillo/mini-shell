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

	while (1) {

		while(waitpid(-1, NULL, WNOHANG) > 0);

		fprintf(stderr, "%s", "msh> ");	/* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		if (ret == 0) break;		/* EOF */
		if (ret == -1) continue;	/* Syntax error */
		argvc = ret - 1;		/* Line */
		if (argvc == 0) continue;	/* Empty line */

		pid_t *pids = malloc(argvc * sizeof(pid_t));

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

		// Si no está en el segundo plano
		if(!bg) {
			for(int i = 0; i < argvc; i++) {
				waitpid(pids[i], &st, 0);
			}
		}

	}
	exit(0);
	return 0;
}