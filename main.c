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
#include <stdio.h>			/* setbuf, printf, fgets */
#include <stdlib.h>			/* malloc, getenv, putenv, strtol */
#include <string.h>			/* strcmp, strtok, strlen */
#include <stdbool.h>			/* bool */
#include <limits.h>			/* PATH_MAX */
#include <unistd.h>			/* fork, exec, pipe, dup, chdir */
#include <fcntl.h>			/* open, creat */
#include <signal.h>			/* sigaction */
#include <sys/types.h>			/* pid_t, mode_t */
#include <sys/wait.h>			/* waitpid */
#include <sys/stat.h>			/* umask */
#include <sys/times.h>			/* times */

extern int obtain_order();		/* See parser.y for description */


// ==========================
// --- Variables globales ---
// ==========================

// Instante de arranque del msh (tiempo real de "time" sin argumentos)
static clock_t start_ticks;

// 1 si este proceso es un subshell que ejecuta un mandato interno
static int is_subshell = 0;


// ============================
// --- cd (Mandato Interno) ---
// ============================

// cd [Directorio]
static int int_cd(int argc, char **argv) {
	// Comprobar el número de argumentos
	if(argc > 2) {
		fprintf(stderr, "Número de argumentos incorrecto\n");
		return 1;
	}

	// Construir la dirección
	char *dir;

	if(argc == 2) {
		// Si se especificó una dirección
		dir = argv[1];
	} else {
		// Utilizar la variable de entorno
		dir = getenv("HOME");

		if(dir == NULL) {
			fprintf(stderr, "No se encontró la variable de entorno HOME\n");
			return 1;
		}
	}

	// Cambiar de directorio
	if(chdir(dir) == -1) {
		perror("chdir");
		return 1;
	}

	// Obtener el directorio actual de trabajo
	char cwd[PATH_MAX];

	if(getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd");
		return 1;
	}

	printf("%s\n", cwd);

	return 0;
}


// ===============================
// --- umask (Mandato Interno) ---
// ===============================

// umask [Valor]
static int int_umask(int argc, char **argv) {
	// Comprobar el número de argumentos
	if(argc > 2) {
		fprintf(stderr, "Número de argumentos incorrecto\n");
		return 1;
	}

	char *endptr;
	long new_mask = 0;
	mode_t old_mask;

	if(argc == 2) {
		// Convertir y validar el valor (octal)
		new_mask = strtol(argv[1], &endptr, 8);

		if(endptr == argv[1] || *endptr != '\0' || new_mask < 0 || new_mask > 0777) {
			fprintf(stderr, "Valor no válido para umask\n");
			return 1;
		}

		// Poner la nueva máscara y obtener la anterior
		old_mask = umask((mode_t) new_mask);
	} else {
		// Solo consultar -> "resetear" la máscara para obtener la anterior
		old_mask = umask(0);
		umask(old_mask);
	}

	printf("%o\n", old_mask);

	return 0;
}


// ==============================
// --- time (Mandato Interno) ---
// ==============================

// Imprime usuario, sistema y real (en ticks) con el formato de time
static void print_times(clock_t user_ticks, clock_t sys_ticks, clock_t real_ticks) {
	long ticks_per_sec = sysconf(_SC_CLK_TCK);

	// Convertir ticks a milisegundos
	long user_ms = user_ticks * 1000 / ticks_per_sec;
	long sys_ms = sys_ticks * 1000 / ticks_per_sec;
	long real_ms = real_ticks * 1000 / ticks_per_sec;

	printf("%d.%03du %d.%03ds %d.%03dr\n",
		(int)(user_ms / 1000), (int)(user_ms % 1000),
		(int)(sys_ms / 1000), (int)(sys_ms % 1000),
		(int)(real_ms / 1000), (int)(real_ms % 1000));
}

