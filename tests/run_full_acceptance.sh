#!/usr/bin/env bash
set -euo pipefail

DATE_TAG="$(date +%Y%m%d)"
RESULT_DIR="tests/results-${DATE_TAG}"
SUMMARY_FILE="${RESULT_DIR}/summary.txt"
BUILD_LOG="${RESULT_DIR}/build.log"
ACCEPTANCE_RUN_SMOKE="${ACCEPTANCE_RUN_SMOKE:-1}"

mkdir -p "${RESULT_DIR}"

log_step() {
  local step="$1"
  echo "[$(date +%H:%M:%S)] ${step}" | tee -a "${BUILD_LOG}"
}

run_step() {
  local label="$1"
  shift
  log_step "BEGIN ${label}"
  if "$@" >>"${BUILD_LOG}" 2>&1; then
    echo "PASS: ${label}" | tee -a "${SUMMARY_FILE}"
    log_step "END ${label}"
  else
    echo "FAIL: ${label}" | tee -a "${SUMMARY_FILE}"
    log_step "END ${label}"
    return 1
  fi
}

: >"${SUMMARY_FILE}"
: >"${BUILD_LOG}"

echo "NeverMind Acceptance Summary (${DATE_TAG})" >>"${SUMMARY_FILE}"
echo "======================================" >>"${SUMMARY_FILE}"

run_step "clean+build" make clean all
run_step "unit-tests" make test
run_step "integration-tests" make integration
run_step "userspace-tools" make user-tools

if [[ "${ACCEPTANCE_RUN_SMOKE}" == "1" ]]; then
  run_step "smoke-tests" make smoke
else
  echo "SKIP: smoke-tests (ACCEPTANCE_RUN_SMOKE=${ACCEPTANCE_RUN_SMOKE})" | tee -a "${SUMMARY_FILE}"
fi

if [[ -d build/test-logs ]]; then
  cp -f build/test-logs/*.log "${RESULT_DIR}/" 2>/dev/null || true
fi

echo "" >>"${SUMMARY_FILE}"
echo "Artifacts:" >>"${SUMMARY_FILE}"
echo "- Build log: ${BUILD_LOG}" >>"${SUMMARY_FILE}"
echo "- Kernel image: build/kernel.elf" >>"${SUMMARY_FILE}"
echo "- ISO image: build/nevermind-m1.iso" >>"${SUMMARY_FILE}"

echo "Acceptance completed. See ${SUMMARY_FILE}"
