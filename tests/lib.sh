# Utilidades para tests de tipo NOMBRE.test (se cargan con "source").
# Variables disponibles: MSH (ruta a msh), T (dir temporal fuera del dir de trabajo).
# El test se ejecuta en un directorio de trabajo vacío.

FAILS=0
TIMEOUT=""; command -v timeout >/dev/null && TIMEOUT="timeout 10"

fail() { echo "✗ $*"; FAILS=$((FAILS+1)); }

# msh_run <<'EOF' ... EOF  → ejecuta msh con esas órdenes. Deja $RC, $T/out, $T/err
msh_run() {
    cat > "$T/input"
    $TIMEOUT "$MSH" < "$T/input" > "$T/out" 2> "$T/err"
    RC=$?
}

# Línea N de stdout
line() { sed -n "${1}p" "$T/out"; }

# stdout exactamente igual a las líneas dadas
expect_out() {
    local exp; exp=$(printf '%s\n' "$@")
    if [ "$(cat "$T/out")" != "$exp" ]; then
        fail "stdout distinto de lo esperado"
        diff -u --label esperado --label msh <(echo "$exp") "$T/out" | tail -n +3 | sed 's/^/      /' | head -15
    fi
}

# Alguna línea de stdout casa por completo con la regex
expect_line_re() { grep -Eqx -- "$1" "$T/out" || fail "ninguna línea de stdout casa con /$1/"; }

expect_eq() { [ "$1" = "$2" ] || fail "${3:-valor}: esperado '$2', obtenido '$1'"; }
expect_ne() { [ "$1" != "$2" ] || fail "${3:-valor}: no debería ser '$2'"; }

# stderr sin prompts, sin eco de la entrada y sin "[pid]" → mensajes de error reales
err_extra() {
    sed 's/msh> //g' "$T/err" | grep -vxF -f "$T/input" | grep -vxE '\[[0-9]+\]' | grep -v '^$'
}
expect_err_msg() { [ -n "$(err_extra)" ] || fail "${1:-no se notificó ningún error por stderr}"; }

alive() { local s; s=$(ps -o stat= -p "$1" 2>/dev/null) && [ -n "$s" ] && [[ "$s" != Z* ]]; }

# Milisegundos actuales
now_ms() { echo $(( $(date +%s%N) / 1000000 )); }

# Todas las líneas "st=N" de stdout deben tener N numérico distinto de 0 (y debe haber $1 de ellas)
expect_st_nonzero() {
    local n; n=$(grep -c '^st=' "$T/out")
    [ "$n" = "$1" ] || fail "se esperaban $1 líneas st=N, hay $n"
    grep '^st=' "$T/out" | grep -vqxE 'st=[1-9][0-9]*' && fail "status debería ser numérico y distinto de 0: $(grep '^st=' "$T/out" | tr '\n' ' ')"
    return 0
}