// time [Mandato]
static int int_time(int argc, char **argv) {
	struct tms tms_now;
	struct tms before, after;
	clock_t real_now, real_start, real_end;
	int cmd_status;

	// Sin argumentos: consumo del msh y sus hijos
	if(argc == 1) {
		real_now = times(&tms_now);

		print_times(tms_now.tms_utime + tms_now.tms_cutime,
		            tms_now.tms_stime + tms_now.tms_cstime,
		            real_now - start_ticks);
		return 0;
	}

	// Con Mandato -> medir antes y después de ejecutarlo
	real_start = times(&before);

	pid_t pid = fork();

	switch(pid) {
		// Error en el fork
		case -1:
			perror("fork");
			return 1;

		// Proceso hijo
		case 0:
			// Si time se ejecuta en el propio msh, el mandato es de primer plano
			if(!is_subshell) {
				struct sigaction sa;
				sa.sa_handler = SIG_DFL;
				sigemptyset(&sa.sa_mask);
				sa.sa_flags = 0;
				sigaction(SIGINT, &sa, NULL);
				sigaction(SIGQUIT, &sa, NULL);
			}

			execvp(argv[1], argv + 1);
			perror(argv[1]);
			exit(1);
	}

	// Proceso padre: esperar al mandato
	waitpid(pid, &cmd_status, 0);

	real_end = times(&after);

	// Consumo del mandato = diferencia de los tiempos de los hijos
	print_times(after.tms_cutime - before.tms_cutime,
	            after.tms_cstime - before.tms_cstime,
	            real_end - real_start);

	if(WIFEXITED(cmd_status)) {
		return WEXITSTATUS(cmd_status);
	}
	return 1;
}


// ==============================
// --- read (Mandato Interno) ---
// ==============================

// Asigna una variable de entorno con putenv
static int put_var(char *name, char *value) {

	char *env_entry = malloc(strlen(name) + strlen(value) + 2);

	if(env_entry == NULL) {
		perror("malloc");
		return 1;
	}

	sprintf(env_entry, "%s=%s", name, value);
	putenv(env_entry);
	return 0;
}

// read Variable [Variable...]
static int int_read(int argc, char **argv) {
	// Comprobar el número de argumentos
	if(argc < 2) {
		fprintf(stderr, "Falta el nombre de la variable\n");
		return 1;
	}

	char line[4096];

	// Leer una línea del stdin
	if(fgets(line, sizeof(line), stdin) == NULL) {
		return 1;
	}

	// Quitar salto de línea
	line[strcspn(line, "\n")] = '\0';

	// Final real de la línea
	char *line_end = line + strlen(line);

	// En donde empieza el resto para la última variable
	char *rest = line;

	// Todas las variables menos la última reciben una palabra
	for(int i = 1; i < argc - 1; i++) {
		char *word = strtok(i == 1 ? line : NULL, " \t");

		// Si hay menos palabras que variables, las restantes no se modifican
		if(word == NULL) {
			return 0;
		}

		if(put_var(argv[i], word) != 0) {
			return 1;
		}

		// El resto empieza después de esta palabra
		rest = word + strlen(word);

		if(rest < line_end) {
			rest++;
		}
	}

	// Saltar los blancos iniciales del resto
	rest += strspn(rest, " \t");

	// Si queda algo, es para la última variable
	if(*rest != '\0') {
		if(put_var(argv[argc - 1], rest) != 0) {
			return 1;
		}
	}

	return 0;
}


// ========================================
// --- Funciones para Mandatos Internos ---
// ========================================

// Indica si el mandato es interno
static bool is_builtin(char **argv) {
	char *builtin_names[] = {"cd", "umask", "time", "read"};

	for(int i = 0; i < 4; i++) {
		if(strcmp(builtin_names[i], argv[0]) == 0) {
			return true;
		}
	}
	return false;
}

// Ejecuta el mandato interno y devuelve su valor de terminación
static int run_builtin(char **argv) {
	// Contar los argumentos
	int argc = 0;
	while(argv[argc] != NULL) {
		argc++;
	}

	if(strcmp(argv[0], "cd") == 0) {
		return int_cd(argc, argv);
	}

	if(strcmp(argv[0], "umask") == 0) {
		return int_umask(argc, argv);
	}

	if(strcmp(argv[0], "time") == 0) {
		return int_time(argc, argv);
	}

	if(strcmp(argv[0], "read") == 0) {
		return int_read(argc, argv);
	}

	return 1;
}


