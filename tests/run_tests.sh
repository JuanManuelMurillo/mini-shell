#!/usr/bin/env bash
# Ejecuta los tests de tests/cases. Dos tipos:
#   NOMBRE.msh [+ NOMBRE.sh]  Compara stdout y ficheros creados con /bin/sh.
#                             (No se compara el código de salida de msh: el enunciado no lo define.)
#                             Si existe NOMBRE.sh se usa como entrada de referencia para sh.
#   NOMBRE.test               Script bash con aserciones propias (ver tests/lib.sh).
# Uso: tests/run_tests.sh [patrón]     p. ej.  tests/run_tests.sh read
cd "$(dirname "$0")/.."
ROOT=$(pwd)
MSH=$ROOT/msh
[ -x "$MSH" ] || { echo "No existe ./msh (ejecuta make)"; exit 1; }

TIMEOUT=""; command -v timeout >/dev/null && TIMEOUT="timeout 5"
pass=0; fail=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# run <shell> <input> <tag>: ejecuta en un dir limpio y vuelca stdout + ficheros creados
run() {
    local d="$tmp/$3"; rm -rf "$d"; mkdir -p "$d"
    (cd "$d" && $TIMEOUT $1 <"$2" >"$tmp/$3.out" 2>/dev/null
     for f in $(ls); do echo "--- $f"; cat "$f"; done >>"$tmp/$3.out")
}

result() {  # result <nombre> <ok?> <fichero con detalles>
    if [ "$2" = 0 ]; then
        printf '\e[32mPASS\e[0m %s\n' "$1"; pass=$((pass+1))
    else
        printf '\e[31mFAIL\e[0m %s\n' "$1"; fail=$((fail+1))
        sed 's/^/    /' "$3" | head -25
    fi
}

for t in $(ls tests/cases/*${1:-}*.msh tests/cases/*${1:-}*.test 2>/dev/null | sort); do
    name=$(basename "$t"); name=${name%.*}
    case "$t" in
    *.msh)
        ref=$t; [ -f "tests/cases/$name.sh" ] && ref="tests/cases/$name.sh"
        run "$MSH" "$ROOT/$t" msh
        run sh "$ROOT/$ref" sh
        diff -u --label "sh (esperado)" --label msh "$tmp/sh.out" "$tmp/msh.out" | tail -n +3 > "$tmp/diff"
        result "$name" "$([ -s "$tmp/diff" ] && echo 1 || echo 0)" "$tmp/diff" ;;
    *.test)
        rm -rf "$tmp/w" "$tmp/t"; mkdir -p "$tmp/w" "$tmp/t"
        (cd "$tmp/w" && MSH=$MSH T=$tmp/t ROOT=$ROOT ${TIMEOUT:+timeout 30} \
            bash -c 'source "$ROOT/tests/lib.sh"; source "$1"; [ $FAILS -eq 0 ]' _ "$ROOT/$t") > "$tmp/msgs" 2>&1
        rc=$?; [ $rc = 124 ] && echo "✗ timeout (¿msh se queda bloqueado?)" >> "$tmp/msgs"
        result "$name" "$([ $rc = 0 ] && echo 0 || echo 1)" "$tmp/msgs" ;;
    esac
done
echo "----"; echo "$pass pasados, $fail fallidos"
[ "$fail" -eq 0 ]
