#! /bin/sh

# 1. generate a module with "wasm-tools smith"
# 2. run it with toywasm and the spec interpreter
# 3. check if the results matches
# 4. if matches, repeat from #1

set -e
TOYWASM=${TOYWASM:-./b/toywasm}
SPEC_INTERP=${SPEC_INTERP:-~/git/wasm/spec/interpreter/wasm}
WASM_TOOLS=${WASM_TOOLS:-wasm-tools}

LOG=$(mktemp)

# toywasm's trap messages look like:
# Error: [trap] unreachable executed (4): unreachable at 00007b

# spec interpreter's trap messages look like:
# test.wasm:0x7b: runtime trap: unreachable executed

TEST_MODULE=test.wasm

TOTAL=0
TRAP=0
ERROR=0
NO_IMPORT=0
TIMEDOUT=0
while :; do
    dd if=/dev/urandom bs=10240 count=100 2> /dev/null | ${WASM_TOOLS} smith \
    --max-imports=0 \
    --ensure-termination \
    --fuel=100 \
    --maybe-invalid \
    --reference-types-enabled=true \
    --simd-enabled=true \
    -o ${TEST_MODULE}
    set +e
    # we specify --max-frames 100 because wasm-tools smith sometimes
    # generates an infinite recursive call.
    ${TOYWASM} --max-frames 100 --timeout 5000 --load ${TEST_MODULE} > ${LOG} 2>&1
    RESULT=$?
    set -e
    TOYWASM_TRAP=
    TOYWASM_ERROR=
    if [ ${RESULT} -ne 0 ]; then
        if grep -F 'Error: [trap]' ${LOG} > /dev/null; then
            TOYWASM_TRAP=$(sed -ne '/\[trap\]/{s/^Error: \[trap\] //;s/ ([0-9]*).*//;p;}' ${LOG})
            TRAP=$((TRAP + 1))
        elif grep -F 'instantiation error: No entry for import ' ${LOG} > /dev/null; then
            TOYWASM_ERROR="unknown import"
            NO_IMPORT=$((NO_IMPORT + 1))
        elif grep -F 'load/validation error: ' ${LOG} > /dev/null; then
            TOYWASM_ERROR="load error"
            ERROR=$((ERROR + 1))
        elif grep -F 'execution timed out' ${LOG} > /dev/null; then
            TOYWASM_ERROR="timed out"
            TIMEDOUT=$((TIMEDOUT + 1))
        else
            cat ${LOG}
            exit 1
        fi
    fi
    # if toywasm timed out, don't run spec interpreter.
    if [ "${TOYWASM_ERROR}" = "timed out" ]; then
        # .. but save the module for later investigation.
        mv ${TEST_MODULE} timedout${TOTAL}.wasm
    else
        set +e
        ${SPEC_INTERP} ${TEST_MODULE} > ${LOG} 2>&1
        RESULT2=$?
        set -e
        SPEC_INTERP_TRAP=
        SPEC_INTERP_ERROR=
        if [ ${RESULT2} -ne 0 ]; then
            if grep -F 'runtime trap' ${LOG} > /dev/null; then
                SPEC_INTERP_TRAP=$(sed -ne '/.*runtime trap: \(.*\)/s//\1/;p' ${LOG})
            elif grep -F 'link failure: unknown import ' ${LOG} > /dev/null; then
                SPEC_INTERP_ERROR="unknown import"
            elif grep -F 'decoding error: ' ${LOG} > /dev/null; then
                SPEC_INTERP_ERROR="load error"
            elif grep -F 'resource exhaustion: call stack exhausted' ${LOG} > /dev/null; then
                SPEC_INTERP_TRAP="stack overflow"
            else
                cat ${LOG}
                exit 1
            fi
            # normalize a bit by removing trailing numbers like:
            #   uninitialized element 4767
            #   undefined element -69319001
            SPEC_INTERP_TRAP=$(printf "${SPEC_INTERP_TRAP}" | sed -e 's/ -*[0-9][0-9]*$//')
            if [ "${SPEC_INTERP_TRAP}" != "${TOYWASM_TRAP}" ]; then
                echo "TOYWASM_TRAP    : '${TOYWASM_TRAP}'"
                echo "SPEC_INTERP_TRAP: '${SPEC_INTERP_TRAP}'"
                exit 1
            fi
            if [ "${SPEC_INTERP_ERROR}" != "${TOYWASM_ERROR}" ]; then
                echo "TOYWASM_ERROR    : '${TOYWASM_ERROR}'"
                echo "SPEC_INTERP_ERROR: '${SPEC_INTERP_ERROR}'"
                exit 1
            fi
            test ${RESULT} -ne 0
        else
            test ${RESULT} -eq 0
        fi
    fi
    TOTAL=$((TOTAL + 1))
    echo "total ${TOTAL} trap ${TRAP} error ${ERROR} no-import ${NO_IMPORT} timedout ${TIMEDOUT}"
done