// ===========
// --- msh ---
// ===========

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

	// Guardar el instante de arranque (para time)
	struct tms tms_start;
	start_ticks = times(&tms_start);

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
		// Recoger los procesos en background que ya terminaron
		while(waitpid(-1, NULL, WNOHANG) > 0);

		fprintf(stderr, "%s", "msh> ");	/* Prompt */
		ret = obtain_order(&argvv, filev, &bg);
		if (ret == 0) break;		/* EOF */
		if (ret == -1) continue;	/* Syntax error */
		argvc = ret - 1;		/* Line */
		if (argvc == 0) continue;	/* Empty line */

		pid_t *pids = malloc(argvc * sizeof(pid_t));
		int st_builtin = 0;


		int redir[3] = { -1, -1, -1 };
		int redir_failed = 0;

		// < fichero
		if(filev[0]) {
			if((redir[0] = open(filev[0], O_RDONLY)) == -1) {
				perror(filev[0]);
				redir_failed = 1;
			}
		}

		// > fichero
		if(!redir_failed && filev[1]) {
			if((redir[1] = creat(filev[1], 0666)) == -1) {
				perror(filev[1]);
				redir_failed = 1;
			}
		}

		// >& fichero
		if(!redir_failed && filev[2]) {
			if((redir[2] = creat(filev[2], 0666)) == -1) {
				perror(filev[2]);
				redir_failed = 1;
			}
		}

		// Si alguna redirección falla, se suspende la línea
		if(redir_failed) {
			for(int k = 0; k < 3; k++) {
				if(redir[k] != -1) close(redir[k]);
			}
			free(pids);
			continue;
		}


		prev_in = -1;

		for(int i = 0; i < argvc; i++) {
			argv = argvv[i];
			int is_last = (i == argvc - 1);

			// Mandato interno en el propio msh
			if(is_last && !bg && is_builtin(argv)) {
				// No lee de la tubería anterior
				if(prev_in != -1) {
					close(prev_in);
				}

				// Guarda los descriptores del msh para restaurarlos después
				int saved_in = dup(0);
				int saved_out = dup(1);
				int saved_err = dup(2);

				// Aplica las redirecciones
				if(i == 0 && redir[0] != -1) {
					dup2(redir[0], 0);
				}

				if(redir[1] != -1) {
					dup2(redir[1], 1);
				}

				if(redir[2] != -1) {
					dup2(redir[2], 2);
				}

				st_builtin = run_builtin(argv);

				// Restaura los descriptores del msh
				dup2(saved_in, 0);
				dup2(saved_out, 1);
				dup2(saved_err, 2);
				close(saved_in);
				close(saved_out);
				close(saved_err);

				// No hay proceso al que esperar
				pids[i] = -1;
				break;
			}

			// Tubería hacia el siguiente mandato
			if(!is_last) {
				if(pipe(fd) == -1) {
					perror("pipe");
					exit(1);
				}
			}

			pid = fork();

			switch(pid) {
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

					// Redirecciones: < al primero, > al último, >& a todos
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

					// Entrada desde la tubería anterior
					if(prev_in != -1) {
						dup2(prev_in, 0);
						close(prev_in);
					}

					// Salida hacia la tubería siguiente
					if(!is_last) {
						dup2(fd[1], 1);
						close(fd[0]);
						close(fd[1]);
					}

					// Mandato interno en un subshell
					if(is_builtin(argv)) {
						is_subshell = 1;
						exit(run_builtin(argv));
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

		// El padre cierra sus copias de las redirecciones
		close(redir[0]);
		close(redir[1]);
		close(redir[2]);


		if(!bg) {
			// Primer plano -> esperar a todos los mandatos de la secuencia
			for(int i = 0; i < argvc; i++) {
				if(pids[i] != -1) {
					waitpid(pids[i], &st, 0);
				}
			}
		} else {
			// Buffer estático, putenv guarda el puntero
			static char bgpid_env[32];

			// Guarda el pid del último mandato en la variable bgpid
			snprintf(bgpid_env, sizeof(bgpid_env), "bgpid=%d", pids[argvc - 1]);
			putenv(bgpid_env);

			// Muestra el pid por el estándar error
			fprintf(stderr, "[%d]\n", pids[argvc - 1]);
		}

		free(pids);
	}

	exit(0);
	return 0;
}
