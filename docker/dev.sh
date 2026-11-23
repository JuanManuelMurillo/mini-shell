#!/usr/bin/env bash
# Entorno Linux para msh. Uso: docker/dev.sh [shell|build|run|test|valgrind|clean|rebuild-image]
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/.."

IMAGE=msh-dev
DIR="$(pwd)"

if ! command -v docker >/dev/null 2>&1; then
    echo "No se encontró 'docker'. Instala un runtime, por ejemplo:" >&2
    echo "  brew install colima docker && colima start" >&2
    echo "  (o OrbStack / Docker Desktop)" >&2
    exit 1
fi

build_image() {
    docker build -t "$IMAGE" --build-arg UID="$(id -u)" --build-arg GID="$(id -g)" "$@" "$SCRIPT_DIR"
}
docker image inspect "$IMAGE" >/dev/null 2>&1 || build_image

run() {
    local tty=()
    [ -t 0 ] && tty=(-it) || tty=(-i)
    docker run --rm "${tty[@]}" \
        --hostname msh-linux \
        --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
        -v "$DIR":/msh -w /msh "$IMAGE" "$@"
}

cmd="${1:-shell}"; shift || true
case "$cmd" in
    shell)         run bash ;;
    build)         run bash -c 'make clean >/dev/null && make' ;;
    run)           run bash -c 'make -s && bin/msh' ;;
    test)          run bash -c 'make clean >/dev/null && make -s && tests/run_tests.sh "$@"' _ "$@" ;;
    valgrind)      run bash -c 'make -s && valgrind --leak-check=full --track-fds=yes bin/msh' ;;
    clean)         run make clean ;;
    rebuild-image) build_image --no-cache ;;
    *)             run "$cmd" "$@" ;;
esac
