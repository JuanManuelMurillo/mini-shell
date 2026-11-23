# >msh: Minishell Unix desde cero

Práctica de **Sistemas Operativos (UPM)**: un intérprete de mandatos escrito en C para Unix, usando directamente las llamadas al sistema (`fork`, `exec`, `wait`, `pipe`, `dup`, `signal`…).

- **Enunciado:** `msh.2026b`, en [docs/msh.2026b.pdf](docs/msh.2026b.pdf)
- **Autor:** Juan Manuel Murillo López

---

## Índice

1. [Qué hace el minishell](#qué-hace-el-minishell)
2. [Estado de la implementación](#estado-de-la-implementación)
3. [Estructura y módulos](#estructura-y-módulos)
4. [Compilación y ejecución](#compilación-y-ejecución)
5. [Tests](#tests)
6. [Depuración](#depuración)

---

## Qué hace el minishell

msh lee líneas de mandatos de la **entrada estándar** y las ejecuta. Los descriptores estándar se usan así:

| Descriptor | Uso |
|---|---|
| 0: entrada estándar | Líneas de mandatos que se interpretan |
| 1: salida estándar | Resultado de los mandatos internos |
| 2: error estándar | Prompt, valor de `bgpid` y mensajes de error (con `perror`) |

### Sintaxis

```
mandato [args...] [| mandato [args...]]... [< fichero] [> fichero] [>& fichero] [&]
```

| Elemento | Significado |
|---|---|
| **Mandato** | Textos separados por blancos (espacio o tabulador). El primero es el programa; se ejecuta como hijo directo del minishell. Su valor es su estado de terminación. |
| **Secuencia** `a \| b` | La salida de cada mandato se conecta por tubería a la entrada del siguiente. El valor es el del último. |
| `< fichero` | Entrada estándar desde `fichero`. |
| `> fichero` | Salida estándar a `fichero` (se crea con modo 0666 o se trunca). |
| `>& fichero` | Error estándar a `fichero` (se crea con modo 0666 o se trunca). |
| `&` | Ejecución en segundo plano: no se espera al mandato; se guarda su pid en `bgpid` y se muestra como `[%d]`. |

Si falla alguna redirección, se notifica el error y no se ejecuta la línea.

### Mandatos internos

Se ejecutan dentro del propio minishell para que su efecto sea permanente. Solo se ejecutan en un subshell si van en background o forman parte de una secuencia sin ser el último. Todos comprueban sus argumentos y, si hay un error, lo notifican y terminan con valor distinto de 0.

| Mandato | Función |
|---|---|
| `cd [Directorio]` | Cambia de directorio (a `$HOME` si no se indica) e imprime el directorio actual con `"%s\n"`. |
| `umask [Valor]` | Imprime la máscara actual con `"%o\n"` y, si se da `Valor` (en octal), la cambia. |
| `time [Mandato]` | Muestra el tiempo de usuario, de sistema y real con `"%d.%03du %d.%03ds %d.%03dr\n"`: del minishell y sus hijos, o de `Mandato` si se indica. |
| `read Var [Var...]` | Lee una línea y asigna sus palabras a variables de entorno. La última variable recibe el resto de la línea. |

### Metacaracteres y comodines

| Expresión | Sustitución |
|---|---|
| `~` | Valor de `$HOME` |
| `~usuario` | Directorio home de `usuario`, según `/etc/passwd` |
| `$Variable` | Valor de la variable de entorno |
| `?` | Casa con un carácter cualquiera, entre los ficheros del directorio actual. No se aplica si el texto contiene `/`. Si nada casa, el texto queda igual. |

### Variables especiales

| Variable | Contenido |
|---|---|
| `prompt` | Mensaje antes de leer cada línea (por defecto `msh> `) |
| `mypid` | PID del minishell |
| `bgpid` | PID del último proceso lanzado en background |
| `status` | Valor de terminación del último mandato o secuencia en primer plano |

### Señales

Ni el minishell ni los procesos en background mueren con `SIGINT` (Ctrl+C) ni con `SIGQUIT` (Ctrl+\\). Los mandatos en primer plano conservan la acción por defecto.

---

## Estado de la implementación

| Funcionalidad | Estado | Tests |
|---|:---:|---|
| Mandato simple | ✅ | 01, 02, 10, 29 |
| Secuencias (`\|`) | ⬜ | 03, 04, 11 |
| Redirecciones (`<`, `>`, `>&`) | ⬜ | 05–07, 12–14 |
| Background (`&`, `bgpid`) | ⬜ | 15, 16 |
| Señales | ⬜ | 20 |
| Mandatos internos `cd`, `umask` | ⬜ | 08, 09, 21–24 |
| Mandato interno `time` | ⬜ | 25 |
| Mandato interno `read` | ⬜ | 26 |
| Variables especiales | ⬜ | 17–19 |
| Metacaracteres `~`, `$` | ⬜ | 27 |
| Comodín `?` | ⬜ | 28 |

---

## Estructura y módulos

```
.
├── src/
│   ├── main.c        ← Lógica del minishell (único fichero a modificar)
│   └── base/         ← Código proporcionado — NO MODIFICAR
│       ├── parser.y  ← Gramática (yacc/bison)
│       └── scanner.l ← Analizador léxico (lex/flex)
├── Makefile          ← Compilación
├── autores.txt       ← Datos del autor (se entrega junto a main.c)
├── docs/
│   └── msh.2026b.pdf ← Enunciado
├── docker/
│   ├── Dockerfile    ← Imagen Linux de desarrollo
│   └── dev.sh        ← Atajos para compilar, ejecutar y probar en Linux
├── tests/
│   ├── run_tests.sh  ← Ejecuta los tests
│   ├── lib.sh        ← Funciones de ayuda para los tests .test
│   └── cases/        ← Casos de prueba
├── build/            ← Objetos y fuentes generadas (parser.c, scanner.c, y.tab.h) — se crea al compilar
└── bin/
    └── msh           ← Ejecutable — se crea al compilar
```

### `src/base/scanner.l`: analizador léxico

Generado con `flex`. Separa la entrada en tokens:

- **Separadores:** `|`, `<`, `>`, `&` y el salto de línea, que se devuelven como caracteres sueltos.
- **Textos (`TXT`):** cualquier secuencia sin blancos ni separadores. Se copian con `strdup`.

Cuando la entrada **no** es un terminal (por ejemplo `bin/msh < fichero`), repite por stderr lo que lee. Así las trazas de prueba muestran qué línea se está ejecutando.

> No reconoce comillas: `echo "a b"` produce los textos `"a` y `b"`.

### `src/base/parser.y`: analizador gramatical

Generado con `bison --yacc`. Reconoce líneas con esta forma:

```
línea  := secuencia redirecciones [&]
secuencia := mandato ( '|' mandato )*
redirecciones := ( '<' TXT | '>' TXT | '>' '&' TXT )*
```

Las redirecciones solo pueden ir **al final de la línea**, después de toda la secuencia, y cada tipo como mucho una vez.

Exporta la única función que usa `main.c`:

```c
int obtain_order(char ****argvv, char *filev[3], int *bg);
```

| Parámetro | Contenido al volver |
|---|---|
| `argvv` | Array de mandatos terminado en `NULL`; cada `argvv[i]` es un `argv` terminado en `NULL` |
| `filev[0]` | Fichero de `<` o `NULL` |
| `filev[1]` | Fichero de `>` o `NULL` |
| `filev[2]` | Fichero de `>&` o `NULL` |
| `bg` | 1 si la línea termina en `&` |

| Valor devuelto | Significado |
|---|---|
| `0` | Fin de fichero (Ctrl+D) |
| `-1` | Error de sintaxis |
| `n > 0` | Línea correcta con `n - 1` mandatos (`1` = línea vacía) |

La memoria de `argvv` y `filev` la gestiona el parser: se libera en la siguiente llamada.

### `src/main.c`: el minishell

Bucle principal: mostrar el prompt, llamar a `obtain_order` y ejecutar la línea. Aquí se implementa todo lo descrito en [Qué hace el minishell](#qué-hace-el-minishell).

### `Makefile`

| Objetivo | Acción |
|---|---|
| `make` | Genera `parser.c`, `y.tab.h` y `scanner.c` en `build/`, compila los `.o` allí y enlaza `bin/msh` |
| `make clean` | Borra `build/` y `bin/` |

---

## Compilación y ejecución

La práctica se evalúa en **Linux**. Aunque compila en macOS, el comportamiento cambia: por ejemplo, macOS tiene `/usr/bin/cd`, `/usr/bin/umask` y `/usr/bin/read`, que ocultan que faltan los mandatos internos, y los mensajes de error son distintos. Por eso hay un entorno Linux en Docker.

### Requisitos (macOS)

```bash
brew install colima docker
colima start          # arrancar la VM Linux (una vez por sesión)
```

O, para que arranque sola al iniciar sesión: `brew services start colima`.

> Colima solo comparte `$HOME` con la máquina virtual: el proyecto debe estar dentro de tu carpeta de usuario.

### Entorno Linux: `docker/dev.sh`

La primera ejecución construye la imagen `msh-dev`: Debian con `gcc`, `make`, `bison`, `flex`, `gdb`, `valgrind`, `strace` y las páginas `man` de POSIX. El proyecto se monta en `/msh` y los ficheros generados siguen perteneciendo a tu usuario.

| Orden | Acción |
|---|---|
| `docker/dev.sh` | Shell bash dentro de Linux |
| `docker/dev.sh run` | Compila y ejecuta `msh` |
| `docker/dev.sh build` | Recompila desde cero |
| `docker/dev.sh test [patrón]` | Recompila y pasa los tests (todos o los que contengan `patrón`) |
| `docker/dev.sh valgrind` | Ejecuta `msh` con valgrind (fugas de memoria y descriptores abiertos) |
| `docker/dev.sh clean` | `make clean` |
| `docker/dev.sh rebuild-image` | Reconstruye la imagen desde cero |
| `docker/dev.sh <orden...>` | Ejecuta cualquier orden en el contenedor |

### Uso

```bash
docker/dev.sh run
msh> ls -l | wc -l
msh> sleep 10 &
[1234]
msh> cd /tmp
/tmp
```

**Salir:** Ctrl+D (fin de fichero).

Para ejecutar un guion de órdenes: `bin/msh < fichero`. Cada línea se repite por stderr.

### En un Linux nativo

```bash
make
bin/msh
```

---

## Tests

```bash
docker/dev.sh test          # todos
docker/dev.sh test pipe     # solo los que contienen "pipe"
```

Cada test se ejecuta en un directorio vacío. Hay dos tipos:

| Tipo | Funcionamiento |
|---|---|
| `NN_nombre.msh` | Se ejecuta con `msh` y con `/bin/sh` (dash, POSIX) y se comparan la salida estándar y los ficheros creados. Si existe `NN_nombre.sh`, se usa como entrada de `sh` (cuando la sintaxis o la salida esperada difieren de `sh`). |
| `NN_nombre.test` | Script bash con comprobaciones propias, para lo que no se puede comparar con `sh`: pids, tiempos, señales, permisos, mensajes por stderr… Las funciones de ayuda están en [tests/lib.sh](tests/lib.sh). |

### Añadir un test

**Comparando con `sh`:**
```bash
printf 'echo hola | wc -c\n' > tests/cases/30_mi_test.msh
```

**Con comprobaciones propias:**
```bash
cat > tests/cases/31_mi_test.test <<'EOF'
msh_run <<'IN'
echo hola
IN
expect_out hola
EOF
```

Funciones disponibles en [tests/lib.sh](tests/lib.sh):

| Función | Comprueba |
|---|---|
| `msh_run` | Ejecuta msh con las órdenes de la entrada (heredoc) |
| `expect_out l1 l2…` | La salida estándar es exactamente esas líneas |
| `expect_line_re re` | Alguna línea de la salida casa con la regex |
| `expect_eq a b` / `expect_ne a b` | Igualdad / desigualdad |
| `expect_err_msg` | Se escribió algún mensaje de error por stderr (sin contar prompt ni eco) |
| `expect_st_nonzero n` | Hay `n` líneas `st=N` con `N` distinto de 0 |
| `line n` | Línea `n` de la salida |

> El scanner no admite comillas: escribe las órdenes de los tests sin ellas.

---

## Depuración

Todo esto se ejecuta dentro de `docker/dev.sh`:

```bash
gdb bin/msh                               # depurador
valgrind --track-fds=yes bin/msh          # descriptores sin cerrar al salir
strace -f -e trace=process,pipe,dup2,open bin/msh   # llamadas al sistema de msh y sus hijos
man 2 fork      man 3p pipe              # manual de Linux / POSIX
```

**Problemas frecuentes:**
- **Una secuencia se queda colgada:** algún extremo de escritura de una tubería sigue abierto, en el padre o en algún hijo. Usa `valgrind --track-fds=yes` o `ls -l /proc/<pid>/fd`.
- **Hay dos prompts o líneas repetidas:** un hijo cuyo `exec` falló no hizo `exit` y volvió al bucle principal.
- **Ctrl+C cierra msh:** falta ignorar `SIGINT` en el minishell.
- **Ctrl+C no mata un mandato en primer plano:** el hijo heredó la señal ignorada; restaura `SIG_DFL` antes del `exec`.
